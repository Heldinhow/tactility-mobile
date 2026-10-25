# Mobile home: footprint

Status: implemented on `feat/mobile-home`, device `es3c28p`, build with
`CONFIG_TT_MOBILE_HOME=y`.

## Method

- Baseline: the pre-work build of the same `es3c28p` configuration (`9bf61d2`), recorded before
  ticket 01. The gesture review's table and the current build agree on 3,885,008 B for the binary.
- After: the final `CONFIG_TT_MOBILE_HOME=y` build on this branch.
- Build: `source ~/esp/esp-idf-v6.1/export.sh && python3 device.py es3c28p && idf.py build`.
- Sizes: `Tactility.bin` from the build directory and `python -m esp_idf_size build/Tactility.map`
  (no second `idf.py size` run, which would rebuild).
- The flagged-off build was measured separately to confirm that the Home, lock and clock font are
  not in it.

## Size

| Metric | Before | After | Delta |
|---|---:|---:|---:|
| `Tactility.bin` | 3,885,008 B | 3,894,512 B | **+9,504 B** |
| Flash `.text` (code) | 2,766,982 B | 2,777,186 B | +10,204 B |
| Flash `.rodata` | 961,872 B | 965,104 B | +3,232 B |
| Static DRAM (`.data` + `.bss`) | 65,007 B | 65,007 B | **0 B** |
| DIRAM in use | 167,494 B (49.01%) | 167,526 B (49.02%) | +32 B |
| IRAM | 16,384 B (100%) | 16,384 B (100%) | 0 B |

The clock font is 2,828 B of that `.rodata` (`text_clock_35_4bpp_53b8b701.bin`; digits, colon,
hyphen and space only). The remaining flash growth is the Home, dock, lock and resolver code.

The flagged-off build is 3,885,424 B, i.e. 416 B above the same baseline: the quick-panel volume
row and the enabled ES3C28P backlight are shared changes, not part of `TT_MOBILE_HOME`. Its
generated fonts contain no clock font.

### Budget

- Flash: **+9,504 B** against a +20 KB cap. Met.
- IRAM: **0 B**. Met.
- Static DRAM: **0 B** against a 0 B target. Met.

The clock font is no longer a static font-registry slot: the Home and lock screen load it on
demand and own it for as long as their widgets exist. The lock overlay state is a member of the
idle service (which is itself heap-allocated), so the lock pointer is not static either.

## Runtime heap estimate

The estimate is for the new widgets and their LVGL allocations, against the old launcher's
bottom bar. Sizes are from the final ELF (`sizeof(lv_obj_t)` 56 B, `lv_obj_spec_attr_t` 56 B,
`lv_event_dsc_t` 12 B, `lv_timer_t` 28 B, `lv_group_t` 32 B).

| Item | Estimate |
|---|---:|
| New Home widgets (header, labels, dock card, buttons, icons) | ~1.5 KB |
| Old launcher bottom bar removed (bar, page buttons, mode button) | ~-0.6 KB |
| **Home net** | **~0.9 KB** |
| Lock overlay while visible (root, labels, hint, timer, group) | ~0.9 KB |
| `LockWidgets` allocation owned by the idle service | ~0.1 KB |
| Clock font loaded for the Home | ~0.2 KB |
| Clock font loaded while the lock is visible | ~0.2 KB |
| **Steady Home + lock widget total** | **~2.3 KB** |

The two model vectors used to build one page (`collectHomeApps` and `resolveHomeItems`) are
transient: with about 20 installed apps they add roughly 3.2 KB while the grid repopulates and are
freed immediately afterwards. They are not steady-state; the launcher grid's own item vector
already existed. The lock overlay is freed when the screen goes off and rebuilt on wake, so the
per-cycle allocations do not accumulate.

The steady-state target of 4 KB is met (~2.3 KB). The transient rebuild peak is not part of that
target; if it matters, the resolver can fill the grid vector directly in a follow-up.

## Verification

- `idf.py build` green with `CONFIG_TT_MOBILE_HOME=y`.
- `idf.py build` green with the flag removed (old launcher, no lock, no clock font).
- Pure tests, compiled standalone with doctest:
  - `HomeClockTest`: 4 cases, 19 assertions.
  - `HomeModelTest`: 10 cases, 31 assertions.
  - `LockStateTest`: 8 cases, 35 assertions.
  - Total: 22 cases, 85 assertions, all passing.
- No device was flashed in this workflow. The owner runs the checklist below after flashing.

### Owner checklist

- Backlight: brightness slider appears; dragging dims the panel; Auto screen off switches the
  panel off; wake restores the configured brightness.
