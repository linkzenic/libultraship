#include "ship/controller/controldevice/controller/mapping/factories/GyroMappingFactory.h"
#include "ship/controller/controldevice/controller/mapping/sdl/SDLGyroMapping.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/utils/StringHelper.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

#if defined(__IOS__)
extern "C" int TwoShipApple_GetNativeControllerGyro(float* gyroX, float* gyroY, float* gyroZ);
#endif

namespace Ship {
#if defined(__ANDROID__) || (defined(__IOS__) && !defined(__TVOS__))
static bool MobileDeviceHasGyroSensor() {
    if ((SDL_WasInit(SDL_INIT_SENSOR) & SDL_INIT_SENSOR) == 0) {
        SDL_InitSubSystem(SDL_INIT_SENSOR);
    }
    for (int32_t i = 0; i < SDL_NumSensors(); i++) {
        if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_GYRO) {
            return true;
        }
    }
    return false;
}
#endif

std::shared_ptr<ControllerGyroMapping> GyroMappingFactory::CreateGyroMappingFromConfig(uint8_t portIndex,
                                                                                       std::string id) {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".GyroMappings." + id;
    const std::string mappingClass = Ship::Context::GetRawInstance()->GetConsoleVariables()->GetString(
        StringHelper::Sprintf("%s.GyroMappingClass", mappingCvarKey.c_str()).c_str(), "");

    float sensitivity = Ship::Context::GetRawInstance()->GetConsoleVariables()->GetFloat(
        StringHelper::Sprintf("%s.Sensitivity", mappingCvarKey.c_str()).c_str(), 2.0f);
    if (sensitivity < 0.0f || sensitivity > 1.0f) {
        // something about this mapping is invalid
        Ship::Context::GetRawInstance()->GetConsoleVariables()->ClearVariable(mappingCvarKey.c_str());
        Ship::Context::GetRawInstance()->GetConsoleVariables()->Save();
        return nullptr;
    }

    if (mappingClass == "SDLGyroMapping") {
        float neutralPitch = Ship::Context::GetRawInstance()->GetConsoleVariables()->GetFloat(
            StringHelper::Sprintf("%s.NeutralPitch", mappingCvarKey.c_str()).c_str(), 0.0f);
        float neutralYaw = Ship::Context::GetRawInstance()->GetConsoleVariables()->GetFloat(
            StringHelper::Sprintf("%s.NeutralYaw", mappingCvarKey.c_str()).c_str(), 0.0f);
        float neutralRoll = Ship::Context::GetRawInstance()->GetConsoleVariables()->GetFloat(
            StringHelper::Sprintf("%s.NeutralRoll", mappingCvarKey.c_str()).c_str(), 0.0f);

        return std::make_shared<SDLGyroMapping>(portIndex, sensitivity, neutralPitch, neutralYaw, neutralRoll);
    }

    return nullptr;
}

std::shared_ptr<ControllerGyroMapping> GyroMappingFactory::CreateGyroMappingFromSDLInput(uint8_t portIndex) {
    std::shared_ptr<ControllerGyroMapping> mapping = nullptr;

#if defined(__IOS__)
    float appleGyroData[3] = {};
    const bool appleControllerHasGyro =
        portIndex == 0 &&
        TwoShipApple_GetNativeControllerGyro(&appleGyroData[0], &appleGyroData[1], &appleGyroData[2]);
#endif
#if defined(__ANDROID__) || (defined(__IOS__) && !defined(__TVOS__))
    const bool mobileDeviceHasGyro = portIndex == 0 && MobileDeviceHasGyroSensor();
#endif

    for (auto [instanceId, gamepad] : Context::GetRawInstance()
                                          ->GetControlDeck()
                                          ->GetConnectedPhysicalDeviceManager()
                                          ->GetConnectedSDLGamepadsForPort(portIndex)) {
        if (!SDL_GameControllerHasSensor(gamepad, SDL_SENSOR_GYRO)
#if defined(__IOS__)
            && !appleControllerHasGyro
#endif
#if defined(__ANDROID__) || (defined(__IOS__) && !defined(__TVOS__))
            && !mobileDeviceHasGyro
#endif
        ) {
            continue;
        }

        for (int32_t button = SDL_CONTROLLER_BUTTON_A; button < SDL_CONTROLLER_BUTTON_MAX; button++) {
            if (SDL_GameControllerGetButton(gamepad, static_cast<SDL_GameControllerButton>(button))) {
                mapping = std::make_shared<SDLGyroMapping>(portIndex, 1.0f, 0.0f, 0.0f, 0.0f);
                mapping->Recalibrate();
                break;
            }
        }

        if (mapping != nullptr) {
            break;
        }

        for (int32_t i = SDL_CONTROLLER_AXIS_LEFTX; i < SDL_CONTROLLER_AXIS_MAX; i++) {
            const auto axis = static_cast<SDL_GameControllerAxis>(i);
            const auto axisValue = SDL_GameControllerGetAxis(gamepad, axis) / 32767.0f;
            int32_t axisDirection = 0;
            if (axisValue < -0.7f) {
                axisDirection = NEGATIVE;
            } else if (axisValue > 0.7f) {
                axisDirection = POSITIVE;
            }

            if (axisDirection == 0) {
                continue;
            }

            mapping = std::make_shared<SDLGyroMapping>(portIndex, 1.0f, 0.0f, 0.0f, 0.0f);
            mapping->Recalibrate();
            break;
        }
    }

#if defined(__IOS__) && !defined(__TVOS__)
    if (mapping == nullptr && (appleControllerHasGyro || mobileDeviceHasGyro)) {
        mapping = std::make_shared<SDLGyroMapping>(portIndex, 1.0f, 0.0f, 0.0f, 0.0f);
        mapping->Recalibrate();
    }
#endif

    return mapping;
}
} // namespace Ship
