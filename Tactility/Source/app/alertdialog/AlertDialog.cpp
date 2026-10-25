#include "Tactility/app/alertdialog/AlertDialog.h"

#include <app/event.h>
#include <app/manager.h>
#include <app/manifest.h>
#include <app/scheduler.h>
#include <app/start.h>

#include <lvgl_window_manager/window_manager.h>

#include <tactility/check.h>
#include <tactility/log.h>
#include <tactility/memory.h>

#include <Tactility/lvgl/MobileTheme.h>
#include <Tactility/lvgl/MobileUi.h>

#include <lvgl.h>
#include <lvgl/devices/indev.h>
#include <lvgl/theme.h>
#include <lvgl/widgets/toolbar.h>

namespace tt::app::alertdialog {

constexpr auto* TAG = "AlertDialog";

extern const ::AppManifest manifest;

namespace {

struct Context {
    uint32_t appInstanceId;
    // Set once in appMain() from its own argc/argv parameters, read by createWidgets() (which
    // may run on a different task - the LVGL task, or another app's task via
    // window_manager_remove()'s cross-thread rebuild-on-remove path). Safe to hold onto without a
    // lock: the deep copy stays valid for exactly as long as appMain() is running, which is
    // longer than createWidgets() ever needs it.
    int argc = 0;
    char** argv = nullptr;
    // The eventual appMain() return value (= this dialog's APP_EVENT_RESULT result code) -
    // written here by onButtonPressed() (LVGL thread) before it emits APP_EVENT_CLOSE, read by
    // appMain() (this dialog's own thread) after waking from that event. No atomic/lock needed:
    // the emit/await pair between the two already establishes happens-before ordering, same as
    // every other cross-thread Context field write in this codebase's converted apps.
    int32_t result = 1; // Cancelled - safety-net default if closed without pressing a button
#if defined(CONFIG_TT_MOBILE_HOME)
    bool result_set = false;
    bool overlay_shown = false;
#endif
};

struct ButtonContext {
    Context* ctx;
    int32_t index;
};

void onButtonDeleted(lv_event_t* e) {
    delete static_cast<ButtonContext*>(lv_event_get_user_data(e));
}

void onButtonPressed(lv_event_t* e) {
    auto* btnCtx = static_cast<ButtonContext*>(lv_event_get_user_data(e));
    LOG_I(TAG, "Selected item at index %d", (int)btnCtx->index);
    btnCtx->ctx->result = btnCtx->index;
    app_event_emit_close(btnCtx->ctx->appInstanceId);
}

lv_obj_t* createButton(Context* ctx, lv_obj_t* parent, const std::string& text, int32_t index) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_t* button_label = lv_label_create(button);
    lv_obj_align(button_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(button_label, text.c_str());
    auto* btnCtx = new ButtonContext { ctx, index };
    lv_obj_add_event_cb(button, onButtonPressed, LV_EVENT_SHORT_CLICKED, btnCtx);
    lv_obj_add_event_cb(button, onButtonDeleted, LV_EVENT_DELETE, btnCtx);
    return button;
}

void createLegacyWidgets(lv_obj_t* parent, void* userData) {
    auto* ctx = static_cast<Context*>(userData);
    // argv layout: [0]=title, [1]=message, [2..argc)=button labels.
    int argc = ctx->argc;
    char** argv = ctx->argv;

    lv_obj_t* toolbar = lvgl_toolbar_create(parent, argv[0]);
    lv_obj_align(toolbar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* message_label = lv_label_create(parent);
    lv_obj_align(message_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_width(message_label, LV_PCT(80));
    lv_obj_set_style_text_align(message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(message_label, argv[1]);
    lv_label_set_long_mode(message_label, LV_LABEL_LONG_WRAP);

    lv_obj_t* button_wrapper = lv_obj_create(parent);
    lv_obj_set_flex_flow(button_wrapper, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(button_wrapper, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(button_wrapper, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(button_wrapper, 0, 0);
    lv_obj_align(button_wrapper, LV_ALIGN_BOTTOM_MID, 0, -4);
    // Ensures showing selection state for keyboard/encoder devices
    lv_obj_set_style_pad_all(button_wrapper, 0, LV_ALIGN_DEFAULT);
    lv_obj_set_style_pad_column(button_wrapper, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(button_wrapper, 4, LV_STATE_DEFAULT);

    lv_obj_t* first_button = nullptr;
    for (int32_t index = 0; index < argc - 2; index++) {
        lv_obj_t* button = createButton(ctx, button_wrapper, argv[2 + index], index);
        if (first_button == nullptr) {
            first_button = button;
        }
    }

    if (first_button != nullptr && !lvgl_indev_exists(LV_INDEV_TYPE_POINTER)) {
        lv_group_focus_obj(first_button);
        lv_obj_add_state(first_button, LV_STATE_FOCUS_KEY);
    }
}


#if defined(CONFIG_TT_MOBILE_HOME)

namespace mobile = tt::lvgl::mobile;

/** The dialog runs as an overlay, so the caller's window stays live behind the scrim. */
void onDialogButtonPressed(lv_event_t* event) {
    auto* button_ctx = static_cast<ButtonContext*>(lv_event_get_user_data(event));
    button_ctx->ctx->result = button_ctx->index;
    button_ctx->ctx->result_set = true;
    window_manager_overlay_hide();
    app_event_emit_close(button_ctx->ctx->appInstanceId);
}

void destroyMobileWidgets(void* user_data) {
    auto* ctx = static_cast<Context*>(user_data);
    ctx->overlay_shown = false;
    if (!ctx->result_set) {
        // Dismissed by a backdrop click or Escape: cancel
        ctx->result = 1;
        ctx->result_set = true;
        app_event_emit_close(ctx->appInstanceId);
    }
}

void createMobileWidgets(lv_obj_t* parent, void* user_data) {
    auto* ctx = static_cast<Context*>(user_data);
    ctx->overlay_shown = true;

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, 216);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_align(card, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(card, mobile::color(mobile::COLOR_SURFACE), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(card, mobile::RADIUS_CARD, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(card, mobile::color(mobile::COLOR_LINE), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 16, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollable(card, false);

    lv_obj_t* title = mobile::createLabel(card, ctx->argv[0], FONT_SIZE_LARGE, mobile::COLOR_TEXT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);

    lv_obj_t* message = mobile::createLabel(card, ctx->argv[1], FONT_SIZE_DEFAULT, mobile::COLOR_TEXT_2);
    lv_obj_set_width(message, LV_PCT(100));
    lv_label_set_long_mode(message, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_margin_top(message, 8, LV_STATE_DEFAULT);

    lv_obj_t* button_row = lv_obj_create(card);
    lv_obj_remove_style_all(button_row);
    lv_obj_set_size(button_row, LV_PCT(100), 40);
    lv_obj_set_style_margin_top(button_row, 16, LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(button_row, 8, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(button_row, false);

    // Index 0 is the confirming action and sits on the right, like the mockup
    lv_obj_t* first_button = nullptr;
    for (int32_t index = ctx->argc - 3; index >= 0; index--) {
        lv_obj_t* button = lv_button_create(button_row);
        mobile::styleButton(button, index == 0);
        lv_obj_set_height(button, 40);
        lv_obj_set_flex_grow(button, 1);
        lv_obj_t* label = mobile::createLabel(
            button,
            ctx->argv[2 + index],
            FONT_SIZE_DEFAULT,
            index == 0 ? mobile::COLOR_ON_ACCENT : mobile::COLOR_TEXT
        );
        lv_obj_center(label);
        auto* button_ctx = new ButtonContext { ctx, index };
        lv_obj_add_event_cb(button, onDialogButtonPressed, LV_EVENT_SHORT_CLICKED, button_ctx);
        lv_obj_add_event_cb(button, onButtonDeleted, LV_EVENT_DELETE, button_ctx);
        if (first_button == nullptr) {
            first_button = button;
        }
    }

    if (first_button != nullptr && !lvgl_indev_exists(LV_INDEV_TYPE_POINTER)) {
        lv_group_focus_obj(first_button);
        lv_obj_add_state(first_button, LV_STATE_FOCUS_KEY);
    }
}

#endif

int32_t appMain(int argc, char* argv[]) {
    uint32_t appInstanceId = app_scheduler_current_app_id();
    Context ctx { appInstanceId };
    ctx.argc = argc;
    ctx.argv = argv;

    TaskEventGroup event_group {};
    task_event_group_construct(&event_group);

    AppEventSubscription sub {};
    check(app_event_subscribe(&sub, &event_group) == ERROR_NONE);

#if defined(CONFIG_TT_MOBILE_HOME)
    if (window_manager_overlay_show(createMobileWidgets, destroyMobileWidgets, &ctx) != ERROR_NONE) {
        check(app_event_unsubscribe(&sub) == ERROR_NONE);
        task_event_group_destruct(&event_group);
        return 1;
    }

    while (true) {
        task_event_group_wait_any(&event_group, nullptr, portMAX_DELAY);

        bool shouldClose = false;
        AppEvent event {};
        while (app_event_poll(&sub, &event) == ERROR_NONE) {
            if (event.type == APP_EVENT_CLOSE) {
                shouldClose = true;
                break;
            }
        }
        if (shouldClose) break;
    }

    if (ctx.overlay_shown) {
        window_manager_overlay_hide();
    }
    check(app_event_unsubscribe(&sub) == ERROR_NONE);
    task_event_group_destruct(&event_group);

    return ctx.result;
#else
    WindowId window = window_manager_create(appInstanceId, createLegacyWidgets, &ctx);

    while (true) {
        task_event_group_wait_any(&event_group, nullptr, portMAX_DELAY);

        bool shouldClose = false;
        AppEvent event {};
        while (app_event_poll(&sub, &event) == ERROR_NONE) {
            if (event.type == APP_EVENT_CLOSE) {
                shouldClose = true;
                break;
            }
        }
        if (shouldClose) break;
    }

    window_manager_remove(window);
    check(app_event_unsubscribe(&sub) == ERROR_NONE);
    task_event_group_destruct(&event_group);

    return ctx.result;
#endif
}

} // namespace

namespace {

// Builds argv = [title, message, buttonLabels...] for app_start_with_context().
std::vector<const char*> buildArgv(const std::string& title, const std::string& message, const std::vector<std::string>& buttonLabels) {
    std::vector<const char*> argv { title.c_str(), message.c_str() };
    for (const auto& label: buttonLabels) {
        argv.push_back(label.c_str());
    }
    return argv;
}

} // namespace

uint32_t start(uint32_t callerAppInstanceId, const std::string& title, const std::string& message, const std::vector<std::string>& buttonLabels) {
    auto argv = buildArgv(title, message, buttonLabels);
    uint32_t instanceId = 0;
    AppStartContext context = app_start_context_for_manifest(&manifest);
    app_start_context_set_arguments_ext(&context, static_cast<int>(argv.size()), argv.data());
    app_start_context_set_parent(&context, callerAppInstanceId);
    app_start_with_context(&context, &instanceId);
    return instanceId;
}

uint32_t start(uint32_t callerAppInstanceId, const std::string& title, const std::string& message) {
    return start(callerAppInstanceId, title, message, std::vector<std::string> { "OK" });
}

extern const ::AppManifest manifest = {
    .id = "tactility.alertdialog",
    .name = "Alert Dialog",
    .category = APP_CATEGORY_SYSTEM,
    .location = { .type = APP_LOCATION_MEMORY, .location = reinterpret_cast<void*>(appMain) },
    .flags = APP_MANIFEST_FLAG_HIDDEN,
    // window_manager_remove() rebuilds the resurfaced window's widgets on this task
    .stack = { .depth =  6144, .desired_memory_capability = MEMORY_CAPABILITY_EXTERNAL },
};

}
