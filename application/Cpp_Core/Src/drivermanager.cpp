#include "enums.hpp"
#include "drivermanager.hpp"
#include "drivers/net/NetDriver.hpp"
#include "drivers/xinput/XInputDriver.hpp"
#include "drivers/switch/SwitchDriver.hpp"
#include "drivers/ps4/PS4Driver.hpp"
// #include "drivers/astro/AstroDriver.h"
// #include "drivers/egret/EgretDriver.h"
// #include "drivers/hid/HIDDriver.h"
// #include "drivers/keyboard/KeyboardDriver.h"
// #include "drivers/mdmini/MDMiniDriver.h"
// #include "drivers/neogeo/NeoGeoDriver.h"
// #include "drivers/pcengine/PCEngineDriver.h"
// #include "drivers/psclassic/PSClassicDriver.h"
// #include "drivers/ps3/PS3Driver.h"
// #include "drivers/xbone/XBOneDriver.h"
// #include "drivers/xboxog/XboxOriginalDriver.h"

// #include "usbhostmanager.hpp"

void DriverManager::setup(InputMode mode) { 
    switch (mode) {
        case INPUT_MODE_CONFIG:
            driver = new NetDriver();
            break;
        case INPUT_MODE_SWITCH:
            driver = new SwitchDriver();
            break;
        case INPUT_MODE_XINPUT:
            driver = new XInputDriver();
            break;
        case INPUT_MODE_PS4:
            driver = new PS4Driver(PS4_CONTROLLER);
            break;
        case INPUT_MODE_PS5:
            driver = new PS4Driver(PS4_ARCADESTICK);
            break;
        // case INPUT_MODE_ASTRO:
        //     driver = new AstroDriver();
        //     break;
        // case INPUT_MODE_EGRET:
        //     driver = new EgretDriver();
        //     break;
        // case INPUT_MODE_KEYBOARD:
        //     driver = new KeyboardDriver();
        //     break;
        // case INPUT_MODE_GENERIC:
        //     driver = new HIDDriver();
        //     break;
        // case INPUT_MODE_MDMINI:
        //     driver = new MDMiniDriver();
        //     break;
        // case INPUT_MODE_NEOGEO:
        //     driver = new NeoGeoDriver();
        //     break;
        // case INPUT_MODE_PSCLASSIC:
        //     driver = new PSClassicDriver();
        //     break;
        // case INPUT_MODE_PCEMINI:
        //     driver = new PCEngineDriver();
        //     break;
        // case INPUT_MODE_PS3:
        //     driver = new PS3Driver();
        //     break;
        // case INPUT_MODE_XBONE:
        //     driver = new XBOneDriver();
        //     break;
        // case INPUT_MODE_XBOXORIGINAL:
        //     driver = new XboxOriginalDriver();
        //     break;
        default:
            return;
    }

    // Initialize our chosen driver
    driver->initialize();
    inputMode = mode;

}
