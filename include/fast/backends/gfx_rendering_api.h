#pragma once

#include <stdint.h>

#include <unordered_map>
#include <set>
#include "imconfig.h"
#include "fast/toon_shading.h"

namespace Fast {
struct ShaderProgram;

struct GfxClipParameters {
    bool z_is_from_0_to_1;
    bool invertY;
};

enum FilteringMode { FILTER_THREE_POINT, FILTER_LINEAR, FILTER_NONE };

// SOH [Enhancement] World light casting: per-draw stencil mode for the Wind Waker-style stencil
// light-volume technique. The interpreter pushes this via SetStencilMode (from a gSPStencil command);
// backends apply the matching stencil state in their per-draw path. Off (0) is normal rendering, so
// ordinary draws are unaffected. These values must match the WorldLighting policy module.
enum class StencilMode {
    Off = 0,        // no stencil test/write (normal rendering)
    VolumeIncr = 1, // mask: stencil += 1 where a volume face fails the depth test (z-fail)
    VolumeDecr = 2, // mask: stencil -= 1 where a volume face fails the depth test (z-fail)
    Composite = 3,  // draw where stencil != 0, zeroing it as it goes (self-clearing composite)
    // SOH [Enhancement] Actor shadows: single-layer "paint once per tap" mask. A fragment passes only
    // where the stored stencil is below the per-draw ref, then writes the ref. With a fresh, increasing
    // ref per shadow tap this paints each pixel exactly once per tap (overlapping limbs don't blotch),
    // while each successive tap (higher ref) re-passes and adds one accumulation layer (soft penumbra).
    ShadowMask = 4,
};

// A hash function used to hash a: pair<float, float>
struct hash_pair_ff {
    size_t operator()(const std::pair<float, float>& p) const {
        const auto hash1 = std::hash<float>{}(p.first);
        const auto hash2 = std::hash<float>{}(p.second);

        // If hash1 == hash2, their XOR is zero.
        return (hash1 != hash2) ? hash1 ^ hash2 : hash1;
    }
};

class GfxRenderingAPI {
  public:
    virtual ~GfxRenderingAPI() = default;
    virtual const char* GetName() = 0;
    virtual int GetMaxTextureSize() = 0;
    virtual GfxClipParameters GetClipParameters() = 0;
    virtual void UnloadShader(ShaderProgram* oldPrg) = 0;
    virtual void LoadShader(ShaderProgram* newPrg) = 0;
    virtual ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint32_t shaderId1) = 0;
    virtual ShaderProgram* LookupShader(uint64_t shaderId0, uint32_t shaderId1) = 0;
    virtual void ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) = 0;
    virtual uint32_t NewTexture() = 0;
    virtual void SelectTexture(int tile, uint32_t textureId) = 0;
    virtual void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) = 0;
    virtual void SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) = 0;
    virtual void SetDepthTestAndMask(bool depth_test, bool z_upd) = 0;
    virtual void SetZmodeDecal(bool decal) = 0;
    virtual void SetViewport(int x, int y, int width, int height) = 0;
    virtual void SetScissor(int x, int y, int width, int height) = 0;
    virtual void SetUseAlpha(bool useAlpha) = 0;
    virtual void DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) = 0;
    virtual void Init() = 0;
    virtual void OnResize() = 0;
    virtual void StartFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void FinishRender() = 0;
    virtual int CreateFramebuffer() = 0;
    virtual void UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                             bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                             bool can_extract_depth) = 0;
    virtual void StartDrawToFramebuffer(int fbId, float noiseScale) = 0;
    virtual void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0,
                                 int dstY0, int dstX1, int dstY1) = 0;
    virtual void ClearFramebuffer(bool color, bool depth) = 0;
    virtual void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) = 0;
    virtual void ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) = 0;
    virtual std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
    GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) = 0;
    virtual void* GetFramebufferTextureId(int fbId) = 0;
    virtual void SelectTextureFb(int fbId) = 0;
    virtual void DeleteTexture(uint32_t texId) = 0;
    virtual void SetTextureFilter(FilteringMode mode) = 0;
    virtual FilteringMode GetTextureFilter() = 0;
    virtual void SetSrgbMode() = 0;
    virtual ImTextureID GetTextureById(int id) = 0;

    // SOH [Enhancement] Toon lighting: the interpreter pushes the per-object dominant light here
    // before each batch; backends read the mToon* members in their per-draw uniform paths.
    virtual void SetToonLighting(const float dir[3], const float color[3], const float ambient[3]) {
        for (int i = 0; i < 3; i++) {
            mToonLightDir[i] = dir[i];
            mToonLightColor[i] = color[i];
            mToonAmbient[i] = ambient[i];
        }
    }

    // SOH [Enhancement] Toon lighting: the application pushes the frame-global ramp shape here (the
    // values are app-side tuning, so the framework never reaches into the app's config to read them).
    // Backends read the mToonRamp* members in their per-draw uniform paths; they keep their default
    // (a plain two-tone ramp) until the application overrides them.
    // debug != 0 switches the toon variant to a diagnostic view: each relit object is drawn as flat
    // white on the lit side of the ramp and flat black on the shadow side (albedo discarded), so it is
    // obvious at a glance which draws actually receive toon lighting.
    virtual void SetToonRamp(float center, float softness, float highlight, float shadow, float debug) {
        mToonRampCenter = center;
        mToonRampSoftness = softness;
        mToonHighlightIntensity = highlight;
        mToonShadowIntensity = shadow;
        mToonDebug = debug;
    }

    // SOH [Enhancement] World light casting / actor shadows: the interpreter pushes the current stencil
    // mode here when a gSPStencil command is seen, or directly from FlushToonShadow; backends read
    // mStencilMode in their per-draw path. Off (0) is normal rendering, so ordinary draws are unaffected.
    // ref is only consumed by the ShadowMask mode (the per-tap reference value); the volume modes compare
    // against a constant 0 and ignore it, so the default keeps existing call sites unchanged.
    virtual void SetStencilMode(int mode, int ref = 0) {
        mStencilMode = mode;
        mStencilRef = ref;
    }

  protected:
    float mToonLightDir[3] = { 0.0f, 0.0f, 1.0f };
    float mToonLightColor[3] = { 1.0f, 1.0f, 1.0f };
    float mToonAmbient[3] = { 0.0f, 0.0f, 0.0f };
    float mToonRampCenter = TOON_SHADING_DEFAULT_RAMP_CENTER;
    float mToonRampSoftness = TOON_SHADING_DEFAULT_RAMP_SOFTNESS;
    float mToonHighlightIntensity = TOON_SHADING_DEFAULT_HIGHLIGHT;
    float mToonShadowIntensity = TOON_SHADING_DEFAULT_SHADOW;
    float mToonDebug = 0.0f;
    int mStencilMode = 0; // SOH [Enhancement] world light casting (see StencilMode)
    int mStencilRef = 0;  // SOH [Enhancement] actor shadows: per-tap reference value for ShadowMask
    int8_t mCurrentDepthTest = 0;
    int8_t mCurrentDepthMask = 0;
    int8_t mCurrentZmodeDecal = 0;
    int8_t mLastDepthTest = -1;
    int8_t mLastDepthMask = -1;
    int8_t mLastZmodeDecal = -1;
    bool mSrgbMode = false;
};
} // namespace Fast
