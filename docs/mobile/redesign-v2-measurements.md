# Redesign v2: device measurements (probe build)

Status: the owner-approved probe flash is done. The numbers below are from the ES3C28P
(`/dev/cu.usbmodem101`) running the `feat/redesign-v2` probe build. The probe was removed after the
capture; the tree is back to the committed `5bb40c8f` state and only this document is new.

Spec: `docs/mobile/redesign-v2-spec.md`
Footprint: `docs/mobile/redesign-v2-footprint.md` (this file fills its pending runtime section)
Retro: `docs/mobile/redesign-v2-retro.md`

## Method

- Board: CYD ES3C28P, ESP32-S3 with native USB, `CONFIG_TT_MOBILE_HOME=y`, portrait.
- Probe: a temporary `Tactility/Source/StackProbe.cpp` (temporary `Timer`, 10 s period) that logs
  `heap_caps_get_free_size()`, `heap_caps_get_minimum_free_size()` and
  `heap_caps_get_largest_free_block()` for `MALLOC_CAP_INTERNAL` and `MALLOC_CAP_SPIRAM`, then
  `uxTaskGetSystemState()` for every task's `usStackHighWaterMark`. It was called once at the end of
  `tt::init()`, after `startBootApp()`, and was not committed.
- Values are **bytes**: ESP-IDF's Xtensa port defines `portSTACK_TYPE` as `uint8_t`
  (`components/freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos/portmacro.h:78`), so both
  the task stack sizes and `uxTaskGetStackHighWaterMark()` are byte counts here.
- Flash: `idf.py -p /dev/cu.usbmodem101 flash`; the capture started right after the post-flash hard
  reset, so it includes the boot from 243 ms.
- Capture: read-only pyserial, 115200, no DTR/RTS writes, 90 s, one file, parsed afterwards.

## Size and flash

| Build | `Tactility.bin` | Delta |
|---|---:|---:|
| `feat/redesign-v2` `5bb40c8f`, no probe | 3,923,024 B | — |
| Same tree with the probe | 3,923,664 B | +640 B |
| After removing the probe (rebuilt) | 3,923,024 B | 0 B |

Flash write: 3,923,664 B (2,318,433 B compressed) at `0x00010000`, 20.1 s, 1558.5 kbit/s, esptool
reported `Hash of data verified.` and reset the board with `Hard resetting via RTS pin`.
The probe's +640 B is code plus log format strings; it is inside the +100 KB flash budget.

## Heap

Boot (`memory` service, before the Wi-Fi radio starts):

```text
I (2879) memory: Heap: 167587 / 326047 available
I (2879) memory: External: 4521052 / 4584240 available
I (2881) app_manager: [instance 2] starting tactility.launcher with parent 0
I (2882) memory: Heap: 166871 / 326047 available
I (2882) memory: External: 4514468 / 4584240 available
```

The launcher (the Home) costs 716 B of internal heap and 6,584 B of PSRAM at start.

Steady state, probe samples every 10 s from 11.6 s to 91.6 s (free / all-time minimum / largest
block, in bytes):

| t (s) | Internal free | Internal min | Internal largest | PSRAM free | PSRAM min | PSRAM largest |
|---:|---:|---:|---:|---:|---:|---:|
| 11.6 | 140,763 | 100,744 | 59,392 | 4,481,128 | 4,436,096 | 4,456,448 |
| 21.6 | 140,799 | 100,744 | 59,392 | 4,481,128 | 4,436,096 | 4,456,448 |
| 31.6 | 140,763 | 100,744 | 59,392 | 4,481,128 | 4,436,096 | 4,456,448 |
| 41.6 | 140,919 | 100,744 | 59,392 | 4,481,068 | 4,436,096 | 4,456,448 |
| 51.6 | 140,919 | 100,744 | 59,392 | 4,481,068 | 4,436,096 | 4,456,448 |
| 61.6 | 140,839 | 100,744 | 59,392 | 4,481,068 | 4,436,096 | 4,456,448 |
| 81.6 | 140,667 | 100,744 | 59,392 | 4,480,128 | 4,436,096 | 4,456,448 |
| 91.6 | 140,667 | 100,744 | 59,392 | 4,479,792 | 4,436,096 | 4,456,448 |

(The 71.6 s sample lost its heap line to a log-line interleave; the task list of that sample is
present.)

- The idle Home holds **140.7 KB of internal heap free** (43% of 326,047 B) and **4.48 MB of PSRAM
  free** (98% of 4,584,240 B).
- The internal all-time minimum, **100,744 B**, is set before 11.6 s: the Wi-Fi driver start, the
  association at 6.39 s, the DHCP address at 10.91 s and the radio's static buffers. It is the boot
  figure, not a Home cost.
- Between 11.6 s and 91.6 s the internal heap drifts **-96 B** and PSRAM **-1,336 B**. No allocation
  per tick and no leak in the idle Home.
- The largest internal free block stays at 59,392 B and the largest PSRAM block at 4,456,448 B, so
  no fragmentation cliff is in sight.

The footprint target "steady UI heap at most +6 KB over the old Home" cannot be settled from this
run: the pre-v2 firmware is not on the device (the backup at
`~/esp32/backups/es3c28p-mobile-home-a5b75fa9-app-0x10000-20261025.bin` was not re-flashed). The
last pre-v2 boot capture quoted in the footprint report used a different config (different heap
totals) and is not comparable. What this run does establish is the absolute free heap above.

