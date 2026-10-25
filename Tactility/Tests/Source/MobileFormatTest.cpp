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

TEST_CASE("RSSI maps to the three signal buckets") {
    CHECK_EQ(rssiBucket(-30), 3);
    CHECK_EQ(rssiBucket(-60), 3);
    CHECK_EQ(rssiBucket(-61), 2);
    CHECK_EQ(rssiBucket(-75), 2);
    CHECK_EQ(rssiBucket(-76), 1);
    CHECK_EQ(rssiBucket(-90), 1);
}

TEST_CASE("file sizes use bytes, whole kilobytes and whole megabytes") {
    CHECK_EQ(formatFileSize(0), "0 B");
    CHECK_EQ(formatFileSize(512), "512 B");
    CHECK_EQ(formatFileSize(1024), "1 KB");
    CHECK_EQ(formatFileSize(2048), "2 KB");
    CHECK_EQ(formatFileSize(18432), "18 KB");
    CHECK_EQ(formatFileSize(1024 * 1024), "1 MB");
    CHECK_EQ(formatFileSize(1024 * 1024 + 1), "2 MB");
}

TEST_CASE("item counts and the footer use singular forms") {
    CHECK_EQ(formatItemCount(0), "0 items");
    CHECK_EQ(formatItemCount(1), "1 item");
    CHECK_EQ(formatItemCount(12), "12 items");
    CHECK_EQ(formatFolderFooter(0, 0), "0 items - 0 folders");
    CHECK_EQ(formatFolderFooter(1, 1), "1 item - 1 folder");
    CHECK_EQ(formatFolderFooter(6, 2), "6 items - 2 folders");
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
