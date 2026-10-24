#include "doctest.h"

#include <Tactility/lvgl/ClockText.h>

#include <string>

using namespace tt::lvgl;

namespace {

tm makeTime(int year, int month, int day, int hour, int minute, int second = 30) {
    tm result = {};
    result.tm_year = year - 1900;
    result.tm_mon = month - 1;
    result.tm_mday = day;
    result.tm_hour = hour;
    result.tm_min = minute;
    result.tm_sec = second;
    // 2026-10-24 is a Saturday
    result.tm_wday = 6;
    result.tm_yday = 296;
    return result;
}

}

TEST_CASE("the clock uses a placeholder before the time is set") {
    const auto text = formatClock(makeTime(2024, 10, 24, 9, 5), true);
    CHECK_FALSE(text.valid);
    CHECK_EQ(std::string(text.time), "--:--");
    CHECK_EQ(std::string(text.date), "");
    CHECK(text.meridiem == nullptr);
}

TEST_CASE("24-hour time has no leading zero and a short English date") {
    const auto text = formatClock(makeTime(2026, 10, 24, 9, 5), true);
    CHECK(text.valid);
    CHECK_EQ(std::string(text.time), "9:05");
    CHECK_EQ(std::string(text.date), "Sat 24 Oct");
    CHECK(text.meridiem == nullptr);
}

TEST_CASE("12-hour time uses 12 for midnight and noon and shows AM/PM") {
    const auto midnight = formatClock(makeTime(2026, 10, 24, 0, 0), false);
    CHECK_EQ(std::string(midnight.time), "12:00");
    CHECK_EQ(std::string(midnight.meridiem), "AM");

    const auto morning = formatClock(makeTime(2026, 10, 24, 9, 5), false);
    CHECK_EQ(std::string(morning.time), "9:05");
    CHECK_EQ(std::string(morning.meridiem), "AM");

    const auto noon = formatClock(makeTime(2026, 10, 24, 12, 0), false);
    CHECK_EQ(std::string(noon.time), "12:00");
    CHECK_EQ(std::string(noon.meridiem), "PM");

    const auto evening = formatClock(makeTime(2026, 10, 24, 21, 5), false);
    CHECK_EQ(std::string(evening.time), "9:05");
    CHECK_EQ(std::string(evening.meridiem), "PM");
}

TEST_CASE("the clock timer waits for the next whole minute") {
    CHECK_EQ(millisToNextMinute(makeTime(2026, 10, 24, 9, 5, 0)), 60000U);
    CHECK_EQ(millisToNextMinute(makeTime(2026, 10, 24, 9, 5, 59)), 1000U);
    CHECK_EQ(millisToNextMinute(makeTime(2026, 10, 24, 9, 5, 30)), 30000U);
}
