#include <app/event.h>
#include <app/location.h>
#include <app/manager.h>
#include <app/manifest.h>
#include <app/scheduler.h>
#include <app/start.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <lvgl/fonts.h>
#include <lvgl/grid_navigation.h>
#include <lvgl/icons/shared.h>
#include <lvgl/lvgl.h>
#include <lvgl/theme.h>
#include <lvgl/widgets/card.h>
#include <lvgl/widgets/icon_button.h>

#include <lvgl_window_manager/window_manager.h>

#include <tactility/check.h>
#include <tactility/file_ops.h>
#include <tactility/log.h>
#include <tactility/memory.h>

#include <Tactility/app/TileGrid.h>
#include <Tactility/app/launcher/HomeModel.h>
#include <Tactility/app/launcher/LauncherMode.h>
#include <Tactility/app/setup/Setup.h>
#include <Tactility/lvgl/ClockText.h>
#include <Tactility/lvgl/Fonts.h>
#include <Tactility/lvgl/MobileIcons.h>
#include <Tactility/lvgl/MobilePresentation.h>
#include <Tactility/lvgl/MobileTheme.h>
#include <Tactility/lvgl/MobileUi.h>
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

#if defined(CONFIG_TT_MOBILE_HOME)

// region The v2 Home

namespace mobile = tt::lvgl::mobile;

constexpr uint32_t HOME_COLUMNS = 4;
constexpr uint32_t HOME_ROWS = 2;
constexpr uint32_t HOME_PAGE_SIZE = HOME_COLUMNS * HOME_ROWS;
constexpr int32_t HOME_GRID_TOP = 62;      // mockup y=86, minus the 24 px status bar
constexpr int32_t HOME_CELL_HEIGHT = 64;
constexpr int32_t HOME_ROW_GAP = 8;
constexpr int32_t HOME_DOTS_TOP = 206;
constexpr int32_t HOME_DOCK_LEFT = 8;
constexpr int32_t HOME_DOCK_TOP = 220;
constexpr int32_t HOME_DOCK_WIDTH = 224;
constexpr int32_t HOME_DOCK_HEIGHT = 56;

struct Context;
std::vector<TileGridItem> collectFallbackItems(void* userData);
void onFallbackClicked(const TileGridItem& item, void* userData);
void onFallbackLongPressed(const TileGridItem& item, void* userData);
void onFallbackKey(const TileGridItem& item, uint32_t key, void* userData);

struct Context {
    /** True while the absolute 240x320 Home is built, false for the legacy fallback grid. */
    bool v2Home = false;
    uint32_t page = 0;

    lv_obj_t* clockLabel = nullptr;
    lv_obj_t* ampmLabel = nullptr;
    lv_obj_t* weekdayLabel = nullptr;
    lv_obj_t* dateLabel = nullptr;
    lv_obj_t* gridPage = nullptr;
    lv_obj_t* dots = nullptr;
    lv_obj_t* dock = nullptr;
    lv_obj_t* dockDots[4] = {};
    const char* dockIds[4] = {};
    size_t dockCount = 0;

    lv_timer_t* clockTimer = nullptr;
    lvgl::ClockFont* clockFont = nullptr;
    char weekdayText[32] = {};
    char dateText[32] = {};

    std::vector<HomeItem> items;
    std::vector<std::string> cellIds;

    /** The legacy layout, used when the display isn't 240x320 portrait. */
    TileGrid grid { TileGrid::Callbacks {
        .collect = collectFallbackItems,
        .onClicked = onFallbackClicked,
        .onLongPressed = onFallbackLongPressed,
        .onKey = onFallbackKey,
        .resolveIcon = nullptr,
        .onPageChanged = nullptr,
        .userData = this,
    } };

    ~Context() {
        if (clockFont != nullptr) {
            lvgl::destroyClockFont(clockFont);
        }
    }
};

