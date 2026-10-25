# Redesign v2: footprint and verification

Status: implemented on `feat/redesign-v2`; the binary measurements are final, the runtime heap and
the task high-water marks are pending one owner-approved flash. No device was flashed, no branch was
pushed and no PR was opened.

Spec: `docs/mobile/redesign-v2-spec.md`
Tickets: `docs/mobile/redesign-v2-tickets.md`
Screen captures: `docs/mobile/design-checks/`

## Method

- Before: a clean `es3c28p` build of `a5b75fa9` (`feat/mobile-home`), the same device properties and
  `CONFIG_TT_MOBILE_HOME=y`.
- After: the final `feat/redesign-v2` build.
- Both with `source ~/esp/esp-idf-v6.1/export.sh && idf.py build`; the numbers come from
  `Tactility.bin` and `python -m esp_idf_size build/Tactility.map`.
- Screen captures: the simulator at 240x320 RGB565 with the ES3C28P profile
  (`TACTILITY_SIM_SCREENSHOT`, see `docs/mobile/design-checks/README.md`).

## Size

| Metric | Before | After | Delta |
|---|---:|---:|---:|
| `Tactility.bin` | 3,894,864 B | 3,923,024 B | **+28,160 B** |
| Flash `.text` (code) | 2,777,426 B | 2,784,946 B | +7,520 B |
| Flash `.rodata` | 965,216 B | 985,856 B | +20,640 B |
| Static DRAM (`.data` + `.bss`) | 65,039 B | 65,135 B | **+96 B** |
| DIRAM in use | 167,558 B (49.03%) | 167,654 B (49.06%) | +96 B |
| IRAM | 16,384 B (100%) | 16,384 B (100%) | 0 B |

The app partition is 6 MB; the final binary leaves about 2.36 MB free. The `+28,160 B` is 28% of the
`+100 KB` flash cap.

### Budget

- Flash: **met**, +29,104 B against +100 KB.
- IRAM: **met**, 0 B.
- Static DRAM: **missed by 96 B** against the +0 B target. The `.data` section is unchanged; the
  `.bss` growth is the five-card `SystemRecents` overlay state and small counters. The state is
  created at boot and reused; it is not a leak. The alternative is moving the cards to the overlay's
  heap lifetime, which would still leave a small static counter. Recorded as a known cost rather
  than hidden.
- Steady UI heap: **not measured on the device**; the spec's target is at most +6 KB. The Home, the
  lock and the overlays create their widgets when shown and delete them when hidden, so the increase
  is bounded by the visible screen, not by the feature set. The exact figure needs the owner's flash.
- Boot heap (internal and PSRAM): **not measured on the device**. The last captured boot heap on this
  branch family was the boot-crash capture before that fix: internal 126,299 B / 327,359 B and PSRAM
  4,492,232 B / 4,605,168 B. It predates the v2 work and is not a v2 result.

### Where the flash went

- Text fonts: the six Inter roles replace the four Adwaita roles; the net text-font cost is inside
  the `+20,640 B` `.rodata` line.
- Icon masks: 35 A8 masks are read from flash; only the masks the screens reference are linked.
- Code: `+7,520 B` of `.text` for the tokens, the Home, the Settings hub, the lock, the panel, the
  Recents, the dialog, the Wi-Fi/Files/password screens, the keyboard handler and the font/capture
  fixes.

The original mockup estimated about 41-50 KB of assets and did not include all screen code; the
actual is smaller because the linker drops unreferenced data and the Latin-1 range is only used by
the 15 px role.

## Runtime memory and stack

The simulator's heap is the host heap and is not evidence for the device. The device numbers below
need a flash:

- Internal heap and PSRAM free/total at boot and after the Home, Settings, Wi-Fi, Files and Recents
  are opened.
- `uxTaskGetStackHighWaterMark()` for the launcher (6,144 B), the LVGL task (9,120 B), the timer
  daemon (5,120 B) and the Settings, Files and Wi-Fi app tasks (8,192 B default).
- The boot high-water after five consecutive boots.

The temporary probe is not part of the tree. Apply it as:

