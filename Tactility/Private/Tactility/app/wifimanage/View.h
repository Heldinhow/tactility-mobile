#pragma once

#include "./Bindings.h"
#include "./State.h"

#include <cstdint>
#include <lvgl.h>

namespace tt::app::wifimanage {

class View final {

    Bindings* bindings;
    State* state;
    uint32_t appInstanceId = 0;
    lv_obj_t* root = nullptr;
    lv_obj_t* enable_switch = nullptr;
    lv_obj_t* scanning_spinner = nullptr;
    lv_obj_t* refresh_button = nullptr;
    lv_obj_t* networks_list = nullptr;
    lv_obj_t* connect_to_hidden = nullptr;

    void updateWifiToggle();
    void updateScanning();
    void updateRefreshButton();
    void updateNetworkList();
    void updateConnectToHidden();
    void createSsidListItem(lv_obj_t* list, const WifiApRecord& record, bool isConnecting, size_t index);

#if defined(CONFIG_TT_MOBILE_HOME)
    lv_obj_t* toolbar = nullptr;
    lv_obj_t* switch_track = nullptr;
    lv_obj_t* switch_knob = nullptr;
    lv_obj_t* caption = nullptr;
    lv_obj_t* list_card = nullptr;
    lv_obj_t* connected_card = nullptr;
    lv_obj_t* status = nullptr;
    bool switch_on = false;
    std::string connected_ssid;
    std::string error_message;

    void initMobile(uint32_t appInstanceId, lv_obj_t* parent);
    void updateMobile(bool rebuildList);
    void buildMobileList();
    void buildConnectedCard(const std::string& ssid);
    void updateMobileSwitch(bool on);
    void updateMobileCaption();
    void showStatus(const char* text);
    static void onMobileSwitchClicked(lv_event_t* event);
    static void onConnectedCardClicked(lv_event_t* event);
    static void onNetworkRowClicked(lv_event_t* event);
#endif

    static void showDetails(lv_event_t* event);
    static void connect(lv_event_t* event);
    static void onRefreshPressed(lv_event_t* event);

public:

    View(Bindings* bindings, State* state) : bindings(bindings), state(state) {}

    void init(uint32_t appInstanceId, lv_obj_t* parent);
#if defined(CONFIG_TT_MOBILE_HOME)
    /** Shows an inline error under the connected card, or clears it with an empty string. */
    void setError(const std::string& message);
#endif
    /** @param[in] rebuildList recreates the network list, which resets the focused item */
    void update(bool rebuildList);
    /** Called when this window's widgets have been (or are about to be) deleted out from under
     * it - see WindowDestroyWidgetsFn. Only nulls out pointers; must stay lock-free. */
    void reset();
};


} // namespace
