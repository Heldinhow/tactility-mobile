#pragma once

#include <cstdint>
#include <cstdio>
#include <ctime>

namespace tt::lvgl {

/** The text that the Home and the lock screen show for one moment in time. */
struct ClockText {
    /** "9:05", "12:00" or the "--:--" placeholder */
    char time[8];
    /** "Sat 24 Oct", or empty before the clock is set */
    char date[32];
    /** "AM"/"PM" in 12-hour format, otherwise nullptr */
    const char* meridiem;
    /** False before the clock is set; time then holds the placeholder */
    bool valid;
};

/**
 * Formats a broken-down local time for the large clocks.
 * @param[in] local the local time, as returned by localtime()
 * @param[in] is_24_hour whether the system uses 24-hour format
 */
inline ClockText formatClock(const tm& local, bool is_24_hour) {
    ClockText result = {};
    // Before the clock is set, the year is meaningless - the same rule the statusbar uses
    if (local.tm_year < (2025 - 1900)) {
        std::snprintf(result.time, sizeof(result.time), "--:--");
        return result;
    }

    int hour = local.tm_hour;
    if (!is_24_hour) {
        result.meridiem = hour < 12 ? "AM" : "PM";
        hour = hour % 12;
        if (hour == 0) {
            hour = 12;
        }
    }
    std::snprintf(result.time, sizeof(result.time), "%d:%02d", hour, local.tm_min);
    std::strftime(result.date, sizeof(result.date), "%a %d %b", &local);
    result.valid = true;
    return result;
}

/** @return the milliseconds until the next whole minute, for a minute-aligned clock timer */
inline uint32_t millisToNextMinute(const tm& local) {
    const uint32_t seconds = local.tm_sec < 60 ? 60U - static_cast<uint32_t>(local.tm_sec) : 60U;
    return seconds * 1000U;
}

}
