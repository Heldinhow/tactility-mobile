// SPDX-License-Identifier: Apache-2.0
#include <lvgl/devices/keyboard.h>
#include <lvgl/devices/device_context.h>
#include <lvgl/devices/keyboard_private.h>
#include <lvgl/grid_navigation.h>
#include <lvgl/lvgl.h>
#include <lvgl/fonts.h>
#include <lvgl/theme.h>

#include <tactility/drivers/keyboard.h>
#include <tactility/log.h>

#include <cstring>
#include <vector>

#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

constexpr auto* TAG = "lvgl_keyboard";

static LvglSoftwareKeyboard last_software_keyboard = {
    .object = nullptr
};

static lv_group_t* keyboard_group;
// The group that keypad indevs are attached to: keyboard_group, or the group of an overlay
static lv_group_t* active_group;


#if defined(CONFIG_TT_MOBILE_HOME)

// The v2 keyboard: four 40 px rows of 1 px gaps. The widget stays an lv_keyboard (one
// buttonmatrix), but its stock key handler is replaced so the labels can be the design's
// "123", "space" and "Go" and the mode keys can use icons. The colours mirror
// Tactility/lvgl/MobileTheme.h, which this module cannot include.
namespace {

constexpr uint32_t KB_BG = 0x0E1116;
constexpr uint32_t KB_SURFACE = 0x1A1F27;
constexpr uint32_t KB_SURFACE_2 = 0x242A33;
constexpr uint32_t KB_LINE_SOFT = 0x1F2530;
constexpr uint32_t KB_TEXT = 0xE8ECF1;
constexpr uint32_t KB_ON_ACCENT = 0x0B1118;
constexpr uint32_t KB_ACCENT = 0x4C8DFF;
constexpr int32_t KB_HEIGHT = 168;
constexpr int32_t KB_TOP = 320 - KB_HEIGHT;

// Widths are in half-key units (20 per row), as in the mockup
const char* const KB_MAP_LOWER[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    " ", "a", "s", "d", "f", "g", "h", "j", "k", "l", " ", "\n",
    LV_SYMBOL_UP, "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
    "123", "space", "Go", ""
};
const uint32_t KB_CTRL_LOWER[] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    LV_BUTTONMATRIX_CTRL_HIDDEN | 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, LV_BUTTONMATRIX_CTRL_HIDDEN | 1,
    3, 2, 2, 2, 2, 2, 2, 2, 3,
    4, 11, LV_BUTTONMATRIX_CTRL_CHECKED | 5,
    0
};

const char* const KB_MAP_UPPER[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    " ", "A", "S", "D", "F", "G", "H", "J", "K", "L", " ", "\n",
    LV_SYMBOL_UP, "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "123", "space", "Go", ""
};
const uint32_t KB_CTRL_UPPER[] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    LV_BUTTONMATRIX_CTRL_HIDDEN | 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, LV_BUTTONMATRIX_CTRL_HIDDEN | 1,
    LV_BUTTONMATRIX_CTRL_CHECKED | 3, 2, 2, 2, 2, 2, 2, 2, 3,
    4, 11, LV_BUTTONMATRIX_CTRL_CHECKED | 5,
    0
};

const char* const KB_MAP_SPECIAL[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    " ", "-", "/", ":", ";", "(", ")", "$", "&", "\"", " ", "\n",
    "~", "!", "?", ",", ".", "'", "*", "#", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "space", "Go", ""
};
const uint32_t KB_CTRL_SPECIAL[] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    LV_BUTTONMATRIX_CTRL_HIDDEN | 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, LV_BUTTONMATRIX_CTRL_HIDDEN | 1,
    3, 2, 2, 2, 2, 2, 2, 2, 3,
    4, 11, LV_BUTTONMATRIX_CTRL_CHECKED | 5,
    0
};

