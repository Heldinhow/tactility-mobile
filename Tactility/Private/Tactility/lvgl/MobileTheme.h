// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <lvgl.h>

#include <cstdint>

/**
 * The v2 mobile design values. Screens use these instead of DPX or multiples of a font size, so the
 * layout stays the one in the approved mockups at 240x320.
 */
namespace tt::lvgl::mobile {

constexpr uint32_t COLOR_BG = 0x0E1116;
constexpr uint32_t COLOR_SURFACE = 0x1A1F27;
constexpr uint32_t COLOR_SURFACE_2 = 0x242A33;
constexpr uint32_t COLOR_LINE = 0x242A33;
constexpr uint32_t COLOR_LINE_SOFT = 0x1F2530;
constexpr uint32_t COLOR_TEXT = 0xE8ECF1;
constexpr uint32_t COLOR_TEXT_2 = 0x9AA4B2;
constexpr uint32_t COLOR_TEXT_3 = 0x8A94A3;
constexpr uint32_t COLOR_ACCENT = 0x4C8DFF;
constexpr uint32_t COLOR_ACCENT_TEXT = 0x9CC0FF;
constexpr uint32_t COLOR_ACCENT_SOFT = 0x1A2B45;
constexpr uint32_t COLOR_ACCENT_LINE = 0x2B4C7E;
constexpr uint32_t COLOR_ON_ACCENT = 0x0B1118;

constexpr int RADIUS_CARD = 12;
constexpr int RADIUS_CONTROL = 8;
constexpr int RADIUS_SWITCH = 10;
constexpr int RADIUS_BAR = 2;

constexpr int SCREEN_WIDTH = 240;
constexpr int SCREEN_HEIGHT = 320;
constexpr int SIDE_MARGIN = 12;
constexpr int TEXT_MARGIN = 16;

constexpr int STATUS_BAR_HEIGHT = 24;
constexpr int TOOLBAR_HEIGHT = 44;
constexpr int CONTENT_TOP = 68;
constexpr int TOOLBAR_ACTION = 44;

constexpr int ROW_HEIGHT = 44;
constexpr int TILE_CELL_WIDTH = 60;
constexpr int TILE_CELL_HEIGHT = 64;
constexpr int TILE_SIZE = 44;
constexpr int ICON_SIZE = 24;
constexpr int ICON_SIZE_SMALL = 16;
constexpr int BUTTON_HEIGHT = 40;

inline lv_color_t color(uint32_t value) {
    return lv_color_hex(value);
}

} // namespace tt::lvgl::mobile
