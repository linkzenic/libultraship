#ifndef FAST_TOON_SHADING_H
#define FAST_TOON_SHADING_H

// Shared default parameters for the per-pixel toon-lighting effect.
//
// The effect lives in the toon variant of the Fast3D fragment shaders (a distinct shader compiled
// when the SHADER_OPT(TOON) bit is set). The application emits a gSPToon(true/false) marker around
// the draws it wants relit; the interpreter forwards the object-space vertex normal and the single
// dominant light, and the fragment shader ramps N·L. The ramp shape (center/softness/highlight/
// shadow) is frame-global tuning the application pushes via GfxRenderingAPI::SetToonRamp(); until it
// does, the backends use the defaults below. The framework never reads the application's config, so
// no app-specific CVar keys live here.

// Defaults shared by every rendering backend (half-Lambert N·L mapped to 0..1).
#define TOON_SHADING_DEFAULT_RAMP_CENTER 0.5f
#define TOON_SHADING_DEFAULT_RAMP_SOFTNESS 0.1f
// Highlight = brightness of the lit band; Shadow = how dark the shadow band gets (1 = ambient).
// Both default to 1.0, which reproduces the plain "ambient + ramp*lightColor" two-tone.
#define TOON_SHADING_DEFAULT_HIGHLIGHT 1.0f
#define TOON_SHADING_DEFAULT_SHADOW 1.0f

#endif // FAST_TOON_SHADING_H
