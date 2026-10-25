// SPDX-License-Identifier: Apache-2.0
#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

#if defined(CONFIG_TT_MOBILE_HOME)

#include <Tactility/app/launcher/HomeModel.h>
#include <Tactility/lvgl/MobileFormat.h>
#include <Tactility/lvgl/MobileIcons.h>
#include <Tactility/lvgl/MobileTheme.h>
#include <Tactility/lvgl/MobileUi.h>
#include <Tactility/service/audio/Audio.h>
#include <Tactility/settings/DisplaySettings.h>

#include <app/event.h>
#include <app/manifest.h>
#include <app/scheduler.h>
#include <app/start.h>

#include <bluetooth/bluetooth_service.h>
#include <bluetooth/bluetooth_settings.h>
#include <wifi/wifi_settings.h>

#include <lvgl/lvgl.h>
#include <lvgl/fonts.h>
#include <lvgl/widgets/toolbar.h>

#include <lvgl_window_manager/window_manager.h>

#include <tactility/check.h>
#include <tactility/device.h>
#include <tactility/drivers/bluetooth.h>
#include <tactility/drivers/wifi.h>

#include <algorithm>
#include <cmath>
#include <new>
#include <string>
#include <vector>

namespace tt::app::settingshub {

extern const ::AppManifest manifest;

namespace {

constexpr auto* TAG = "SettingsHub";

namespace mobile = tt::lvgl::mobile;
using mobile::formatOnOff;
using mobile::formatPercent;
using mobile::formatTimeout;
using mobile::formatWifiValue;

// Screen positions from the mockup, minus the 24 px status bar and the 44 px toolbar
constexpr int CARD_LEFT = 12;
constexpr int CARD_TOP = 52;
constexpr int CARD_WIDTH = 216;
constexpr int CARD_HEIGHT = 220;
constexpr int ROW_HEIGHT = 44;
constexpr int ROW_ICON_X = 12;
constexpr int ROW_TEXT_X = 48;
constexpr int ROW_CHEVRON_RIGHT = 8;
constexpr int ROW_TEXT_GAP = 12;
constexpr int ROW_VALUE_RIGHT = 36;

constexpr const char* DISPLAY_APP_ID = "tactility.display";
constexpr const char* AUDIO_APP_ID = "tactility.audiosettings";
constexpr const char* WIFI_APP_ID = "tactility.wifimanage";
constexpr const char* BLUETOOTH_APP_ID = "tactility.btmanage";

struct Context {
    uint32_t appInstanceId = 0;
    // Keeps the ids of the rows alive: their pointers are the rows' event data
    std::vector<std::string> rowIds;
};

/** The five values that the hub shows. Each read when the hub is built, never on a timer. */
struct Values {
    std::string display;
    std::string sound;
    std::string wifi;
    std::string bluetooth;
    std::string power;
};

int dutyToPercent(uint8_t duty) {
    return (static_cast<int>(duty) * 100 + 127) / 255;
}

std::string readWifiValue() {
    Device* device = nullptr;
    if (device_get_first_by_type(&WIFI_TYPE, &device) != ERROR_NONE) {
        return formatWifiValue(false, false, "");
    }

    WifiRadioState radio_state = WIFI_RADIO_STATE_OFF;
    WifiStationState station_state = WIFI_STATION_STATE_DISCONNECTED;
    char ssid[33] = {};
    wifi_get_radio_state(device, &radio_state);
    const bool connected =
        wifi_get_station_state(device, &station_state) == ERROR_NONE &&
        station_state == WIFI_STATION_STATE_CONNECTED &&
        wifi_station_get_target_ssid(device, ssid) == ERROR_NONE;
    device_put(device);

    const bool radio_on = radio_state == WIFI_RADIO_STATE_ON || radio_state == WIFI_RADIO_STATE_ON_PENDING;
    return formatWifiValue(radio_on, connected, ssid);
}

std::string readBluetoothValue() {
    Device* device = nullptr;
    if (device_get_first_by_type(&BLUETOOTH_TYPE, &device) != ERROR_NONE) {
        return formatOnOff(false);
    }
    const bool enabled = bluetooth_is_radio_on_or_pending(device);
    device_put(device);
    return formatOnOff(enabled);
}

Values readValues() {
    const auto display_settings = settings::display::loadOrGetDefault();
    Values values;
    values.display = formatPercent(dutyToPercent(display_settings.backlightDuty));
    values.sound = formatPercent(static_cast<int>(std::lround(service::audio::getOutputVolume())));
    values.wifi = readWifiValue();
    values.bluetooth = readBluetoothValue();
    values.power = formatTimeout(display_settings.backlightTimeoutMs);
    return values;
}

void onRowClicked(lv_event_t* event) {
    const auto* app_id = static_cast<const char*>(lv_event_get_user_data(event));
    if (app_id == nullptr) {
        return;
    }
    AppStartContext start_context;
    uint32_t instance_id = 0;
    if (app_start_context_from_id(app_id, &start_context) == ERROR_NONE) {
        app_start_with_context(&start_context, &instance_id);
    }
}

/**
 * One 44 px row: 24 px icon, 15 px label, optional 13 px value and a 16 px chevron.
 * The divider is the row's own first pixel, inset to x=48 of the screen.
 */
void addRow(Context* ctx, lv_obj_t* card, const char* app_id, const char* label, MobileIcon icon, const char* value) {
    const bool first_row = lv_obj_get_child_count(card) == 0;

    lv_obj_t* row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), ROW_HEIGHT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(row, mobile::color(mobile::COLOR_SURFACE_2), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_clickable(row, true);
    lv_obj_set_scrollable(row, false);

    // The divider lies on top of the row below the first one
    if (!first_row) {
        lv_obj_t* line = lv_obj_create(row);
        lv_obj_remove_style_all(line);
        lv_obj_set_size(line, CARD_WIDTH - ROW_TEXT_X, 1);
        lv_obj_set_pos(line, ROW_TEXT_X, 0);
        lv_obj_set_style_bg_color(line, mobile::color(mobile::COLOR_LINE), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_clickable(line, false);
    }

    lv_obj_t* icon_image = mobile::createIcon(row, icon, mobile::COLOR_TEXT_2);
    lv_obj_align(icon_image, LV_ALIGN_LEFT_MID, ROW_ICON_X, 0);

    lv_obj_t* label_obj = mobile::createLabel(row, label, FONT_SIZE_DEFAULT, mobile::COLOR_TEXT);
    lv_label_set_long_mode(label_obj, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(label_obj, value != nullptr ? 120 : 132);
    lv_obj_align(label_obj, LV_ALIGN_LEFT_MID, ROW_TEXT_X, 0);

    if (value != nullptr) {
        lv_obj_t* value_obj = mobile::createLabel(row, value, FONT_SIZE_SECONDARY, mobile::COLOR_TEXT_3);
        lv_label_set_long_mode(value_obj, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(value_obj, 120);
        lv_obj_set_style_text_align(value_obj, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
        lv_obj_align(value_obj, LV_ALIGN_RIGHT_MID, -ROW_VALUE_RIGHT, 0);
    }

    lv_obj_t* chevron = mobile::createIcon(row, MobileIcon::Chev, mobile::COLOR_TEXT_3);
    lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -ROW_CHEVRON_RIGHT, 0);

    ctx->rowIds.emplace_back(app_id);
    lv_obj_add_event_cb(row, onRowClicked, LV_EVENT_SHORT_CLICKED, ctx->rowIds.back().data());
}

bool isInstalled(const std::vector<launcher::HomeApp>& apps, const char* id) {
    return std::ranges::any_of(apps, [id](const launcher::HomeApp& app) { return app.id == id; });
}

void createWidgets(lv_obj_t* parent, void* user_data) {
    auto* ctx = static_cast<Context*>(user_data);
    ctx->rowIds.clear();
    ctx->rowIds.reserve(32);

    lv_obj_set_style_bg_color(parent, mobile::color(mobile::COLOR_BG), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(parent, false);

    lv_obj_t* toolbar = lvgl_toolbar_create(parent, "Settings");
    lvgl_toolbar_set_nav_icon(toolbar, mobileIconGet(MobileIcon::Back));
    lv_obj_align(toolbar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, CARD_LEFT, CARD_TOP);
    lv_obj_set_size(card, CARD_WIDTH, CARD_HEIGHT);
    lv_obj_set_style_bg_color(card, mobile::color(mobile::COLOR_SURFACE), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(card, mobile::RADIUS_CARD, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(card, true, LV_STATE_DEFAULT);
    lv_obj_set_style_width(card, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(card, 1, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(card, mobile::color(mobile::COLOR_TEXT_3), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(card, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_ACTIVE);

    const Values values = readValues();
    const std::vector<launcher::HomeApp> apps = launcher::collectHomeApps();

    // The five designed rows, in the mockup's order
    addRow(ctx, card, DISPLAY_APP_ID, "Display", MobileIcon::Display, values.display.c_str());
    if (isInstalled(apps, AUDIO_APP_ID)) {
        addRow(ctx, card, AUDIO_APP_ID, "Sound", MobileIcon::Vol, values.sound.c_str());
    }
    addRow(ctx, card, WIFI_APP_ID, "Wi-Fi", MobileIcon::Wifi, values.wifi.c_str());
    addRow(ctx, card, BLUETOOTH_APP_ID, "Bluetooth", MobileIcon::Bt, values.bluetooth.c_str());
    // Power shows the screen-off timeout and opens Display, which owns that setting
    addRow(ctx, card, DISPLAY_APP_ID, "Power", MobileIcon::Power, values.power.c_str());

    // Every other visible settings app, alphabetically
    std::vector<const launcher::HomeApp*> rest;
    for (const auto& app : apps) {
        if (!app.settings || app.hidden || !app.compatible) {
            continue;
        }
        if (app.id == DISPLAY_APP_ID || app.id == AUDIO_APP_ID || app.id == WIFI_APP_ID || app.id == BLUETOOTH_APP_ID) {
            continue;
        }
        rest.push_back(&app);
    }
    std::ranges::sort(rest, [](const launcher::HomeApp* a, const launcher::HomeApp* b) { return a->name < b->name; });
    for (const auto* app : rest) {
        addRow(ctx, card, app->id.c_str(), app->name.c_str(), mobileIconForApp(app->id.c_str()), nullptr);
    }

}

void destroyWidgets(void* user_data) {
    // Nothing to release: the widgets own no resources, and the row ids go with the next build
    (void)user_data;
}

int32_t appMain(int argc, char* argv[]) {
    uint32_t app_instance_id = app_scheduler_current_app_id();

    auto* ctx = new (std::nothrow) Context();
    if (ctx == nullptr) {
        return -1;
    }
    ctx->appInstanceId = app_instance_id;

    TaskEventGroup event_group {};
    task_event_group_construct(&event_group);

    AppEventSubscription sub {};
    check(app_event_subscribe(&sub, &event_group) == ERROR_NONE);

    WindowId window = window_manager_create_ext(app_instance_id, createWidgets, destroyWidgets, ctx);
    if (window == 0) {
        delete ctx;
        check(app_event_unsubscribe(&sub) == ERROR_NONE);
        task_event_group_destruct(&event_group);
        return -1;
    }

    bool should_close = false;
    while (!should_close) {
        task_event_group_wait_any(&event_group, nullptr, portMAX_DELAY);
        AppEvent event {};
        while (app_event_poll(&sub, &event) == ERROR_NONE) {
            if (event.type == APP_EVENT_CLOSE) {
                should_close = true;
                break;
            }
        }
    }

    window_manager_remove(window);
    delete ctx;
    check(app_event_unsubscribe(&sub) == ERROR_NONE);
    task_event_group_destruct(&event_group);
    return 0;
}

} // namespace

extern const ::AppManifest manifest = {
    .id = "tactility.settings",
    .name = "Settings",
    .category = APP_CATEGORY_SYSTEM,
    .location = { APP_LOCATION_MEMORY, reinterpret_cast<void*>(tt::app::settingshub::appMain) },
    .flags = 0,
    .stack = {}
};

} // namespace tt::app::settingshub

#endif // CONFIG_TT_MOBILE_HOME