void onV2KeyboardValue(lv_event_t* event) {
    auto* keyboard = lv_event_get_target_obj(event);
    const uint32_t id = lv_buttonmatrix_get_selected_button(keyboard);
    if (id == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }
    const char* text = lv_buttonmatrix_get_button_text(keyboard, id);
    if (text == nullptr) {
        return;
    }
    lv_obj_t* textarea = lv_keyboard_get_textarea(keyboard);
    if (textarea == nullptr) {
        return;
    }
    if (strcmp(text, "123") == 0) {
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_SPECIAL);
    } else if (strcmp(text, "abc") == 0) {
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    } else if (strcmp(text, LV_SYMBOL_UP) == 0) {
        const auto mode = lv_keyboard_get_mode(keyboard);
        lv_keyboard_set_mode(keyboard, mode == LV_KEYBOARD_MODE_TEXT_UPPER ? LV_KEYBOARD_MODE_TEXT_LOWER : LV_KEYBOARD_MODE_TEXT_UPPER);
    } else if (strcmp(text, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_delete_char(textarea);
    } else if (strcmp(text, "space") == 0) {
        lv_textarea_add_char(textarea, ' ');
    } else if (strcmp(text, "Go") == 0) {
        lv_obj_send_event(textarea, LV_EVENT_READY, nullptr);
    } else {
        lv_textarea_add_text(textarea, text);
    }
}

void styleV2Keyboard(lv_obj_t* keyboard) {
    // The keyboard is a child of the window root, which lays its children out with flex. Keep the
    // keyboard out of that layout so its design position (0,152) is honoured.
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_IGNORE_LAYOUT);
    // lv_keyboard's constructor aligns the widget to the bottom; clear that alignment so the
    // design position survives the layout pass.
    lv_obj_set_align(keyboard, LV_ALIGN_DEFAULT);
    lv_obj_set_pos(keyboard, 0, KB_TOP);
    lv_obj_set_size(keyboard, 240, KB_HEIGHT);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(KB_BG), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(keyboard, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(keyboard, LV_BORDER_SIDE_TOP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(keyboard, lv_color_hex(KB_LINE_SOFT), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(keyboard, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(keyboard, 4, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(keyboard, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(keyboard, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(keyboard, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(keyboard, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(keyboard, 1, LV_STATE_DEFAULT);

    const lv_style_selector_t items = LV_PART_ITEMS;
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(KB_SURFACE_2), items);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, items);
    lv_obj_set_style_radius(keyboard, 8, items);
    lv_obj_set_style_border_width(keyboard, 0, items);
    lv_obj_set_style_text_color(keyboard, lv_color_hex(KB_TEXT), items);
    lv_obj_set_style_text_font(keyboard, lvgl_get_text_font(FONT_SIZE_DENSE), items);
    // The stock keyboard marks the submit key checked, which gives it the accent role
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(KB_SURFACE), items | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(KB_ACCENT), items | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(keyboard, lv_color_hex(KB_ON_ACCENT), items | LV_STATE_CHECKED);
}

void installV2Map(lv_obj_t* keyboard) {
    // lv_keyboard's stock handler recognises its own "1#"/"abc"/"ABC"/new-line tokens by text.
    // Replace it so the design's labels and the mode keys keep their meaning.
    while (lv_obj_get_event_count(keyboard) > 0) {
        lv_event_dsc_t* dsc = lv_obj_get_event_dsc(keyboard, 0);
        if (dsc == nullptr || !lv_obj_remove_event_dsc(keyboard, dsc)) {
            break;
        }
    }
    lv_obj_add_event_cb(keyboard, onV2KeyboardValue, LV_EVENT_VALUE_CHANGED, nullptr);

    const auto* lower_ctrl = reinterpret_cast<const lv_buttonmatrix_ctrl_t*>(KB_CTRL_LOWER);
    const auto* upper_ctrl = reinterpret_cast<const lv_buttonmatrix_ctrl_t*>(KB_CTRL_UPPER);
    const auto* special_ctrl = reinterpret_cast<const lv_buttonmatrix_ctrl_t*>(KB_CTRL_SPECIAL);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, KB_MAP_LOWER, lower_ctrl);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, KB_MAP_UPPER, upper_ctrl);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_SPECIAL, KB_MAP_SPECIAL, special_ctrl);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_NUMBER, KB_MAP_SPECIAL, special_ctrl);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_popovers(keyboard, false);
    styleV2Keyboard(keyboard);
}

bool isV2KeyboardDisplay(lv_obj_t* keyboard) {
    lv_display_t* display = lv_obj_get_display(keyboard);
    return lv_display_get_horizontal_resolution(display) == 240 && lv_display_get_vertical_resolution(display) == 320;
}

void animateKeyboard(lv_obj_t* keyboard, bool show) {
    LvglThemeSettings theme_settings = {};
    lvgl_theme_get_settings(&theme_settings);
    if (!theme_settings.animations_enabled) {
        lv_obj_set_style_translate_y(keyboard, 0, LV_STATE_DEFAULT);
        lv_obj_set_hidden(keyboard, !show);
        return;
    }

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, keyboard);
    lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
        lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(object), value, LV_STATE_DEFAULT);
    });
    lv_anim_set_values(&animation, show ? KB_HEIGHT : 0, show ? 0 : KB_HEIGHT);
    lv_anim_set_duration(&animation, 140);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    if (!show) {
        lv_anim_set_completed_cb(&animation, [](lv_anim_t* animation) {
            lv_obj_set_hidden(static_cast<lv_obj_t*>(animation->var), true);
        });
    }
    lv_anim_start(&animation);
}

}

