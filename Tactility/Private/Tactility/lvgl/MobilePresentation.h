// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

/**
 * Which Home layout a v2 build shows. The absolute 240x320 design cannot stretch: on any other
 * logical display it falls back to the legacy launcher, which the v2 chrome still styles.
 */
namespace tt::lvgl::mobile {

constexpr int32_t HOME_LAYOUT_WIDTH = 240;
constexpr int32_t HOME_LAYOUT_HEIGHT = 320;

/**
 * @param[in] mobile_home_enabled the build has the v2 presentation
 * @param[in] width the logical display width after rotation
 * @param[in] height the logical display height after rotation
 * @return true when the absolute 240x320 Home may be built
 */
inline bool isV2HomeLayout(bool mobile_home_enabled, int32_t width, int32_t height) {
    return mobile_home_enabled && width == HOME_LAYOUT_WIDTH && height == HOME_LAYOUT_HEIGHT;
}

} // namespace tt::lvgl::mobile