```cpp
// Temporary: log the calling task's stack high-water (bytes on ESP-IDF) per build stage.
#if defined(TT_STACK_PROBE)
#include <tactility/freertos/task.h>
#define STACK_PROBE(stage) LOG_I(TAG, "stack high-water after %s: %u", stage, \
    static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)))
#else
#define STACK_PROBE(stage)
#endif
```

Call sites: the end of the Home build, the end of the Settings hub, Wi-Fi and Files builds, the end
of each overlay build (panel, Recents, lock, dialog), and `appMain` entry. Build with
`-DTT_STACK_PROBE`, flash, record the lines, then remove the flag. The probe and its numbers are the
only missing part of the footprint account.

The static frame budget from the boot-crash work still applies: no screen build path puts a large
object on the stack; lists are built in loops with fixed buffers; the Home's transient collections
are freed before the build returns.

## Captures

The nine captures are in `docs/mobile/design-checks/`:

| File | Screen | Mockup |
|---|---|---|
| `06-settings.png` | Settings hub | `png/1x/settings.png` |
| `07-home.png` | Home | `png/1x/home.png` |
| `08-lock.png` | Lock | `png/1x/lock.png` |
| `09-panel.png` | Quick panel | `png/1x/quick.png` |
| `10-recents.png` | Recents | `png/1x/recents.png` |
| `11-dialog.png` | Dialog | `png/1x/dialog.png` |
| `12-wifi.png` | Wi-Fi | `png/1x/wifi.png` |
| `13-files.png` | Files | `png/1x/files.png` |
| `14-keyboard.png` | Wi-Fi password + keyboard | `png/1x/keyboard.png` |

Geometry, colours, band positions and type roles match the 1x mockups. The visible differences are:

- **Dynamic content**: the statusbar clock, the Home date, SSIDs, app names and file names differ
  from the mockup capture. This is expected.
- **Simulator audio**: the simulator has no audio service, so the Settings hub has no Sound row and
  shows Appearance as the fifth row. The ES3C28P has the ES8311 and registers the Audio app; on the
  board the five designed rows are Display, Sound, Wi-Fi, Bluetooth and Power.
- **Simulator Wi-Fi mock**: the mock driver reports the radio off to the statusbar and the Settings
  hub while the Wi-Fi app's event-driven card shows the fixture connected. The board's driver state
  is consistent; the owner checklist covers the real status icon and value.
- **Statusbar Wi-Fi glyph**: the statusbar's live status icons, including Wi-Fi, use the existing
  16 px Material statusbar glyphs. The v2 Wi-Fi mask is used on the lock screen and the panel. The
  size and colour match; the glyph shape is the one deviation from the mockup.
- **Keyboard modifiers**: Shift, 123 and space use the letter-key background (`#242A33`); a single
  `lv_buttonmatrix` has no per-key background style. The labels, row geometry, Go accent and the
  other key roles match. Recorded in RV2-15.
- **Home dock**: Grok Bot and Camera are not installed on this unit, so the dock collapses to Files
  and Settings. The resolver omits missing apps without a gap, as designed.
- **Home labels**: long installed app names truncate at 52 px, as designed.
- **Panel values**: the simulator has no dimmable backlight or audio output, so the sliders show 0%.
  The board shows its real brightness and volume.
- **Lock gradient**: the simulator renders the 2-stop gradient smoothly; RGB565 banding is a panel
  check and remains pending.
- **Fonts**: text anti-aliasing and the system clock differ from Chrome; the type sizes, weights and
  baselines are the design's.

## Owner checklist

Before starting: **Display → Orientation = Portrait**. The v2 Home only builds at 240x320.

- [ ] Five consecutive boots reach the Home; no crash screen and no stack-overflow log.
- [ ] Home: clock and date correct; 4x2 grid; page dots only with more than 8 apps; swipe pages; dock
      in the grid's columns; open-app dots; Settings opens the hub; long-press still favourites.
- [ ] Lock: gradient (check banding); clock/date; swipe up unlocks; the waking press does not; any key
      unlocks; no quick panel/Recents over the lock; no lock at boot.
- [ ] Quick panel: slides from the top; brightness and volume sliders live and persist; three tiles;
      swipe up closes; cannot open while locked.
