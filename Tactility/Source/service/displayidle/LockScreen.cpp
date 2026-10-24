// SPDX-License-Identifier: Apache-2.0

#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

#if defined(CONFIG_TT_MOBILE_HOME)

#include <Tactility/lvgl/ClockText.h>
#include <Tactility/lvgl/Fonts.h>
#include <Tactility/service/displayidle/LockScreen.h>
#include <Tactility/settings/Time.h>

#include <lvgl/devices/keyboard.h>
#include <lvgl/fonts.h>
#include <lvgl/icons/shared.h>
#include <lvgl/lvgl.h>

#include <cstring>
#include <ctime>
#include <new>

namespace tt::service::displayidle {

namespace {

struct LockWidgets {
    lv_obj_t* root = nullptr;
    lv_obj_t* clock = nullptr;
    lv_obj_t* ampm = nullptr;
    lv_obj_t* date = nullptr;
    lv_timer_t* timer = nullptr;
    lv_group_t* group = nullptr;
    lv_group_t* previous_group = nullptr;
    char date_text[32] = {};
    LockUnlockCallback callback = nullptr;
    void* context = nullptr;
};

LockWidgets* lock = nullptr;

void updateClock() {
    const time_t now = time(nullptr);
    const tm* local = localtime(&now);
    if (local == nullptr) {
        lv_label_set_text(lock->clock, "--:--");
        lv_obj_set_hidden(lock->ampm, true);
        return;
    }

    const auto text = lvgl::formatClock(*local, settings::isTimeFormat24Hour());
    lv_label_set_text(lock->clock, text.time);
    if (text.meridiem != nullptr) {
        lv_label_set_text(lock->ampm, text.meridiem);
        lv_obj_set_hidden(lock->ampm, false);
    } else {
        lv_obj_set_hidden(lock->ampm, true);
    }
    if (strcmp(lock->date_text, text.date) != 0) {
        strncpy(lock->date_text, text.date, sizeof(lock->date_text) - 1);
        lock->date_text[sizeof(lock->date_text) - 1] = '\0';
        lv_label_set_text(lock->date, lock->date_text);
    }
}

void onTimer(lv_timer_t* timer) {
    updateClock();

    const time_t now = time(nullptr);
    const tm* local = localtime(&now);
    lv_timer_set_period(timer, local != nullptr ? lvgl::millisToNextMinute(*local) : 60000U);
}

void onKey(lv_event_t* event) {
    lv_event_stop_processing(event);
    lockScreenRequestUnlock();
}

// The release of the press that woke the display must not click whatever was under the finger
void consumePressedPointers() {
    for (lv_indev_t* indev = lv_indev_get_next(nullptr); indev != nullptr; indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
            lv_indev_wait_release(indev);
        }
    }
}

}

void lockScreenSetUnlockCallback(LockUnlockCallback callback, void* context) {
    if (callback == nullptr && context == nullptr) {
        if (lock != nullptr) {
            lock->callback = nullptr;
            lock->context = nullptr;
            // The owner hides the widgets under the LVGL lock before clearing the callback
            if (lock->root == nullptr) {
                delete lock;
                lock = nullptr;
            }
        }
        return;
    }

    if (lock == nullptr) {
        lock = new (::std::nothrow) LockWidgets();
    }
    if (lock != nullptr) {
        lock->callback = callback;
        lock->context = context;
    }
}

void lockScreenRequestUnlock() {
    if (lock != nullptr && lock->callback != nullptr) {
        lock->callback(lock->context);
    }
}

bool lockScreenIsVisible() {
    return lock != nullptr && lock->root != nullptr;
}

bool lockScreenShow() {
    if (lock == nullptr) {
        lock = new (::std::nothrow) LockWidgets();
        if (lock == nullptr) {
            return false;
        }
    }
    if (lock->root != nullptr) {
        return true;
    }

    auto* root = lv_obj_create(lv_layer_top());
    if (root == nullptr) {
        return false;
    }
    lock->root = root;
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(root, lv_obj_get_style_bg_color(lv_screen_active(), LV_PART_MAIN), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(root, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(root, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(root, 0, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(root, false);
    lv_obj_set_clickable(root, true);
    lv_obj_add_event_cb(root, onKey, LV_EVENT_KEY, nullptr);

    auto* clock_row = lv_obj_create(root);
    lv_obj_remove_style_all(clock_row);
    lv_obj_set_size(clock_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(clock_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(clock_row, 4, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(clock_row, false);
    lv_obj_align(clock_row, LV_ALIGN_CENTER, 0, -20);

    lock->clock = lv_label_create(clock_row);
    lv_obj_set_style_text_font(lock->clock, lvgl::getClockFont(), LV_STATE_DEFAULT);
    lv_label_set_text(lock->clock, "--:--");

    lock->ampm = lv_label_create(clock_row);
    lv_obj_set_style_text_font(lock->ampm, lvgl_get_text_font(FONT_SIZE_SMALL), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(lock->ampm, 3, LV_STATE_DEFAULT);
    lv_obj_set_hidden(lock->ampm, true);

    lock->date = lv_label_create(root);
    lv_obj_set_style_text_font(lock->date, lvgl_get_text_font(FONT_SIZE_SMALL), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(lock->date, LV_OPA_70, LV_STATE_DEFAULT);
    lv_obj_update_layout(root);
    lv_obj_align_to(lock->date, clock_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    auto* hint = lv_obj_create(root);
    lv_obj_remove_style_all(hint);
    lv_obj_set_size(hint, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hint, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hint, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hint, 4, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(hint, false);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -24);

    auto* hint_icon = lv_label_create(hint);
    lv_obj_set_style_text_font(hint_icon, lvgl_get_shared_icon_default_font(), LV_STATE_DEFAULT);
    lv_label_set_text(hint_icon, LVGL_ICON_SHARED_KEYBOARD_ARROW_UP);

    auto* hint_text = lv_label_create(hint);
    lv_obj_set_style_text_font(hint_text, lvgl_get_text_font(FONT_SIZE_SMALL), LV_STATE_DEFAULT);
    lv_label_set_text(hint_text, "Swipe up");

    updateClock();
    const time_t now = time(nullptr);
    const tm* local = localtime(&now);
    lock->timer = lv_timer_create(onTimer, local != nullptr ? lvgl::millisToNextMinute(*local) : 60000U, nullptr);

    lock->previous_group = lv_group_get_default();
    lock->group = lv_group_create();
    if (lock->group != nullptr) {
        lv_group_add_obj(lock->group, root);
        lv_group_focus_obj(root);
        lvgl_keyboard_set_active_group(lock->group);
    }

    consumePressedPointers();
    return true;
}

void lockScreenHide() {
    if (lock == nullptr) {
        return;
    }
    if (lock->timer != nullptr) {
        lv_timer_delete(lock->timer);
        lock->timer = nullptr;
    }

    if (lock->group != nullptr) {
        // Put the keypad/encoder indevs back on the app group before the lock group disappears
        lvgl_keyboard_set_active_group(lock->previous_group);
    }
    if (lock->root != nullptr) {
        lv_obj_delete(lock->root);
    }
    if (lock->group != nullptr) {
        lv_group_delete(lock->group);
    }

    lock->root = nullptr;
    lock->clock = nullptr;
    lock->ampm = nullptr;
    lock->date = nullptr;
    lock->group = nullptr;
    lock->previous_group = nullptr;
    lock->date_text[0] = '\0';
}

}

#endif
