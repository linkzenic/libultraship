#pragma once

#include <cstdint>
#include <string>

#include <imgui.h>

namespace Ship {

class Mobile {
  public:
    static void Init();
    static void Exit();
    static void ImGuiProcessEvent(bool wantsTextInput);
    static bool IsUsingTouchscreenControls();
    static void EnableTouchArea();
    static void DisableTouchArea();
    static float GetCameraYaw();
    static float GetCameraPitch();
#ifdef __ANDROID__
    static bool ConsumeGamepadBackPress();
#endif
};
}; // namespace Ship
