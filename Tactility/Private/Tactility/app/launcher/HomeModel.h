// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <lvgl/icons/shared.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace tt::app::launcher {

/** The two contents of the launcher: apps, and the settings apps behind the Settings tile */
enum class HomeView {
    Apps,
    Settings,
};

/** The parts of an app manifest that decide where it appears on the Home */
struct HomeApp {
    std::string id;
    std::string name;
    const char* icon;
    bool favourite;
    /** Hidden or headless: never shown in either view */
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

constexpr auto* HOME_MODE_ITEM_ID = "tactility.launcher.mode";

/** @return the tile that switches between the apps and settings views */
inline HomeItem makeHomeModeItem(HomeView view) {
    return view == HomeView::Apps
        ? HomeItem { HOME_MODE_ITEM_ID, "Settings", LVGL_ICON_SHARED_SETTINGS, false }
        : HomeItem { HOME_MODE_ITEM_ID, "Apps", LVGL_ICON_SHARED_APPS, false };
}

/**
 * Orders the tiles of one Home view.
 * Apps: the mode tile, the pinned apps in the given order, then every other app with favourites
 * first and names alphabetical. Settings: the mode tile, then the settings apps by name.
 * Hidden and incompatible apps are omitted; an app id appears at most once.
 */
inline std::vector<HomeItem> resolveHomeItems(
    HomeView view,
    const std::vector<HomeApp>& apps,
    const std::vector<std::string>& pinned
) {
    std::vector<HomeItem> items;
    items.push_back(makeHomeModeItem(view));

    auto add = [&items](const HomeApp& app, bool highlighted) {
        for (const auto& existing : items) {
            if (existing.id == app.id) {
                return;
            }
        }
        items.push_back(HomeItem { app.id, app.name, app.icon, highlighted });
    };

    if (view == HomeView::Settings) {
        for (const auto& app : apps) {
            if (!app.hidden && app.compatible && app.settings) {
                add(app, false);
            }
        }
        ::std::sort(items.begin() + 1, items.end(), [](const HomeItem& a, const HomeItem& b) {
            return a.name < b.name;
        });
        return items;
    }

    const auto is_pinned = [&pinned](const std::string& id) {
        return ::std::find(pinned.begin(), pinned.end(), id) != pinned.end();
    };
    for (const auto& app : apps) {
        // A pinned app is shown even when it belongs to the settings category (Wi-Fi is one)
        if (!app.hidden && app.compatible && (!app.settings || is_pinned(app.id))) {
            add(app, app.favourite);
        }
    }

    const auto rank = [&pinned](const HomeItem& item) {
        const auto found = ::std::find(pinned.begin(), pinned.end(), item.id);
        return found == pinned.end() ? pinned.size() : static_cast<size_t>(found - pinned.begin());
    };
    ::std::sort(items.begin() + 1, items.end(), [&rank](const HomeItem& a, const HomeItem& b) {
        const size_t rank_a = rank(a);
        const size_t rank_b = rank(b);
        if (rank_a != rank_b) {
            return rank_a < rank_b;
        }
        if (a.highlighted != b.highlighted) {
            return a.highlighted;
        }
        return a.name < b.name;
    });
    return items;
}

/** One always-visible shortcut in the dock. The settings slot is a mode switch, not an app. */
struct DockItem {
    const char* id;
    const char* name;
    const char* icon;
    bool settings;
};

/**
 * @return the dock slots that the device can show, in their fixed order. Wi-Fi, Files and the
 * Grok Bot carousel are omitted when they are not installed or compatible; Settings is always
 * present. No empty slots are returned.
 */
inline std::vector<DockItem> resolveDockItems(const std::vector<HomeApp>& apps) {
    static constexpr DockItem SLOTS[] = {
        { "tactility.wifimanage", "Wi-Fi", LVGL_ICON_SHARED_WIFI, false },
        { HOME_MODE_ITEM_ID, "Settings", LVGL_ICON_SHARED_SETTINGS, true },
        { "tactility.files", "Files", LVGL_ICON_SHARED_FOLDER, false },
        { "one.grokbot.carousel", "Grok Bot", LVGL_ICON_SHARED_FORUM, false },
    };

    std::vector<DockItem> items;
    for (const auto& slot : SLOTS) {
        if (slot.settings) {
            items.push_back(slot);
            continue;
        }
        for (const auto& app : apps) {
            if (!app.hidden && app.compatible && app.id == slot.id) {
                items.push_back(slot);
                break;
            }
        }
    }
    return items;
}

/** Reads the installed apps and their compatibility for the Home. */
std::vector<HomeApp> collectHomeApps();

}
