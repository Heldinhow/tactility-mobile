// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <lvgl.h>

#include <cstdint>

/** One-colour A8 masks drawn from the v2 design sources. Sizes are fixed by the design. */
enum class MobileIcon : uint8_t {
    Appearance,
    Back,
    Bot,
    Bt,
    Calendar,
    Camera,
    Chip,
    Code,
    Del,
    Display,
    Eye,
    File,
    Folder,
    Hub,
    Info,
    Location,
    More,
    Notes,
    Power,
    Recents,
    Settings,
    Shift,
    Sig1,
    Sig2,
    Sig3,
    Sun,
    Target,
    Terminal,
    Usb,
    Vol,
    Wifi,
    Wifis,
    Chev,
    Close,
    Lock,
};

/** @return the 24 or 16 px mask for a named icon */
const lv_image_dsc_t* mobileIconGet(MobileIcon icon);

/** @return the v2 icon for an app id, or the generic fallback */
MobileIcon mobileIconForApp(const char* id);
