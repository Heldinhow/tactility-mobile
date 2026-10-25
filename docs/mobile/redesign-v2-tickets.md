# Tickets: Redesign v2

Spec: `docs/mobile/redesign-v2-spec.md`
Integration branch: `feat/redesign-v2`
Design source: `design/redesign-mockups` at `5e5e06d0`, `docs/design-v2/`
Device: `es3c28p` (ESP32-S3, 240x320 portrait)

Definition of done for every ticket: its acceptance criteria are met and
`source ~/esp/esp-idf-v6.1/export.sh && python3 device.py es3c28p && idf.py build` exits 0.
The device must boot after every ticket. The v2 Home cutover is RV2-07; the lock cutover is
RV2-08/09.

Status vocabulary: `ready-for-agent` (can start now), `in progress`, `blocked`, `done`
(complete on the branch, build green).

No ticket flashes a device, opens a PR, pushes or merges. The hardware checks are collected in
RV2-17 and are the owner's call.

Approved test seams (spec `Testing Decisions`):

1. Home resolver (existing, extended).
2. Mobile formatting (new pure module: Settings values, file meta/footer, RSSI buckets).
3. Presentation guard (new pure function).
4. App icon table (new pure function).

Seam 4 is implemented in RV2-03, seam 2 in RV2-06, and seams 1 and 3 in RV2-07.

Footprint figures below are expectations, not measurements. RV2-17 records the measured numbers.
The expected total is about +80 KB of flash (assets about 40 KB, code about 40 KB) against the
+100 KB cap, 0 B of static DRAM, no IRAM change and at most +6 KB of steady UI heap.

## Before any hardware check

Set **Display → Orientation = Portrait** first. The v2 Home is guarded to 240x320; in landscape the
legacy launcher is shown and the Home comparison does not apply.

---

## RV2-01: A 240x320 simulator for screen comparison

**What to build:** A development-only simulator profile at 240x320 (RGB565) with the v2 flag on, plus
a repeatable way to capture the active screen to PNG. This is the verification loop for every screen
ticket; it does not ship in firmware.

**Blocked by:** None (can start immediately).

**Status:** done

**Test seam:** None (developer tool). The acceptance is that the simulator renders the real UI at
240x320 and that the screens can be captured.

**Expected footprint:** 0 bytes of firmware.

- [ ] The simulator builds and runs on macOS with the v2 flag on.
- [ ] A 240x320 RGB565 profile is available and selected for the comparisons.
- [ ] The current Home, lock, quick panel, Recents, Wi-Fi, Files and a dialog can be captured to PNG
      at 1:1 through `lv_snapshot`.
- [ ] The captures are stored outside the firmware tree or in an ignored folder; nothing is linked
      into the firmware.
- [ ] No product source changes beyond the development profile.

---

## RV2-02: Inter at six fixed type sizes

**What to build:** A v2 build replaces Adwaita at the UI layer with Inter 400/500 at the six sizes of
the design (12, 13, 14, 15, 22, 48), generated at build time from committed subsets: 12/13/14/22
ASCII, 15 Latin-1, 48 digits/colon/hyphen/space. The clock becomes 48 px. The legacy roles map
small=12, default=15, large=22 so every existing screen adopts the scale. The Appearance font-size
row disappears on v2 builds; non-v2 builds keep the 10–32 scaling path.

**Blocked by:** None (can start immediately).

**Status:** done

**Test seam:** No pure seam. Asset audit: the generated font table contains exactly the six roles
and the clock subset's codepoints. The visual check is the 240x320 simulator from RV2-01.

**Expected footprint:** about +20 KB flash (fonts in `.rodata`), 0 B static DRAM, 0 IRAM, 0 steady
heap. Fonts are opened in place from flash.

- [ ] Inter 400 and 500 sources and subsets are committed with the OFL notice; the font version
      header changes so the device deletes its stale cached fonts.
- [ ] A build generates exactly the six v2 text fonts plus the 48 px clock font; runtime generation
      is not used on this device.
- [ ] The clock is generated at 48 px (not 2.5x the default size) for the Home and the lock.
- [ ] small/default/large map to 12/15/22; the older 11/14/18/35 set is no longer generated for the
      v2 build.