#endif // CONFIG_TT_MOBILE_HOME

extern "C" {

void lvgl_keyboard_on_start_lvgl() {
    lvgl_lock();
    keyboard_group = lv_group_create();
    check(keyboard_group);
    // We currently set this group as the default, so it doesn't only get (manually added) textareas,
    // but gets all widgets by default. This is a temporary work-around until a proper default group is
    // created to fix the trackball issue (see ideas.md, search for "group")
    lv_group_set_default(keyboard_group);
    active_group = keyboard_group;
    lvgl_unlock();
}

void lvgl_keyboard_on_stop_lvgl() {
    last_software_keyboard = {
        .object = nullptr
    };

    lvgl_lock();
    lv_group_delete(keyboard_group);
    lvgl_unlock();

    keyboard_group = nullptr;
    active_group = nullptr;
}

void lvgl_keyboard_set_active_group(lv_group_t* group) {
    active_group = group != nullptr ? group : keyboard_group;
    lv_group_set_default(active_group);
    auto* indev = lv_indev_get_next(nullptr);
    while (indev != nullptr) {
        const lv_indev_type_t type = lv_indev_get_type(indev);
        if ((type == LV_INDEV_TYPE_KEYPAD || type == LV_INDEV_TYPE_ENCODER) && lv_indev_get_group(indev) != nullptr) {
            lv_indev_set_group(indev, active_group);
        }
        indev = lv_indev_get_next(indev);
    }
}

lv_group_t* lvgl_keyboard_get_app_group() {
    return keyboard_group;
}

// KeyboardKeyData::key is always a Unicode codepoint (see its doc comment / the CodePoint enum) -
// drivers never emit LV_KEY_* directly, including for pure focus-navigation concepts that have no
// ordinary character of their own. LVGL itself hardcodes specific sentinel values in its own
// indev/group/textarea code that don't match the real Unicode codepoint chosen for the same key,
// so those are translated here rather than each driver having to know about LVGL's internals.
// CODEPOINT_BACKSPACE/TAB/ESCAPE/DELETE already equal their LV_KEY_* counterpart numerically, so
// they need no case below - they fall through `default` unchanged.
static uint32_t codepoint_to_lv_key(uint32_t key) {
    switch (key) {
        case CODEPOINT_ENTER: return LV_KEY_ENTER;
        case CODEPOINT_ARROW_LEFT: return LV_KEY_LEFT;
        case CODEPOINT_ARROW_UP: return LV_KEY_PREV;
        case CODEPOINT_ARROW_RIGHT: return LV_KEY_RIGHT;
        case CODEPOINT_ARROW_DOWN: return LV_KEY_NEXT;
        case CODEPOINT_HOME: return LV_KEY_HOME;
        case CODEPOINT_END: return LV_KEY_END;
        case CODEPOINT_YES: return LV_KEY_ENTER;
        case CODEPOINT_NO: return LV_KEY_ESC;
        default: return key;
    }
}

uint32_t lvgl_keyboard_translate_key(lv_indev_t* indev, uint32_t codepoint) {
    lv_group_t* group = lv_indev_get_group(indev);
    lv_obj_t* focused = group != nullptr ? lv_group_get_focused(group) : nullptr;
    if (lvgl_grid_navigation_is_container(focused)) {
        // Grid navigation moves the focus in two dimensions with the arrow keys
        if (codepoint == CODEPOINT_ARROW_UP) {
            return LV_KEY_UP;
        } else if (codepoint == CODEPOINT_ARROW_DOWN) {
            return LV_KEY_DOWN;
        }
    }
    return codepoint_to_lv_key(codepoint);
}

static void lvgl_keyboard_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* wrapper = static_cast<LvglDeviceContext*>(lv_indev_get_driver_data(indev));

    KeyboardKeyData key_data = {};
    if (keyboard_read_key(wrapper->device, &key_data) != ERROR_NONE) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
        return;
    }

    lv_group_t* group = lv_indev_get_group(indev);
    if (key_data.key == CODEPOINT_FOCUS_NEXT || key_data.key == CODEPOINT_FOCUS_PREVIOUS) {
        // Devices that can only step through widgets also visit the children of grid navigation containers.
        // LVGL doesn't see these keys: the group would skip the children, as only the container is in the group.
        if (key_data.pressed && group != nullptr) {
            lvgl_grid_navigation_step(group, key_data.key == CODEPOINT_FOCUS_NEXT);
        }
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = key_data.continue_reading;
        return;
    }

    data->key = lvgl_keyboard_translate_key(indev, key_data.key);
    data->state = key_data.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = key_data.continue_reading;
}

