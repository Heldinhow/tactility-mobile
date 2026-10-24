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