- [ ] Clock digits are tabular in the simulator (compare 1/7/9 and a minute rollover); if they are
      not, the clock subset is regenerated from Inter's tabular figures.
- [ ] The Appearance font-size row is hidden on v2 builds; the 10–32 setting path still exists for
      builds without the flag.
- [ ] The build is green and five existing screens are not clipped at 240x320.

---

## RV2-03: The v2 icon set as A8 masks

**What to build:** The 27 mockup icons as A8 alpha masks at 24 and 16 px, plus eight 24 px glyphs in
the same style for the Settings rows that exist on this board (Appearance, Location, Time & Date,
USB, Touch Calibration, Development, Kernel) and System Info. A named icon enum, a lookup from app id
to icon and a generic fallback replace the Material glyph on the v2 surfaces. Masks are recoloured
at draw time.

**Blocked by:** None (can start immediately).

**Status:** done

**Test seam:** Seam 4, app icon table: a pure function from app id to icon, with the eight mockup
apps, the settings apps, a third-party app and an unknown id.

**Expected footprint:** about +20 KB flash (27 base masks ≈ 15 KB, extras ≈ 5 KB), 0 B static DRAM,
0 IRAM; the masks are read from flash.

- [ ] 23 masks at 24x24 and 4 at 16x16, alpha only, with the real stroke of 2 px at both sizes.
- [ ] An asset check verifies each mask's dimensions and that the set contains no A4 image.
- [ ] Recolour works for text, text-2 and accent (verified when the status bar uses the 16 px Wi-Fi
      mask in RV2-05).
- [ ] The app id table covers the eight mockup apps and the listed settings rows; unknown ids get the
      fallback; unit tests cover the table.
- [ ] The extra glyphs are only for the listed rows and stay within 5 KB; if not, the remainder uses
      the current Material icon and the report records it.
- [ ] Material Symbols remains for non-v2 builds, the status icons other than Wi-Fi, and app
      internals.
- [ ] The mask sources are committed and the regeneration step is documented; the build takes no new
      dependency.

---

## RV2-04: The v2 palette, tokens and a truthful Appearance

**What to build:** A token module with the v2 palette, radii, bands and component sizes in raw pixels.
The system theme is forced to the v2 palette at boot (dark, primary `#4C8DFF`, surface `#0E1116`).
The Appearance app hides theme mode, mono, theme colours and system bars on v2 builds, and keeps
density and animations. The vendored theme is not edited.

**Blocked by:** RV2-01, RV2-02.

**Status:** done

**Test seam:** None (visual consistency). Simulator screenshots of Appearance, Display and Files.

**Expected footprint:** about +1–2 KB flash; 0 B static DRAM; no new heap.

- [ ] The palette, radii, bands and sizes live as constants in one module; no v2 screen uses DPX or a
      font-size multiple.
- [ ] The boot theme is dark with primary `#4C8DFF` and surface `#0E1116`; an existing app's primary
      button and surface match the palette.
- [ ] Appearance hides theme mode, mono, theme colours and system bars on v2 builds; density and
      animations remain and still work.
- [ ] The font-size row is already hidden (RV2-02); no control remains that silently does nothing.
- [ ] Non-v2 builds keep the old theme and every control.
- [ ] Build green; the three screenshots are compared.

---

## RV2-05: The v2 chrome and shared primitives

**What to build:** The status bar at 24 px (time 13 at the left, live status icons 16 at the right),
the toolbar at 44 px (24–68) with a 22 px title and 44x44 targets, 40 px buttons with radius 8 and
the primary/secondary roles, the shared card/row/gesture-bar primitives, the 60% scrim for overlays,
16 px edge strips and the top-edge quick panel in the top layout. Every app inherits the chrome.

**Blocked by:** RV2-01, RV2-03, RV2-04.

**Status:** done

**Test seam:** None (layout). Simulator screenshots of five existing apps plus the v1 Home; device
spot check.

**Expected footprint:** about +3–5 KB flash; +1–2 KB heap only while an overlay is open; 0 static
DRAM.

- [ ] Status bar 0–24: time 13 at the left, active status icons 16 at the right, 12 px padding; the
      Wi-Fi indicator uses the v2 mask and the other status icons keep the Material status font.
