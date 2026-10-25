#include <Tactility/lvgl/FontSizes.h>

#include <cmath>

#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

namespace tt::lvgl {

static uint16_t scale(uint16_t size, float factor) {
    return static_cast<uint16_t>(std::lround(size * factor));
}

#if defined(CONFIG_TT_MOBILE_HOME)
uint16_t getTextFontSize(uint16_t defaultSize, LvglFontSize fontSize) {
    (void)defaultSize;
    switch (fontSize) {
        case FONT_SIZE_SMALL: return 12;
        case FONT_SIZE_SECONDARY: return 13;
        case FONT_SIZE_DENSE: return 14;
        case FONT_SIZE_LARGE: return 22;
        case FONT_SIZE_DEFAULT:
        default: return 15;
    }
}
#else
uint16_t getTextFontSize(uint16_t defaultSize, LvglFontSize fontSize) {
    switch (fontSize) {
        case FONT_SIZE_SMALL: return scale(defaultSize, 0.75f);
        case FONT_SIZE_LARGE: return scale(defaultSize, 1.25f);
        case FONT_SIZE_SECONDARY:
        case FONT_SIZE_DENSE:
        case FONT_SIZE_DEFAULT:
        default: return defaultSize;
    }
}
#endif

uint16_t getIconFontSize(uint16_t defaultSize, LvglIconFont iconFont) {
    switch (iconFont) {
        case LVGL_ICON_FONT_LAUNCHER: return scale(defaultSize, 2.6f);
        case LVGL_ICON_FONT_SHARED_DEFAULT: return scale(defaultSize, 1.25);
        case LVGL_ICON_FONT_SHARED_LARGE: return getIconFontSize(defaultSize, LVGL_ICON_FONT_SHARED) * 2;
        case LVGL_ICON_FONT_STATUSBAR:
        default: return scale(defaultSize, 1.15f);
    }
}

uint16_t getClockFontSize(uint16_t defaultSize) {
#if defined(CONFIG_TT_MOBILE_HOME)
    (void)defaultSize;
    return 48;
#else
    return scale(defaultSize, 2.5f);
#endif
}

}
