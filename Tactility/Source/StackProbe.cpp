// SPDX-License-Identifier: Apache-2.0
//
// Temporary measurement probe for docs/mobile/redesign-v2-footprint.md. It logs the timer
// daemon's and the LVGL task's stack high-water marks and the boot heap split once, 20 s after
// boot. Remove this file (and its call in Init.cpp) before the final commit: it exists only so the
// owner can flash an intermediate build and record the numbers.
#ifdef ESP_PLATFORM
#include <sdkconfig.h>
#endif

#if defined(ESP_PLATFORM) && defined(CONFIG_TT_MOBILE_HOME)

#include <Tactility/Timer.h>
#include <tactility/time.h>

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <tactility/log.h>

namespace tt {

namespace {

constexpr auto* TAG = "StackProbe";

void logTask(const char* name, TaskHandle_t task) {
    LOG_I(TAG, "%s high-water: %u bytes", name, task != nullptr ? uxTaskGetStackHighWaterMark(task) : 0);
}

void probe() {
    // The app scheduler logs every app task's high-water when that task finishes
    logTask("timer daemon", xTimerGetTimerDaemonTaskHandle());
    logTask("lvgl", xTaskGetHandle("taskLVGL"));
    LOG_I(
        TAG,
        "heap internal: %u free / %u total, PSRAM: %u free / %u total",
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
        static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM))
    );
}

}

/** Starts the one-shot measurement; see the file comment. */
void startStackProbe() {
    static Timer timer(Timer::Type::Once, millis_to_ticks(20000), probe);
    timer.start();
}

}

#endif
