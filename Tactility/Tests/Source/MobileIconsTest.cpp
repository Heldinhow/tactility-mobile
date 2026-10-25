// SPDX-License-Identifier: Apache-2.0
#include "doctest.h"

#include <Tactility/lvgl/MobileIcons.h>

TEST_CASE("Mobile icon table") {
    CHECK(mobileIconForApp("tactility.settings") == MobileIcon::Settings);
    CHECK(mobileIconForApp("one.grokbot.carousel") == MobileIcon::Bot);
    CHECK(mobileIconForApp("tactility.wifimanage") == MobileIcon::Wifi);
    CHECK(mobileIconForApp("tactility.files") == MobileIcon::Folder);
    CHECK(mobileIconForApp("tactility.audiosettings") == MobileIcon::Vol);
    CHECK(mobileIconForApp("tactility.timedatesettings") == MobileIcon::Calendar);
    CHECK(mobileIconForApp("com.example.unknown") == MobileIcon::Hub);
    CHECK(mobileIconForApp("") == MobileIcon::Hub);
    CHECK(mobileIconGet(MobileIcon::Settings) != nullptr);
    CHECK(mobileIconGet(MobileIcon::Close) != nullptr);
}
