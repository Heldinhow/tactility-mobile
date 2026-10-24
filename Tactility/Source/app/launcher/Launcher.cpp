#include <app/event.h>
#include <app/location.h>
#include <app/manifest.h>
#include <app/scheduler.h>
#include <app/start.h>

#include <cstring>
#include <ctime>
#include <new>
#include <string>
#include <vector>

#include <lvgl/fonts.h>
#include <lvgl/icons/shared.h>
#include <lvgl/lvgl.h>
#include <lvgl/theme.h>

#include <lvgl_window_manager/window_manager.h>

#include <tactility/check.h>
#include <tactility/file_ops.h>
#include <tactility/log.h>
#include <tactility/memory.h>

#include <Tactility/app/TileGrid.h>
#include <Tactility/app/launcher/LauncherMode.h>
#include <Tactility/app/setup/Setup.h>
#include <Tactility/lvgl/ClockText.h>
#include <Tactility/lvgl/SystemBars.h>
#include <Tactility/settings/BootSettings.h>
#include <Tactility/settings/Time.h>
#include <Tactility/Tactility.h>

#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

namespace tt::app::launcher {

constexpr auto* TAG = "Launcher";

#if defined(CONFIG_TT_MOBILE_HOME)
#define LAUNCHER_NAME "Home"
#else
#define LAUNCHER_NAME "Launcher"
#endif

namespace {

constexpr auto* MODE_ITEM_ID = "tactility.launcher.mode";

struct Context;

std::vector<TileGridItem> collectItems(void* userData);
void onAppClicked(const TileGridItem& item, void* userData);
void onAppLongPressed(const TileGridItem& item, void* userData);
void onAppKey(const TileGridItem& item, uint32_t key, void* userData);
void switchMode(Context* ctx);

enum class Mode {
    Apps,
    Settings,
};

const LauncherMode& getLauncherMode(Mode mode) {
    return mode == Mode::Settings ? SETTINGS_MODE : APPS_MODE;
}

Mode getOtherMode(Mode mode) {
    return mode == Mode::Settings ? Mode::Apps : Mode::Settings;
}

struct Context {
    Mode mode = Mode::Apps;
    lv_obj_t* modeButtonIcon = nullptr;
    lvgl::SystemBarsListenerId systemBarsListener = 0;
#if defined(CONFIG_TT_MOBILE_HOME)
    lv_obj_t* clockLabel = nullptr;
    lv_obj_t* ampmLabel = nullptr;
    lv_obj_t* dateLabel = nullptr;
    lv_timer_t* clockTimer = nullptr;
    char dateText[32] = {};
#endif
    TileGrid grid { TileGrid::Callbacks {
        .collect = collectItems,
        .onClicked = onAppClicked,
        .onLongPressed = onAppLongPressed,
        .onKey = onAppKey,
        .resolveIcon = nullptr,
        .onPageChanged = nullptr,
        .userData = this,
    } };
};

std::vector<TileGridItem> collectItems(void* userData) {
    auto* ctx = static_cast<Context*>(userData);
#if defined(CONFIG_TT_MOBILE_HOME)
    std::vector<TileGridItem> items = getLauncherMode(ctx->mode).collect();
    const bool apps_mode = ctx->mode == Mode::Apps;
    items.insert(
        items.begin(),
        TileGridItem {
            MODE_ITEM_ID,
            apps_mode ? "Settings" : "Apps",
            apps_mode ? LVGL_ICON_SHARED_SETTINGS : LVGL_ICON_SHARED_APPS,
            false
        }
    );
    return items;
#else
    return getLauncherMode(ctx->mode).collect();
#endif
}

struct StartAppJob {
    std::string appId;
};

// Starting an app can load it from a file, so it runs on the file operations task, which frees the job
void startApp(void* context) {
    auto* job = static_cast<StartAppJob*>(context);
    uint32_t instance_id = 0;
    AppStartContext start_context;
    if (app_start_context_from_id(job->appId.c_str(), &start_context) == ERROR_NONE) {
        app_start_with_context(&start_context, &instance_id);
    }
    delete job;
}

void queueAppStart(const std::string& appId) {
    auto* job = new (::std::nothrow) StartAppJob { appId };
    if (job == nullptr || file_ops_queue(startApp, job, nullptr) != ERROR_NONE) {
        LOG_E(TAG, "Failed to queue starting %s", appId.c_str());
        delete job;
    }
}

void onAppClicked(const TileGridItem& item, void* userData) {
#if defined(CONFIG_TT_MOBILE_HOME)
    if (item.id == MODE_ITEM_ID) {
        switchMode(static_cast<Context*>(userData));
        return;
    }
#else
    (void)userData;
#endif
    queueAppStart(item.id);
}

void onAppLongPressed(const TileGridItem& item, void* userData) {
#if defined(CONFIG_TT_MOBILE_HOME)
    if (item.id == MODE_ITEM_ID) {
        return;
    }
#endif
    auto* ctx = static_cast<Context*>(userData);
    const LauncherMode& mode = getLauncherMode(ctx->mode);
    if (mode.onLongPressed != nullptr) {
        mode.onLongPressed(ctx->grid, item);
    }
}

void onAppKey(const TileGridItem& item, uint32_t key, void* userData) {
#if defined(CONFIG_TT_MOBILE_HOME)
    if (item.id == MODE_ITEM_ID) {
        return;
    }
#endif
    auto* ctx = static_cast<Context*>(userData);
    const LauncherMode& mode = getLauncherMode(ctx->mode);
    if (mode.onKey != nullptr) {
        mode.onKey(ctx->grid, item, key);
    }
}

void applyIconColor(Context* ctx) {
    // The monochrome theme's primary color is black, also on a black background
    if (!lvgl_theme_is_mono()) {
        ctx->grid.setIconColor(getLauncherMode(ctx->mode).iconColor);
    }
}

void switchMode(Context* ctx) {
    ctx->mode = getOtherMode(ctx->mode);
    applyIconColor(ctx);
    ctx->grid.showFirstPage();
    ctx->grid.requestRepopulate(false);
}

#if !defined(CONFIG_TT_MOBILE_HOME)
void onModeButtonPressed(lv_event_t* e) {
    auto* ctx = static_cast<Context*>(lv_event_get_user_data(e));
    switchMode(ctx);
    lv_label_set_text(ctx->modeButtonIcon, getLauncherMode(getOtherMode(ctx->mode)).buttonIcon);
}
#endif

#if defined(CONFIG_TT_MOBILE_HOME)
static void updateHomeClock(Context* ctx) {
    const time_t now = time(nullptr);
    const tm* local = localtime(&now);
    if (local == nullptr) {
        lv_label_set_text(ctx->clockLabel, "--:--");
        lv_obj_set_hidden(ctx->ampmLabel, true);
        return;
    }

    const auto text = lvgl::formatClock(*local, settings::isTimeFormat24Hour());
    lv_label_set_text(ctx->clockLabel, text.time);
    if (text.meridiem != nullptr) {
        lv_label_set_text(ctx->ampmLabel, text.meridiem);
        lv_obj_set_hidden(ctx->ampmLabel, false);
    } else {
        lv_obj_set_hidden(ctx->ampmLabel, true);
    }
    if (strcmp(ctx->dateText, text.date) != 0) {
        strncpy(ctx->dateText, text.date, sizeof(ctx->dateText) - 1);
        ctx->dateText[sizeof(ctx->dateText) - 1] = '\0';
        lv_label_set_text(ctx->dateLabel, ctx->dateText);
    }
}

static void onHomeClockTimer(lv_timer_t* timer) {
    auto* ctx = static_cast<Context*>(lv_timer_get_user_data(timer));
    updateHomeClock(ctx);

    const time_t now = time(nullptr);
    const tm* local = localtime(&now);
    lv_timer_set_period(timer, local != nullptr ? lvgl::millisToNextMinute(*local) : 60000U);
}

static void createHomeHeader(lv_obj_t* parent, Context* ctx) {
    auto* header = lv_obj_create(parent);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(header, 8, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(header, 4, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(header, 2, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(header, false);

    auto* clock_row = lv_obj_create(header);
    lv_obj_remove_style_all(clock_row);
    lv_obj_set_size(clock_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(clock_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(clock_row, 4, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(clock_row, false);

    ctx->clockLabel = lv_label_create(clock_row);
    lv_obj_set_style_text_font(ctx->clockLabel, lvgl_get_text_font(FONT_SIZE_LARGE), LV_STATE_DEFAULT);
    lv_label_set_text(ctx->clockLabel, "--:--");

    ctx->ampmLabel = lv_label_create(clock_row);
    lv_obj_set_style_text_font(ctx->ampmLabel, lvgl_get_text_font(FONT_SIZE_SMALL), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ctx->ampmLabel, 3, LV_STATE_DEFAULT);
    lv_obj_set_hidden(ctx->ampmLabel, true);

    ctx->dateLabel = lv_label_create(header);
    lv_obj_set_style_text_font(ctx->dateLabel, lvgl_get_text_font(FONT_SIZE_SMALL), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ctx->dateLabel, LV_OPA_70, LV_STATE_DEFAULT);

    updateHomeClock(ctx);

    const time_t now = time(nullptr);
    const tm* local = localtime(&now);
    ctx->clockTimer = lv_timer_create(
        onHomeClockTimer,
        local != nullptr ? lvgl::millisToNextMinute(*local) : 60000U,
        ctx
    );
}
#endif

void createWidgets(lv_obj_t* parent, void* userData) {
    auto* ctx = static_cast<Context*>(userData);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 0, LV_STATE_DEFAULT);

#if defined(CONFIG_TT_MOBILE_HOME)
    createHomeHeader(parent, ctx);
    applyIconColor(ctx);
    ctx->grid.setSwipeNavigation(true);
    ctx->grid.createWidgets(parent);
#else
    applyIconColor(ctx);
    ctx->grid.setSwipeNavigation(true);
    ctx->grid.createWidgetsWithBottomBar(parent);
    lv_obj_t* modeButton = ctx->grid.addBarButton(getLauncherMode(getOtherMode(ctx->mode)).buttonIcon, onModeButtonPressed, ctx);
    ctx->modeButtonIcon = lv_obj_get_child(modeButton, 0);

    // In the side layout, the bar's buttons are in the system bars strip
    ctx->grid.setBarSideContainer(lvgl::systemBarsGetActionContainer());
    ctx->systemBarsListener = lvgl::systemBarsAddListener([ctx] {
        ctx->grid.setBarSideContainer(lvgl::systemBarsGetActionContainer());
    });
#endif
}

void destroyWidgets(void* userData) {
    auto* ctx = static_cast<Context*>(userData);
#if defined(CONFIG_TT_MOBILE_HOME)
    if (ctx->clockTimer != nullptr) {
        lv_timer_delete(ctx->clockTimer);
        ctx->clockTimer = nullptr;
    }
    ctx->clockLabel = nullptr;
    ctx->ampmLabel = nullptr;
    ctx->dateLabel = nullptr;
    ctx->dateText[0] = '\0';
#else
    lvgl::systemBarsRemoveListener(ctx->systemBarsListener);
    ctx->systemBarsListener = 0;
#endif
    ctx->grid.destroyWidgets();
}

// Reads settings files, so it runs on the file operations task
void runAutoStart(void*) {
    settings::BootSettings boot_properties;
    AppStartContext context;
    if (
        // Auto-start due to built-in requirement
        strcmp(CONFIG_TT_AUTO_START_APP_ID, "") != 0 &&
        app_start_context_from_id(CONFIG_TT_AUTO_START_APP_ID, &context) == ERROR_NONE
    ) {
        LOG_I(TAG, "Starting %s", CONFIG_TT_AUTO_START_APP_ID);
        uint32_t app_launch_id;
        app_start_with_context(&context, &app_launch_id);
    } else if (
        // Auto-start due to user configuration
        settings::loadBootSettings(boot_properties) &&
        !boot_properties.autoStartAppId.empty() &&
        app_start_context_from_id(boot_properties.autoStartAppId.c_str(), &context) == ERROR_NONE
    ) {
        LOG_I(TAG, "Starting %s", boot_properties.autoStartAppId.c_str());
        uint32_t app_launch_id;
        app_start_with_context(&context, &app_launch_id);
    } else {
        // No auto-start, consider running system setup
        if (!setup::isCompleted()) {
            setup::start();
        }
    }
}

int32_t appMain(int argc, char* argv[]) {
    uint32_t appInstanceId = app_scheduler_current_app_id();

    TaskEventGroup event_group {};
    task_event_group_construct(&event_group);

    AppEventSubscription sub {};
    check(app_event_subscribe(&sub, &event_group) == ERROR_NONE);

    FileOpsSubscription file_ops_sub {};
    check(file_ops_subscribe(&file_ops_sub, &event_group) == ERROR_NONE);

    Context ctx;
    WindowId window = window_manager_create_ext(appInstanceId, createWidgets, destroyWidgets, &ctx);

    // The file operations run in order: the favourites are available when auto-start runs
    check(file_ops_queue(loadFavourites, nullptr, &file_ops_sub) == ERROR_NONE);
    check(file_ops_queue(runAutoStart, nullptr, nullptr) == ERROR_NONE);

    // The launcher is meant to stay resident (it's the home screen) - it only gives up its
    // thread when app-module's scheduler asks it to (e.g. another new-model app is started).
    while (true) {
        task_event_group_wait_any(&event_group, nullptr, portMAX_DELAY);

        if (file_ops_poll(&file_ops_sub) == ERROR_NONE) {
            // The favourites were loaded
            lvgl_lock();
            ctx.grid.requestRepopulate(false);
            lvgl_unlock();
        }

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
    file_ops_unsubscribe(&file_ops_sub);
    check(app_event_unsubscribe(&sub) == ERROR_NONE);
    task_event_group_destruct(&event_group);
    return 0;
}

} // namespace

extern const ::AppManifest manifest = {
    .id = "tactility.launcher",
    .name = LAUNCHER_NAME,
    .category = APP_CATEGORY_SYSTEM,
    .location = { .type = APP_LOCATION_MEMORY, .location = reinterpret_cast<void*>(appMain) },
    .flags = APP_MANIFEST_FLAG_HIDDEN,
    // All file operations run on the file operations task, so the stack can be in external RAM
    .stack = { .depth = 3072, .desired_memory_capability = MEMORY_CAPABILITY_EXTERNAL },
};

} // namespace
