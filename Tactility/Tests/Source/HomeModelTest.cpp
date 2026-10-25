// SPDX-License-Identifier: Apache-2.0

#include "doctest.h"

#include <Tactility/app/launcher/HomeModel.h>

#include <string>
#include <vector>

using namespace tt::app::launcher;

namespace {

HomeApp app(
    const char* id,
    const char* name,
    bool settings = false,
    bool favourite = false,
    bool hidden = false,
    bool compatible = true
) {
    return HomeApp { id, name, nullptr, favourite, hidden, settings, compatible };
}

std::vector<std::string> ids(const std::vector<HomeItem>& items) {
    std::vector<std::string> result;
    for (const auto& item : items) {
        result.push_back(item.id);
    }
    return result;
}

std::vector<HomeApp> boardApps() {
    return {
        app(SETTINGS_APP_ID, "Settings"),
        app(GROK_BOT_APP_ID, "Grok Bot"),
        app(WIFI_APP_ID, "Wi-Fi", true),
        app("tactility.files", "Files"),
        app("tactility.camera", "Camera"),
        app("tactility.notes", "Notes"),
    };
}

}

TEST_CASE("the Home starts with Settings, Grok Bot and Wi-Fi, then the rest by name") {
    const auto items = resolveHomeItems(boardApps());

    CHECK_EQ(ids(items), (std::vector<std::string> {
        SETTINGS_APP_ID,
        GROK_BOT_APP_ID,
        WIFI_APP_ID,
        "tactility.camera",
        "tactility.files",
        "tactility.notes",
    }));
}

TEST_CASE("favourites come before the rest, but after the pinned apps") {
    std::vector<HomeApp> apps = {
        app(SETTINGS_APP_ID, "Settings"),
        app("tactility.zeta", "Zeta", false, true),
        app("tactility.alpha", "Alpha"),
        app("tactility.beta", "Beta", false, true),
    };
    const auto items = resolveHomeItems(apps);

    CHECK_EQ(ids(items), (std::vector<std::string> {
        SETTINGS_APP_ID,
        "tactility.beta",
        "tactility.zeta",
        "tactility.alpha",
    }));
}

TEST_CASE("hidden, headless and unpinned settings apps stay out of the Home") {
    std::vector<HomeApp> apps = {
        app(SETTINGS_APP_ID, "Settings"),
        app("tactility.visible", "Visible"),
        app("tactility.hidden", "Hidden", false, false, true),
        app("tactility.display", "Display", true),
        app("tactility.incompatible", "Incompatible", false, false, false, false),
    };
    const auto items = resolveHomeItems(apps);

    CHECK_EQ(ids(items), (std::vector<std::string> { SETTINGS_APP_ID, "tactility.visible" }));
}

TEST_CASE("a missing or incompatible pinned app leaves no gap") {
    std::vector<HomeApp> apps = {
        app(SETTINGS_APP_ID, "Settings"),
        app(WIFI_APP_ID, "Wi-Fi", true),
        app("tactility.files", "Files"),
    };
    const auto items = resolveHomeItems(apps);

    CHECK_EQ(ids(items), (std::vector<std::string> {
        SETTINGS_APP_ID,
        WIFI_APP_ID,
        "tactility.files",
    }));
}

TEST_CASE("more than one page is returned whole, so the UI can slice it") {
    std::vector<HomeApp> apps = { app(SETTINGS_APP_ID, "Settings") };
    for (int i = 0; i < 12; i++) {
        apps.push_back(app(("tactility.app" + std::to_string(i)).c_str(), ("App " + std::to_string(i)).c_str()));
    }
    const auto items = resolveHomeItems(apps);

    CHECK_EQ(items.size(), 13);
    CHECK_EQ(items.front().id, SETTINGS_APP_ID);
    CHECK_EQ(items.back().name, "App 9");
}

TEST_CASE("an app id appears once") {
    std::vector<HomeApp> apps = {
        app(SETTINGS_APP_ID, "Settings"),
        app("tactility.notes", "Notes"),
        app("tactility.notes", "Notes again"),
    };
    const auto items = resolveHomeItems(apps);

    CHECK_EQ(items.size(), 2);
    CHECK_EQ(items[1].name, "Notes");
}

TEST_CASE("there is no mode tile: the Settings hub is a real app") {
    const std::vector<HomeApp> apps = { app("tactility.notes", "Notes") };
    const auto items = resolveHomeItems(apps);

    REQUIRE(items.size() == 1);
    CHECK_EQ(items[0].id, "tactility.notes");
}

TEST_CASE("the dock keeps its fixed order and omits missing apps") {
    auto apps = boardApps();
    auto dock = resolveDockItems(apps);

    REQUIRE(dock.size() == 4);
    CHECK_EQ(std::string(dock[0].id), GROK_BOT_APP_ID);
    CHECK_EQ(std::string(dock[1].id), "tactility.files");
    CHECK_EQ(std::string(dock[2].id), "tactility.camera");
    CHECK_EQ(std::string(dock[3].id), SETTINGS_APP_ID);

    apps.erase(apps.begin() + 2); // Wi-Fi is not in the dock anyway
    apps.erase(std::remove_if(apps.begin(), apps.end(), [](const HomeApp& app) {
        return app.id == "tactility.camera";
    }), apps.end());
    dock = resolveDockItems(apps);
    REQUIRE(dock.size() == 3);
    CHECK_EQ(std::string(dock[2].id), SETTINGS_APP_ID);

    apps.erase(std::remove_if(apps.begin(), apps.end(), [](const HomeApp& app) {
        return app.id == GROK_BOT_APP_ID;
    }), apps.end());
    dock = resolveDockItems(apps);
    REQUIRE(dock.size() == 2);
    CHECK_EQ(std::string(dock[0].id), "tactility.files");
    CHECK_EQ(std::string(dock[1].id), SETTINGS_APP_ID);
}

TEST_CASE("the dock is empty without apps") {
    CHECK(resolveDockItems({}).empty());
}