error_t lvgl_keyboard_add(struct Device* device, lv_display_t* display, lv_indev_t** out_indev) {
    if (device == NULL || out_indev == NULL) {
        return ERROR_INVALID_ARGUMENT;
    }
    if (device_get_type(device) != &KEYBOARD_TYPE) {
        return ERROR_INVALID_ARGUMENT;
    }

    // A device can reach here twice: the boot scan in lvgl_devices_attach() binds every
    // KEYBOARD_TYPE device, and a device that wasn't started yet then fires DEVICE_EVENT_STARTED
    // later. A second indev for the same device would be left dangling when the device stops.
    lv_indev_t* existing = lvgl_keyboard_find_by_device(device);
    if (existing != NULL) {
        *out_indev = existing;
        return ERROR_NONE;
    }

    auto* wrapper = new(std::nothrow) LvglDeviceContext(nullptr);
    if (wrapper == NULL) {
        return ERROR_OUT_OF_MEMORY;
    }
    wrapper->device = device;

    lv_indev_t* indev = lv_indev_create();
    if (indev == NULL) {
        delete wrapper;
        return ERROR_OUT_OF_MEMORY;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, lvgl_keyboard_read_cb);
    lv_indev_set_driver_data(indev, wrapper);
    if (display != NULL) {
        lv_indev_set_display(indev, display);
    }

    lvgl_keyboard_enable(indev);

    *out_indev = indev;
    return ERROR_NONE;
}

void lvgl_keyboard_remove(lv_indev_t* indev) {
    if (indev == NULL) {
        return;
    }

    auto* wrapper = static_cast<LvglDeviceContext*>(lv_indev_get_driver_data(indev));
    lv_indev_delete(indev);
    delete wrapper;
}

lv_indev_t* lvgl_keyboard_find_by_device(Device* device) {
    lv_indev_t* indev = lv_indev_get_next(nullptr);
    while (indev != nullptr) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
            auto* wrapper = static_cast<LvglDeviceContext*>(lv_indev_get_driver_data(indev));
            if (wrapper != nullptr && wrapper->device == device) {
                return indev;
            }
        }
        indev = lv_indev_get_next(indev);
    }
    return nullptr;
}

void lvgl_keyboard_enable(lv_indev_t* indev) {
    check(active_group != nullptr);
    lv_indev_set_group(indev, active_group);
}

void lvgl_keyboard_disable(lv_indev_t* indev) {
    lv_indev_set_group(indev, nullptr);
}

static bool lvgl_hardware_keyboard_check_present(Device* device, void* context) {
    bool ready = device_is_ready(device);
    bool present = ready && keyboard_is_present(device);
    LOG_D(TAG, "keyboard device %s: ready=%d present=%d", device->name, (int)ready, (int)present);
    if (!present) {
        return true; // keep looking
    }
    *static_cast<bool*>(context) = true;
    return false; // found one, stop iterating
}

bool lvgl_hardware_keyboard_is_available() {
    // TODO: Refactor the driver subsystem to so it does proper probing/releasing of such devices
    // This work-around exists for the Tab5 keyboard driver.
    bool present = false;
    device_for_each_of_type(&KEYBOARD_TYPE, &present, lvgl_hardware_keyboard_check_present);
    LOG_D(TAG, "lvgl_hardware_keyboard_is_available() -> %d", (int)present);
    return present;
}

void lvgl_hardware_keyboard_add_custom(lv_indev_t* indev) {
    auto* wrapper = new(std::nothrow) LvglDeviceContext(nullptr);
    if (wrapper == nullptr) {
        return;
    }

    lv_indev_set_driver_data(indev, wrapper);

    lvgl_keyboard_enable(indev);
}

