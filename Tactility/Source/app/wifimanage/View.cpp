#include <format>
#include <string>
#include <set>

#include <Tactility/network/HttpdReq.h>
#include <Tactility/app/wifimanage/View.h>
#include <Tactility/app/wifimanage/WifiManagePrivate.h>
#include <Tactility/lvgl/MobileFormat.h>
#include <Tactility/lvgl/MobileIcons.h>
#include <Tactility/lvgl/MobileTheme.h>
#include <Tactility/lvgl/MobileUi.h>
#include <Tactility/lvgl/Style.h>
#include <Tactility/Tactility.h>

#include <app/event.h>
#include <lvgl/grid_navigation.h>
#include <lvgl/theme.h>
#include <lvgl/widgets/list.h>
#include <lvgl/widgets/toolbar.h>

#include <tactility/log.h>
#include <lvgl/lvgl.h>
#include <lvgl/widgets/card.h>

#include <wifi/wifi_settings.h>

namespace tt::app::wifimanage {

constexpr auto* TAG = "WifiManageView";

static void onBackPressed(lv_event_t* event) {
    auto* appInstanceId = static_cast<uint32_t*>(lv_event_get_user_data(event));
    app_event_emit_close(*appInstanceId);
}

static uint8_t mapRssiToPercentage(int rssi) {
    auto abs_rssi = std::abs(rssi);
    if (abs_rssi < 30U) {
        abs_rssi = 30U;
    } else if (abs_rssi > 90U) {
        abs_rssi = 90U;
    }

    auto percentage = (float)(90U - abs_rssi) / 60.f * 100.f;
    return static_cast<uint8_t>(percentage);
}

static void onEnableSwitchChanged(lv_event_t* event) {
    auto* enable_switch = static_cast<lv_obj_t*>(lv_event_get_target(event));
    bool is_on = lv_obj_has_state(enable_switch, LV_STATE_CHECKED);
    auto* bindings = static_cast<Bindings*>(lv_event_get_user_data(event));
    bindings->onWifiToggled(is_on);
}

static void onConnectToHiddenClicked(lv_event_t* event) {
    auto* bindings = (Bindings*)lv_event_get_user_data(event);
    bindings->onConnectToHidden();
}

// region Secondary updates

void View::connect(lv_event_t* event) {
    LOG_D(TAG, "connect()");
    auto* widget = lv_event_get_current_target_obj(event);
    auto index = reinterpret_cast<size_t>(lv_obj_get_user_data(widget));
    auto* self = static_cast<View*>(lv_event_get_user_data(event));
    auto ap_records = self->state->getApRecords();

    if (index < ap_records.size()) {
        LOG_I(TAG, "Clicked %zu/%zu", index, ap_records.size() - 1);
        std::string ssid = ap_records[index].ssid;
        LOG_I(TAG, "Clicked AP: %s", ssid.c_str());
        std::string connection_target = self->state->getConnectionTarget();
        if (connection_target == ssid) {
            self->bindings->onDisconnect();
        } else {
            self->bindings->onConnectSsid(ssid);
        }
    } else {
        LOG_W(TAG, "Clicked AP: record %zu/%zu does not exist", index, ap_records.size() - 1);
    }
}

void View::showDetails(lv_event_t* event) {
    LOG_D(TAG, "showDetails()");
    auto* widget = lv_event_get_current_target_obj(event);
    auto index = reinterpret_cast<size_t>(lv_obj_get_user_data(widget));
    auto* self = static_cast<View*>(lv_event_get_user_data(event));
    auto ap_records = self->state->getApRecords();

    if (index < ap_records.size()) {
        std::string ssid = ap_records[index].ssid;
        LOG_I(TAG, "Clicked AP: %s", ssid.c_str());
        self->bindings->onShowApSettings(ssid);
    } else {
        LOG_W(TAG, "Clicked AP: record %zu/%zu does not exist", index, ap_records.size() - 1);
    }
}

void View::onRefreshPressed(lv_event_t* event) {
    auto* self = static_cast<View*>(lv_event_get_user_data(event));
    self->state->requestListRefresh();
    self->bindings->onRefresh();
}

/** Creates a title with a card below it, and returns the list in the card */
static lv_obj_t* createSection(lv_obj_t* parent, const char* title) {
    auto* label = lv_label_create(parent);
    lv_label_set_text(label, title);

    auto* card = lvgl_card_create(parent);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 0, LV_STATE_DEFAULT);
    // The list items' pressed and focused backgrounds follow the card's rounded corners
    lv_obj_set_style_clip_corner(card, true, LV_STATE_DEFAULT);

