// SPDX-License-Identifier: Apache-2.0

#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

#if defined(CONFIG_TT_MOBILE_HOME)

#include <Tactility/app/AppIcon.h>
#include <Tactility/app/launcher/Favourites.h>
#include <Tactility/app/launcher/HomeModel.h>

#include <app/manager.h>
#include <app/manifest.h>
#include <app/package_manifest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace tt::app::launcher {

namespace {

void collectManifest(const ::AppManifest* manifest, void* context) {
    static_cast<std::vector<::AppManifest>*>(context)->push_back(*manifest);
}

// Apps from installed packages this device can't run (wrong device, too little RAM)
void collectIncompatibleAppIds(const AppPackage* package, void* context) {
    if (app_package_manifest_is_compatible(&package->package)) {
        return;
    }
    auto* ids = static_cast<std::vector<std::string>*>(context);
    for (size_t i = 0; i < package->app_id_count; i++) {
        ids->emplace_back(package->app_ids[i]);
    }
}

}

std::vector<HomeApp> collectHomeApps() {
    std::vector<::AppManifest> manifests;
    app_manager_for_each_manifest(collectManifest, &manifests);

    std::vector<std::string> incompatible_ids;
    app_manager_for_each_package(collectIncompatibleAppIds, &incompatible_ids);

    const Favourites favourites;
    const std::vector<std::string> favourite_ids = favourites.ids();
    std::vector<HomeApp> apps;
    apps.reserve(manifests.size());
    for (const auto& manifest : manifests) {
        const bool settings = manifest.category == APP_CATEGORY_SETTINGS;
        const bool hidden = (manifest.flags & (APP_MANIFEST_FLAG_HIDDEN | APP_MANIFEST_FLAG_HEADLESS)) != 0;
        const bool compatible = ::std::find(incompatible_ids.begin(), incompatible_ids.end(), manifest.id) == incompatible_ids.end();
        apps.push_back(HomeApp {
            manifest.id,
            manifest.name,
            tt::app::getAppIcon(manifest),
            Favourites::contains(favourite_ids, manifest.id),
            hidden,
            settings,
            compatible,
        });
    }
    return apps;
}

}

#endif
