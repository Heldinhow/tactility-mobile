#include <Tactility/lvgl/SystemMenu.h>

#include <Tactility/Backlight.h>
#include <Tactility/TactilityPrivate.h>
#include <Tactility/lvgl/MobileIcons.h>
#include <Tactility/lvgl/MobileTheme.h>
#include <Tactility/lvgl/MobileUi.h>
#include <Tactility/lvgl/SystemRecents.h>
#include <Tactility/service/audio/Audio.h>
#include <Tactility/settings/DisplaySettings.h>
#include <Tactility/settings/RadioSettings.h>

#ifdef ESP_PLATFORM
#include <Tactility/service/displayidle/DisplayIdleService.h>
#endif

#include <app/manager.h>
#include <app/start.h>

#include <location/location.h>

#include <bluetooth/bluetooth_service.h>
#include <bluetooth/bluetooth_settings.h>
#include <wifi/wifi_settings.h>

#include <lvgl/devices/indev.h>
#include <lvgl/fonts.h>
#include <lvgl/grid_navigation.h>
#include <lvgl/icons/shared.h>
#include <lvgl/lvgl.h>
#include <lvgl/theme.h>
#include <lvgl/widgets/card.h>
#include <lvgl/widgets/icon_button.h>
#include <lvgl/widgets/sliderbox.h>

#include <lvgl_window_manager/window_manager.h>

#include <mesh/mesh.h>

#include <tactility/concurrent/dispatcher.h>
#include <tactility/device.h>
#include <tactility/drivers/backlight.h>
#include <tactility/drivers/bluetooth.h>
#include <tactility/drivers/display.h>
#include <tactility/drivers/power_supply.h>
#include <tactility/drivers/sleep_controller.h>
#include <tactility/drivers/wifi.h>
#include <tactility/log.h>

#include <cmath>
#include <cstdint>
#include <ctime>
#include <new>

#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

namespace tt::lvgl {

constexpr auto* TAG = "SystemMenu";
constexpr auto* POWER_OFF_APP_ID = "tactility.poweroff";

static void* toContext(bool enabled) {
    return reinterpret_cast<void*>(static_cast<intptr_t>(enabled));
}

static bool fromContext(void* context) {
    return reinterpret_cast<intptr_t>(context) != 0;
}

static bool hasWifi() {
    return device_exists_of_type(&WIFI_TYPE);
}

static bool hasBluetooth() {
    return device_exists_of_type(&BLUETOOTH_TYPE);
}

/** Mesh can be toggled once it was configured: a backend is selected and the region is set */
static bool hasMesh() {
    char backend[MESH_BACKEND_ID_MAX_LENGTH + 1];
    return mesh_is_available() &&
        mesh_get_backend(backend, sizeof(backend)) == ERROR_NONE &&
        settings::radio::loadRegion() != RADIO_REGION_UNSET;
}

static bool isWifiEnabled() {
    Device* device = nullptr;
    if (device_get_first_by_type(&WIFI_TYPE, &device) != ERROR_NONE) {
        return false;
    }
    WifiRadioState state = WIFI_RADIO_STATE_OFF;
    wifi_get_radio_state(device, &state);
    device_put(device);
    return state == WIFI_RADIO_STATE_ON || state == WIFI_RADIO_STATE_ON_PENDING;
}

static void setWifiEnabled(void* context) {
    Device* device = nullptr;
    if (device_get_first_by_type(&WIFI_TYPE, &device) != ERROR_NONE) {
        return;
    }
    const bool enabled = fromContext(context);
    error_t result = enabled ? wifi_set_radio_on(device) : wifi_set_radio_off(device);
    if (result == ERROR_NONE) {
        if (wifi_settings_set_enable(enabled) != ERROR_NONE) {
            LOG_E(TAG, "Failed to save the Wi-Fi state");
        }
    } else {
        LOG_E(TAG, "Failed to set Wi-Fi radio state (%s)", error_to_string(result));
    }
    device_put(device);
}

static bool isLocationEnabled() {
    return location_is_enabled();
}

static void setLocationEnabled(void* context) {
    const error_t result = location_set_enabled(fromContext(context));
    if (result != ERROR_NONE) {
        LOG_E(TAG, "Failed to set location services state (%s)", error_to_string(result));
    }
}

static bool isBluetoothEnabled() {
    Device* device = nullptr;
    if (device_get_first_by_type(&BLUETOOTH_TYPE, &device) != ERROR_NONE) {
        return false;
    }
    bool enabled = bluetooth_is_radio_on_or_pending(device);
    device_put(device);
    return enabled;
}

static void setBluetoothEnabled(void* context) {
    Device* device = nullptr;
    if (device_get_first_by_type(&BLUETOOTH_TYPE, &device) != ERROR_NONE) {
        return;
    }
    const bool enabled = fromContext(context);
    bluetooth_settings_set_enable(enabled);
    if (enabled != bluetooth_is_radio_on_or_pending(device)) {
        bluetooth_service_set_radio_enabled(enabled);
    }
    device_put(device);
}


static bool isMeshEnabled() {
    return mesh_is_enabled();
}

static void setMeshEnabled(void* context) {
    const error_t result = mesh_set_enabled(fromContext(context));
    if (result != ERROR_NONE) {
        LOG_E(TAG, "Failed to set mesh state (%s)", error_to_string(result));
    }
}

/** Persists the brightness the way the Display app does, off the LVGL task as it writes a file. */
static void saveBrightness(void* context) {
    const auto brightness = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(context));
    auto display_settings = settings::display::loadOrGetDefault();
    display_settings.backlightDuty = brightness;
    settings::display::save(display_settings);
#ifdef ESP_PLATFORM
    if (auto display_idle = service::displayidle::findService()) {
        display_idle->reloadSettings();
    }
#endif
}