- [ ] Toolbar 24–68 with a 44x44 nav target, a 22 px title and 44x44 actions; content starts at 68.
- [ ] Buttons are 40 px, radius 8; primary accent/on-accent, secondary surface-2/text; pressed states
      are the colour step; focus is the 2 px accent outline offset by 2 px.
- [ ] Card, row and gesture-bar primitives match the spec and are reusable by the later screens.
- [ ] The three overlays use one full-screen 60% scrim; no other transparency is added.
- [ ] Edge strips are 16 px; the top-edge quick panel opens in the top layout; the lock still blocks
      it.
- [ ] Five existing apps and the v1 Home are not clipped or overlapping; screenshots recorded.

---

## RV2-06: The Settings hub

**What to build:** A new visible system app, "Settings" (`tactility.settings`), listing every visible
settings-category app plus a Power row: the five designed rows first with live values (Display %,
Sound %, Wi-Fi SSID/Off/Not connected, Bluetooth On/Off, Power timeout) and the rest alphabetically.
The card is 216x220 at (12,76) with internal scroll and an auto-hidden scrollbar; values refresh on
show; rows open their targets; Power opens Display. No polling or listeners.

**Blocked by:** RV2-01, RV2-03, RV2-04, RV2-05.

**Status:** done

**Test seam:** Seam 2, mobile formatting: a pure module for the five value strings and their
documented edge cases, unit-tested with doctest.

**Expected footprint:** about +4–6 KB flash; ~1 KB heap while the hub is open; 0 static DRAM.

- [ ] The hub is registered as a visible system app and opens from the app list; it does not list
      itself.
- [ ] The five designed rows show the live values with the exact strings; each value refreshes when
      the hub is shown again and never on a timer.
- [ ] Display/Sound/Wi-Fi/Bluetooth/Power open the correct screens (Sound opens Audio; Power opens
      Display).
- [ ] The remaining settings apps (Appearance, Development, Kernel, Location, Time & Date, Touch
      Calibration, USB) appear below, alphabetically.
- [ ] The card is 216x220 at (12,76), scrolls internally, and its scrollbar is invisible at rest.
- [ ] Unit tests cover the value formatters, including "Off", "Not connected", "Never" and "30 s".
- [ ] Build green; the simulator capture matches png 05 for the five rows.

---

## RV2-07: The Home at 240x320

**What to build:** Replace the v1 mobile Home with the v2 Home: clock 48 at (12,28), the date block at
the right, the 4x60 grid of 44 px tiles with 12/16 labels, eight slots per page with dots in the
22 px gap, the 224x56 dock in the same columns with open-app dots, and the new resolver (Settings hub
first, then Grok, then Wi-Fi, favourites-first, no modes). The Home is guarded to 240x320 portrait;
elsewhere the legacy layout appears inside the v2 chrome. Delete the v1 mobile header, three-column
grid and mode wiring.

**Blocked by:** RV2-01, RV2-05, RV2-06.

**Status:** done

**Test seam:** Seam 1 (Home resolver, extended) and seam 3 (presentation guard), both pure and
unit-tested.

**Expected footprint:** about +4–6 KB flash; ~1.5 KB steady heap for the Home widgets; the
transient collections stay freed before the build returns; 0 static DRAM.

- [ ] At 240x320 portrait the Home matches png 01: clock, date, 4x2 grid at y=86, dock at y=244,
      nothing below y=300.
- [ ] Tiles are 60x64 with a 44x44 surface, a 24 px icon and a 12/16 label capped at 52 px.
- [ ] Eight slots per page; dots only when there is more; swipe changes page; the page survives a
      raise; no vertical scroll.
- [ ] The dock is Grok Bot, Files, Camera, Settings, omitting absent apps; open-app dots read the
      window stack on raise; no poll.
- [ ] Settings opens the hub; long-press still favourites; no badge is drawn.
- [ ] Resolver tests cover the new order, more than eight items, the dock table and the absence of
      modes; guard tests cover 240x320, 320x240 and another resolution.
- [ ] Landscape or another resolution shows the legacy launcher inside the v2 chrome.
- [ ] The v1 mobile Home code and its mode wiring are deleted; the non-mobile launcher is untouched.
- [ ] Build green; the simulator capture matches png 01; the owner check starts with Portrait.

---

## RV2-08: Build the lock on the LVGL task (BC-08)

