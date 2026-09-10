#include "ship/controller/controldevice/controller/mapping/sdl/SDLRumbleMapping.h"

#include "ship/config/ConsoleVariable.h"
#include "ship/utils/StringHelper.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

#ifdef __ANDROID__
#include <SDL2/SDL.h>
#include <jni.h>
#endif

namespace Ship {
SDLRumbleMapping::SDLRumbleMapping(uint8_t portIndex, uint8_t lowFrequencyIntensityPercentage,
                                   uint8_t highFrequencyIntensityPercentage)
    : ControllerRumbleMapping(PhysicalDeviceType::SDLGamepad, portIndex, lowFrequencyIntensityPercentage,
                              highFrequencyIntensityPercentage) {
    SetLowFrequencyIntensity(lowFrequencyIntensityPercentage);
    SetHighFrequencyIntensity(highFrequencyIntensityPercentage);
}

void SDLRumbleMapping::StartRumble() {
#ifdef __ANDROID__
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    jclass cls = env->GetObjectClass(activity);
    jmethodID m = env->GetMethodID(cls, "startRumble", "(II)V");
    if (m == nullptr) {
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return;
    }
    env->CallVoidMethod(activity, m, (jint)mLowFrequencyIntensityPercentage, (jint)mHighFrequencyIntensityPercentage);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
#else
    for (const auto& [instanceId, gamepad] :
         Context::GetRawInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        SDL_GameControllerRumble(gamepad, mLowFrequencyIntensity, mHighFrequencyIntensity, 0);
    }
#endif
}

void SDLRumbleMapping::StopRumble() {
#ifdef __ANDROID__
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    jclass cls = env->GetObjectClass(activity);
    jmethodID m = env->GetMethodID(cls, "stopRumble", "()V");
    if (m == nullptr) {
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return;
    }
    env->CallVoidMethod(activity, m);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
#else
    for (const auto& [instanceId, gamepad] :
         Context::GetRawInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        SDL_GameControllerRumble(gamepad, 0, 0, 0);
    }
#endif
}

void SDLRumbleMapping::SetLowFrequencyIntensity(uint8_t intensityPercentage) {
    mLowFrequencyIntensityPercentage = intensityPercentage;
    mLowFrequencyIntensity = UINT16_MAX * (intensityPercentage / 100.0f);
}

void SDLRumbleMapping::SetHighFrequencyIntensity(uint8_t intensityPercentage) {
    mHighFrequencyIntensityPercentage = intensityPercentage;
    mHighFrequencyIntensity = UINT16_MAX * (intensityPercentage / 100.0f);
}

std::string SDLRumbleMapping::GetRumbleMappingId() {
    return StringHelper::Sprintf("P%d", mPortIndex);
}

void SDLRumbleMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".RumbleMappings." + GetRumbleMappingId();
    Ship::Context::GetRawInstance()->GetConsoleVariables()->SetString(
        StringHelper::Sprintf("%s.RumbleMappingClass", mappingCvarKey.c_str()).c_str(), "SDLRumbleMapping");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.LowFrequencyIntensity", mappingCvarKey.c_str()).c_str(),
        mLowFrequencyIntensityPercentage);
    Ship::Context::GetRawInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.HighFrequencyIntensity", mappingCvarKey.c_str()).c_str(),
        mHighFrequencyIntensityPercentage);
    Ship::Context::GetRawInstance()->GetConsoleVariables()->Save();
}

void SDLRumbleMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".RumbleMappings." + GetRumbleMappingId();

    Ship::Context::GetRawInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.RumbleMappingClass", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetRawInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.LowFrequencyIntensity", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetRawInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.HighFrequencyIntensity", mappingCvarKey.c_str()).c_str());

    Ship::Context::GetRawInstance()->GetConsoleVariables()->Save();
}

std::string SDLRumbleMapping::GetPhysicalDeviceName() {
    return "SDL Gamepad";
}
} // namespace Ship