// The brightness that the panel changed, saved once when it closes instead of on every slider step
struct BrightnessState {
    uint8_t duty = 0;
    bool changed = false;
};

static BrightnessState brightness_state;

static void onBrightnessChanged(lv_event_t* event) {
    auto* slider_box = lv_event_get_current_target_obj(event);
    Device* backlight_device = tt::backlight::findDisplayBacklight();
    if (backlight_device == nullptr) {
        return;
    }
    const auto brightness = static_cast<uint8_t>(tt::backlight::toBrightness(backlight_device, lvgl_sliderbox_get_value(slider_box)));
    backlight_set_brightness(backlight_device, brightness);
    device_put(backlight_device);
    brightness_state.duty = brightness;
    brightness_state.changed = true;
}

static void onPanelClosed(void* /*userData*/) {
    if (!brightness_state.changed) {
        return;
    }
    brightness_state.changed = false;
    dispatchOnMainDispatcher(saveBrightness, reinterpret_cast<void*>(static_cast<uintptr_t>(brightness_state.duty)));
}

static void onVolumeChanged(lv_event_t* event) {
    auto* slider_box = lv_event_get_current_target_obj(event);
    service::audio::setOutputVolume(static_cast<float>(lvgl_sliderbox_get_value(slider_box)));
}

static void setClockText(lv_obj_t* label) {
    const time_t now = ::time(nullptr);
    const tm* local = localtime(&now);
    // Before the clock is set, the year is meaningless - the same rule the statusbar uses
    if (local != nullptr && local->tm_year >= (2025 - 1900)) {
        lv_label_set_text_fmt(label, "%d:%02d", local->tm_hour, local->tm_min);
    } else {
        lv_label_set_text(label, "--:--");
    }
}

static void setWifiStatusText(lv_obj_t* label) {
    if (!isWifiEnabled()) {
        lv_label_set_text(label, "Wi-Fi off");
        return;
    }
    Device* device = nullptr;
    if (device_get_first_by_type(&WIFI_TYPE, &device) != ERROR_NONE) {
        lv_label_set_text(label, "Wi-Fi on");
        return;
    }
    WifiStationState station_state = WIFI_STATION_STATE_DISCONNECTED;
    char ssid[33] = {};
    const bool connected =
        wifi_get_station_state(device, &station_state) == ERROR_NONE &&
        station_state == WIFI_STATION_STATE_CONNECTED &&
        wifi_station_get_target_ssid(device, ssid) == ERROR_NONE;
    device_put(device);
    if (connected && ssid[0] != '\0') {
        lv_label_set_text_fmt(label, "Wi-Fi: %s", ssid);
    } else {
        lv_label_set_text(label, "Wi-Fi on");
    }
}

#if defined(CONFIG_TT_SYSTEM_GESTURES)
static void onRecentsClicked(lv_event_t* /*event*/) {
    window_manager_overlay_hide();
    systemRecentsShow();
}
#endif