**What to build:** The lock overlay is created and destroyed on the LVGL task; the idle timer callback
only decides the transition and schedules the work. The v1 lock visuals do not change. This removes
widget creation from the 5,120 B timer daemon stack.

**Blocked by:** RV2-01, RV2-02.

**Status:** ready-for-agent

**Test seam:** No pure seam; the high-water probe of the timer daemon and the LVGL task.

**Expected footprint:** about +0.3 KB flash; no RAM or DRAM change.

- [ ] The overlay is created and destroyed on the LVGL task; the timer callback only schedules.
- [ ] Lock, unlock, no-lock-at-boot, timeout-while-locked and screen-off behave as before.
- [ ] The timer daemon's high-water no longer includes widget creation; the LVGL task's reading is
      recorded.
- [ ] The lock still blocks the quick panel and Recents.
- [ ] Build green; owner check on the device.

---

## RV2-09: The lock screen

**What to build:** The v2 lock: a two-stop gradient from `bg` at 0–45% to `accent-soft`, no status
bar, a 16 px Wi-Fi icon when the radio is on, the 48 px clock at y=100, the date 15 at y=154, the
swipe caption at y=276 and the 36x4 gesture bar at y=302. Behaviour is unchanged.

**Blocked by:** RV2-01, RV2-03, RV2-04, RV2-05, RV2-08.

**Status:** ready-for-agent

**Test seam:** No pure seam; the simulator capture and the on-device banding check.

**Expected footprint:** about +1–2 KB flash; ~1 KB heap while visible; the gradient cache is about
0.6 KB only on the lock.

- [ ] The simulator capture matches png 02 (gradient, clock, date, caption, bar; no status bar).
- [ ] The two-stop gradient uses no image and no other transparency; banding is checked on the panel;
      if it bands, the shortened bottom-third gradient is applied and recorded.
- [ ] Swipe up, any key and the waking press behave exactly as before.
- [ ] No quick panel, Recents or app action can appear over the lock.
- [ ] The overlay is freed when the screen goes off and rebuilt on wake; no lock at boot.
- [ ] Build green; owner check.

---

## RV2-10: The quick panel

**What to build:** The v2 panel: 240x230 at the top (built at y=-12, height 242), 60% scrim, time 22
and date 13, brightness and volume rows with 6 px tracks and 20 px thumbs, three 68x52 tiles
(Wi-Fi/Bluetooth/Recents) with 14 px labels, the gesture bar at y=218, open 160 ms and close 120 ms.
No notifications, no power/location/mesh tiles; persistence is unchanged.

**Blocked by:** RV2-01, RV2-03, RV2-04, RV2-05.

**Status:** ready-for-agent

**Test seam:** No pure seam; the simulator capture plus the owner checklist.

**Expected footprint:** about +2–3 KB flash; ~2 KB heap while open.

- [ ] The capture matches png 03 (panel, scrim, sliders, tiles, bar; rounded bottom only).
- [ ] The sliders show their values and change brightness/volume; brightness saves on close.
- [ ] Wi-Fi and Bluetooth toggle their services and reflect state; Recents opens the switcher.
- [ ] Swipe up closes; back/Escape closes without a pointer; no extra tile is drawn.
- [ ] 160/120 ms; the app behind is not redrawn; animations off makes it instant.
- [ ] The panel refuses to open while locked.
- [ ] Build green; owner check.

---

## RV2-11: Recents

**What to build:** The opaque Recents screen: header 40 px at y=28, list from y=76, up to five 208x44
cards with 4 px gaps, a 24 px icon, a 15 px name, a 16 px X in a 40x44 target and a 3x20 active bar.
Tap raises, X closes, swipe up closes, 120 ms animation, empty state, no thumbnails. It covers the
status bar.

**Blocked by:** RV2-01, RV2-03, RV2-04, RV2-05.

**Status:** ready-for-agent

**Test seam:** No pure seam; the simulator capture plus the owner checklist.

**Expected footprint:** about +2–3 KB flash; ~1.5 KB heap while open.

- [ ] The capture matches png 04; nothing from the app behind is visible.
- [ ] Up to five cards; the active window is marked; no thumbnails.
- [ ] Tap raises; the X closes only that app; swipe up closes; 120 ms; cards below move up 48 px.
- [ ] Empty state shows "No open apps".
- [ ] Back/Escape closes the screen without a pointer.
- [ ] Build green; owner check.

