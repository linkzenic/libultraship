#pragma once

#include <cstdint>
#include <string>

#include <imgui.h>

namespace Ship {

class Mobile {
  public:
    static void ImGuiProcessEvent(bool wantsTextInput);
    static void SetToggleButtonVisible(bool visible);
#ifdef __ANDROID__
    static bool ConsumeGamepadBackPress();
    static void InjectMenuNavKeys();
    static bool HasTouchCameraInput();
    static void HandleTouchCamera(float* camX, float* camY);
    static void SetFreeLookTouchEnabled(bool enabled);
    static void SetFirstPersonAimingActive(bool active);
    static bool IsTouchItemButtonPulse();
    static bool IsTouchItemButtonHeld();
#endif
};
}; // namespace Ship