// Changing the radio and device states can block, so it happens on the main task
struct ToggleOption {
    const char* enabledIcon;
    const char* disabledIcon;
    bool (*isEnabled)();
    DispatcherCallback setEnabled;
};

static const char* getToggleIcon(const ToggleOption* option, bool enabled) {
    return enabled ? option->enabledIcon : option->disabledIcon;
}

static void onToggleChanged(lv_event_t* event) {
    auto* toggle = lv_event_get_target_obj(event);
    const auto* option = static_cast<const ToggleOption*>(lv_event_get_user_data(event));
    const bool enabled = lv_obj_has_state(toggle, LV_STATE_CHECKED);
    lv_label_set_text(lv_obj_get_child(toggle, 0), getToggleIcon(option, enabled));
    tt::dispatchOnMainDispatcher(option->setEnabled, toContext(enabled));
}

static void onCloseClicked(lv_event_t* /*event*/) {
    window_manager_overlay_hide();
}

static void onBackdropGesture(lv_event_t* /*event*/) {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr && lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
        // The release must not click the pressed widget, e.g. a toggle
        lv_indev_wait_release(indev);
        window_manager_overlay_hide();
    }
}

static bool supportsPowerOff() {
    ::AppManifest manifest;
    if (app_manager_find_manifest(POWER_OFF_APP_ID, &manifest) != ERROR_NONE) {
        return false;
    }
    bool supported = false;
    device_for_each_of_type(&POWER_SUPPLY_TYPE, &supported, [](Device* device, void* context) {
        if (device_is_ready(device) && power_supply_supports_power_off(device)) {
            *static_cast<bool*>(context) = true;
            return false;
        }
        return true;
    });
    device_for_each_of_type(&SLEEP_CONTROLLER_TYPE, &supported, [](Device* device, void* context) {
        if (device_is_ready(device)) {
            *static_cast<bool*>(context) = true;
            return false;
        }
        return true;
    });
    return supported;
}

static void onPowerOffClicked(lv_event_t* /*event*/) {
    // The app's window is created in the app layer, which the overlay would cover
    window_manager_overlay_hide();
    AppStartContext context;
    uint32_t instance_id = 0;
    if (app_start_context_from_id(POWER_OFF_APP_ID, &context) == ERROR_NONE) {
        app_start_with_context(&context, &instance_id);
    }
}

// A square icon button, sized for the menu's tiles
static lv_obj_t* createTile(lv_obj_t* parent, const char* icon) {
    const auto icon_height = static_cast<int32_t>(lvgl_get_shared_icon_default_font_height());

    auto* tile = lvgl_icon_button_create(parent);
    lv_obj_set_size(tile, icon_height * 2, icon_height * 2);

    auto* icon_label = lv_label_create(tile);
    lv_obj_set_style_text_font(icon_label, lvgl_get_shared_icon_default_font(), LV_STATE_DEFAULT);
    lv_label_set_text(icon_label, icon);
    lv_obj_center(icon_label);
    return tile;
}

// A leading icon and a slider box that shows its value, both on the card's background
static lv_obj_t* createSliderRow(lv_obj_t* parent, const char* icon, int32_t value) {
    auto* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, LV_DPX(8), LV_STATE_DEFAULT);
    lv_obj_set_scrollable(row, false);

    auto* icon_label = lv_label_create(row);
    lv_obj_set_style_text_font(icon_label, lvgl_get_shared_icon_default_font(), LV_STATE_DEFAULT);
    lv_label_set_text(icon_label, icon);

    auto* slider_box = lvgl_sliderbox_create(row, 0, 100, 10, value);
    lv_obj_set_width(slider_box, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(slider_box, 1);
    return slider_box;
}

