#if defined(__ANDROID__) || defined(__IOS__)
#include "ship/port/mobile/MobileImpl.h"
#include <SDL2/SDL.h>
#include "libultraship/bridge/consolevariablebridge.h"

#include <imgui_internal.h>

static bool isShowingVirtualKeyboard = true;

void Ship::Mobile::ImGuiProcessEvent(bool wantsTextInput) {
    ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetActiveID());

    if (wantsTextInput) {
        if (!isShowingVirtualKeyboard) {
            state->ClearText();

            isShowingVirtualKeyboard = true;
            SDL_StartTextInput();
        }
    } else {
        if (isShowingVirtualKeyboard) {
            isShowingVirtualKeyboard = false;
            SDL_StopTextInput();
        }
    }
}
#endif

#ifdef __ANDROID__
#include <SDL_gamecontroller.h>
#include <jni.h>
#include <atomic>

static bool isUsingTouchscreenControls = false;
static std::atomic<bool> sGamepadBackPressed{false};

extern "C" void JNICALL Java_com_dishii_soh_MainActivity_nativeGamepadBackPressed(JNIEnv* env, jclass clazz) {
    sGamepadBackPressed = true;
}

bool Ship::Mobile::ConsumeGamepadBackPress() {
    return sGamepadBackPressed.exchange(false);
}
static int virtual_joystick_id = -1;
static SDL_Joystick* virtual_joystick = nullptr;

extern "C" void JNICALL Java_com_dishii_soh_MainActivity_attachController(JNIEnv* env, jobject obj) {
    virtual_joystick_id = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 18, 0);
    if (virtual_joystick_id == -1) {
        SDL_Log("Could not create overlay virtual controller");
        return;
    }
    // Register mapping before SDL_PumpEvents so SDL_IsGameController returns true as "Touch Overlay".
    SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(virtual_joystick_id);
    char guidStr[33];
    SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
    char mappingStr[512];
    int written = SDL_snprintf(mappingStr, sizeof(mappingStr),
        "%s,Touch Overlay,"
        "a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,"
        "leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,"
        "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,"
        "leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,"
        "platform:Android,",
        guidStr);
    if (written >= (int)sizeof(mappingStr)) {
        SDL_Log("Touch Overlay mapping string truncated; skipping SDL_GameControllerAddMapping");
        return;
    }
    SDL_GameControllerAddMapping(mappingStr);
    virtual_joystick = SDL_JoystickOpen(virtual_joystick_id);
    if (virtual_joystick == nullptr)
        SDL_Log("Could not create virtual joystick");
    isUsingTouchscreenControls = true;
}

extern "C" void JNICALL Java_com_dishii_soh_MainActivity_detachController(JNIEnv* env, jobject obj) {
    SDL_JoystickClose(virtual_joystick);
    SDL_JoystickDetachVirtual(virtual_joystick_id);
    virtual_joystick = nullptr;
    virtual_joystick_id = -1;
    isUsingTouchscreenControls = false;
}

extern "C" void JNICALL Java_com_dishii_soh_MainActivity_setButton(JNIEnv* env, jobject obj, jint button, jboolean value) {
    if (button < 0) {
        SDL_JoystickSetVirtualAxis(virtual_joystick, -button, value ? SDL_MAX_SINT16 : -SDL_MAX_SINT16);
    } else {
        SDL_JoystickSetVirtualButton(virtual_joystick, button, value);
    }
}

extern "C" void JNICALL Java_com_dishii_soh_MainActivity_setAxis(JNIEnv* env, jobject obj, jint axis, jshort value) {
    SDL_JoystickSetVirtualAxis(virtual_joystick, axis, value);
}

// D-pad bits set from UI thread, consumed by InjectMenuNavKeys() on the game thread.
static std::atomic<uint8_t> sDpadPressedBits{0};
static std::atomic<uint8_t> sDpadReleasedBits{0};

extern "C" void JNICALL Java_com_dishii_soh_MainActivity_nativeMenuNavKey(JNIEnv* env, jobject obj, jint dir, jboolean pressed) {
    if (dir >= 0 && dir < 6) {
        if (pressed) {
            sDpadPressedBits.fetch_or(1 << dir, std::memory_order_relaxed);
        } else {
            sDpadReleasedBits.fetch_or(1 << dir, std::memory_order_relaxed);
        }
    }
}

