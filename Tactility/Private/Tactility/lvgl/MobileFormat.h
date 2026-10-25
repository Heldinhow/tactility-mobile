// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

/**
 * The value strings that the v2 screens show. Pure so the branches stay out of the LVGL code and
 * can be tested without a display.
 */
namespace tt::lvgl::mobile {

/** @return the clamped integer percentage, e.g. "70%" */
inline std::string formatPercent(int value) {
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    char text[8];
    std::snprintf(text, sizeof(text), "%d%%", value);
    return text;
}

/**
 * The Wi-Fi settings value.
 * @param[in] radioOn the radio is on (or turning on)
 * @param[in] connected the station is connected to an access point
 * @param[in] ssid the connected network, may be empty while connecting
 * @return "Off" when the radio is off, "Not connected" when it is on without a network, else the SSID
 */
inline std::string formatWifiValue(bool radioOn, bool connected, const std::string& ssid) {
    if (!radioOn) {
        return "Off";
    }
    if (!connected || ssid.empty()) {
        return "Not connected";
    }
    return ssid;
}

/** @return "On" or "Off" */
inline const char* formatOnOff(bool enabled) {
    return enabled ? "On" : "Off";
}

/**
 * The signal strength bucket that picks the sig1/sig2/sig3 mask.
 * @param[in] rssi the access point's RSSI in dBm
 * @return 3 for at least -60 dBm, 2 for at least -75 dBm, otherwise 1
 */
inline int rssiBucket(int rssi) {
    if (rssi >= -60) {
        return 3;
    }
    if (rssi >= -75) {
        return 2;
    }
    return 1;
}

/**
 * The auto screen-off timeout.
 * @param[in] milliseconds 0 for never, otherwise rounded up to whole seconds below a minute and
 * to whole minutes from a minute up (65500 -> "1 min", 90000 -> "2 min")
 * @return e.g. "30 s", "5 min" or "Never"
 */
inline std::string formatTimeout(uint32_t milliseconds) {
    if (milliseconds == 0) {
        return "Never";
    }
    char text[16];
    if (milliseconds < 60000U) {
        std::snprintf(text, sizeof(text), "%u s", static_cast<unsigned>((milliseconds + 999U) / 1000U));
    } else {
        std::snprintf(text, sizeof(text), "%u min", static_cast<unsigned>((milliseconds + 59999U) / 60000U));
    }
    return text;
}

} // namespace tt::lvgl::mobile