    // The card provides the background
    auto* list = lvgl_list_create(card);
    lv_obj_set_size(list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(list, 0, LV_STATE_DEFAULT);
    return list;
}

void View::createSsidListItem(lv_obj_t* list, const WifiApRecord& record, bool isConnecting, size_t index) {
    if (isConnecting) {
        auto* button = lvgl_list_add_button(list, LV_SYMBOL_WIFI, record.ssid);
        lv_obj_add_event_cb(button, showDetails, LV_EVENT_SHORT_CLICKED, this);
    } else {
        const std::string auth_info = (record.authentication_type == WIFI_AUTHENTICATION_TYPE_OPEN) ? "(open) " : " ";
        const auto percentage = mapRssiToPercentage(record.rssi);
        const auto label = std::format("{} {}{}%", std::string(record.ssid), auth_info, percentage);
        auto* button = lvgl_list_add_button(list, nullptr, label.c_str());
        lv_obj_set_user_data(button, reinterpret_cast<void*>(index));
        if (wifi_settings_contains(record.ssid)) {
            lv_obj_add_event_cb(button, showDetails, LV_EVENT_SHORT_CLICKED, this);
        } else {
            lv_obj_add_event_cb(button, connect, LV_EVENT_SHORT_CLICKED, this);
        }
    }
}

void View::updateConnectToHidden() {
    if (connect_to_hidden == nullptr) {
        return;
    }

    if (state->getRadioState() == WIFI_RADIO_STATE_ON) {
        lv_obj_set_hidden(connect_to_hidden, false);
    } else {
        lv_obj_set_hidden(connect_to_hidden, true);
    }
}

void View::updateNetworkList() {
    lv_obj_clean(networks_list);

    switch (state->getRadioState()) {
        case WIFI_RADIO_STATE_ON_PENDING:
        case WIFI_RADIO_STATE_ON: {

            std::string connection_target = state->getConnectionTarget();
            auto station_state = state->getStationState();

            // Make safe copy
            auto ap_records = state->getApRecords();

            bool is_connected = !connection_target.empty() &&
                station_state == WIFI_STATION_STATE_CONNECTED;
            bool added_connected = false;
            if (is_connected && !ap_records.empty()) {
                for (int i = 0; i < ap_records.size(); ++i) {
                    auto& record = ap_records[i];
                    if (record.ssid == connection_target) {
                        createSsidListItem(createSection(networks_list, "Connected"), record, false, i);
                        added_connected = true;
                        break;
                    }
                }
            }

            auto* networks_section = createSection(networks_list, "Networks");
            std::set<std::string> used_ssids;
            if (!ap_records.empty()) {
                for (int i = 0; i < ap_records.size(); ++i) {
                    auto& record = ap_records[i];
                    if (!used_ssids.contains(record.ssid)) {
                        bool connection_target_match = (record.ssid == connection_target);
                        bool is_connecting = connection_target_match
                            && station_state == WIFI_STATION_STATE_CONNECTION_PENDING &&
                            !connection_target.empty();
                        bool skip = connection_target_match && added_connected;
                        if (!skip) {
                            createSsidListItem(networks_section, record, is_connecting, i);
                        }
                        used_ssids.insert(record.ssid);
                    }
                }
                lv_obj_set_hidden(networks_list, false);
            } else if (!state->hasScannedAfterRadioOn() || state->isScanning()) {
                // hasScannedAfterRadioOn() prevents briefly showing "No networks found" when turning radio on.
                lv_obj_set_hidden(networks_list, true);
            } else {
                lv_obj_set_hidden(networks_list, false);
                lvgl_list_add_text(networks_section, "No networks found.");
            }

            connect_to_hidden = lv_button_create(networks_list);
            lv_obj_set_width(connect_to_hidden, LV_PCT(100));
            lv_obj_set_style_margin_ver(connect_to_hidden, 4, LV_STATE_DEFAULT);
            auto* connect_to_hidden_label = lv_label_create(connect_to_hidden);
            lv_label_set_text(connect_to_hidden_label, "Connect to hidden SSID");
            lv_obj_add_event_cb(connect_to_hidden, onConnectToHiddenClicked, LV_EVENT_SHORT_CLICKED, bindings);
            break;
        }

        default:
            connect_to_hidden = nullptr;
            // Nothing to do
            break;
    }

}

void View::updateScanning() {
    if (state->getRadioState() == WIFI_RADIO_STATE_ON && state->getStationState() == WIFI_STATION_STATE_DISCONNECTED && state->isScanning()) {
        lv_obj_set_hidden(scanning_spinner, false);
    } else {
        lv_obj_set_hidden(scanning_spinner, true);
    }
}

void View::updateRefreshButton() {
    lv_obj_set_hidden(refresh_button, state->getRadioState() != WIFI_RADIO_STATE_ON);
}

void View::updateWifiToggle() {
    lv_obj_clear_state(enable_switch, LV_STATE_ANY);
    switch (state->getRadioState()) {
        case WIFI_RADIO_STATE_ON:
            lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
            break;
        case WIFI_RADIO_STATE_ON_PENDING:
            lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
            lv_obj_add_state(enable_switch, LV_STATE_DISABLED);
            break;
        case WIFI_RADIO_STATE_OFF:
            lv_obj_remove_state(enable_switch, LV_STATE_CHECKED);
            lv_obj_remove_state(enable_switch, LV_STATE_DISABLED);
            break;
        case WIFI_RADIO_STATE_OFF_PENDING:
            lv_obj_remove_state(enable_switch, LV_STATE_CHECKED);
            lv_obj_add_state(enable_switch, LV_STATE_DISABLED);
            break;
    }
}

// endregion Secondary updates


#if defined(CONFIG_TT_MOBILE_HOME)

namespace mobile = tt::lvgl::mobile;

namespace {

constexpr int32_t CARD_LEFT = 12;
constexpr int32_t CARD_TOP = 52;
constexpr int32_t CAPTION_TOP = 120;
constexpr int32_t LIST_TOP = 138;
constexpr int32_t LIST_WIDTH = 216;
constexpr int32_t LIST_HEIGHT = 132;
constexpr int32_t ROW_HEIGHT = 44;

void setSwitchVisual(lv_obj_t* track, lv_obj_t* knob, bool on, bool animate) {
    lv_obj_set_style_bg_color(track, mobile::color(on ? mobile::COLOR_ACCENT : mobile::COLOR_SURFACE_2), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(knob, mobile::color(on ? mobile::COLOR_TEXT : mobile::COLOR_TEXT_2), LV_STATE_DEFAULT);
    const int32_t target = on ? 18 : 2;
    if (animate) {
        LvglThemeSettings theme_settings = {};
        lvgl_theme_get_settings(&theme_settings);
        if (theme_settings.animations_enabled) {
            lv_anim_t animation;
            lv_anim_init(&animation);
            lv_anim_set_var(&animation, knob);
            lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
                lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
            });
            lv_anim_set_values(&animation, lv_obj_get_x(knob), target);
            lv_anim_set_duration(&animation, 100);
            lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
            lv_anim_start(&animation);
            return;
        }
    }
    lv_obj_set_x(knob, target);
}

}

