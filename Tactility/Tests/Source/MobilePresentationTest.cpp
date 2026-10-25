// SPDX-License-Identifier: Apache-2.0
#include "doctest.h"

#include <Tactility/lvgl/MobilePresentation.h>

using namespace tt::lvgl::mobile;

TEST_CASE("the v2 Home is built for 240x320 portrait") {
    CHECK(isV2HomeLayout(true, 240, 320));
}

TEST_CASE("a flipped portrait rotation still builds the v2 Home") {
    // Rotating 180 degrees swaps the rotation but not the logical dimensions
    CHECK(isV2HomeLayout(true, 240, 320));
}

TEST_CASE("landscape and other resolutions fall back to the legacy layout") {
    CHECK_FALSE(isV2HomeLayout(true, 320, 240));
    CHECK_FALSE(isV2HomeLayout(true, 480, 320));
    CHECK_FALSE(isV2HomeLayout(true, 240, 240));
    CHECK_FALSE(isV2HomeLayout(true, 0, 0));
}

TEST_CASE("a build without the flag never uses the v2 layout") {
    CHECK_FALSE(isV2HomeLayout(false, 240, 320));
}
