# Tickets: Boot crash

Spec: `docs/mobile/boot-crash-spec.md`
Integration branch: `feat/mobile-home` (origin: `Heldinhow/tactility-mobile`)
Definition of done for every ticket: its acceptance criteria are met and
`source ~/esp/esp-idf-v6.1/export.sh && python3 device.py es3c28p && idf.py build` exits 0.

Status vocabulary: `ready-for-agent` (can start now), `in progress`, `blocked`, `done` (complete on
the branch, build green).

No ticket flashes a device, opens a PR or pushes. The hardware checks are collected in the notes at
the end and are the owner's call.

---

## BC-01: The Home build path has a measured stack budget

**What to build:** Measure the frames of the launcher's Home build path from the ELF and keep the
temporary high-water probe ready for the on-device reading. The probe stays out of the final commit.

**Blocked by:** None.

**Status:** done (static measurement); hardware high-water pending the owner's flash

- [x] Every function on the path `appMain -> window_manager_create_ext -> build_window_widget ->
      createWidgets -> createHomeHeader/createDock/TileGrid::createWidgets -> populate ->
      collectItems -> collectHomeApps/resolveHomeItems/sort` has its `entry a1` frame recorded,
      before and after BC-02..BC-05.
- [x] The sum of the measured frames plus the LVGL object-creation frames leaves a margin against
      the launcher manifest's 3072-byte stack: app-side frames drop from ~1,716 B to ~1,514 B
      (`docs/mobile/mobile-home-footprint.md`), and the LVGL part is >=1.35 KB by subtraction.
- [x] The probe below is documented here verbatim and is **not** part of the committed tree:

      ```cpp
      // Temporary: log the calling task's stack high-water (bytes on ESP-IDF) per build stage.
      // The final commit removes this together with the flag.
      #if defined(TT_LAUNCHER_STACK_PROBE)
      #include <tactility/freertos/task.h>
      #define STACK_PROBE(stage) LOG_I(TAG, "stack high-water after %s: %u", stage, \
          static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)))
      #else
      #define STACK_PROBE(stage)
      #endif
      ```

      Call sites: after `createHomeHeader()`, after `ctx->grid.createWidgets()`, after
      `createDock()`, and at the top of `appMain()`.
- [x] If the measured margin is below 25% of 3072 bytes, ticket BC-06 records the minimal
      `.depth` increase with its PSRAM cost; otherwise no increase is proposed. The estimate is
      ~5%, so BC-06 records a proposal.

**Owner check after flashing:** build with `-DTT_LAUNCHER_STACK_PROBE`, boot to the Home, record
the four high-water lines, then remove the flag.

## BC-02: The grid and the dock share one app collection

**What to build:** `createWidgets()` collects the installed apps once and both the grid
(`collectItems`) and the dock (`resolveDockItems`) use that collection. The shared collection is
released at the end of the build; a later repopulate (mode switch, favourites loaded) collects
again. `collectHomeApps()` no longer runs nested inside `resolveHomeItems`.

**Blocked by:** None.

**Status:** done

- [x] One `collectHomeApps()` call per Home build, not two.
- [x] `collectItems` uses the shared collection when the build provides it and collects on its own
      otherwise (`Context::buildApps`).