void View::onMobileSwitchClicked(lv_event_t* event) {
    auto* self = static_cast<View*>(lv_event_get_user_data(event));
    const bool enabled = !self->switch_on;
    self->updateMobileSwitch(enabled);
    self->bindings->onWifiToggled(enabled);
}

void View::onConnectedCardClicked(lv_event_t* event) {
    auto* self = static_cast<View*>(lv_event_get_user_data(event));
    if (!self->connected_ssid.empty()) {
        self->bindings->onForget(self->connected_ssid);
    }
}

void View::onNetworkRowClicked(lv_event_t* event) {
    auto* self = static_cast<View*>(lv_event_get_user_data(event));
    const auto index = reinterpret_cast<size_t>(lv_obj_get_user_data(lv_event_get_target_obj(event)));
    auto records = self->state->getApRecords();
    if (index < records.size()) {
        self->bindings->onConnectSsid(records[index].ssid);
    }
}

void View::updateMobileSwitch(bool on) {
    switch_on = on;
    if (switch_track != nullptr && switch_knob != nullptr) {
        setSwitchVisual(switch_track, switch_knob, on, true);
    }
}

void View::updateMobileCaption() {
    if (caption == nullptr) {
        return;
    }
    if (!error_message.empty()) {
        lv_label_set_text(caption, error_message.c_str());
        lv_obj_set_style_text_font(caption, lvgl_get_text_font(FONT_SIZE_SECONDARY), LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(caption, mobile::color(mobile::COLOR_ACCENT_TEXT), LV_STATE_DEFAULT);
    } else if (state->isScanning()) {
        lv_label_set_text(caption, "Scanning…");
        lv_obj_set_style_text_font(caption, lvgl_get_text_font(FONT_SIZE_SECONDARY), LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(caption, mobile::color(mobile::COLOR_TEXT_2), LV_STATE_DEFAULT);
    } else {
        lv_label_set_text(caption, "Available networks");
        lv_obj_set_style_text_font(caption, lvgl_get_text_font(FONT_SIZE_SMALL), LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(caption, mobile::color(mobile::COLOR_TEXT_2), LV_STATE_DEFAULT);
    }
}

void View::showStatus(const char* text) {
    if (status == nullptr) {
        status = mobile::createLabel(root, "", FONT_SIZE_DEFAULT, mobile::COLOR_TEXT_2);
        lv_obj_set_width(status, 240);
        lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
        lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 160);
    }
    lv_label_set_text(status, text);
    lv_obj_remove_flag(status, LV_OBJ_FLAG_HIDDEN);
}

void View::setError(const std::string& message) {
    error_message = message;
    updateMobileCaption();
}

void View::buildConnectedCard(const std::string& ssid) {
    if (connected_card != nullptr) {
        lv_obj_delete(connected_card);
        connected_card = nullptr;
    }
    connected_ssid.clear();
    if (ssid.empty()) {
        return;
    }
    connected_ssid = ssid;

    connected_card = lv_button_create(root);
    lv_obj_set_pos(connected_card, CARD_LEFT, CARD_TOP);
    lv_obj_set_size(connected_card, LIST_WIDTH, 56);
    lv_obj_set_style_bg_color(connected_card, mobile::color(mobile::COLOR_ACCENT_SOFT), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(connected_card, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(connected_card, mobile::RADIUS_CARD, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(connected_card, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(connected_card, mobile::color(mobile::COLOR_ACCENT_LINE), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(connected_card, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(connected_card, 8, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(connected_card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(connected_card, 12, LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(connected_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(connected_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(connected_card, false);
    lv_obj_add_event_cb(connected_card, onConnectedCardClicked, LV_EVENT_SHORT_CLICKED, this);

    lv_obj_t* icon = mobile::createIcon(connected_card, MobileIcon::Wifi, mobile::COLOR_ACCENT_TEXT);
    (void)icon;

    lv_obj_t* text = lv_obj_create(connected_card);
    lv_obj_remove_style_all(text);
    lv_obj_set_height(text, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollable(text, false);
    lv_obj_t* name = mobile::createLabel(text, ssid.c_str(), FONT_SIZE_DEFAULT, mobile::COLOR_ACCENT_TEXT);
    lv_obj_set_size(name, LV_PCT(100), 20);
    lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
    mobile::createLabel(text, "Connected", FONT_SIZE_SECONDARY, mobile::COLOR_ACCENT_TEXT);
    lv_obj_t* chevron = mobile::createIcon(connected_card, MobileIcon::Chev, mobile::COLOR_ACCENT_TEXT);
    (void)chevron;
}

void View::buildMobileList() {
    if (list_card == nullptr) {
        return;
    }
    lv_obj_clean(list_card);
    lv_obj_set_hidden(list_card, false);

    const auto records = state->getApRecords();
    const std::string connection_target = state->getConnectionTarget();
    const bool connected = state->getStationState() == WIFI_STATION_STATE_CONNECTED && !connection_target.empty();
    buildConnectedCard(connected ? connection_target : "");

    size_t row_index = 0;
    std::set<std::string> used_ssids;
    for (size_t i = 0; i < records.size(); i++) {
        const auto& record = records[i];
        if (used_ssids.contains(record.ssid) || record.ssid == connection_target) {
            continue;
        }
        used_ssids.insert(record.ssid);

        lv_obj_t* row = lv_button_create(list_card);
        lv_obj_set_size(row, LV_PCT(100), ROW_HEIGHT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(row, mobile::color(mobile::COLOR_SURFACE_2), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(row, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_STATE_DEFAULT);
        // The theme's focus margin would make a 44 px row 52 px in the list
        lv_obj_set_style_margin_all(row, 0, LV_STATE_DEFAULT);
        lv_obj_set_scrollable(row, false);
        lv_obj_set_user_data(row, reinterpret_cast<void*>(i));
        lv_obj_add_event_cb(row, onNetworkRowClicked, LV_EVENT_SHORT_CLICKED, this);

        if (row_index > 0) {
            lv_obj_t* divider = lv_obj_create(row);
            lv_obj_remove_style_all(divider);
            lv_obj_set_size(divider, LIST_WIDTH - 36, 1);
            lv_obj_set_pos(divider, 36, 0);
            lv_obj_set_style_bg_color(divider, mobile::color(mobile::COLOR_LINE), LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_STATE_DEFAULT);
            lv_obj_set_clickable(divider, false);
        }

        const int bucket = mobile::rssiBucket(record.rssi);
        const MobileIcon signal = bucket == 3 ? MobileIcon::Sig3 : (bucket == 2 ? MobileIcon::Sig2 : MobileIcon::Sig1);
        lv_obj_t* signal_icon = mobile::createIcon(row, signal, mobile::COLOR_TEXT);
        lv_obj_align(signal_icon, LV_ALIGN_LEFT_MID, 12, 0);

        lv_obj_t* name = mobile::createLabel(row, record.ssid, FONT_SIZE_DEFAULT, mobile::COLOR_TEXT);
        lv_obj_set_size(name, 120, 20);
        lv_obj_set_pos(name, 48, 12);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);

        if (record.authentication_type != WIFI_AUTHENTICATION_TYPE_OPEN) {
            lv_obj_t* lock = mobile::createIcon(row, MobileIcon::Lock, mobile::COLOR_TEXT_3);
            lv_obj_align(lock, LV_ALIGN_RIGHT_MID, -8, 0);
        }
        row_index++;
    }

    // A hidden network has no row of its own in a scan; keep the existing form reachable
    lv_obj_t* hidden_row = lv_button_create(list_card);
    lv_obj_set_size(hidden_row, LV_PCT(100), ROW_HEIGHT);
    lv_obj_set_style_bg_opa(hidden_row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(hidden_row, mobile::color(mobile::COLOR_SURFACE_2), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(hidden_row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(hidden_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(hidden_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(hidden_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_margin_all(hidden_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(hidden_row, false);
    lv_obj_t* hidden_label = mobile::createLabel(hidden_row, "Connect to hidden network", FONT_SIZE_DEFAULT, mobile::COLOR_TEXT_2);
    lv_obj_align(hidden_label, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_add_event_cb(hidden_row, [](lv_event_t* event) {
        static_cast<View*>(lv_event_get_user_data(event))->bindings->onConnectToHidden();
    }, LV_EVENT_SHORT_CLICKED, this);

    if (row_index == 0 && !state->isScanning() && state->hasScannedAfterRadioOn()) {
        lv_obj_t* empty = mobile::createLabel(list_card, "No networks found", FONT_SIZE_DEFAULT, mobile::COLOR_TEXT_2);
        lv_obj_set_width(empty, LV_PCT(100));
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
        lv_obj_align(empty, LV_ALIGN_TOP_MID, 0, (LIST_HEIGHT - 20) / 2);
    }
}

void View::initMobile(uint32_t newAppInstanceId, lv_obj_t* parent) {
    appInstanceId = newAppInstanceId;
    root = parent;
    lv_obj_set_scrollable(parent, false);
    lv_obj_set_style_bg_color(parent, mobile::color(mobile::COLOR_BG), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_STATE_DEFAULT);

    toolbar = lvgl_toolbar_create(parent, "Wi-Fi");
    lvgl_toolbar_set_nav_icon(toolbar, mobileIconGet(MobileIcon::Back));
    lv_obj_align(toolbar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* switch_wrapper = lv_obj_create(toolbar);
    lv_obj_remove_style_all(switch_wrapper);
    lv_obj_set_size(switch_wrapper, 44, 44);
    lv_obj_set_clickable(switch_wrapper, true);
    lv_obj_set_scrollable(switch_wrapper, false);
    lv_obj_add_event_cb(switch_wrapper, onMobileSwitchClicked, LV_EVENT_SHORT_CLICKED, this);

    switch_track = lv_obj_create(switch_wrapper);
    lv_obj_remove_style_all(switch_track);
    lv_obj_set_size(switch_track, 36, 20);
    lv_obj_set_style_radius(switch_track, mobile::RADIUS_SWITCH, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(switch_track, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_center(switch_track);
    lv_obj_set_clickable(switch_track, false);

    switch_knob = lv_obj_create(switch_track);
    lv_obj_remove_style_all(switch_knob);
    lv_obj_set_size(switch_knob, 16, 16);
    lv_obj_set_pos(switch_knob, 2, 2);
    lv_obj_set_style_radius(switch_knob, 8, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(switch_knob, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_clickable(switch_knob, false);

    caption = mobile::createLabel(parent, "Available networks", FONT_SIZE_SMALL, mobile::COLOR_TEXT_2);
    lv_obj_set_pos(caption, 16, CAPTION_TOP);

    list_card = lv_obj_create(parent);
    lv_obj_set_pos(list_card, CARD_LEFT, LIST_TOP);
    lv_obj_set_size(list_card, LIST_WIDTH, LIST_HEIGHT);
    lv_obj_set_style_bg_color(list_card, mobile::color(mobile::COLOR_SURFACE), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(list_card, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(list_card, mobile::RADIUS_CARD, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(list_card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(list_card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(list_card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(list_card, true, LV_STATE_DEFAULT);
    lv_obj_set_style_width(list_card, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(list_card, 1, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list_card, mobile::color(mobile::COLOR_TEXT_3), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list_card, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(list_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(list_card, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_card, LV_SCROLLBAR_MODE_ACTIVE);

    updateMobile(true);
}

void View::updateMobile(bool rebuildList) {
    if (root == nullptr) {
        return;
    }
    const auto radio_state = state->getRadioState();
    const bool off = radio_state == WIFI_RADIO_STATE_OFF || radio_state == WIFI_RADIO_STATE_OFF_PENDING;
    updateMobileSwitch(!off);

    if (off) {
        if (connected_card != nullptr) {
            lv_obj_delete(connected_card);
            connected_card = nullptr;
        }
        connected_ssid.clear();
        if (caption != nullptr) {
            lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);
        }
        if (list_card != nullptr) {
            lv_obj_add_flag(list_card, LV_OBJ_FLAG_HIDDEN);
        }
        showStatus("Wi-Fi is off");
        return;
    }

    if (status != nullptr) {
        lv_obj_add_flag(status, LV_OBJ_FLAG_HIDDEN);
    }
    if (caption != nullptr) {
        lv_obj_remove_flag(caption, LV_OBJ_FLAG_HIDDEN);
    }
    updateMobileCaption();
    if (rebuildList) {
        buildMobileList();
    }
}

#endif

// region Main

void View::init(uint32_t newAppInstanceId, lv_obj_t* parent) {
#if defined(CONFIG_TT_MOBILE_HOME)
    initMobile(newAppInstanceId, parent);
    return;
#else
    appInstanceId = newAppInstanceId;

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 0, LV_STATE_DEFAULT);

    root = parent;

    // Toolbar

    lv_obj_t* toolbar = lvgl_toolbar_create(parent, "Wi-Fi");
    lvgl_toolbar_set_nav_action(toolbar, LV_SYMBOL_CLOSE, onBackPressed, &appInstanceId);

    scanning_spinner = lvgl_toolbar_add_spinner_action(toolbar);

    refresh_button = lvgl_toolbar_add_image_button_action(toolbar, LV_SYMBOL_REFRESH, onRefreshPressed, this);

    enable_switch = lvgl_toolbar_add_switch_action(toolbar);
    lv_obj_add_event_cb(enable_switch, onEnableSwitchChanged, LV_EVENT_VALUE_CHANGED, bindings);

     // Networks

    networks_list = lv_obj_create(parent);
    lv_obj_set_flex_grow(networks_list, 1);
    lv_obj_set_width(networks_list, LV_PCT(100));
    lv_obj_set_flex_flow(networks_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(networks_list, 0, LV_STATE_DEFAULT);
#endif
}

void View::update(bool rebuildList) {
    if (root == nullptr) {
        // Buried (or not yet built) - see reset().
        return;
    }
#if defined(CONFIG_TT_MOBILE_HOME)
    updateMobile(rebuildList);
#else
    updateWifiToggle();
    updateScanning();
    updateRefreshButton();
    if (rebuildList) {
        updateNetworkList();
    }
    updateConnectToHidden();
#endif
}

void View::reset() {
    root = nullptr;
#if defined(CONFIG_TT_MOBILE_HOME)
    toolbar = nullptr;
    switch_track = nullptr;
    switch_knob = nullptr;
    caption = nullptr;
    list_card = nullptr;
    connected_card = nullptr;
    status = nullptr;
    connected_ssid.clear();
    error_message.clear();
#endif
    enable_switch = nullptr;
    scanning_spinner = nullptr;
    refresh_button = nullptr;
    networks_list = nullptr;
    connect_to_hidden = nullptr;
}

} // namespace
