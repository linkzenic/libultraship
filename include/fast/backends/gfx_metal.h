//
//  gfx_metal.h
//  libultraship
//
//  Created by David Chavez on 16.08.22.
//
#pragma once
#ifdef __APPLE__

#include "gfx_rendering_api.h"
#include "../interpreter.h"

#include <imgui_impl_sdl2.h>
#include <simd/simd.h>

static constexpr size_t kMaxVertexBufferPoolSize = 3;
// SOH [Enhancement] Initial per-frame vertex buffer size (bytes). Matches the CPU mBufVbo stride
// (256 tris * VBO_MAX_FLOATS_PER_VERTEX floats/vertex * 3 verts) with room for ~50 flushes/frame.
// Large scenes (e.g. Hyrule Field) can need more, so the buffer grows on demand in StartFrame (see
// mVertexBufferTargetLength) and can never overflow/crash.
static constexpr size_t kInitialVertexBufferLength = 256 * VBO_MAX_FLOATS_PER_VERTEX * 3 * sizeof(float) * 50;
static constexpr size_t METAL_MAX_MULTISAMPLE_SAMPLE_COUNT = 8;
static constexpr size_t MAX_PIXEL_DEPTH_COORDS = 1024;

namespace MTL {
class Texture;
class SamplerState;
class CommandBuffer;
class RenderPassDescriptor;
class RenderCommandEncoder;
class SamplerState;
class ScissorRect;
class Device;
class Function;
class Buffer;
class RenderPipelineState;
class CommandQueue;
class Viewport;
} // namespace MTL

namespace CA {
class MetalDrawable;
class MetalLayer;
} // namespace CA

namespace NS {
class AutoreleasePool;
}

static int cantor(uint64_t a, uint64_t b) {
    return (a + b + 1.0) * (a + b) / 2 + b;
}

struct hash_pair_shader_ids {
    size_t operator()(const std::pair<uint64_t, uint32_t>& p) const {
        auto value1 = p.first;
        auto value2 = p.second;
        return cantor(value1, value2);
    }
};

namespace Fast {

struct ShaderProgramMetal {
    uint64_t shader_id0;
    uint32_t shader_id1;

    uint8_t numInputs;
    uint8_t numFloats;
    bool usedTextures[SHADER_MAX_TEXTURES];
    bool opt_toon = false; // SOH [Enhancement] toon lighting variant

    // hashed by msaa_level
    MTL::RenderPipelineState* pipeline_state_variants[9];
};

struct TextureDataMetal {
    MTL::Texture* texture;
    MTL::Texture* msaaTexture;
    MTL::SamplerState* sampler;
    uint32_t width;
    uint32_t height;
    uint32_t filtering;
    bool linear_filtering;
};

struct FramebufferMetal {
    MTL::CommandBuffer* mCommandBuffer;
    MTL::RenderPassDescriptor* mRenderPassDescriptor;
    MTL::RenderCommandEncoder* mCommandEncoder;

    MTL::Texture* mDepthTexture;
    MTL::Texture* mMsaaDepthTexture;
    // SOH [Enhancement] World light casting: a separate Stencil8 plane for the stencil light-volume
    // technique, attached alongside the (unchanged) depth texture so depth + GetPixelDepth are untouched.
    MTL::Texture* mStencilTexture = nullptr;
    MTL::Texture* mMsaaStencilTexture = nullptr;
    uint32_t mTextureId;
    bool mHasDepthBuffer;
    uint32_t mMsaaLevel;
    bool mRenderTarget;

    // State
    bool mHasEndedEncoding;
    bool mHasBoundVertexShader;
    bool mHasBoundFragShader;

    struct ShaderProgramMetal* mLastShaderProgram;
    MTL::Texture* mLastBoundTextures[SHADER_MAX_TEXTURES];
    MTL::SamplerState* mLastBoundSamplers[SHADER_MAX_TEXTURES];
    MTL::ScissorRect* mScissorRect;
    MTL::Viewport* mViewport;

