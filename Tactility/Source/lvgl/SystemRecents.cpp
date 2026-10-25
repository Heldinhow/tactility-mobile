#include <Tactility/lvgl/SystemRecents.h>

#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

#if defined(CONFIG_TT_SYSTEM_GESTURES)

#include <Tactility/app/AppIcon.h>
#include <Tactility/lvgl/MobileIcons.h>
#include <Tactility/lvgl/MobileTheme.h>
#include <Tactility/lvgl/MobileUi.h>
#include <Tactility/lvgl/SystemWindow.h>

#include <app/event.h>
#include <app/manager.h>
#include <app/manifest.h>

#include <lvgl/devices/indev.h>
#include <lvgl/fonts.h>
#include <lvgl/icons/shared.h>
#include <lvgl/lvgl.h>
#include <lvgl/theme.h>
#include <lvgl/widgets/card.h>
#include <lvgl/widgets/icon_button.h>

#include <lvgl_window_manager/window_manager.h>

#include <cstdint>
#include <cstring>


namespace tt::lvgl {

namespace {

/** How many cards the switcher shows: the most recently used apps. */
constexpr size_t MAX_ENTRIES = 5;
constexpr auto* LAUNCHER_APP_ID = "tactility.launcher";

struct RecentsEntry {
    WindowId window;
    AppInstanceId app_instance_id;
    char app_id[APP_MANIFEST_ID_LENGTH + 1];
    char name[APP_MANIFEST_NAME_LENGTH + 1];
    const char* icon;
};

/** Looks up which app owns a window, so a card action can act on it. */
AppInstanceId findAppInstance(WindowId window) {
    AppInstanceId found = 0;
    forEachWindow([&found, window](const WindowInfo& candidate) {
        if (candidate.id != window) {
            return true;
        }
        found = candidate.app_instance_id;
        return false;
    });
    return found;
}

/** Collects the most recent apps, topmost first. The Home is not a recent app. */
size_t collectEntries(RecentsEntry* entries) {
    const size_t total = window_manager_get_windows(nullptr, 0, 0);
    const size_t shown = total > MAX_ENTRIES * 2 ? MAX_ENTRIES * 2 : total;
    if (shown == 0) {
        return 0;
    }

    WindowInfo windows[MAX_ENTRIES * 2];
    window_manager_get_windows(windows, total - shown, shown);

    size_t entry_count = 0;
    for (size_t i = shown; i > 0 && entry_count < MAX_ENTRIES; i--) {
        const WindowInfo& window = windows[i - 1];
        char app_id[APP_MANIFEST_ID_LENGTH + 1];
        if (app_manager_get_instance_app_id(window.app_instance_id, app_id, sizeof(app_id)) != ERROR_NONE) {
            continue;
        }
        if (strcmp(app_id, LAUNCHER_APP_ID) == 0) {
            continue;
        }
        ::AppManifest manifest {};
        if (app_manager_find_manifest(app_id, &manifest) != ERROR_NONE) {
            continue;
        }
        RecentsEntry& entry = entries[entry_count++];
        entry.window = window.id;
        entry.app_instance_id = window.app_instance_id;
        strncpy(entry.app_id, app_id, APP_MANIFEST_ID_LENGTH);
        entry.app_id[APP_MANIFEST_ID_LENGTH] = '\0';
        entry.icon = ::tt::app::getAppIcon(manifest);
        strncpy(entry.name, manifest.name, APP_MANIFEST_NAME_LENGTH);
        entry.name[APP_MANIFEST_NAME_LENGTH] = '\0';
    }
    return entry_count;
}

#if defined(CONFIG_TT_MOBILE_HOME)

namespace mobile = tt::lvgl::mobile;

constexpr int32_t CARD_LEFT = 16;
constexpr int32_t CARD_TOP = 76;
constexpr int32_t CARD_WIDTH = 208;
constexpr int32_t CARD_HEIGHT = 44;
constexpr int32_t CARD_GAP = 4;
constexpr int32_t CARD_PITCH = CARD_HEIGHT + CARD_GAP;
constexpr int32_t REMOVE_DURATION = 120;
constexpr int32_t OPEN_DURATION = 150;

struct Card {
    lv_obj_t* object = nullptr;
    WindowId window = WINDOW_ID_NONE;
    AppInstanceId app_instance_id = 0;
    lv_obj_t* active_bar = nullptr;
};

struct State {
    Card cards[MAX_ENTRIES];
    size_t count = 0;
    lv_obj_t* empty_label = nullptr;
    lv_obj_t* content = nullptr;
};

State state;

void updateActiveBar(size_t active_index) {
    for (size_t i = 0; i < state.count; i++) {
        lv_obj_set_hidden(state.cards[i].active_bar, i != active_index);
    }
}

void onCardClicked(lv_event_t* event) {
    auto* card = static_cast<Card*>(lv_event_get_user_data(event));
    // Hide first: the app's window is rebuilt below where the (deleted) overlay was
    window_manager_overlay_hide();
    raiseWindow(card->window);
}

void closeCard(size_t index);

void onCardGesture(lv_event_t* event) {
    auto* card = static_cast<Card*>(lv_event_get_user_data(event));
    lv_indev_t* indev = lv_indev_active();
    if (indev == nullptr || lv_indev_get_gesture_dir(indev) != LV_DIR_TOP) {
        return;
    }
    // The release must not click the card
    lv_indev_wait_release(indev);
    for (size_t i = 0; i < state.count; i++) {
        if (state.cards[i].object == card->object) {
            closeCard(i);
            return;
        }
    }
}

void onCloseCardClicked(lv_event_t* event) {
    lv_event_stop_bubbling(event);
    auto* card = static_cast<Card*>(lv_event_get_user_data(event));
    for (size_t i = 0; i < state.count; i++) {
        if (state.cards[i].object == card->object) {
            closeCard(i);
            return;
        }
    }
}

void onCardRemoved(lv_anim_t* animation) {
    lv_obj_delete(static_cast<lv_obj_t*>(animation->var));
}

void closeCard(size_t index) {
    if (index >= state.count) {
        return;
    }
    Card& removed = state.cards[index];
    // The app closes itself, which raises whichever window is below it
    app_event_emit_close(removed.app_instance_id);

    LvglThemeSettings theme_settings = {};
    lvgl_theme_get_settings(&theme_settings);

    if (theme_settings.animations_enabled) {
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, removed.object);
        lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
            lv_obj_set_y(static_cast<lv_obj_t*>(object), value);
        });
        lv_anim_set_values(&animation, lv_obj_get_y(removed.object), lv_obj_get_y(removed.object) - CARD_HEIGHT);
        lv_anim_set_duration(&animation, REMOVE_DURATION);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
        lv_anim_set_completed_cb(&animation, onCardRemoved);
        lv_anim_start(&animation);
    } else {
        lv_obj_delete(removed.object);
    }

    // Remove from the list and move the cards below up
    for (size_t i = index; i + 1 < state.count; i++) {
        state.cards[i] = state.cards[i + 1];
    }
    state.count--;

    for (size_t i = index; i < state.count; i++) {
        lv_obj_t* object = state.cards[i].object;
        const int32_t target = CARD_TOP + static_cast<int32_t>(i) * CARD_PITCH;
        if (theme_settings.animations_enabled) {
            lv_anim_t animation;
            lv_anim_init(&animation);
            lv_anim_set_var(&animation, object);
            lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
                lv_obj_set_y(static_cast<lv_obj_t*>(object), value);
            });
            lv_anim_set_values(&animation, lv_obj_get_y(object), target);
            lv_anim_set_duration(&animation, REMOVE_DURATION);
            lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
            lv_anim_start(&animation);
        } else {
            lv_obj_set_y(object, target);
        }
    }

    updateActiveBar(0);
    if (state.count == 0) {
        lv_obj_remove_flag(state.empty_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void onBackdropGesture(lv_event_t* /*event*/) {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr && lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
        // The release must not click the pressed widget
        lv_indev_wait_release(indev);
        window_manager_overlay_hide();
    }
}

void createCard(lv_obj_t* parent, const RecentsEntry& entry, bool active, size_t index) {
    auto* card = lv_button_create(parent);
    lv_obj_set_pos(card, CARD_LEFT, CARD_TOP + static_cast<int32_t>(index) * CARD_PITCH);
    lv_obj_set_size(card, CARD_WIDTH, CARD_HEIGHT);
    lv_obj_set_style_bg_color(card, mobile::color(mobile::COLOR_SURFACE), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(card, mobile::RADIUS_CARD, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, mobile::color(mobile::COLOR_SURFACE_2), LV_STATE_PRESSED);
    lv_obj_set_scrollable(card, false);
    lv_obj_add_event_cb(card, onCardClicked, LV_EVENT_SHORT_CLICKED, &state.cards[index]);
    lv_obj_set_gesture_bubble(card, false);
    lv_obj_add_event_cb(card, onCardGesture, LV_EVENT_GESTURE, &state.cards[index]);

    lv_obj_t* active_bar = lv_obj_create(card);
    lv_obj_remove_style_all(active_bar);
    lv_obj_set_pos(active_bar, 4, 12);
    lv_obj_set_size(active_bar, 3, 20);
    lv_obj_set_style_bg_color(active_bar, mobile::color(mobile::COLOR_ACCENT), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(active_bar, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(active_bar, mobile::RADIUS_BAR, LV_STATE_DEFAULT);
    lv_obj_set_clickable(active_bar, false);
    lv_obj_set_hidden(active_bar, !active);
    state.cards[index].active_bar = active_bar;

    lv_obj_t* icon = mobile::createIcon(
        card,
        mobileIconForApp(entry.app_id),
        active ? mobile::COLOR_TEXT : mobile::COLOR_TEXT_2
    );
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t* name = mobile::createLabel(card, entry.name, FONT_SIZE_DEFAULT, mobile::COLOR_TEXT);
    lv_obj_set_width(name, 108);
    lv_obj_set_pos(name, 48, 12);
    lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);

    lv_obj_t* close = lv_button_create(card);
    lv_obj_set_pos(close, CARD_WIDTH - 40, 0);
    lv_obj_set_size(close, 40, CARD_HEIGHT);
    lv_obj_set_style_bg_opa(close, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(close, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(close, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(close, 0, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(close, false);
    lv_obj_t* close_icon = mobile::createIcon(close, MobileIcon::Close, mobile::COLOR_TEXT_2);
    lv_obj_center(close_icon);
    lv_obj_add_event_cb(close, onCloseCardClicked, LV_EVENT_SHORT_CLICKED, &state.cards[index]);

    state.cards[index].object = card;
    state.cards[index].window = entry.window;
    state.cards[index].app_instance_id = entry.app_instance_id;
}

void createWidgets(lv_obj_t* parent, void*) {
    // Opaque: nothing of the app behind may be visible, including the status bar
    lv_obj_set_style_bg_color(parent, mobile::color(mobile::COLOR_BG), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_gesture_bubble(parent, false);
    lv_obj_add_event_cb(parent, onBackdropGesture, LV_EVENT_GESTURE, nullptr);

    state = {};
    state.content = lv_obj_create(parent);
    lv_obj_remove_style_all(state.content);
    lv_obj_set_size(state.content, 240, 320);
    lv_obj_set_pos(state.content, 0, 0);
    lv_obj_set_scrollable(state.content, false);

    lv_obj_t* title = mobile::createLabel(state.content, "Recents", FONT_SIZE_LARGE, mobile::COLOR_TEXT);
    lv_obj_set_pos(title, 16, 28);
    lv_obj_t* hint = mobile::createLabel(state.content, "Swipe up to close", FONT_SIZE_SMALL, mobile::COLOR_TEXT_2);
    lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -16, 36);

    state.empty_label = mobile::createLabel(state.content, "No open apps", FONT_SIZE_DEFAULT, mobile::COLOR_TEXT_2);
    lv_obj_align(state.empty_label, LV_ALIGN_CENTER, 0, 0);

    RecentsEntry entries[MAX_ENTRIES];
    state.count = collectEntries(entries);
    lv_obj_set_hidden(state.empty_label, state.count > 0);

    for (size_t i = 0; i < state.count; i++) {
        createCard(state.content, entries[i], i == 0, i);
    }

    LvglThemeSettings theme_settings = {};
    lvgl_theme_get_settings(&theme_settings);
    if (theme_settings.animations_enabled) {
        lv_obj_set_style_translate_y(state.content, 24, LV_STATE_DEFAULT);
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, state.content);
        lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
            lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(object), value, LV_STATE_DEFAULT);
        });
        lv_anim_set_values(&animation, 24, 0);
        lv_anim_set_duration(&animation, OPEN_DURATION);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
}

#else

// region The legacy switcher, unchanged

void onCardClicked(lv_event_t* event) {
    auto* card = lv_event_get_current_target_obj(event);
    const auto window = fromWindowContext(lv_obj_get_user_data(card));
    // Hide first: the app's window is rebuilt below where the (deleted) overlay was
    window_manager_overlay_hide();
    raiseWindow(window);
}

void onCardGesture(lv_event_t* event) {
    lv_indev_t* indev = lv_indev_active();
    if (indev == nullptr || lv_indev_get_gesture_dir(indev) != LV_DIR_TOP) {
        return;
    }
    auto* card = lv_event_get_current_target_obj(event);
    const auto window = fromWindowContext(lv_obj_get_user_data(card));
    const AppInstanceId app_instance_id = findAppInstance(window);
    // The release must not click the card
    lv_indev_wait_release(indev);
    window_manager_overlay_hide();
    if (app_instance_id != 0) {
        // The app closes itself, which raises whichever window is below it
        app_event_emit_close(app_instance_id);
    }
}

void onBackdropGesture(lv_event_t* /*event*/) {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr && lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
        // The release must not click the pressed widget
        lv_indev_wait_release(indev);
        window_manager_overlay_hide();
    }
}

void onCloseClicked(lv_event_t* /*event*/) {
    window_manager_overlay_hide();
}

void createCard(lv_obj_t* parent, const RecentsEntry& entry) {
    auto* card = lvgl_card_create(parent);
    lv_obj_set_size(card, LV_PCT(66), LV_PCT(100));
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, lvgl_theme_is_compact() ? 4 : 8, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(card, false);
    lv_obj_set_user_data(card, toWindowContext(entry.window));
    lv_obj_add_event_cb(card, onCardClicked, LV_EVENT_SHORT_CLICKED, nullptr);
    // Swiping up closes this card's app
    lv_obj_set_gesture_bubble(card, false);
    lv_obj_add_event_cb(card, onCardGesture, LV_EVENT_GESTURE, nullptr);

    auto* icon = lv_label_create(card);
    lv_obj_set_style_text_font(icon, lvgl_get_shared_icon_large_font(), LV_STATE_DEFAULT);
    lv_label_set_text(icon, entry.icon);

    auto* name = lv_label_create(card);
    lv_obj_set_style_text_font(name, lvgl_get_text_font(FONT_SIZE_DEFAULT), LV_STATE_DEFAULT);
    lv_obj_set_width(name, LV_PCT(100));
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(name, entry.name);
}

void createWidgets(lv_obj_t* parent, void* /*userData*/) {
    // Swiping up anywhere closes the switcher
    lv_obj_set_gesture_bubble(parent, false);
    lv_obj_add_event_cb(parent, onBackdropGesture, LV_EVENT_GESTURE, nullptr);

    auto* title = lv_label_create(parent);
    lv_label_set_text(title, "Recent apps");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, lvgl_theme_is_compact() ? 4 : 8);

    auto* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_PCT(60));
    lv_obj_center(row);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(row, lvgl_theme_is_compact() ? 4 : 8, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(row, lvgl_theme_is_compact() ? 4 : 8, LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(row, LV_DIR_HOR);

    RecentsEntry entries[MAX_ENTRIES];
    const size_t entry_count = collectEntries(entries);
    for (size_t i = 0; i < entry_count; i++) {
        createCard(row, entries[i]);
    }

    // A keyboard or encoder can't tap outside of a card or swipe up
    if (lvgl_indev_exists(LV_INDEV_TYPE_KEYPAD) || lvgl_indev_exists(LV_INDEV_TYPE_ENCODER)) {
        auto* close_button = lvgl_icon_button_create(parent);
        lv_obj_align(close_button, LV_ALIGN_BOTTOM_MID, 0, lvgl_theme_is_compact() ? -4 : -8);
        lv_obj_add_event_cb(close_button, onCloseClicked, LV_EVENT_SHORT_CLICKED, nullptr);
        auto* close_icon = lv_label_create(close_button);
        lv_obj_set_style_text_font(close_icon, lvgl_get_shared_icon_default_font(), LV_STATE_DEFAULT);
        lv_label_set_text(close_icon, LVGL_ICON_SHARED_CLOSE);
        lv_group_focus_obj(close_button);
    }
}

// endregion

#endif // CONFIG_TT_MOBILE_HOME

}

void systemRecentsShow() {
#if defined(CONFIG_TT_MOBILE_HOME)
    // The empty state is part of the screen, so it opens even with no recent apps
    window_manager_overlay_show(createWidgets, nullptr, nullptr);
#else
    // Nothing to list (no app has a window)
    if (window_manager_get_windows(nullptr, 0, 0) == 0) {
        return;
    }
    window_manager_overlay_show(createWidgets, nullptr, nullptr);
#endif
}

}

#else

namespace tt::lvgl {

void systemRecentsShow() {}

}

#endif
