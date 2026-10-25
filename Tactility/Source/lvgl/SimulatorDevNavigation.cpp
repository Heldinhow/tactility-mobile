// SPDX-License-Identifier: Apache-2.0
//
// Development aid for the v2 screen checks: F1-F9 in the simulator open a screen or an overlay,
// so TACTILITY_SIM_SCREENSHOT can capture it without driving the UI by hand. Simulator-only: this
// file is empty in firmware builds.
#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

#if !defined(ESP_PLATFORM) && defined(CONFIG_TT_MOBILE_HOME)

#include <Tactility/app/alertdialog/AlertDialog.h>
#include <Tactility/app/wificonnect/WifiConnect.h>
#include <Tactility/lvgl/SystemMenu.h>
#include <Tactility/lvgl/SystemRecents.h>
#include <Tactility/service/displayidle/LockScreen.h>

#include <app/manager.h>
#include <app/start.h>

#include <lvgl_window_manager/window_manager.h>

#include <wifi/wifi_settings.h>

#include <tactility/device.h>
#include <tactility/drivers/wifi.h>
#include <tactility/error.h>

#include <chrono>
#include <cstring>
#include <thread>

namespace {

using namespace std::chrono_literals;

void startApp(const char* app_id) {
    AppStartContext start_context;
    uint32_t instance_id = 0;
    if (app_start_context_from_id(app_id, &start_context) == ERROR_NONE) {
        app_start_with_context(&start_context, &instance_id);
    }
}

void showHome() {
    WindowId launcher = WINDOW_ID_NONE;
    const size_t count = window_manager_get_windows(nullptr, 0, 0);
    for (size_t i = 0; i < count; i++) {
        WindowInfo info {};
        if (window_manager_get_windows(&info, i, 1) != 1) {
            continue;
        }
        char app_id[APP_MANIFEST_ID_LENGTH + 1] = {};
        if (app_manager_get_instance_app_id(info.app_instance_id, app_id, sizeof(app_id)) != ERROR_NONE) {
            continue;
        }
        if (strcmp(app_id, "tactility.launcher") == 0) {
            launcher = info.id;
            break;
        }
    }
    if (launcher != WINDOW_ID_NONE) {
        window_manager_raise(launcher);
    }
}

/** The mock driver's fixture network, so the captures show a connected state */
void connectToFixtureNetwork() {
    Device* device = nullptr;
    if (device_get_first_by_type(&WIFI_TYPE, &device) != ERROR_NONE) {
        return;
    }
    wifi_set_radio_on(device);
    wifi_settings_set_enable(true);
    WifiApSettings ap = {};
    strncpy(ap.ssid, "Home Wifi", sizeof(ap.ssid) - 1);
    ap.auto_connect = true;
    wifi_settings_save(&ap);
    wifi_station_connect(device, ap.ssid, "", 0);
    device_put(device);
}

// Unlocking is not part of the check: the capture happens while the lock is up
void ignoreUnlock(void*) {}

void showLock() {
    static tt::service::displayidle::LockWidgets* lock = nullptr;
    connectToFixtureNetwork();
    tt::service::displayidle::lockScreenSetUnlockCallback(lock, ignoreUnlock, nullptr);
    tt::service::displayidle::lockScreenShow(lock);
}

}

/**
 * @param[in] index 1-based function key number: F1 home, F2 Settings, F3 Wi-Fi, F4 Files, F5 quick
 * panel over Files, F6 Recents over four apps, F7 lock, F8 dialog over Wi-Fi, F9 password screen
 */
void simulatorDevAction(int index) {
    std::thread([index] {
        if (index != 7) {
            connectToFixtureNetwork();
        }
        switch (index) {
            case 1:
                showHome();
                break;
            case 2:
                startApp("tactility.settings");
                break;
            case 3:
                startApp("tactility.wifimanage");
                break;
            case 4:
                startApp("tactility.files");
                break;
            case 5:
                startApp("tactility.files");
                std::this_thread::sleep_for(800ms);
                tt::lvgl::systemMenuShow();
                break;
            case 6:
                startApp("tactility.wifimanage");
                std::this_thread::sleep_for(300ms);
                startApp("tactility.notes");
                std::this_thread::sleep_for(300ms);
                startApp("tactility.terminal");
                std::this_thread::sleep_for(300ms);
                startApp("tactility.files");
                std::this_thread::sleep_for(800ms);
                tt::lvgl::systemRecentsShow();
                break;
            case 7:
                showLock();
                break;
            case 8:
                startApp("tactility.wifimanage");
                std::this_thread::sleep_for(700ms);
                tt::app::alertdialog::start(
                    0,
                    "Forget network?",
                    "You'll need the password to join HomeNet again.",
                    { "Cancel", "Forget" }
                );
                break;
            case 9:
                startApp("tactility.wifimanage");
                std::this_thread::sleep_for(700ms);
                tt::app::wificonnect::start("Cafe_5G");
                break;
            default:
                break;
        }
    }).detach();
}

#endif // !ESP_PLATFORM && CONFIG_TT_MOBILE_HOME