---

## RV2-12: The shared dialog

**What to build:** The shared alert dialog becomes the bottom-anchored v2 card: 216 wide, 12 px side
margins, 16 px padding, radius 12, a 1 px line border, title 22, body 15, buttons 40 with an 8 px gap
(Cancel surface-2, confirm accent) and the 60% scrim. It grows upward and never cuts the background
in half. Input dialogs keep their layout and inherit chrome, buttons and type.

**Blocked by:** RV2-01, RV2-04, RV2-05.

**Status:** ready-for-agent

**Test seam:** No pure seam; a simulator capture of an existing dialog.

**Expected footprint:** about +1 KB flash; no measurable steady heap change.

- [ ] The capture matches png 08 using an existing dialog (for example the Wi-Fi AP forget or the
      Bluetooth peer forget confirmation).
- [ ] Buttons are at thumb height; the background stays visible and readable above the card.
- [ ] Cancel is secondary, confirm is accent; destructive actions are accent, not red.
- [ ] Every app using the shared alert dialog gets the style on v2 builds.
- [ ] Input dialogs inherit chrome, buttons and type and still return their text.
- [ ] Build green.

---

## RV2-13: The Wi-Fi screen and Forget

**What to build:** The v2 Wi-Fi screen: a 36x20 switch in a 44 target, the connected card 216x56 in
`accent-soft` with an `accent-line` border, the caption, the list card with signal icons and locks,
the scanning/empty/off states, and the forget flow (tap the connected card, confirm in the dialog,
disconnect with auto-connect suppressed, remove the saved settings). Signal buckets are pure. Tapping
a protected network still opens the existing connect form until RV2-16.

**Blocked by:** RV2-01, RV2-03, RV2-04, RV2-05, RV2-06, RV2-12.

**Status:** ready-for-agent

**Test seam:** Seam 2 (mobile formatting), extended with the SSID state and the RSSI buckets.

**Expected footprint:** about +4–6 KB flash; ~1.5 KB heap while open.

- [ ] The capture matches png 06 (switch, connected card, caption, rows, lock icons).
- [ ] The switch toggles the radio; the connected card names the network; the list scrolls inside a
      fixed 132 px viewport (three rows) and the current network stays fixed.
- [ ] Signal buckets: at least -60 three bars, at least -75 two, otherwise one; unit tests.
- [ ] Scanning shows "Scanning…"; empty shows "No networks found"; off shows "Wi-Fi is off".
- [ ] Tapping the connected card opens the forget dialog; confirming disconnects and removes the
      saved settings; errors show inline; the card stays visible behind the scrim.
- [ ] Protected networks open the existing connect form (replaced by RV2-16).
- [ ] Build green; owner check.

---

## RV2-14: Files as a list

**What to build:** The mobile Files screen becomes a list: toolbar back/title/menu, path 13 at
(16,76), rows 44 from y=96 to 288 with a 24 px icon, a 15 px name and a 13 px meta, dividers from
x=52, a fixed footer at 288 with "N items · M folders", an empty state, and the existing actions in
the menu. Only the list scrolls. Non-mobile keeps the grid.

**Blocked by:** RV2-01, RV2-03, RV2-04, RV2-05, RV2-06.

**Status:** ready-for-agent

**Test seam:** Seam 2 (mobile formatting), extended with the file meta and the footer count (singular
and plural).

**Expected footprint:** about +4–6 KB flash; ~1–2 KB heap while open.

- [ ] The capture matches png 07 (path, four visible rows, footer, menu).
- [ ] Rows show a size for files and an item count for folders; the footer counts items and folders
      with correct singulars.
- [ ] The menu holds the existing actions and they still work.
- [ ] Only the list scrolls; the toolbar and footer stay fixed; an empty folder shows its state.
- [ ] The non-mobile Files view is unchanged.
- [ ] Unit tests cover the meta and footer formatters.
- [ ] Build green; owner check.

---

## RV2-15: The v2 software keyboard

**What to build:** A single global software keyboard: four rows of 40 px keys with 1 px gaps, radius
8, 14/16 labels, no popup, modifier colours, Go in accent, layers lower/upper/123 and a 140 ms slide.
It replaces the stock keyboard on v2 builds and stays hidden when a hardware keyboard is present.

