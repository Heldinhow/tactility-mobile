// SPDX-License-Identifier: Apache-2.0
#include "doctest.h"

#include <Tactility/lvgl/MobileFormat.h>

using namespace tt::lvgl::mobile;

TEST_CASE("percent values are clamped integers") {
    CHECK_EQ(formatPercent(70), "70%");
    CHECK_EQ(formatPercent(0), "0%");
    CHECK_EQ(formatPercent(100), "100%");
    CHECK_EQ(formatPercent(-5), "0%");
    CHECK_EQ(formatPercent(140), "100%");
}

TEST_CASE("the Wi-Fi value distinguishes off, idle and connected") {
    CHECK_EQ(formatWifiValue(false, false, ""), "Off");
    CHECK_EQ(formatWifiValue(false, true, "HomeNet"), "Off");
    CHECK_EQ(formatWifiValue(true, false, ""), "Not connected");
    CHECK_EQ(formatWifiValue(true, false, "HomeNet"), "Not connected");
    CHECK_EQ(formatWifiValue(true, true, ""), "Not connected");
    CHECK_EQ(formatWifiValue(true, true, "HomeNet"), "HomeNet");
}

TEST_CASE("on and off are the radio words") {
    CHECK_EQ(std::string(formatOnOff(true)), "On");
    CHECK_EQ(std::string(formatOnOff(false)), "Off");
}

TEST_CASE("the timeout uses seconds below a minute and whole minutes above") {
    CHECK_EQ(formatTimeout(0), "Never");
    CHECK_EQ(formatTimeout(15000), "15 s");
    CHECK_EQ(formatTimeout(30000), "30 s");
    CHECK_EQ(formatTimeout(1500), "2 s");
    CHECK_EQ(formatTimeout(60000), "1 min");
    CHECK_EQ(formatTimeout(65500), "2 min");
    CHECK_EQ(formatTimeout(90000), "2 min");
    CHECK_EQ(formatTimeout(300000), "5 min");
}
