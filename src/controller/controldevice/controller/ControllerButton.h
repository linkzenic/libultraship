#pragma once

#include "mapping/ControllerButtonMapping.h"
#include <memory>
#include <unordered_map>
#include <string>
#include "libultraship/libultra/controller.h"
#include "controller/controldevice/controller/mapping/keyboard/KeyboardScancodes.h"

namespace Ship {

#define BUTTON_BITMASK(bitmask) static_cast<CONTROLLERBUTTONS_T>(bitmask)
#define BUTTON_BITMASKS                                                                                             \
    BUTTON_BITMASK(BTN_A), BUTTON_BITMASK(BTN_B), BUTTON_BITMASK(BTN_L), BUTTON_BITMASK(BTN_R),                     \
        BUTTON_BITMASK(BTN_Z), BUTTON_BITMASK(BTN_START), BUTTON_BITMASK(BTN_CLEFT),                                \
        BUTTON_BITMASK(BTN_CRIGHT), BUTTON_BITMASK(BTN_CUP), BUTTON_BITMASK(BTN_CDOWN),                             \
        BUTTON_BITMASK(BTN_DLEFT), BUTTON_BITMASK(BTN_DRIGHT), BUTTON_BITMASK(BTN_DUP),                             \
        BUTTON_BITMASK(BTN_DDOWN), BUTTON_BITMASK(BTN_CUSTOM_MODIFIER1), BUTTON_BITMASK(BTN_CUSTOM_MODIFIER2),      \
        BUTTON_BITMASK(BTN_CUSTOM_OCARINA_NOTE_D4), BUTTON_BITMASK(BTN_CUSTOM_OCARINA_NOTE_F4),                     \
        BUTTON_BITMASK(BTN_CUSTOM_OCARINA_NOTE_A4), BUTTON_BITMASK(BTN_CUSTOM_OCARINA_NOTE_B4),                     \
        BUTTON_BITMASK(BTN_CUSTOM_OCARINA_NOTE_D5), BUTTON_BITMASK(BTN_CUSTOM_OCARINA_DISABLE_SONGS),               \
        BUTTON_BITMASK(BTN_CUSTOM_OCARINA_PITCH_UP), BUTTON_BITMASK(BTN_CUSTOM_OCARINA_PITCH_DOWN)

class ControllerButton {
  public:
    ControllerButton(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask);
    ~ControllerButton();

    std::shared_ptr<ControllerButtonMapping> GetButtonMappingById(std::string id);
    std::unordered_map<std::string, std::shared_ptr<ControllerButtonMapping>> GetAllButtonMappings();
    void AddButtonMapping(std::shared_ptr<ControllerButtonMapping> mapping);
    void ClearButtonMappingId(std::string id);
    void ClearButtonMapping(std::string id);
    void ClearButtonMapping(std::shared_ptr<ControllerButtonMapping> mapping);
    void AddDefaultMappings(ShipDeviceIndex shipDeviceIndex);

    void LoadButtonMappingFromConfig(std::string id);
    void SaveButtonMappingIdsToConfig();
    void ReloadAllMappingsFromConfig();
    void ClearAllButtonMappings();
    void ClearAllButtonMappingsForDevice(ShipDeviceIndex shipDeviceIndex);

    bool AddOrEditButtonMappingFromRawPress(CONTROLLERBUTTONS_T bitmask, std::string id);

    void UpdatePad(CONTROLLERBUTTONS_T& padButtons);

    bool ProcessKeyboardEvent(KbEventType eventType, KbScancode scancode);

    bool HasMappingsForShipDeviceIndex(ShipDeviceIndex lusIndex);

  private:
    uint8_t mPortIndex;
    CONTROLLERBUTTONS_T mBitmask;
    std::unordered_map<std::string, std::shared_ptr<ControllerButtonMapping>> mButtonMappings;
    std::string GetConfigNameFromBitmask(CONTROLLERBUTTONS_T bitmask);

    bool mUseKeydownEventToCreateNewMapping;
    KbScancode mKeyboardScancodeForNewMapping;
};
} // namespace Ship