- [ ] Recents: opaque; up to five cards; X closes one; swipe up closes; current app marked; empty
      state; no thumbnails.
- [ ] Settings hub: the five values are current; each row opens its screen; the remaining settings
      scroll; values refresh when returning.
- [ ] Wi-Fi: switch; connected card; signal icons and locks; scanning/empty/off states; connect;
      Forget (disconnect + remove); inline error; the dialog.
- [ ] Files: path; rows with size/item count; footer; menu actions; empty folder; only the list
      scrolls.
- [ ] Keyboard: all four rows; Shift/123/space/delete/Go; no popup; the bottom row taps.
- [ ] Password: empty Join unavailable; dots; static caret; eye toggles; wrong password error;
      success returns to the list; hidden SSID keeps the old form.
- [ ] Gestures: 16 px edges; top panel; bottom Home; hold Recents; left Back.
- [ ] If you approve the probe build: record the boot/steady internal and PSRAM heap and the
      high-water marks above, then remove the probe.

## Code review

### Standards

- **Hard, fixed:** `SettingsHub.cpp` was the one new source file without the
  `// SPDX-License-Identifier: Apache-2.0` header required by `CODING_STYLE_CPP.md`.
- **Hard, fixed:** the temporary `StackProbe.cpp` and its `Init.cpp` call were still in the tree.
  The ticket says the probe is removed before the final commit; removed, and the probe stays as the
  snippet in the runtime section.
- **Judgement:** the toolbar has two navigation APIs, `lvgl_toolbar_set_nav_icon` (icon only, the
  default close callback stays) and `lvgl_toolbar_set_nav_image` (icon and callback). The screens use
  both; one API with an optional callback would be smaller.
- **Judgement:** the Recents overlay keeps its five-card `State` as a static object (the static DRAM
  finding below). Moving it to the overlay's heap lifetime removes it from `.bss` at the cost of an
  allocation per open.
- **Judgement:** Settings, Wi-Fi and Files repeat the row/divider composition locally even though
  `MobileUi` supplies the card, row and label primitives. The repetition is small and the screens
  differ in their trailing widgets; not worth another abstraction now.
- The seven pure test suites were compiled standalone and pass (36 cases, 153 assertions). The full
  repository test target was not run on this checkout; the local FreeRTOS/mbedtls test environment is
  the known-awkward one from the previous retros.

### Spec

- **Missing / partial:** the statusbar's Wi-Fi indicator is the existing Material statusbar glyph,
  not the v2 `wifis` mask. The mask is used on the lock screen and the panel. This is the only
  visible icon-shape deviation from the mockup.
- **Budget:** static DRAM grew instead of staying flat (exact delta in the table above; the Recents
  overlay state is the main contributor). Flash and IRAM meet their caps.
- **Deviation accepted in RV2-15:** the keyboard's Shift, 123 and space keys use the letter-key
  background; a single `lv_buttonmatrix` cannot give per-key backgrounds. The labels, geometry,
  modes and Go accent match.
- **Pending the owner's flash:** the runtime heap (internal and PSRAM) and the task high-water
  marks. The firmware must not be flashed without the owner's OK; the probe snippet above is the
  complete procedure.
- **Simulator fixture differences, not product defects:** no audio service (so no Sound row in the
  Settings capture), and the mock Wi-Fi driver reports the radio off to the statusbar and the
  Settings hub while the Wi-Fi app's event-driven card shows the fixture connected. The ES3C28P's
  audio and Wi-Fi drivers are consistent; the owner checklist verifies them.
- **Scope:** the only simulator-only additions are the F1-F9 navigation aid and the capture hooks;
  the probe was removed. No firmware feature outside the spec was added.

## Follow-ups

- Move the Recents overlay state to the overlay's heap lifetime (recover the 128 B of static DRAM).
- Add the statusbar Wi-Fi mask path so the last Material Wi-Fi glyph matches the v2 set.
- Deep redesign of the settings screens that were not drawn and the internals of the remaining apps.
- Default Portrait orientation on the first boot of the ES3C28P.
- Run the probe build after the owner's flash and append the runtime heap/high-water table here.