std::vector<TileGridItem> collectFallbackItems(void*) {
    const auto apps = collectHomeApps();
    const auto items = resolveHomeItems(apps);
    std::vector<TileGridItem> tiles;
    tiles.reserve(items.size());
    for (const auto& item : items) {
        tiles.push_back(TileGridItem { item.id, item.name, item.icon, false });
    }
    return tiles;
}

void onFallbackClicked(const TileGridItem& item, void*) {
    queueAppStart(item.id);
}

void onFallbackLongPressed(const TileGridItem& item, void* userData) {
    if (toggleFavourite(item.id.c_str())) {
        static_cast<Context*>(userData)->grid.requestRepopulate(false);
    }
}

void onFallbackKey(const TileGridItem& item, uint32_t key, void* userData) {
    if ((key == 'f' || key == 'F') && toggleFavourite(item.id.c_str())) {
        static_cast<Context*>(userData)->grid.requestRepopulate(true);
    }
}

void updateHomeClock(Context* ctx) {
    const time_t now = time(nullptr);
    const tm* local = localtime(&now);
    if (local == nullptr) {
        lv_label_set_text(ctx->clockLabel, "--:--");
        lv_obj_add_flag(ctx->ampmLabel, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ctx->weekdayLabel, "");
        lv_label_set_text(ctx->dateLabel, "");
        ctx->weekdayText[0] = '\0';
        ctx->dateText[0] = '\0';
        return;
    }

    const auto text = lvgl::formatClock(*local, settings::isTimeFormat24Hour());
    lv_label_set_text(ctx->clockLabel, text.time);
    if (text.meridiem != nullptr) {
        lv_label_set_text(ctx->ampmLabel, text.meridiem);
        lv_obj_remove_flag(ctx->ampmLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ctx->ampmLabel, LV_OBJ_FLAG_HIDDEN);
    }

    const auto long_date = lvgl::formatLongDate(*local);
    if (strcmp(ctx->weekdayText, long_date.weekday) != 0) {
        strncpy(ctx->weekdayText, long_date.weekday, sizeof(ctx->weekdayText) - 1);
        ctx->weekdayText[sizeof(ctx->weekdayText) - 1] = '\0';
        lv_label_set_text(ctx->weekdayLabel, ctx->weekdayText);
    }
    if (strcmp(ctx->dateText, long_date.date) != 0) {
        strncpy(ctx->dateText, long_date.date, sizeof(ctx->dateText) - 1);
        ctx->dateText[sizeof(ctx->dateText) - 1] = '\0';
        lv_label_set_text(ctx->dateLabel, ctx->dateText);
    }
}

void onHomeClockTimer(lv_timer_t* timer) {
    auto* ctx = static_cast<Context*>(lv_timer_get_user_data(timer));
    updateHomeClock(ctx);

    const time_t now = time(nullptr);
    const tm* local = localtime(&now);
    lv_timer_set_period(timer, local != nullptr ? lvgl::millisToNextMinute(*local) : 60000U);
}

