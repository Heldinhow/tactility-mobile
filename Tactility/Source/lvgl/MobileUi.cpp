// SPDX-License-Identifier: Apache-2.0
#include <Tactility/lvgl/MobileUi.h>

#include <Tactility/lvgl/MobileTheme.h>

#include <lvgl/fonts.h>

namespace tt::lvgl::mobile {

lv_obj_t* createCard(lv_obj_t* parent, int32_t width, int32_t height) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_bg_color(card, color(COLOR_SURFACE), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(card, RADIUS_CARD, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(card, true, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(card, false);
    return card;
}

lv_obj_t* createRow(lv_obj_t* parent) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, ROW_HEIGHT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(row, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(row, 8, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(row, 12, LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(row, false);
    return row;
}

lv_obj_t* createGestureBar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 36, 4);
    lv_obj_set_style_bg_color(bar, color(COLOR_TEXT_2), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, RADIUS_BAR, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bar, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(bar, 0, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(bar, false);
    lv_obj_set_clickable(bar, false);
    return bar;
}

void styleButton(lv_obj_t* button, bool primary) {
    lv_obj_set_height(button, BUTTON_HEIGHT);
    lv_obj_set_style_radius(button, RADIUS_CONTROL, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(button, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(button, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, color(primary ? COLOR_ACCENT : COLOR_SURFACE_2), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(button, color(primary ? COLOR_ON_ACCENT : COLOR_TEXT), LV_STATE_DEFAULT);
}

lv_obj_t* createButton(lv_obj_t* parent, const char* text, bool primary) {
    lv_obj_t* button = lv_button_create(parent);
    styleButton(button, primary);
    lv_obj_t* label = lv_label_create(button);
    lv_obj_set_style_text_font(label, lvgl_get_text_font(FONT_SIZE_DEFAULT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, color(primary ? COLOR_ON_ACCENT : COLOR_TEXT), LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

} // namespace tt::lvgl::mobile