void Ship::Mobile::InjectMenuNavKeys() {
    static const ImGuiKey kDirKeys[] = {
        ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown,
        ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight,
        ImGuiKey_GamepadFaceDown,  // A / select
        ImGuiKey_GamepadFaceRight, // B / back
    };
    uint8_t pressed  = sDpadPressedBits.exchange(0, std::memory_order_relaxed);
    uint8_t released = sDpadReleasedBits.exchange(0, std::memory_order_relaxed);
    for (int i = 0; i < 6; i++) {
        if (pressed  & (1 << i)) ImGui::GetIO().AddKeyEvent(kDirKeys[i], true);
        if (released & (1 << i)) ImGui::GetIO().AddKeyEvent(kDirKeys[i], false);
    }
}

static std::atomic<float> sTouchCamX{0.0f};
static std::atomic<float> sTouchCamY{0.0f};
static std::atomic<bool> sFreeLookTouchEnabled{false};
static std::atomic<bool> sTouchItemButtonPulse{false};
static std::atomic<bool> sTouchItemButtonHeld{false};

extern "C" void JNICALL Java_com_dishii_soh_MainActivity_setCameraState(JNIEnv* env, jobject obj, jint axis, jfloat value) {
    if (axis == 0) {
        sTouchCamX.store(value);
    } else if (axis == 1) {
        sTouchCamY.store(value);
    }
}

bool Ship::Mobile::HasTouchCameraInput() {
    if (!sFreeLookTouchEnabled.load()) {
        return false;
    }
    return sTouchCamX.load() != 0.0f || sTouchCamY.load() != 0.0f;
}

void Ship::Mobile::HandleTouchCamera(float* camX, float* camY) {
    if (!sFreeLookTouchEnabled.load()) {
        return;
    }
    *camX += sTouchCamX.exchange(0.0f);
    *camY += sTouchCamY.exchange(0.0f);
}

extern "C" bool Ship_Mobile_HasTouchCameraInput(void) {
    return Ship::Mobile::HasTouchCameraInput();
}

extern "C" void Ship_Mobile_HandleTouchCamera(float* camX, float* camY) {
    Ship::Mobile::HandleTouchCamera(camX, camY);
}

void Ship::Mobile::SetFreeLookTouchEnabled(bool enabled) {
    SDL_Log("[SoH_Touch] SetFreeLookTouchEnabled=%d", (int)enabled);
    sFreeLookTouchEnabled.store(enabled);
}

bool Ship::Mobile::IsTouchItemButtonPulse() {
    if (!sFreeLookTouchEnabled.load()) {
        sTouchItemButtonPulse.store(false);
        return false;
    }
    return sTouchItemButtonPulse.exchange(false);
}

bool Ship::Mobile::IsTouchItemButtonHeld() {
    if (!sFreeLookTouchEnabled.load()) {
        return false;
    }
    return sTouchItemButtonHeld.load();
}

extern "C" bool Ship_Mobile_IsTouchItemButtonPulse(void) {
    return Ship::Mobile::IsTouchItemButtonPulse();
}

extern "C" bool Ship_Mobile_IsItemButtonHeld(void) {
    return Ship::Mobile::IsTouchItemButtonHeld();
}

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_MainActivity_setItemButtonPulse(JNIEnv*, jobject) {
    sTouchItemButtonPulse.store(true);
}

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_MainActivity_setItemButtonHeld(JNIEnv*, jobject, jboolean held) {
    sTouchItemButtonHeld.store((bool)held);
}

void Ship::Mobile::SetToggleButtonVisible(bool visible) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject javaObject = (jobject)SDL_AndroidGetActivity();
    jclass javaClass = env->GetObjectClass(javaObject);
    jmethodID method = env->GetMethodID(javaClass, "SetToggleButtonVisible", "(Z)V");
    env->CallVoidMethod(javaObject, method, (jboolean)visible);
}

void Ship::Mobile::SetFirstPersonAimingActive(bool active) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject javaObject = (jobject)SDL_AndroidGetActivity();
    jclass javaClass = env->GetObjectClass(javaObject);
    jmethodID method = env->GetMethodID(javaClass, "SetFirstPersonAimingActive", "(Z)V");
    env->CallVoidMethod(javaObject, method, (jboolean)active);
}

#endif