**Blocked by:** RV2-01, RV2-03, RV2-04.

**Status:** ready-for-agent

**Test seam:** No pure seam; a simulator capture with a focused text field, plus the owner checklist.

**Expected footprint:** about +2–3 KB flash (map data in `.rodata`); ~1.5 KB heap while open.

- [ ] Any text field (for example the Wi-Fi hidden-network form or a Files rename) shows the v2
      keyboard.
- [ ] Four rows, 1 px gaps, 40 px keys, radius 8, 14/16 letters, modifiers and Go in the spec colours.
- [ ] Shift, 123, space, delete and Go work; Go sends Enter; no popup bubble.
- [ ] 140 ms slide; hidden on blur; a hardware keyboard still suppresses it.
- [ ] The last row's taps work at the screen edge.
- [ ] Build green; owner check.

---

## RV2-16: The Wi-Fi password screen

**What to build:** For a protected network, replace the generic form with the v2 password screen:
SSID title 22, field 160x40 at (12,76) with an accent focus border, 15 px dots, a static 2x18 caret,
an eye in a 40x40 target, Join 48x40 (unavailable while empty), the WPA2 caption at y=124 and a 13 px
error line; the v2 keyboard fills y=152–320. Hidden SSIDs keep the existing form.

**Blocked by:** RV2-01, RV2-13, RV2-15.

**Status:** ready-for-agent

**Test seam:** No new pure seam; the existing formatting seam and the simulator capture.

**Expected footprint:** about +1–2 KB flash; no measurable steady heap change.

- [ ] The capture matches png 09 (field, dots, caret, eye, Join, caption, keyboard).
- [ ] The eye toggles visibility; Join is unavailable while empty and sends the password.
- [ ] A wrong password keeps the screen and shows the error line; success returns to the list.
- [ ] The v2 keyboard is the one used; hidden-SSID entry still uses the old form.
- [ ] Build green; owner check.

---

## RV2-17: Verify, measure and document

**What to build:** The final integration pass: capture the nine screens at 240x320 and compare them
with `png/1x`; record every deviation; rebuild the baseline and the final firmware with the same
method and produce the footprint report; run the stack probe and remove it; write the owner checklist
(Portrait first) and the follow-ups.

**Blocked by:** RV2-01 through RV2-16.

**Status:** ready-for-agent

**Test seam:** All four pure suites, the simulator captures and the owner checklist.

**Expected footprint:** records the measured total; the target is at most +100 KB flash, 0 B static
DRAM, IRAM unchanged, at most +6 KB steady UI heap and at most +2 KB internal heap.

- [ ] Nine captures at 240x320 (Home, lock, panel, Recents, Settings, Wi-Fi, Files, dialog, password
      keyboard) compared with `png/1x`; deviations listed and either fixed or accepted in writing.
- [ ] Baseline and final builds use the same command; the report lists `Tactility.bin`, `.text`,
      `.rodata`, static DRAM, DIRAM, IRAM, and the boot heap (internal and PSRAM free/total).
- [ ] High-water marks recorded for the launcher, LVGL task, timer daemon, Settings, Files and Wi-Fi;
      the probe is removed before the final commit.
- [ ] The budget is met; if not, the cut (extra glyphs, assets) is recorded.
- [ ] The owner checklist covers Portrait, backlight, lock/gradient, panel, Recents, Settings values,
      Wi-Fi connect/forget, Files, keyboard, gestures and five consecutive boots without a crash.
- [ ] `docs/mobile/redesign-v2-footprint.md` is written and the spec status is updated.
- [ ] No flash, push, PR or merge.

---

## Follow-ups (not in this batch)

- Default Portrait orientation on the first boot of the ES3C28P. Today the v2 Home needs a one-time
  Display change; the guard falls back to the legacy launcher until it is made.
- Deep redesign of the settings screens that were not drawn (Display, Audio, Bluetooth, Power,
  Appearance, Location, Time & Date, USB, Touch Calibration, Development, Kernel), plus the internals
  of the other apps beyond chrome, palette and type.
- Everything the spec lists as out of scope: notifications, secure lock, wallpapers, Recents
  thumbnails, landscape v2, other boards and upstream layouts.