## Task stack high-water

Worst (smallest free) of the eight samples. Stack size is the configured depth; `% free` is the
high-water over that size.

| Task | Stack (B) | Free high-water (B) | % free | Note |
|---|---:|---:|---:|---|
| tcpip | 3,072 | 580 | 18.9% | `CONFIG_LWIP_TCPIP_TASK_STACK_SIZE`; dropped 2,068 → 580 between 31.6 s and 41.6 s |
| ipc0 | 1,280 | 592 | 46.3% | |
| ipc1 | 1,280 | 600 | 46.9% | |
| IDLE1 | 1,536 | 756 | 49.2% | |
| IDLE0 | 1,536 | 764 | 49.7% | |
| usb_hid_inp | 3,072 | 1,124 | 36.6% | `Tactility/Source/lvgl/UsbHidInput.cpp` |
| wifi-autoconnec | n/a | 1,180 | n/a | |
| esp32_wifi_pinn | n/a | 1,292 | n/a | |
| sys_evt | 3,072 | 1,432 | 46.6% | `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` |
| ble_hid_host | 8,192 | 2,168 | 26.5% | `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE` |
| log_drain | 4,096 | 2,712 | 66.2% | `TactilityKernel/Source/log_queue.cpp` |
| **Tmr Svc** | 5,120 | 2,716 | 53.0% | timer daemon, `CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH` |
| **app_2** (launcher) | 6,144 | 2,944 | 47.9% | Home build runs on this task; stack in PSRAM |
| **taskLVGL** | 9,120 | 2,960 | 32.5% | lowest free stack of the v2 paths |
| esp_timer | 3,584 | 3,012 | 84.0% | |
| wifi | 6,656 | 3,548 | 53.3% | size from the driver's own boot line |
| bluetooth | 6,144 | 4,208 | 68.5% | |
| development-ser | 8,192 | 6,268 | 76.5% | development server listener |

- **No task is near its limit.** The tightest v2-relevant one is `taskLVGL` with 2,960 B free of
  9,120 B after the Home and the statusbar ran; `app_2` (the Home build inline) has 2,944 B free of
  6,144 B. Both are the same candidates the boot-crash work sized, and both keep about half or a
  third of their stack.
- There is **no `main` task** to report: `Tactility/Source/Main.cpp:14` deletes it after
  `tt::init()`. Its boot usage is not covered by this probe.
- `tcpip` (580 B free, 19%) is the tightest task on the device, but it is the ESP-IDF/lwIP task at
  its stock 3,072 B, not a v2 allocation and not a v2 change. Flagged for the record; no overflow.
- The LVGL free stack fell 3,568 → 2,960 B between 51.6 s and 61.6 s with no log line at that
  moment (a statusbar/LVGL timer path, most likely the Wi-Fi event redraw). It stayed there.

## Crash scan

`grep -icE "stack overflow|Guru|Backtrace|panic|assert failed|abort|watchdog|Stack canary"` over the
90 s capture: **0 matches**. The capture starts at 243 ms and covers the full boot, the Home build,
the Wi-Fi association and 80 s of the idle Home. Only two benign warnings appear, both pre-existing:

```text
W (5555) crypt: Using tt_secure_* code with secure boot and/or flash encryption disabled.
W (5555) crypt: An attacker with physical access to your ESP32 can decrypt your secure data.
```

## Did the Home v2 come up

The boot log shows the launcher as instance 2 (`app_manager: [instance 2] starting
tactility.launcher`), the LVGL task bound to the display, Wi-Fi associated as `Venerando` and an IP
of `192.168.0.10`; the launcher task stayed alive for the whole capture and the LVGL task kept
painting (its high-water moved after boot). No crash screen, no reboot, no panic. The visual proof
(the 4x2 grid, clock, date, dock) is the owner's screen check; this run proves the build boots and
stays up, not what the pixels look like.

## Limits

- Read-only capture: no screen was opened during the 90 s, so this is the boot + idle Home, not the
  Settings/Wi-Fi/Files/Recents/panel/lock paths. Those still need a capture while the owner taps, or
  probe counters read after the fact.
- The probe logs `heap_caps_get_minimum_free_size()` since boot, so the minimum includes Wi-Fi
  bring-up; it is not a UI-only figure.
- The probe build is what is currently flashed. Flashing the cleaned build back is a separate
  decision; the backup image was not touched.
- `Libraries/FreeRTOS-Kernel` was already modified in this worktree and was neither used nor
  committed by this run.

## Probe source (as flashed)

```cpp
void logHeap() {
    LOG_I(TAG, "heap internal free=%u min=%u largest=%u | psram free=%u min=%u largest=%u", ...);
}

void logTasks() {
    static TaskStatus_t statuses[32];
    const UBaseType_t count = uxTaskGetSystemState(statuses, 32, nullptr);
    for (UBaseType_t i = 0; i < count; i++) {
        LOG_I(TAG, "task %s high-water=%u bytes", statuses[i].pcTaskName,
              static_cast<unsigned>(statuses[i].usStackHighWaterMark));
    }
}

void startStackProbe() {
    static Timer timer(Timer::Type::Periodic, millis_to_ticks(10000), probe);
    timer.start();
}
```