- Home: fits 240x320; clock and date readable; three labelled columns; swipe and page dots; Settings
  opens the settings list and becomes Apps; pinned Settings/Grok/Wi-Fi order; no gap without the
  carousel; dock taps and Settings toggle; long-press favourite and badge still work; home/back/
  recents gestures unchanged.
- Volume: slider changes the speaker; value survives a reboot; absent on a board without output
  hardware.
- Lock: set Lock + Auto screen off + 30 s in Display settings (fresh installs have this already);
  first touch wakes and shows the lock; that press does not unlock; a second swipe up anywhere
  unlocks; returns to the previous app; quick panel, recents and home/back are unreachable while
  locked; any key unlocks on a keypad/encoder device; no lock at boot; overlay gone while the screen
  is off, rebuilt on wake; a second timeout while locked turns the panel off again.
- Existing decorative screensavers and None still work when selected instead of Lock.
- No bypass: statusbar quick panel and recents cannot be pulled over the lock; unlocking leaves the
  window stack untouched.

## Boot crash fix (a504731f)

Spec: `docs/mobile/boot-crash-spec.md`. Tickets: `docs/mobile/boot-crash-tickets.md`.

The Home build runs on the launcher app task, not on the LVGL task: `lvgl_run_on_task()` only takes
the LVGL lock and runs inline (`Modules/lvgl-module/source/arch/lvgl_esp32.c:46`). That task's
manifest asks for `.depth = 3072`, and on ESP-IDF `StackType_t` is one byte, so its stack is 3 KB,
not the 12 KB the "words" comment implied. The Home's header, clock font, dock and model pushed
that path over the limit on every boot.

### Size (same build method as above)

| Metric | Before (a504731f) | After | Delta |
|---|---:|---:|---:|
| `Tactility.bin` | 3,894,512 B | 3,894,864 B | **+352 B** |
| Flash `.text` (code) | 2,777,186 B | 2,777,422 B | +236 B |
| Flash `.rodata` | 965,104 B | 965,216 B | +112 B |
| Static DRAM (`.data` + `.bss`) | 65,007 B | 65,039 B | **+32 B** |
| DIRAM in use | 167,526 B | 167,558 B | +32 B |
| IRAM | 16,384 B | 16,384 B | 0 B |

The +32 B of static DRAM is the function-local `std::vector<std::string>` object for the pinned ids
(24 B) plus its guard/alignment.

### Runtime memory

| Item | Delta |
|---|---:|
| `Context` moved from the app stack to the heap | +~400 B internal heap, permanent while the launcher runs |
| Pinned id strings kept in a function-local static | +~110 B heap, permanent |
| One `collectHomeApps()` per Home build instead of two | −~3 KB of transient heap churn and one manifest/package scan per build |
| Resolved ids and names moved into `TileGridItem` | −~250 B of transient heap (no duplicate strings) |

### Stack

Frames measured from the ELF (`entry a1`), deepest Home-build chain, app side only:

| B (before) | B (after) | Function |
|---:|---:|---|
| 448 | 224 | `appMain` (`Context` no longer on the stack) |
| 112 | 112 | `window_manager_create_ext` |
| 48 | 48 | `build_window_widget` |
| 32 | 32 | `build_window_widget_locked` |
| 32 | 32 | `lvgl_run_on_task` |
| 48 | 48 | `createWidgets` |
| 160 | 160 | `TileGrid::populate` |
| 128 | 48 | `collectItems` |
| — | 112 | `resolveHomeTiles` (new helper) |
| 112 | 112 | `resolveHomeItems` |
| ~96 | 48 | `getPinnedIds` (no allocation) |
| ~500 | ~500 | `std::sort`/heap machinery, ≈4-5 levels |
| **~1,716** | **~1,514** | **app-side total (−202 B)** |

The remaining chain is LVGL object creation, whose frames this binutils cannot decode past
`lv_obj_create` (the disassembly comes back empty for that section), so it is not in the table. The
crash only proves the pre-fix total exceeded 3,072 B, i.e. LVGL contributed at least 1.35 KB; it
gives no upper bound. Since a 25% margin cannot be shown from that lower bound, BC-06 raised
`.depth` to 6,144 B (+3 KB PSRAM, zero internal RAM, zero flash) instead of trusting it. The
on-device high-water reading is what turns that into a measured number.

### Boot heap

From the crash capture (`/tmp/serial-crash.log`, 4,178 ms, before the fix):

| | Available | Total |
|---|---:|---:|
| Internal | 126,299 B | 327,359 B |
| PSRAM | 4,492,232 B | 4,605,168 B |

The after values need a flash of this branch. The expected internal difference is +~510 B: the
Context (+~400 B) and the pinned ids (+~110 B) now live on the heap.