void createHomeClock(Context* ctx, lv_obj_t* parent) {
    lv_obj_t* clock_row = lv_obj_create(parent);
    lv_obj_remove_style_all(clock_row);
    lv_obj_set_size(clock_row, LV_SIZE_CONTENT, 48);
    lv_obj_set_pos(clock_row, 12, 4);
    lv_obj_set_flex_flow(clock_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(clock_row, 4, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(clock_row, false);

    ctx->clockFont = lvgl::loadClockFont();
    const lv_font_t* clock_font = ctx->clockFont != nullptr
        ? ctx->clockFont->lvglFont
        : lvgl_get_text_font(FONT_SIZE_LARGE);

    ctx->clockLabel = mobile::createLabel(clock_row, "--:--", FONT_SIZE_LARGE, mobile::COLOR_TEXT);
    lv_obj_set_style_text_font(ctx->clockLabel, clock_font, LV_STATE_DEFAULT);

    ctx->ampmLabel = mobile::createLabel(clock_row, "", FONT_SIZE_SECONDARY, mobile::COLOR_TEXT);
    lv_obj_set_style_pad_bottom(ctx->ampmLabel, 8, LV_STATE_DEFAULT);
    lv_obj_add_flag(ctx->ampmLabel, LV_OBJ_FLAG_HIDDEN);

    ctx->weekdayLabel = mobile::createLabel(parent, "", FONT_SIZE_DEFAULT, mobile::COLOR_TEXT);
    lv_obj_align(ctx->weekdayLabel, LV_ALIGN_TOP_RIGHT, -12, 14);
    ctx->dateLabel = mobile::createLabel(parent, "", FONT_SIZE_SECONDARY, mobile::COLOR_TEXT_2);
    lv_obj_align(ctx->dateLabel, LV_ALIGN_TOP_RIGHT, -12, 34);

    updateHomeClock(ctx);
    const time_t now = time(nullptr);
    const tm* local = localtime(&now);
    ctx->clockTimer = lv_timer_create(
        onHomeClockTimer,
        local != nullptr ? lvgl::millisToNextMinute(*local) : 60000U,
        ctx
    );
}

bool isAppOpen(const char* app_id) {
    const size_t count = window_manager_get_windows(nullptr, 0, 0);
    for (size_t i = 0; i < count; i++) {
        WindowInfo info {};
        window_manager_get_windows(&info, i, 1);
        char instance_app_id[APP_MANIFEST_ID_LENGTH + 1] = {};
        if (app_manager_get_instance_app_id(info.app_instance_id, instance_app_id, sizeof(instance_app_id)) != ERROR_NONE) {
            continue;
        }
        if (strcmp(instance_app_id, app_id) == 0) {
            return true;
        }
    }
    return false;
}

void onHomeCellClicked(lv_event_t* event) {
    const auto* app_id = static_cast<const char*>(lv_obj_get_user_data(lv_event_get_target_obj(event)));
    if (app_id != nullptr) {
        queueAppStart(app_id);
    }
}

void onDeferredHomeRefresh(void* context);

void onHomeCellLongPressed(lv_event_t* event) {
    auto* ctx = static_cast<Context*>(lv_event_get_user_data(event));
    const auto* app_id = static_cast<const char*>(lv_obj_get_user_data(lv_event_get_target_obj(event)));
    if (app_id != nullptr && toggleFavourite(app_id)) {
        // The pressed cell is deleted by the refresh, so it runs after the event
        lv_async_call(onDeferredHomeRefresh, ctx);
    }
}

void setHomeCellPressed(lv_event_t* event) {
    auto* surface = lv_obj_get_child(lv_event_get_target_obj(event), 0);
    if (surface != nullptr) {
        lv_obj_set_style_bg_color(surface, mobile::color(mobile::COLOR_SURFACE_2), LV_STATE_DEFAULT);
    }
}

void setHomeCellReleased(lv_event_t* event) {
    auto* surface = lv_obj_get_child(lv_event_get_target_obj(event), 0);
    if (surface != nullptr) {
        lv_obj_set_style_bg_color(surface, mobile::color(mobile::COLOR_SURFACE), LV_STATE_DEFAULT);
    }
}

void updateHomeDots(Context* ctx) {
    if (ctx->dots == nullptr) {
        return;
    }
    const uint32_t page_count = ctx->items.empty()
        ? 1
        : static_cast<uint32_t>((ctx->items.size() + HOME_PAGE_SIZE - 1) / HOME_PAGE_SIZE);
    if (page_count <= 1) {
        lv_obj_add_flag(ctx->dots, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(ctx->dots, LV_OBJ_FLAG_HIDDEN);
    if (static_cast<uint32_t>(lv_obj_get_child_count(ctx->dots)) != page_count) {
        lv_obj_clean(ctx->dots);
        for (uint32_t i = 0; i < page_count; i++) {
            lv_obj_t* dot = lv_obj_create(ctx->dots);
            lv_obj_remove_style_all(dot);
            lv_obj_set_size(dot, 4, 4);
            lv_obj_set_style_radius(dot, mobile::RADIUS_BAR, LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_STATE_DEFAULT);
            lv_obj_set_clickable(dot, false);
        }
    }
    for (uint32_t i = 0; i < page_count; i++) {
        lv_obj_set_style_bg_color(
            lv_obj_get_child(ctx->dots, static_cast<int32_t>(i)),
            mobile::color(i == ctx->page ? mobile::COLOR_ACCENT : mobile::COLOR_TEXT_3),
            LV_STATE_DEFAULT
        );
    }
}

/** Rebuilds the eight cells of the current page from ctx->items. */
void rebuildHomePage(Context* ctx) {
    if (ctx->gridPage == nullptr) {
        return;
    }
    lv_obj_clean(ctx->gridPage);
    ctx->cellIds.clear();
    ctx->cellIds.reserve(HOME_PAGE_SIZE);

    const size_t page_count = ctx->items.empty()
        ? 1
        : (ctx->items.size() + HOME_PAGE_SIZE - 1) / HOME_PAGE_SIZE;
    if (ctx->page >= page_count) {
        ctx->page = static_cast<uint32_t>(page_count - 1);
    }

    const size_t first = ctx->page * HOME_PAGE_SIZE;
    const size_t last = ::std::min<size_t>(ctx->items.size(), static_cast<size_t>(first + HOME_PAGE_SIZE));
    for (size_t i = first; i < last; i++) {
        const auto& item = ctx->items[i];
        const int32_t index = static_cast<int32_t>(i - first);
        const int32_t column = index % static_cast<int32_t>(HOME_COLUMNS);
        const int32_t row = index / static_cast<int32_t>(HOME_COLUMNS);

        lv_obj_t* cell = lv_button_create(ctx->gridPage);
        lv_obj_set_pos(cell, column * mobile::TILE_CELL_WIDTH, row * (HOME_CELL_HEIGHT + HOME_ROW_GAP));
        lv_obj_set_size(cell, mobile::TILE_CELL_WIDTH, HOME_CELL_HEIGHT);
        lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(cell, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(cell, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(cell, 0, LV_STATE_DEFAULT);
        lv_obj_set_scrollable(cell, false);

        lv_obj_t* surface = lv_obj_create(cell);
        lv_obj_set_size(surface, mobile::TILE_SIZE, mobile::TILE_SIZE);
        lv_obj_set_pos(surface, (mobile::TILE_CELL_WIDTH - mobile::TILE_SIZE) / 2, 0);
        lv_obj_set_style_bg_color(surface, mobile::color(mobile::COLOR_SURFACE), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(surface, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(surface, mobile::RADIUS_CARD, LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(surface, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(surface, 0, LV_STATE_DEFAULT);
        lv_obj_set_clickable(surface, false);
        lv_obj_set_scrollable(surface, false);

        lv_obj_t* icon = mobile::createIcon(surface, mobileIconForApp(item.id.c_str()), mobile::COLOR_TEXT);
        lv_obj_center(icon);

        lv_obj_t* label = mobile::createLabel(cell, item.name.c_str(), FONT_SIZE_SMALL, mobile::COLOR_TEXT);
        lv_obj_set_size(label, 52, 16);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, mobile::TILE_SIZE + 4);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

        ctx->cellIds.push_back(item.id);
        lv_obj_set_user_data(cell, ctx->cellIds.back().data());
        lv_obj_add_event_cb(cell, onHomeCellClicked, LV_EVENT_SHORT_CLICKED, nullptr);
        lv_obj_add_event_cb(cell, onHomeCellLongPressed, LV_EVENT_LONG_PRESSED, ctx);
        lv_obj_add_event_cb(cell, setHomeCellPressed, LV_EVENT_PRESSED, nullptr);
        lv_obj_add_event_cb(cell, setHomeCellReleased, LV_EVENT_RELEASED, nullptr);
        lv_obj_add_event_cb(cell, setHomeCellReleased, LV_EVENT_PRESS_LOST, nullptr);
    }

    updateHomeDots(ctx);
}

void refreshHomeItems(Context* ctx) {
    const auto apps = collectHomeApps();
    ctx->items = resolveHomeItems(apps);
    rebuildHomePage(ctx);
}

void onDeferredHomeRefresh(void* context) {
    refreshHomeItems(static_cast<Context*>(context));
}

void onHomeGesture(lv_event_t* event) {
    auto* ctx = static_cast<Context*>(lv_event_get_user_data(event));
    lv_indev_t* indev = lv_indev_active();
    if (indev == nullptr) {
        return;
    }
    const uint32_t page_count = ctx->items.empty()
        ? 1
        : static_cast<uint32_t>((ctx->items.size() + HOME_PAGE_SIZE - 1) / HOME_PAGE_SIZE);
    if (page_count <= 1) {
        return;
    }
    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction != LV_DIR_LEFT && direction != LV_DIR_RIGHT) {
        return;
    }
    // Rebuilding the page deletes the cell the gesture started on
    lv_indev_wait_release(indev);
    if (direction == LV_DIR_LEFT) {
        ctx->page = (ctx->page + 1) % page_count;
    } else {
        ctx->page = (ctx->page + page_count - 1) % page_count;
    }
    lv_async_call(onDeferredHomeRefresh, ctx);
}

void updateHomeDockDots(void* context) {
    auto* ctx = static_cast<Context*>(context);
    if (!ctx->v2Home) {
        return;
    }
    for (size_t i = 0; i < ctx->dockCount; i++) {
        lv_obj_t* dot = ctx->dockDots[i];
        if (dot == nullptr) {
            continue;
        }
        if (isAppOpen(ctx->dockIds[i])) {
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void onDockClicked(lv_event_t* event) {
    const auto* app_id = static_cast<const char*>(lv_event_get_user_data(event));
    if (app_id != nullptr) {
        queueAppStart(app_id);
    }
}

void createHomeDock(Context* ctx, lv_obj_t* parent, const std::vector<HomeApp>& apps) {
    ctx->dock = mobile::createCard(parent, HOME_DOCK_WIDTH, HOME_DOCK_HEIGHT);
    lv_obj_set_pos(ctx->dock, HOME_DOCK_LEFT, HOME_DOCK_TOP);

    const auto items = resolveDockItems(apps);
    ctx->dockCount = ::std::min<size_t>(items.size(), 4);
    for (size_t i = 0; i < ctx->dockCount; i++) {
        ctx->dockIds[i] = items[i].id;
        lv_obj_t* cell = lv_button_create(ctx->dock);
        lv_obj_set_pos(cell, -8 + static_cast<int32_t>(i) * mobile::TILE_CELL_WIDTH, 0);
        lv_obj_set_size(cell, mobile::TILE_CELL_WIDTH, HOME_DOCK_HEIGHT);
        lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(cell, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(cell, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(cell, 0, LV_STATE_DEFAULT);
        lv_obj_set_scrollable(cell, false);

        lv_obj_t* icon = mobile::createIcon(cell, mobileIconForApp(items[i].id), mobile::COLOR_TEXT);
        lv_obj_center(icon);

        lv_obj_t* dot = lv_obj_create(cell);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 4, 4);
        lv_obj_set_style_radius(dot, mobile::RADIUS_BAR, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(dot, mobile::color(mobile::COLOR_ACCENT), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -6);
        lv_obj_set_clickable(dot, false);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        ctx->dockDots[i] = dot;

        lv_obj_add_event_cb(cell, onDockClicked, LV_EVENT_SHORT_CLICKED, const_cast<char*>(items[i].id));
    }
    lvgl_grid_navigation_add(ctx->dock);

    // Reading the window stack takes the app manager's lock, which an app that is starting holds:
    // deferred so the Home build can't wait on it.
    lv_async_call(updateHomeDockDots, ctx);
}

void createV2Home(lv_obj_t* parent, Context* ctx) {
    lv_obj_set_scrollable(parent, false);
    lv_obj_set_style_bg_color(parent, mobile::color(mobile::COLOR_BG), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(parent, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(parent, 0, LV_STATE_DEFAULT);

    createHomeClock(ctx, parent);

    ctx->gridPage = lv_obj_create(parent);
    lv_obj_set_pos(ctx->gridPage, 0, HOME_GRID_TOP);
    lv_obj_set_size(ctx->gridPage, 240, 136);
    lv_obj_set_style_bg_opa(ctx->gridPage, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ctx->gridPage, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ctx->gridPage, 0, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(ctx->gridPage, false);
    lv_obj_set_gesture_bubble(ctx->gridPage, false);
    lv_obj_add_event_cb(ctx->gridPage, onHomeGesture, LV_EVENT_GESTURE, ctx);
    lvgl_grid_navigation_add(ctx->gridPage);

    ctx->dots = lv_obj_create(parent);
    lv_obj_remove_style_all(ctx->dots);
    lv_obj_set_pos(ctx->dots, 0, HOME_DOTS_TOP);
    lv_obj_set_size(ctx->dots, 240, 8);
    lv_obj_set_flex_flow(ctx->dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctx->dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctx->dots, 6, LV_STATE_DEFAULT);
    lv_obj_set_scrollable(ctx->dots, false);

    const auto apps = collectHomeApps();
    createHomeDock(ctx, parent, apps);
    ctx->items = resolveHomeItems(apps);
    rebuildHomePage(ctx);
}

void createWidgets(lv_obj_t* parent, void* userData) {
    auto* ctx = static_cast<Context*>(userData);

    lv_display_t* display = lv_obj_get_display(parent);
    const bool v2 = mobile::isV2HomeLayout(
        true,
        lv_display_get_horizontal_resolution(display),
        lv_display_get_vertical_resolution(display)
    );
    ctx->v2Home = v2;

    if (v2) {
        createV2Home(parent, ctx);
        return;
    }

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 0, LV_STATE_DEFAULT);
    ctx->grid.setIconColor(TileGrid::IconColor::Primary);
    ctx->grid.setSwipeNavigation(true);
    ctx->grid.createWidgets(parent);
}

void destroyWidgets(void* user_data) {
    auto* ctx = static_cast<Context*>(user_data);
    // A queued refresh or dot update would touch the widgets that are about to be deleted
    lv_async_call_cancel(updateHomeDockDots, ctx);
    lv_async_call_cancel(onDeferredHomeRefresh, ctx);
    if (ctx->clockTimer != nullptr) {
        lv_timer_delete(ctx->clockTimer);
        ctx->clockTimer = nullptr;
    }
    ctx->clockLabel = nullptr;
    ctx->ampmLabel = nullptr;
    ctx->weekdayLabel = nullptr;
    ctx->dateLabel = nullptr;
    ctx->gridPage = nullptr;
    ctx->dots = nullptr;
    ctx->dock = nullptr;
    ctx->dockCount = 0;
    for (auto& dot : ctx->dockDots) {
        dot = nullptr;
    }
    if (ctx->clockFont != nullptr) {
        lvgl::destroyClockFont(ctx->clockFont);
        ctx->clockFont = nullptr;
    }
    ctx->weekdayText[0] = '\0';
    ctx->dateText[0] = '\0';
    std::vector<HomeItem>().swap(ctx->items);
    std::vector<std::string>().swap(ctx->cellIds);
    if (!ctx->v2Home) {
        ctx->grid.destroyWidgets();
    }
    ctx->v2Home = false;
}

// endregion

#else // !CONFIG_TT_MOBILE_HOME

// region The legacy launcher, unchanged

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

struct Context;
std::vector<TileGridItem> collectLegacyItems(void* userData);
void onLegacyClicked(const TileGridItem& item, void* userData);
void onLegacyLongPressed(const TileGridItem& item, void* userData);
void onLegacyKey(const TileGridItem& item, uint32_t key, void* userData);

struct Context {
    Mode mode = Mode::Apps;
    lv_obj_t* modeButtonIcon = nullptr;
    lvgl::SystemBarsListenerId systemBarsListener = 0;
    TileGrid grid { TileGrid::Callbacks {
        .collect = collectLegacyItems,
        .onClicked = onLegacyClicked,
        .onLongPressed = onLegacyLongPressed,
        .onKey = onLegacyKey,
        .resolveIcon = nullptr,
        .onPageChanged = nullptr,
        .userData = this,
    } };
};

std::vector<TileGridItem> collectLegacyItems(void* userData) {
    return getLauncherMode(static_cast<Context*>(userData)->mode).collect();
}

void onLegacyClicked(const TileGridItem& item, void*) {
    queueAppStart(item.id);
}

void onLegacyLongPressed(const TileGridItem& item, void* userData) {
    auto* ctx = static_cast<Context*>(userData);
    const LauncherMode& mode = getLauncherMode(ctx->mode);
    if (mode.onLongPressed != nullptr) {
        mode.onLongPressed(ctx->grid, item);
    }
}

void onLegacyKey(const TileGridItem& item, uint32_t key, void* userData) {
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

void onModeButtonPressed(lv_event_t* e) {
    auto* ctx = static_cast<Context*>(lv_event_get_user_data(e));
    switchMode(ctx);
    lv_label_set_text(ctx->modeButtonIcon, getLauncherMode(getOtherMode(ctx->mode)).buttonIcon);
}

void createWidgets(lv_obj_t* parent, void* userData) {
    auto* ctx = static_cast<Context*>(userData);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 0, LV_STATE_DEFAULT);

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
}

void destroyWidgets(void* user_data) {
    auto* ctx = static_cast<Context*>(user_data);
    lvgl::systemBarsRemoveListener(ctx->systemBarsListener);
    ctx->systemBarsListener = 0;
    ctx->grid.destroyWidgets();
}

// endregion

#endif // CONFIG_TT_MOBILE_HOME

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

    // The launcher's task stack is small (see the manifest), so the context lives on the heap
    auto* ctx = new (::std::nothrow) Context();
    if (ctx == nullptr) {
        LOG_E(TAG, "Failed to allocate the launcher context");
        return -1;
    }

    TaskEventGroup event_group {};
    task_event_group_construct(&event_group);

    AppEventSubscription sub {};
    check(app_event_subscribe(&sub, &event_group) == ERROR_NONE);

    FileOpsSubscription file_ops_sub {};
    check(file_ops_subscribe(&file_ops_sub, &event_group) == ERROR_NONE);

    WindowId window = window_manager_create_ext(appInstanceId, createWidgets, destroyWidgets, ctx);
    if (window == 0) {
        LOG_E(TAG, "Failed to create the launcher window");
        delete ctx;
        file_ops_unsubscribe(&file_ops_sub);
        check(app_event_unsubscribe(&sub) == ERROR_NONE);
        task_event_group_destruct(&event_group);
        return -1;
    }

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
#if defined(CONFIG_TT_MOBILE_HOME)
            if (ctx->v2Home && ctx->gridPage != nullptr) {
                refreshHomeItems(ctx);
            } else {
                ctx->grid.requestRepopulate(false);
            }
#else
            ctx->grid.requestRepopulate(false);
#endif
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
    delete ctx;
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
    // All file operations run on the file operations task, so the stack can be in external RAM.
    // 6144 bytes: the Home build runs inline on this task and the 3072-byte value was written as
    // if it were words (see docs/mobile/boot-crash-spec.md).
    .stack = { .depth = 6144, .desired_memory_capability = MEMORY_CAPABILITY_EXTERNAL },
};

} // namespace tt::app::launcher
