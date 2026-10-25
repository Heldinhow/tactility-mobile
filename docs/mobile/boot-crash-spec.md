# Boot crash: the Home build overflows the launcher task's stack

Status: ready-for-agent (branch `feat/mobile-home`, device `es3c28p`, 240x320)

## Problem Statement

`feat/mobile-home` at `a504731f` crashes on every boot on the ES3C28P. The device shows the
Tactility crash screen (QR + "Oops! We've crashed ... Touch the screen to continue") ~4 s after
power-on, then reboots into the same screen.

Serial capture (`/tmp/serial-crash.log`), read-only, no reset issued by the tooling:

```
I (4180) app_manager: [instance 2] startin
***ERROR*** A stack overflow in task app_2 has been detected.
```

The QR only carries the panic path (`_frxt_int_exit -> _frxt_dispatch -> vTaskSwitchContext ->
vApplicationStackOverflowHook -> esp_system_abort -> panic_abort -> _xt_handle_exc ->
xt_unhandled_exception -> panic_handler`), because the walk ends at the task's root frame. The
reason string, which names the task, is only visible on serial or in `/sdcard/tactility/crash.txt`.

## Evidence

- `app_2` is `tactility.launcher`, started by `Tactility/Source/app/boot/BootInit.cpp:190` with the
  default launcher id. On this branch its display name is "Home".
- Its manifest requests `.stack = { .depth = 3072, .desired_memory_capability = MEMORY_CAPABILITY_EXTERNAL }`
  (`Tactility/Source/app/launcher/Launcher.cpp`). On ESP-IDF `portSTACK_TYPE` is `uint8_t`
  (`freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos/portmacro.h:78`), so `StackType_t` is
  1 byte and the depth is **3072 bytes**, not 3072 words. `sizeof(StackType_t) == 1` is confirmed in
  the ELF, and the panic SP (`0x3c3b7a40`) is in PSRAM, matching the external-memory request.
  The comment on `AppStackConfig::depth` (`Modules/app-module/include/app/manifest.h`) says "in
  words", which is wrong for this port and is what made the budget look 4x larger than it is.
- `lvgl_run_on_task()` does **not** defer to the LVGL task: `Modules/lvgl-module/source/arch/lvgl_esp32.c:46`
  takes the LVGL lock and calls the function inline on the caller. So `window_manager_create_ext()`
  -> `build_window_widget()` -> `createWidgets()` -> `createHomeHeader()` +
  `TileGrid::createWidgets()` -> `populate()` -> `collectItems()` -> `collectHomeApps()` +
  `resolveHomeItems()` + `std::sort` all run on the launcher's 3072-byte stack.
- Measured frames on that path (`objdump`, `entry a1`): `appMain` 448 (of which `Context` ~400),
  `window_manager_create_ext` 112, `TileGrid::populate` 160, `collectItems` 128,
  `collectHomeApps` 160, `resolveHomeItems` 112, sort machinery 112-144 per recursion level,
  plus LVGL's per-object creation frames and the two `collectHomeApps()` calls (grid and dock).
- `main` carried the same inline build path on the same 3 KB stack, without the Home header, the
  clock font, the dock and the new Home model. The branch's additions are what push it over.

## Root Cause

The Home is built on the launcher app task's 3072-byte stack, and the mobile-home work adds enough
inline depth and locals (Context on the stack, the clock header, the dock, the Home model plus its
sorts) to exceed it. The stack is exhausted, not corrupted: the canary check fires at a context
switch, which reports the owning task (`app_2`) and nothing about the code that consumed it.

## Solution

Cut the peak on the Home build path instead of growing the stack:

1. Build the grid and the dock from **one** `collectHomeApps()`, and take that collection out of
   the nested path (`collectItems` no longer collects while `resolveHomeItems`/`sort` is on the
   stack).
2. Keep the pinned ids in a function-local static instead of building a `vector<string>` per
   populate.
3. Move the resolved ids/names into `TileGridItem` instead of copying them, so the intermediate
   `HomeItem` vector stops duplicating ~20 strings on the heap.
4. Move `Context` from `appMain`'s stack frame to the heap (it lives as long as the app anyway).
5. Fix the `AppStackConfig::depth` comment so the next budget is not computed in the wrong unit.

A `.depth` increase is only allowed if the measured stack frames of the new path leave less than a
25% margin against 3072 bytes. It costs PSRAM (4.4 MB free), not internal RAM or flash, but it hides
the real growth, so it is the last resort, not the fix.

## Non-Goals

- Changing `lvgl_run_on_task()` to really run on the LVGL task, or changing the window manager's
  build contract. Follow-up ticket `BC-07`.
- Moving `lockScreenShow()` off the FreeRTOS timer daemon task. Follow-up ticket `BC-08`.
- Putting the panic reason (task name) into the QR payload. Follow-up ticket `BC-09`, noted only.
- Changing the launcher's external-stack request. Stacks in PSRAM are a deliberate trade on this
  board (see the manifest comment); this crash is about depth, not placement.

## Verification

- `source ~/esp/esp-idf-v6.1/export.sh && python3 device.py es3c28p && idf.py build` exits 0.
- Stack frames of the changed path measured from the new ELF (`entry a1`), before/after, with the
  remaining chain accounted for and a margin >= 25% against 3072 bytes.
- Footprint table updated in `docs/mobile/mobile-home-footprint.md`: binary size and internal/PSRAM
  heap, before/after.
- On hardware (pending the owner's flash approval): 5 consecutive boots without the crash screen,
  then the full owner checklist from the mobile-home footprint document, plus a high-water reading
  from `uxTaskGetStackHighWaterMark()` taken with the temporary probe described in
  `docs/mobile/boot-crash-tickets.md`.

## Risks

- The frame arithmetic is static: indirect calls (LVGL event callbacks, class constructors) are not
  counted. The probe on hardware is what closes the account; until then the margin is an estimate.
- Sharing one collection between grid and dock means a stale collection if a repopulate ever
  happens while the build is in progress. The share window is the synchronous body of
  `createWidgets()`, and the cache is cleared at the end of it.
- Moving `Context` to the heap adds ~400 bytes of internal heap and must not leak when the window
  fails to be created.
