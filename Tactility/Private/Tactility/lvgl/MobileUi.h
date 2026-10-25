// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <lvgl.h>

namespace tt::lvgl::mobile {

/** A flat surface card: surface colour, 12 px radius, no border or padding. */
lv_obj_t* createCard(lv_obj_t* parent, int32_t width, int32_t height);

/** A 44 px list row: 12 px left padding, 8 px right padding, 12 px column gap. */
lv_obj_t* createRow(lv_obj_t* parent);

/** The 36x4 signature bar. The caller aligns it. */
lv_obj_t* createGestureBar(lv_obj_t* parent);

/** A 40 px button with radius 8 in the primary or secondary role. */
lv_obj_t* createButton(lv_obj_t* parent, const char* text, bool primary);

/** Applies the primary/secondary role to an existing button. */
void styleButton(lv_obj_t* button, bool primary);

} // namespace tt::lvgl::mobile
