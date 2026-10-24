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

}

TEST_CASE("the apps view starts with Settings and pins the carousel and Wi-Fi") {
    const std::vector<HomeApp> apps = {
        app("tactility.files", "Files"),
        app("tactility.wifimanage", "Wi-Fi"),
        app("one.grokbot.carousel", "Grok Bot"),
        app("tactility.notes", "Notes"),
    };
    const auto items = resolveHomeItems(HomeView::Apps, apps, { "one.grokbot.carousel", "tactility.wifimanage" });

    CHECK_EQ(items.size(), 5);
    CHECK_EQ(items[0].id, HOME_MODE_ITEM_ID);
    CHECK_EQ(items[0].name, "Settings");
    CHECK_FALSE(items[0].highlighted);
    CHECK_EQ(ids(items), (std::vector<std::string> {
        HOME_MODE_ITEM_ID,
        "one.grokbot.carousel",
        "tactility.wifimanage",
        "tactility.files",
        "tactility.notes",
    }));
}

TEST_CASE("a missing or incompatible pinned app leaves no gap") {
    const std::vector<HomeApp> apps = {
        app("tactility.wifimanage", "Wi-Fi"),
        app("one.grokbot.carousel", "Grok Bot", false, false, false, false),
        app("tactility.files", "Files"),
    };
    const auto items = resolveHomeItems(HomeView::Apps, apps, { "one.grokbot.carousel", "tactility.wifimanage" });

    CHECK_EQ(ids(items), (std::vector<std::string> {
        HOME_MODE_ITEM_ID,
        "tactility.wifimanage",
        "tactility.files",
    }));
}

TEST_CASE("hidden, headless and settings apps are kept out of the apps view") {
    const std::vector<HomeApp> apps = {
        app("tactility.visible", "Visible"),
        app("tactility.hidden", "Hidden", false, false, true),
        app("tactility.headless", "Headless", false, false, true),
        app("tactility.settings", "Display", true),
        app("tactility.incompatible", "Incompatible", false, false, false, false),
    };
    const auto items = resolveHomeItems(HomeView::Apps, apps, {});

    CHECK_EQ(ids(items), (std::vector<std::string> { HOME_MODE_ITEM_ID, "tactility.visible" }));
}

TEST_CASE("favourites come before the rest, but after the pinned apps") {
    const std::vector<HomeApp> apps = {
        app("tactility.zeta", "Zeta", false, true),
        app("tactility.alpha", "Alpha"),
        app("tactility.beta", "Beta", false, true),
        app("tactility.wifimanage", "Wi-Fi"),
    };
    const auto items = resolveHomeItems(HomeView::Apps, apps, { "tactility.wifimanage" });

    CHECK_EQ(ids(items), (std::vector<std::string> {
        HOME_MODE_ITEM_ID,
        "tactility.wifimanage",
        "tactility.beta",
        "tactility.zeta",
        "tactility.alpha",
    }));
}

TEST_CASE("the settings view starts with Apps and lists only settings apps by name") {
    const std::vector<HomeApp> apps = {
        app("tactility.display", "Display", true),
        app("tactility.files", "Files"),
        app("tactility.audio", "Audio", true),
        app("tactility.hidden", "Hidden settings", true, false, true),
    };
    const auto items = resolveHomeItems(HomeView::Settings, apps, { "tactility.wifimanage" });

    CHECK_EQ(items.size(), 3);
    CHECK_EQ(items[0].id, HOME_MODE_ITEM_ID);
    CHECK_EQ(items[0].name, "Apps");
    CHECK_EQ(ids(items), (std::vector<std::string> {
        HOME_MODE_ITEM_ID,
        "tactility.audio",
        "tactility.display",
    }));
}

TEST_CASE("a repeated app id appears once") {
    const std::vector<HomeApp> apps = {
        app("tactility.notes", "Notes"),
        app("tactility.notes", "Notes again"),
    };
    const auto items = resolveHomeItems(HomeView::Apps, apps, {});

    CHECK_EQ(items.size(), 2);
    CHECK_EQ(items[1].name, "Notes");
}
