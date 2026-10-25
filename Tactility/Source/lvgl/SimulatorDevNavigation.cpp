// SPDX-License-Identifier: Apache-2.0
//
// Development aid for the v2 screen checks: F1-F9 in the simulator open a screen or an overlay,
// so TACTILITY_SIM_SCREENSHOT can capture it without driving the UI by hand. Simulator-only: this
// file is empty in firmware builds.
#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

#if !defined(ESP_PLATFORM) && defined(CONFIG_TT_MOBILE_HOME)

#include <Tactility/TactilityPrivate.h>
#include <Tactility/app/alertdialog/AlertDialog.h>
#include <Tactility/app/wificonnect/WifiConnect.h>
#include <Tactility/lvgl/SystemMenu.h>
#include <Tactility/lvgl/SystemRecents.h>
#include <Tactility/service/displayidle/LockScreen.h>

#include <app/manager.h>
#include <app/start.h>

#include <lvgl/lvgl.h>

#include <lvgl_window_manager/window_manager.h>


#include <tactility/delay.h>
#include <tactility/device.h>
#include <tactility/drivers/wifi.h>
#include <tactility/error.h>
#include <tactility/log.h>
#include <tactility/log.h>

#include <cstdlib>
#include <cstring>
#include <string>
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
        window_manager_get_windows(&info, i, 1);
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

// The mock radio is not turned on from here: the Wi-Fi app and autoconnect own that path, and a
// second caller from this thread deadlocks the simulator's dispatcher.

// Unlocking is not part of the check: the capture happens while the lock is up
void ignoreUnlock(void*) {}

void showLock() {
    static tt::service::displayidle::LockWidgets* lock = nullptr;
    // The mock radio is cheap to turn on and makes the lock's Wi-Fi indicator visible
    Device* device = nullptr;
    if (device_get_first_by_type(&WIFI_TYPE, &device) == ERROR_NONE) {
        wifi_set_radio_on(device);
        device_put(device);
    }
    tt::service::displayidle::lockScreenSetUnlockCallback(lock, ignoreUnlock, nullptr);
    // The overlay is built on the LVGL task with the lock held, like the service does
    lvgl_lock();
    const bool shown = tt::service::displayidle::lockScreenShow(lock);
    const bool visible = tt::service::displayidle::lockScreenIsVisible(lock);
    lvgl_unlock();
    LOG_I("sim_dev", "lock shown=%d visible=%d", (int)shown, (int)visible);
}

}

/**
 * @param[in] index 1-based function key number: F1 home, F2 Settings, F3 Wi-Fi, F4 Files, F5 quick
 * panel over Files, F6 Recents over four apps, F7 lock, F8 dialog over Wi-Fi, F9 password screen
 */
static void runDevAction(int index) {
        switch (index) {
            case 1:
                // Raising the launcher takes the window manager's lifecycle mutex, not the LVGL task
                tt::dispatchOnMainDispatcher([](void*) { showHome(); }, nullptr);
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
            case 5: {
                // The mock radio makes the panel's Wi-Fi tile show its on style
                Device* device = nullptr;
                if (device_get_first_by_type(&WIFI_TYPE, &device) == ERROR_NONE) {
                    wifi_set_radio_on(device);
                    device_put(device);
                }
                startApp("tactility.files");
                std::this_thread::sleep_for(800ms);
                tt::lvgl::systemMenuShow();
                break;
            }
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
}

extern void simulator_take_screenshot_now();

// The SDL main loop is not a FreeRTOS task: logging and starting apps there aborts the POSIX
// port. lv_async_call runs the sequence on the LVGL task instead.
static void runActionList(void* context) {
    auto* actions = static_cast<std::string*>(context);
    size_t start = 0;
    while (start < actions->size()) {
        size_t end = actions->find(',', start);
        if (end == std::string::npos) {
            end = actions->size();
        }
        const int index = atoi(actions->substr(start, end - start).c_str());
        if (index > 0) {
            runDevAction(index);
            delay_millis(300);
        }
        start = end + 1;
    }
    delete actions;

    if (getenv("TACTILITY_SIM_SCREENSHOT") != nullptr) {
        const char* delay_text = getenv("TACTILITY_SIM_SCREENSHOT_DELAY_MS");
        const uint32_t delay = delay_text != nullptr ? static_cast<uint32_t>(atoi(delay_text)) : 0;
        const uint32_t now = lv_tick_get();
        const uint32_t wait = delay > now ? delay - now : 100;
        LOG_I("sim_dev", "screenshot in %u ms", (unsigned)wait);
        lv_timer_create([](lv_timer_t* timer) {
            LOG_I("sim_dev", "screenshot");
            simulator_take_screenshot_now();
            lv_timer_delete(timer);
        }, wait, nullptr);
    }
}

void simulatorDevActionList(const char* list) {
    auto* actions = new std::string(list);
    lvgl_lock();
    lv_async_call(runActionList, actions);
    lvgl_unlock();
}

void simulatorDevAction(int index) {
    simulatorDevActionList(std::to_string(index).c_str());
}

#endif // !ESP_PLATFORM && CONFIG_TT_MOBILE_HOME
