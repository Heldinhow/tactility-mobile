// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <lvgl/icons/shared.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace tt::app::launcher {

/** The parts of an app manifest that decide where it appears on the Home */
struct HomeApp {
    std::string id;
    std::string name;
    const char* icon;
    bool favourite;
    /** Hidden or headless: never shown */
    bool hidden;
    /** Belongs to the settings category */
    bool settings;
    /** The installed package can run on this device */
    bool compatible;
};

/** One tile of the Home grid. Mirrors TileGridItem without depending on LVGL. */
struct HomeItem {
    std::string id;
    std::string name;
    const char* icon;
    bool highlighted;
};

constexpr auto* SETTINGS_APP_ID = "tactility.settings";
constexpr auto* GROK_BOT_APP_ID = "one.grokbot.carousel";
constexpr auto* WIFI_APP_ID = "tactility.wifimanage";

inline bool isHomeAppVisible(const HomeApp& app) {
    return !app.hidden && app.compatible;
}

/**
 * Orders the tiles of the Home: the Settings hub, Grok Bot and Wi-Fi first (when installed), then
 * every other app with favourites before the rest and names alphabetical. Settings-category apps
 * are left to the Settings hub, except when they are one of the three pinned apps. The whole list
 * is returned; slicing it into pages of eight is the presentation's concern.
 */
inline std::vector<HomeItem> resolveHomeItems(const std::vector<HomeApp>& apps) {
    std::vector<HomeItem> items;
    items.reserve(apps.size());

    const auto add = [&items](const HomeApp& app) {
        const bool exists = ::std::any_of(items.begin(), items.end(), [&app](const HomeItem& item) {
            return item.id == app.id;
        });
        if (!exists) {
            items.push_back(HomeItem { app.id, app.name, app.icon, app.favourite });
        }
    };
    const auto add_pinned = [&apps, &add](const char* id) {
        for (const auto& app : apps) {
            if (app.id == id && isHomeAppVisible(app)) {
                add(app);
                return;
            }
        }
    };

    add_pinned(SETTINGS_APP_ID);
    add_pinned(GROK_BOT_APP_ID);
    add_pinned(WIFI_APP_ID);

    std::vector<const HomeApp*> rest;
    for (const auto& app : apps) {
        if (!isHomeAppVisible(app) || app.settings) {
            continue;
        }
        if (app.id == SETTINGS_APP_ID || app.id == GROK_BOT_APP_ID || app.id == WIFI_APP_ID) {
            continue;
        }
        rest.push_back(&app);
    }
    ::std::ranges::sort(rest, [](const HomeApp* a, const HomeApp* b) {
        if (a->favourite != b->favourite) {
            return a->favourite;
        }
        return a->name < b->name;
    });
    for (const auto* app : rest) {
        add(*app);
    }
    return items;
}

/** One always-visible shortcut in the dock. */
struct DockItem {
    const char* id;
};

/**
 * @return the dock slots that the device can show, in their fixed order. Missing or incompatible
 * apps are omitted without leaving a gap. Settings is always installed, but is checked like the
 * others so a build without the hub cannot draw a dead slot.
 */
inline std::vector<DockItem> resolveDockItems(const std::vector<HomeApp>& apps) {
    static constexpr const char* SLOTS[] = {
        GROK_BOT_APP_ID,
        "tactility.files",
        "tactility.camera",
        SETTINGS_APP_ID,
    };

    std::vector<DockItem> items;
    for (const auto* id : SLOTS) {
        for (const auto& app : apps) {
            if (app.id == id && isHomeAppVisible(app)) {
                items.push_back(DockItem { id });
                break;
            }
        }
    }
    return items;
}

/** Reads the installed apps and their compatibility for the Home. */
std::vector<HomeApp> collectHomeApps();

} // namespace tt::app::launcher