    int8_t mLastDepthTest = -1;
    int8_t mLastDepthMask = -1;
    int8_t mLastZmodeDecal = -1;
    int mLastStencilMode = -1; // SOH [Enhancement] world light casting
};

struct FrameUniforms {
    simd::int1 frameCount;
    simd::float1 noiseScale;
};

struct DrawUniforms {
    simd::int1 textureFiltering[SHADER_MAX_TEXTURES];
    // SOH [Enhancement] Toon lighting — simd::float1[3] (12 bytes, 4-aligned) matches the .metal
    // packed_float3 fields. Layout must stay in lockstep with the DrawUniforms struct in the .metal shader.
    simd::float1 toonLightDir[3];
    simd::float1 toonLightColor[3];
    simd::float1 toonAmbient[3];
    simd::float1 toonRampCenter;
    simd::float1 toonRampSoftness;
    simd::float1 toonHighlightIntensity;
    simd::float1 toonShadowIntensity;
    simd::float1 toonDebug;
};

struct CoordUniforms {
    simd::uint2 coords[MAX_PIXEL_DEPTH_COORDS];
};

class GfxRenderingAPIMetal final : public GfxRenderingAPI {
  public:
    ~GfxRenderingAPIMetal() override = default;
    const char* GetName() override;
    int GetMaxTextureSize() override;
    GfxClipParameters GetClipParameters() override;
    void UnloadShader(ShaderProgram* oldPrg) override;
    void LoadShader(ShaderProgram* newPrg) override;
    ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint32_t shaderId1) override;
    ShaderProgram* LookupShader(uint64_t shaderId0, uint32_t shaderId1) override;
    void ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) override;
    uint32_t NewTexture() override;
    void SelectTexture(int tile, uint32_t textureId) override;
    void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) override;
    void SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) override;
    void SetDepthTestAndMask(bool depth_test, bool z_upd) override;
    void SetZmodeDecal(bool decal) override;
    void SetViewport(int x, int y, int width, int height) override;
    void SetScissor(int x, int y, int width, int height) override;
    void SetUseAlpha(bool useAlpha) override;
    void DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) override;
    void Init() override;
    void OnResize() override;
    void StartFrame() override;
    void EndFrame() override;
    void FinishRender() override;
    int CreateFramebuffer() override;
    void UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                     bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                     bool can_extract_depth) override;
    void StartDrawToFramebuffer(int fbId, float noiseScale) override;
    void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0,
                         int dstX1, int dstY1) override;
    void ClearFramebuffer(bool color, bool depth) override;
    void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) override;
    void ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) override;
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
    GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) override;
    void* GetFramebufferTextureId(int fbId) override;
    void SelectTextureFb(int fbId) override;
    void DeleteTexture(uint32_t texId) override;
    void SetTextureFilter(FilteringMode mode) override;
    FilteringMode GetTextureFilter() override;
    void SetSrgbMode() override;
    ImTextureID GetTextureById(int id) override;

    void NewFrame();
    void SetupFloatingFrame();
    void RenderDrawData(ImDrawData* drawData);
    bool MetalInit(SDL_Renderer* renderer);

  private:
    bool NonUniformThreadGroupSupported();
    void SetupScreenFramebuffer(uint32_t width, uint32_t height);
    // Elements that only need to be setup once
    SDL_Renderer* mRenderer;
    CA::MetalLayer* mLayer; // CA::MetalLayer*
    MTL::Device* mDevice;
    MTL::CommandQueue* mCommandQueue;

    int mCurrentVertexBufferPoolIndex = 0;
    MTL::Buffer* mVertexBufferPool[kMaxVertexBufferPoolSize];
    // SOH [Enhancement] Desired pool-buffer size (bytes); ratchets up when a frame overflows.
    size_t mVertexBufferTargetLength = kInitialVertexBufferLength;
    std::unordered_map<std::pair<uint64_t, uint32_t>, struct ShaderProgramMetal, hash_pair_shader_ids>
        mShaderProgramPool;

    std::vector<struct TextureDataMetal> mTextures;
    std::vector<FramebufferMetal> mFramebuffers;
    FrameUniforms mFrameUniforms;
    CoordUniforms mCoordUniforms;
    DrawUniforms mDrawUniforms;
    MTL::Buffer* mFrameUniformBuffer;

    uint32_t mMsaaNumQualityLevels[METAL_MAX_MULTISAMPLE_SAMPLE_COUNT];

    // Depth querying
    MTL::Buffer* mCoordUniformBuffer;
    MTL::Buffer* mDepthValueOutputBuffer;
    size_t mCoordBufferSize;
    MTL::Function* mDepthComputeFunction;
    MTL::Function* mConvertToRgb5a1Function;

    // Current state
    struct ShaderProgramMetal* mShaderProgram;
    CA::MetalDrawable* mCurrentDrawable;
    std::set<int> mDrawnFramebuffers;
    NS::AutoreleasePool* mFrameAutoreleasePool;

    int mCurrentTile;
    uint32_t mCurrentTextureIds[SHADER_MAX_TEXTURES];

    int32_t mRenderTargetHeight;
    int mCurrentFramebuffer;
    size_t mCurrentVertexBufferOffset;
    FilteringMode mCurrentFilterMode = FILTER_THREE_POINT;

    bool mNonUniformThreadgroupSupported;
};

} // namespace Fast

bool Metal_IsSupported();

#endif