// A tile that the theme shows as on while it's checked
static void createOption(lv_obj_t* parent, const ToggleOption* option) {
    const bool enabled = option->isEnabled();
    auto* toggle = createTile(parent, getToggleIcon(option, enabled));
    lv_obj_add_flag(toggle, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_state(toggle, LV_STATE_CHECKED, enabled);
    lv_obj_add_event_cb(toggle, onToggleChanged, LV_EVENT_VALUE_CHANGED, const_cast<ToggleOption*>(option));
}

static void createLegacyWidgets(lv_obj_t* parent, void* /*userData*/) {
    // Swiping up anywhere closes the menu
    lv_obj_set_gesture_bubble(parent, false);
    lv_obj_add_event_cb(parent, onBackdropGesture, LV_EVENT_GESTURE, nullptr);

    // The panel hangs from the top edge, which is where the swipe that opens it comes from
    auto* card = lvgl_card_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(card, LV_PCT(100), LV_STATE_DEFAULT);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, lvgl_theme_is_compact() ? 4 : 8, LV_STATE_DEFAULT);

    // The clock, with the Wi-Fi status next to it
    auto* status_row = lv_obj_create(card);
    lv_obj_set_size(status_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(status_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(status_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(status_row, 8, LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(status_row, false);

    auto* clock = lv_label_create(status_row);
    lv_obj_set_style_text_font(clock, lvgl_get_text_font(FONT_SIZE_LARGE), LV_STATE_DEFAULT);
    setClockText(clock);
    lv_obj_set_flex_grow(clock, 1);

    if (hasWifi()) {
        auto* wifi_status = lv_label_create(status_row);
        lv_obj_set_style_text_font(wifi_status, lvgl_get_text_font(FONT_SIZE_SMALL), LV_STATE_DEFAULT);
        setWifiStatusText(wifi_status);
    }

    auto* grid = lv_obj_create(card);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const int32_t gap = lvgl_theme_is_compact() ? 8 : 16;
    lv_obj_set_style_pad_column(grid, gap, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(grid, gap, LV_STATE_DEFAULT);

    static constexpr ToggleOption wifi_option = { LVGL_ICON_SHARED_WIFI, LVGL_ICON_SHARED_WIFI_OFF, isWifiEnabled, setWifiEnabled };
    if (hasWifi()) {
        createOption(grid, &wifi_option);
    }
    static constexpr ToggleOption location_option = { LVGL_ICON_SHARED_LOCATION_ON, LVGL_ICON_SHARED_LOCATION_OFF, isLocationEnabled, setLocationEnabled };
    createOption(grid, &location_option);
    static constexpr ToggleOption bluetooth_option = { LVGL_ICON_SHARED_BLUETOOTH, LVGL_ICON_SHARED_BLUETOOTH_DISABLED, isBluetoothEnabled, setBluetoothEnabled };
    if (hasBluetooth()) {
        createOption(grid, &bluetooth_option);
    }
    // There is no "off" variant of this icon: the toggle's checked state shows whether mesh is on
    static constexpr ToggleOption mesh_option = { LVGL_ICON_SHARED_HUB, LVGL_ICON_SHARED_HUB, isMeshEnabled, setMeshEnabled };
    if (hasMesh()) {
        createOption(grid, &mesh_option);
    }
#if defined(CONFIG_TT_SYSTEM_GESTURES)
    auto* recents_tile = createTile(grid, LVGL_ICON_SHARED_APPS);
    lv_obj_add_event_cb(recents_tile, onRecentsClicked, LV_EVENT_SHORT_CLICKED, nullptr);
#endif
    if (supportsPowerOff()) {
        auto* power_tile = createTile(grid, LVGL_ICON_SHARED_POWER_SETTINGS_NEW);
        lv_obj_add_event_cb(power_tile, onPowerOffClicked, LV_EVENT_SHORT_CLICKED, nullptr);
    }
    // The arrow keys move between the tiles. Toggles that are focused directly would be switched by them.
    lvgl_grid_navigation_add(grid);

    // Brightness, when the display has a backlight that can be dimmed (not just switched on and off)
    Device* backlight_device = tt::backlight::findDisplayBacklight();
    if (backlight_device != nullptr) {
        if (tt::backlight::isDimmable(backlight_device)) {
            uint8_t brightness = 0;
            backlight_get_brightness(backlight_device, &brightness);
            auto* brightness_slider = createSliderRow(card, LVGL_ICON_SHARED_LIGHTBULB, tt::backlight::toPercent(backlight_device, brightness));
            lvgl_sliderbox_add_value_changed_cb(brightness_slider, onBrightnessChanged, nullptr);
        }
        device_put(backlight_device);
    }

    // Volume, when the device has audio output hardware. The audio service persists the level.
    if (service::audio::isOutputAvailable()) {
        const auto volume = static_cast<int32_t>(std::lround(service::audio::getOutputVolume()));
        auto* volume_slider = createSliderRow(card, LVGL_ICON_SHARED_MUSIC_NOTE, volume);
        lvgl_sliderbox_add_value_changed_cb(volume_slider, onVolumeChanged, nullptr);
    }

    // A keyboard or encoder can't tap outside of the panel or swipe up, so it needs a button to close it
    if (lvgl_indev_exists(LV_INDEV_TYPE_KEYPAD) || lvgl_indev_exists(LV_INDEV_TYPE_ENCODER)) {
        auto* close_button = createTile(card, LVGL_ICON_SHARED_CLOSE);
        lv_obj_add_event_cb(close_button, onCloseClicked, LV_EVENT_SHORT_CLICKED, nullptr);
        lv_group_focus_obj(close_button);
        lv_obj_add_state(close_button, LV_STATE_FOCUS_KEY);
    }

    // Slides in from the top edge. The animation goes with the widget when it's deleted.
    LvglThemeSettings theme_settings = {};
    lvgl_theme_get_settings(&theme_settings);
    if (theme_settings.animations_enabled) {
        lv_obj_update_layout(card);
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, card);
        lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
            lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(object), value, LV_STATE_DEFAULT);
        });
        lv_anim_set_values(&animation, -lv_obj_get_height(card), 0);
        lv_anim_set_duration(&animation, 180);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
}


#if defined(CONFIG_TT_MOBILE_HOME)

// The panel is built 12 px above the screen so its visible top corners are square while the
// bottom ones keep the 12 px radius, exactly like the mockup.
constexpr int32_t PANEL_HEIGHT = 242;
constexpr int32_t PANEL_OPEN_Y = -12;
constexpr int32_t PANEL_HIDDEN_TRANSLATE = -230;
constexpr int32_t PANEL_CONTENT_OFFSET = 12;
constexpr int32_t TRACK_WIDTH = 144;

struct Slider {
    lv_obj_t* row = nullptr;
    lv_obj_t* track = nullptr;
    lv_obj_t* fill = nullptr;
    lv_obj_t* thumb = nullptr;
    lv_obj_t* value = nullptr;
    int percent = 0;
    void (*onChanged)(int) = nullptr;
};

struct PanelTile {
    lv_obj_t* pad = nullptr;
    lv_obj_t* icon = nullptr;
    bool on = false;
};

static lv_obj_t* v2_panel = nullptr;
static lv_obj_t* v2_backdrop = nullptr;
static bool v2_closing = false;

static void closePanel();

static int brightnessPercent() {
    Device* backlight = tt::backlight::findDisplayBacklight();
    if (backlight == nullptr) {
        return 0;
    }
    uint8_t brightness = 0;
    backlight_get_brightness(backlight, &brightness);
    const int percent = tt::backlight::toPercent(backlight, brightness);
    device_put(backlight);
    return percent;
}

static void setBrightnessPercent(int percent) {
    Device* backlight = tt::backlight::findDisplayBacklight();
    if (backlight == nullptr) {
        return;
    }
    const auto brightness = static_cast<uint8_t>(tt::backlight::toBrightness(backlight, percent));
    backlight_set_brightness(backlight, brightness);
    device_put(backlight);
    brightness_state.duty = brightness;
    brightness_state.changed = true;
}

static void setVolumePercent(int percent) {
    service::audio::setOutputVolume(static_cast<float>(percent));
}

static void setSliderPercent(Slider* slider, int percent, bool notify) {
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    slider->percent = percent;
    const int32_t fill_width = TRACK_WIDTH * percent / 100;
    lv_obj_set_width(slider->fill, fill_width);
    lv_obj_set_x(slider->thumb, 32 + fill_width - 10);
    lv_label_set_text_fmt(slider->value, "%d%%", percent);
    if (notify && slider->onChanged != nullptr) {
        slider->onChanged(percent);
    }
}

static void onSliderEvent(lv_event_t* event) {
    auto* slider = static_cast<Slider*>(lv_event_get_user_data(event));
    lv_indev_t* indev = lv_indev_active();
    if (indev == nullptr) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_area_t row_area;
    lv_obj_get_coords(slider->row, &row_area);
    const int32_t x = point.x - row_area.x1 - lv_obj_get_x(slider->track);
    setSliderPercent(slider, (x * 100) / TRACK_WIDTH, true);
}

static void onSliderDeleted(lv_event_t* event) {
    delete static_cast<Slider*>(lv_event_get_user_data(event));
}

static Slider* createSlider(lv_obj_t* parent, int32_t y, MobileIcon icon, int percent, void (*onChanged)(int)) {
    auto* slider = new (::std::nothrow) Slider();
    if (slider == nullptr) {
        return nullptr;
    }
    slider->onChanged = onChanged;

    slider->row = lv_obj_create(parent);
    lv_obj_remove_style_all(slider->row);
    lv_obj_set_pos(slider->row, 12, y + PANEL_CONTENT_OFFSET);
    lv_obj_set_size(slider->row, 216, 40);
    lv_obj_set_clickable(slider->row, true);
    lv_obj_set_scrollable(slider->row, false);

    lv_obj_t* icon_image = mobile::createIcon(slider->row, icon, mobile::COLOR_TEXT_2);
    lv_obj_align(icon_image, LV_ALIGN_LEFT_MID, 0, 0);

    slider->track = lv_obj_create(slider->row);
    lv_obj_remove_style_all(slider->track);
    lv_obj_set_pos(slider->track, 32, 10);
    lv_obj_set_size(slider->track, TRACK_WIDTH, 20);
    lv_obj_set_clickable(slider->track, false);
    lv_obj_set_scrollable(slider->track, false);
    // The 20 px thumb is wider than the 6 px track and sticks out at 0 and 100 percent
    lv_obj_add_flag(slider->track, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t* track_bg = lv_obj_create(slider->track);
    lv_obj_remove_style_all(track_bg);
    lv_obj_set_pos(track_bg, 0, 7);
    lv_obj_set_size(track_bg, TRACK_WIDTH, 6);
    lv_obj_set_style_bg_color(track_bg, mobile::color(mobile::COLOR_SURFACE_2), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(track_bg, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(track_bg, 3, LV_STATE_DEFAULT);
    lv_obj_set_clickable(track_bg, false);

    slider->fill = lv_obj_create(slider->track);
    lv_obj_remove_style_all(slider->fill);
    lv_obj_set_pos(slider->fill, 0, 7);
    lv_obj_set_size(slider->fill, 0, 6);
    lv_obj_set_style_bg_color(slider->fill, mobile::color(mobile::COLOR_ACCENT), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(slider->fill, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider->fill, 3, LV_STATE_DEFAULT);
    lv_obj_set_clickable(slider->fill, false);

    // A sibling of the track, not a child: the 20 px thumb sticks out of the 6 px track
    slider->thumb = lv_obj_create(slider->row);
    lv_obj_remove_style_all(slider->thumb);
    lv_obj_set_size(slider->thumb, 20, 20);
    lv_obj_set_pos(slider->thumb, 32, 10);
    lv_obj_set_style_bg_color(slider->thumb, mobile::color(mobile::COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(slider->thumb, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider->thumb, 10, LV_STATE_DEFAULT);
    lv_obj_set_clickable(slider->thumb, false);

    slider->value = mobile::createLabel(slider->row, "", FONT_SIZE_SECONDARY, mobile::COLOR_TEXT_2);
    lv_obj_set_size(slider->value, 32, 16);
    lv_obj_set_style_text_align(slider->value, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_obj_align(slider->value, LV_ALIGN_RIGHT_MID, 0, 0);

    setSliderPercent(slider, percent, false);

    lv_obj_add_event_cb(slider->row, onSliderEvent, LV_EVENT_PRESSED, slider);
    lv_obj_add_event_cb(slider->row, onSliderEvent, LV_EVENT_PRESSING, slider);
    lv_obj_add_event_cb(slider->row, onSliderDeleted, LV_EVENT_DELETE, slider);
    return slider;
}

static void setPanelTile(PanelTile* tile, bool on) {
    tile->on = on;
    lv_obj_set_style_bg_color(tile->pad, mobile::color(on ? mobile::COLOR_ACCENT : mobile::COLOR_SURFACE_2), LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(tile->icon, mobile::color(on ? mobile::COLOR_ON_ACCENT : mobile::COLOR_TEXT_2), LV_STATE_DEFAULT);
}

static void onWifiTileClicked(lv_event_t* event) {
    auto* tile = static_cast<PanelTile*>(lv_event_get_user_data(event));
    const bool enabled = !tile->on;
    setPanelTile(tile, enabled);
    tt::dispatchOnMainDispatcher(setWifiEnabled, toContext(enabled));
}

static void onBluetoothTileClicked(lv_event_t* event) {
    auto* tile = static_cast<PanelTile*>(lv_event_get_user_data(event));
    const bool enabled = !tile->on;
    setPanelTile(tile, enabled);
    tt::dispatchOnMainDispatcher(setBluetoothEnabled, toContext(enabled));
}

static void onRecentsTileClicked(lv_event_t* /*event*/) {
    window_manager_overlay_hide();
#if defined(CONFIG_TT_SYSTEM_GESTURES)
    systemRecentsShow();
#endif
}

static void onPanelTileDeleted(lv_event_t* event) {
    delete static_cast<PanelTile*>(lv_event_get_user_data(event));
}

static PanelTile* createPanelTile(lv_obj_t* parent, int32_t x, const char* label, MobileIcon icon, bool on, lv_event_cb_t onClicked) {
    auto* tile = new (::std::nothrow) PanelTile();
    if (tile == nullptr) {
        return nullptr;
    }

    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_pos(container, x, 148);
    lv_obj_set_size(container, 68, 74);
    lv_obj_set_scrollable(container, false);

    tile->pad = lv_button_create(container);
    lv_obj_set_pos(tile->pad, 0, 0);
    lv_obj_set_size(tile->pad, 68, 52);
    lv_obj_set_style_radius(tile->pad, mobile::RADIUS_CONTROL, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(tile->pad, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(tile->pad, 0, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(tile->pad, false);

    tile->icon = mobile::createIcon(tile->pad, icon, mobile::COLOR_TEXT_2);
    lv_obj_center(tile->icon);
    setPanelTile(tile, on);

    lv_obj_t* tile_label = mobile::createLabel(container, label, FONT_SIZE_DENSE, mobile::COLOR_TEXT);
    lv_obj_set_width(tile_label, 68);
    lv_obj_set_style_text_align(tile_label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_align(tile_label, LV_ALIGN_TOP_MID, 0, 58);

    lv_obj_add_event_cb(tile->pad, onClicked, LV_EVENT_SHORT_CLICKED, tile);
    lv_obj_add_event_cb(tile->pad, onPanelTileDeleted, LV_EVENT_DELETE, tile);
    return tile;
}

static void onV2BackdropGesture(lv_event_t* /*event*/) {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr && lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
        lv_indev_wait_release(indev);
        closePanel();
    }
}

static void onPanelAnimationCompleted(lv_anim_t* /*animation*/) {
    window_manager_overlay_hide();
}

static void animatePanelOpen() {
    LvglThemeSettings theme_settings = {};
    lvgl_theme_get_settings(&theme_settings);
    if (!theme_settings.animations_enabled) {
        return;
    }
    lv_obj_set_style_translate_y(v2_panel, PANEL_HIDDEN_TRANSLATE, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(v2_backdrop, LV_OPA_TRANSP, LV_STATE_DEFAULT);

    lv_anim_t translate;
    lv_anim_init(&translate);
    lv_anim_set_var(&translate, v2_panel);
    lv_anim_set_exec_cb(&translate, [](void* object, int32_t value) {
        lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(object), value, LV_STATE_DEFAULT);
    });
    lv_anim_set_values(&translate, PANEL_HIDDEN_TRANSLATE, 0);
    lv_anim_set_duration(&translate, 160);
    lv_anim_set_path_cb(&translate, lv_anim_path_ease_out);
    lv_anim_start(&translate);

    lv_anim_t scrim;
    lv_anim_init(&scrim);
    lv_anim_set_var(&scrim, v2_backdrop);
    lv_anim_set_exec_cb(&scrim, [](void* object, int32_t value) {
        lv_obj_set_style_bg_opa(static_cast<lv_obj_t*>(object), static_cast<lv_opa_t>(value), LV_STATE_DEFAULT);
    });
    lv_anim_set_values(&scrim, 0, 153);
    lv_anim_set_duration(&scrim, 160);
    lv_anim_set_path_cb(&scrim, lv_anim_path_ease_out);
    lv_anim_start(&scrim);
}

static void closePanel() {
    if (v2_panel == nullptr || v2_closing) {
        window_manager_overlay_hide();
        return;
    }
    v2_closing = true;
    LvglThemeSettings theme_settings = {};
    lvgl_theme_get_settings(&theme_settings);
    if (!theme_settings.animations_enabled) {
        window_manager_overlay_hide();
        return;
    }

    lv_anim_t translate;
    lv_anim_init(&translate);
    lv_anim_set_var(&translate, v2_panel);
    lv_anim_set_exec_cb(&translate, [](void* object, int32_t value) {
        lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(object), value, LV_STATE_DEFAULT);
    });
    lv_anim_set_values(&translate, 0, PANEL_HIDDEN_TRANSLATE);
    lv_anim_set_duration(&translate, 120);
    lv_anim_set_path_cb(&translate, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&translate, onPanelAnimationCompleted);
    lv_anim_start(&translate);

    lv_anim_t scrim;
    lv_anim_init(&scrim);
    lv_anim_set_var(&scrim, v2_backdrop);
    lv_anim_set_exec_cb(&scrim, [](void* object, int32_t value) {
        lv_obj_set_style_bg_opa(static_cast<lv_obj_t*>(object), static_cast<lv_opa_t>(value), LV_STATE_DEFAULT);
    });
    lv_anim_set_values(&scrim, 153, 0);
    lv_anim_set_duration(&scrim, 120);
    lv_anim_set_path_cb(&scrim, lv_anim_path_ease_in);
    lv_anim_start(&scrim);
}

static void onV2PanelGesture(lv_event_t* /*event*/) {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr && lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
        lv_indev_wait_release(indev);
        closePanel();
    }
}

static void createWidgets(lv_obj_t* parent, void* /*userData*/) {
    v2_backdrop = parent;
    v2_closing = false;
    lv_obj_set_gesture_bubble(parent, false);
    lv_obj_add_event_cb(parent, onV2BackdropGesture, LV_EVENT_GESTURE, nullptr);

    const lv_color_t surface = mobile::color(mobile::COLOR_SURFACE);
    auto* panel = lv_obj_create(parent);
    v2_panel = panel;
    lv_obj_set_pos(panel, 0, PANEL_OPEN_Y);
    lv_obj_set_size(panel, 240, PANEL_HEIGHT);
    lv_obj_set_style_bg_color(panel, surface, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(panel, mobile::RADIUS_CARD, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, mobile::color(mobile::COLOR_LINE), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(panel, 0, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(panel, false);
    lv_obj_set_gesture_bubble(panel, false);
    lv_obj_add_event_cb(panel, onV2PanelGesture, LV_EVENT_GESTURE, nullptr);

    auto* time_label = mobile::createLabel(panel, "", FONT_SIZE_LARGE, mobile::COLOR_TEXT);
    lv_obj_set_pos(time_label, 12, 24);
    auto* date_label = mobile::createLabel(panel, "", FONT_SIZE_SECONDARY, mobile::COLOR_TEXT_2);
    lv_obj_align(date_label, LV_ALIGN_TOP_RIGHT, -12, 32);

    const time_t now = ::time(nullptr);
    const tm* local = localtime(&now);
    if (local != nullptr && local->tm_year >= (2025 - 1900)) {
        lv_label_set_text_fmt(time_label, "%d:%02d", local->tm_hour, local->tm_min);
        char date[24];
        ::strftime(date, sizeof(date), "%a, %d %b", local);
        lv_label_set_text(date_label, date);
    } else {
        lv_label_set_text(time_label, "--:--");
        lv_label_set_text(date_label, "");
    }

    createSlider(panel, 48, MobileIcon::Sun, brightnessPercent(), setBrightnessPercent);
    createSlider(panel, 88, MobileIcon::Vol, static_cast<int>(::std::lround(service::audio::getOutputVolume())), setVolumePercent);

    if (hasWifi()) {
        if (createPanelTile(panel, 12, "Wi-Fi", MobileIcon::Wifi, isWifiEnabled(), onWifiTileClicked) == nullptr) {
            return;
        }
    }
    if (hasBluetooth()) {
        createPanelTile(panel, 86, "Bluetooth", MobileIcon::Bt, isBluetoothEnabled(), onBluetoothTileClicked);
    }
    createPanelTile(panel, 160, "Recents", MobileIcon::Recents, false, onRecentsTileClicked);

    lv_obj_t* bar = mobile::createGestureBar(panel);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 230);

    animatePanelOpen();
}

#else

static void createWidgets(lv_obj_t* parent, void* userData) {
    createLegacyWidgets(parent, userData);
}

#endif

void systemMenuShow() {
#if defined(CONFIG_TT_MOBILE_HOME) && defined(ESP_PLATFORM)
    if (auto display_idle = service::displayidle::findService()) {
        if (display_idle->isLocked()) {
            return;
        }
    }
#endif
    brightness_state.changed = false;
    window_manager_overlay_show(createWidgets, onPanelClosed, nullptr);
}

}