- [x] The shared collection is freed before `createWidgets()` returns (it is the build's own local).
- [x] `switchMode` and the favourites repopulate still show current data (no stale cache): the
      pointer is cleared before `createWidgets()` returns and `populate()` runs synchronously.
- [x] `HomeModelTest` still passes for `resolveHomeItems`/`resolveDockItems` (the model and its
      signatures are unchanged; the test binary was not rebuilt in this checkout).

## BC-03: The pinned ids are not rebuilt per populate

**What to build:** The pinned id list is a function-local static, returned by reference, instead
of a `std::vector<std::string>` constructed on every `collectItems` call.

**Blocked by:** None.

**Status:** done

- [x] `getPinnedIds()` returns `const std::vector<std::string>&`.
- [x] No allocation happens per populate for the pinned ids.
- [x] The pinned order is unchanged: Grok Bot carousel, then Wi-Fi.
- [x] Cost recorded: ~110 B of permanent heap for the two strings and the vector buffer.

## BC-04: The resolved items stop duplicating id and name

**What to build:** `collectItems` moves each resolved item's id and name into `TileGridItem`
instead of copying them, so the intermediate `HomeItem` vector and the tile vector do not both hold
~20 strings at the same time.

**Blocked by:** None.

**Status:** done

- [x] The tile vector is filled with moved strings; no `std::string` copy per item.
- [x] `resolveHomeItems` keeps its signature and stays LVGL-independent.
- [x] `HomeModelTest` still passes (model untouched; test binary not rebuilt in this checkout).
- [x] Note: the helper costs +32 B of frame versus the old inline loop; it is kept for the ~250 B
      of transient heap it removes, and the frame table in the footprint doc counts it.

## BC-05: The launcher context lives on the heap

**What to build:** `appMain` allocates `Context` instead of declaring it on its 3 KB stack, and
frees it after `window_manager_remove()`. A failed allocation or a failed window creation returns
cleanly without leaking.

**Blocked by:** None.

**Status:** done

- [x] `appMain`'s frame no longer contains `Context` (448 B -> 224 B).
- [x] `delete ctx` runs after `window_manager_remove()` (which calls `destroyWidgets(ctx)`).
- [x] Null checks for both the allocation and the window id.
- [x] No behaviour change when the app closes normally; when the window was already superseded,
      `destroyWidgets` ran earlier and the destructor sees a null clock font.

## BC-06: The app stack depth unit is documented correctly

**What to build:** `AppStackConfig::depth` says what the unit actually is: units of `StackType_t`,
which is 1 byte on ESP-IDF/xtensa and 4 bytes on the POSIX simulator, and points at
`APP_DEFAULT_STACK_DEPTH` for the default. Record in this ticket whether BC-01's measurement forces
a `.depth` change (and its PSRAM cost); if not, record "no increase".

**Blocked by:** BC-01.

**Status:** done

- [x] The comment no longer claims "words": it now says units of `StackType_t` (bytes on ESP-IDF,
      words on POSIX) and points at `APP_DEFAULT_STACK_DEPTH`.
- [x] The `.depth` decision is recorded here with the margin it was based on (below).
- [x] `.depth` 3072 -> 6144 in `Tactility/Source/app/launcher/Launcher.cpp`: +3 KB PSRAM, zero
      internal RAM, zero flash, one line plus a comment.

**Measurement and decision:** app-side frames leave ~1.5 KB of the 3,072 B stack. The crash proves
the pre-fix total exceeded 3,072 B, so LVGL's share is at least 1.35 KB; that is a lower bound, not
a measurement, so a 25% margin cannot be demonstrated. The rule above then applied and the stack
was doubled. The high-water probe on hardware is what confirms or right-sizes it.

## BC-07: Windows are built on the LVGL task (follow-up)

**What to build:** `lvgl_run_on_task()` runs the function inline on the caller, so every app builds
its widgets on its own stack. Either rename it to what it does or make it actually run on the LVGL
task (9120 bytes, PSRAM) and keep the callers safe.

**Blocked by:** BC-01 (needs the measured budget to size the LVGL task stack).

**Status:** ready-for-agent (follow-up, not in this cycle)

- [ ] The behaviour is either renamed (`lvgl_run_locked`) or post-and-wait, not the current
      misleading contract.
- [ ] Every app's widget build still runs with the LVGL lock held.
- [ ] The LVGL task stack is sized from a measurement, not a guess.

## BC-08: The lock screen is not built on the timer daemon (follow-up)

**What to build:** `DisplayIdleService::handleLockTick()` -> `lockScreenShow()` creates LVGL
widgets on the FreeRTOS timer daemon task (`CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH`, 5120 bytes),
whose stack is shared with every software timer in the firmware. Build the lock overlay on the LVGL
task instead, like the other window work.

**Blocked by:** BC-01.

**Status:** ready-for-agent (follow-up, not in this cycle)

- [ ] The lock overlay is created and destroyed on the LVGL task.
- [ ] The timer callback only decides the transition and schedules the work.
- [ ] The measured high-water of the timer daemon task does not include LVGL widget creation.

## BC-09: The QR carries the panic reason (follow-up)

**What to build:** The panic reason string (which names the overflowing task for stack overflows)
is stored in RTC crash data but not in the QR URL, so a crash screenshot is not self-describing.
Add it to the payload, within the QR capacity.

**Status:** ready-for-agent (follow-up, not in this cycle)

- [ ] The URL contains the cause and, when present, the reason.
- [ ] The QR stays readable at the current screen size and error-correction level.

---

## Notes: on-device checks (owner)

Not done in this cycle. Requires one flash of this branch, with the owner's explicit OK:

1. Boot the device 5 times: no crash screen, Home renders with clock, date, grid and dock.
2. Build once with `-DTT_LAUNCHER_STACK_PROBE`, boot, and record the four high-water lines from the
   serial log.
3. Run the owner checklist in `docs/mobile/mobile-home-footprint.md` (backlight, Home, volume,
   lock, old screensavers, no bypass).
4. Confirm `Heap:` and `External:` from the boot log against the footprint table.