void lvgl_hardware_keyboard_remove_custom(lv_indev_t* indev) {
    lvgl_keyboard_disable(indev);
    auto* wrapper = static_cast<LvglDeviceContext*>(lv_indev_get_driver_data(indev));
    lv_indev_set_driver_data(indev, nullptr);
    delete wrapper;
}

static void textarea_show_keyboard(lv_event_t* event) {
    // Re-checked here rather than gated once at lvgl_keyboard_add_textarea() time, so a hardware
    // keyboard that connects/disconnects after the textarea was created is honored immediately.
    if (!lvgl_software_keyboard_is_enabled()) {
        return;
    }
    lv_obj_t* target = lv_event_get_current_target_obj(event);
    if (last_software_keyboard.object != nullptr) {
        lvgl_software_keyboard_show(&last_software_keyboard, target);
        lv_obj_scroll_to_view(target, LV_ANIM_ON);
    }
}

static void textarea_hide_keyboard(lv_event_t* event) {
    if (last_software_keyboard.object == nullptr) {
        return;
    }
    // Only hide if the keyboard is actually bound to the textarea that triggered this
    lv_obj_t* target = lv_event_get_current_target_obj(event);
    if (lv_keyboard_get_textarea(last_software_keyboard.object) != target) {
        return;
    }
    lvgl_software_keyboard_hide(&last_software_keyboard);
}

void lvgl_software_keyboard_construct(LvglSoftwareKeyboard* keyboard, lv_obj_t* parent) {
    keyboard->object = lv_keyboard_create(parent);
    lv_obj_set_hidden(keyboard->object, true);
#if defined(CONFIG_TT_MOBILE_HOME)
    if (isV2KeyboardDisplay(keyboard->object)) {
        installV2Map(keyboard->object);
    }
#endif
    last_software_keyboard = *keyboard;
}

void lvgl_software_keyboard_destruct(LvglSoftwareKeyboard* keyboard) {
    check(keyboard->object);

    lv_obj_delete(keyboard->object);
    keyboard->object = nullptr;

    last_software_keyboard = *keyboard;
}

void lvgl_software_keyboard_show(LvglSoftwareKeyboard* keyboard, lv_obj_t* textarea) {
    assert(keyboard->object != nullptr);
    lv_keyboard_set_textarea(keyboard->object, textarea);
#if defined(CONFIG_TT_MOBILE_HOME)
    if (isV2KeyboardDisplay(keyboard->object)) {
        lv_obj_set_hidden(keyboard->object, false);
        animateKeyboard(keyboard->object, true);
        return;
    }
#endif
    lv_obj_set_hidden(keyboard->object, false);
}

void lvgl_software_keyboard_hide(LvglSoftwareKeyboard* keyboard) {
    assert(keyboard->object != nullptr);
#if defined(CONFIG_TT_MOBILE_HOME)
    if (isV2KeyboardDisplay(keyboard->object)) {
        animateKeyboard(keyboard->object, false);
        return;
    }
#endif
    lv_obj_set_hidden(keyboard->object, true);
}

bool lvgl_software_keyboard_is_enabled() {
    return !lvgl_hardware_keyboard_is_available();
}

LvglSoftwareKeyboard* lvgl_software_keyboard_get_last() {
    return &last_software_keyboard;
}

void lvgl_keyboard_add_textarea(LvglSoftwareKeyboard* keyboard, lv_obj_t* textarea) {
    lv_obj_add_event_cb(textarea, textarea_show_keyboard, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(textarea, textarea_hide_keyboard, LV_EVENT_DEFOCUSED, nullptr);
    lv_obj_add_event_cb(textarea, textarea_hide_keyboard, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(textarea, textarea_hide_keyboard, LV_EVENT_DELETE, nullptr);

    // lv_obj_t auto-remove themselves from the group when they are destroyed (last checked in LVGL 8.3)
    lv_group_add_obj(keyboard_group, textarea);

    lvgl_software_keyboard_activate(keyboard);
}

void lvgl_software_keyboard_activate(LvglSoftwareKeyboard* keyboard) {
    auto* indev = lv_indev_get_next(nullptr);
    check(active_group);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
            lv_indev_set_group(indev, active_group);
        }
        indev = lv_indev_get_next(indev);
    }
}

void lvgl_software_keyboard_deactivate(LvglSoftwareKeyboard* keyboard) {
    auto* indev = lv_indev_get_next(nullptr);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
            lv_indev_set_group(indev, nullptr);
        }
        indev = lv_indev_get_next(indev);
    }
}

}