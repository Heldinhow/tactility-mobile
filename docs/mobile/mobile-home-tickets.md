# Tickets: Mobile home

Spec: `docs/mobile/mobile-home-spec.md`
Integration branch: `feat/mobile-home` (origin: `Heldinhow/tactility-mobile`)
Definition of done for every ticket: its acceptance criteria are met and
`source ~/esp/esp-idf-v6.1/export.sh && python3 device.py es3c28p && idf.py build` exits 0.

Status vocabulary: `ready-for-agent` (can start now), `in progress`, `blocked`, `done` (complete on
the branch, build green).

Vertical slices: each ticket below is complete through every layer it touches and verifiable on its
own by a green build, its pure-logic tests, or both. The owner's on-device checks are collected in
the notes at the end; no ticket flashes a device, opens a PR or pushes.

---

## 01: The ES3C28P backlight can be dimmed and switched off

**What to build:** Enable the ES3C28P panel's existing PWM backlight device so the display can be
dimmed and switched off. Boot applies the saved brightness, the quick panel's brightness slider
appears, and the idle service's timeout path becomes active. No driver or kernel API is added.

**Blocked by:** None (can start immediately).

**Status:** ready-for-agent

- [ ] The existing PWM backlight device in the ES3C28P definition is enabled; its pin and PWM
      settings are unchanged.
- [ ] No driver, kernel API or other board's definition changes.
- [ ] The build is green for `es3c28p`.
- [ ] Owner check after flashing: the quick panel shows the brightness slider, brightness changes
      the panel, and the idle timeout switches the panel off and restores it on wake (with Auto
      screen off enabled).

## 02: The quick panel has a volume slider

**What to build:** The quick panel gains a volume row directly below brightness. Each row is a
leading icon plus a slider box that shows its value. The volume row appears only on devices with
audio output hardware, even when output is currently disabled, and uses the existing audio service
so the level is persisted without a new settings key.

**Blocked by:** None (can start immediately).

**Status:** ready-for-agent

- [ ] The volume row appears when output hardware exists and is absent otherwise.
- [ ] The slider ranges from 0 to 100 in steps of 10, starts at the current output volume, shows its
      value, and its `-`/`+` buttons work.
- [ ] A change calls the existing output-volume API directly; no new settings file or key is
      introduced, and the audio service's existing debounce persists it.
- [ ] The brightness row uses the same leading-icon and slider layout; its show/hide rule, immediate
      apply and save-on-close behaviour are unchanged.
- [ ] Volume 0 is the lowest codec level, not a separate mute state.
- [ ] The build is green for `es3c28p`.
- [ ] Owner check after flashing: the slider changes the speaker, the level survives a reboot, and
      a board without audio output has no volume row.

## 03: The Home replaces the launcher UI with a clock, grid and Settings entry

**What to build:** A build-time flag switches the launcher to the Home presentation, and the
ES3C28P enables it. The Home keeps the launcher app identity and the global statusbar, and shows a
large clock and date header, a three-column paged icon grid with labels and page dots, and a
Settings tile that opens the existing settings mode (relabelled Apps). The old bottom bar and page
arrows are gone. The clock uses the existing large text font for now; the dedicated clock font is
ticket 07. The time/date formatter is pure and tested.

**Blocked by:** None (can start immediately).

**Status:** ready-for-agent

- [ ] `TT_MOBILE_HOME` exists, defaults off, and is enabled for `es3c28p` through that device's
      configuration; the flag-off build keeps the old launcher.
- [ ] With the flag on, the manifest is named `Home`, the app keeps its identity and starts at boot
      as before, and the recents entry shows Home.
- [ ] The Home shows the header, grid and page dots; the old bottom bar, page arrows and mode button
      are gone.
- [ ] The grid has three columns at 240x320, icon-and-label tiles, swipe pages, dots only when there
      is more than one page, and working keypad/encoder grid navigation.
- [ ] The header clock is large (the existing large font until 07), the date sits below it, the
      24-hour setting is respected, 12-hour format shows AM/PM, and the pre-2025 placeholder is used.
- [ ] The header timer updates on the minute and is deleted when the window manager destroys the
      widgets.
- [ ] The Settings tile is the first item, toggles the existing settings mode, and becomes Apps with
      the apps icon; starting an app still uses the existing queued path.
- [ ] Pure time/date formatter tests cover the placeholder, 24-hour and 12-hour rendering, midnight
      and noon, a single-digit minute, and the short date.
- [ ] The build is green with the flag on and off.
- [ ] Owner check after flashing: the Home fits 240x320, swipe/dots/settings work, the existing
      home/back/recents gestures still work, and the page and mode survive a raise.

## 04: The Home pins Settings, Grok Bot and Wi-Fi, then the rest

**What to build:** The Home's app grid is ordered by a pure resolver: the Settings tile first, the
Grok Bot carousel when it is installed and compatible, then Wi-Fi, then every other app with
favourites first and names alphabetical. Settings-category apps stay behind the Settings tile.
Grok Bot uses the shared `FORUM` icon and disappears cleanly when absent. Long-press favourites and
their badge keep working.

**Blocked by:** 03.

**Status:** ready-for-agent

- [ ] The pinned order is Settings, Grok Bot (when installed and compatible), Wi-Fi; no duplicate
      entries are produced.
- [ ] The remaining apps are filtered to user/system, non-hidden, non-headless, compatible apps,
      sorted with favourites first and then by name; settings-category apps are excluded.
- [ ] Grok Bot appears with the `FORUM` icon and leaves no placeholder when it is absent.
- [ ] Long-press still toggles a favourite and the badge still renders; the favourites file and its
      loading path are unchanged.
- [ ] Pure resolver tests cover the pinned order, a missing or incompatible carousel, hidden,
      headless and settings-category exclusions, favourite sorting, alphabetical order, and the
      Settings sentinel.
- [ ] The build is green for `es3c28p`.
- [ ] Owner check after flashing: the order and badge are visible, and installing or removing the
      carousel changes the pin without breaking the grid.

## 05: The Home has a four-slot dock

**What to build:** A fixed dock sits below the page dots in both Home modes. It offers up to four
icon-only shortcuts in the order Wi-Fi, Settings, Files, Grok Bot; an absent or incompatible app is
omitted without leaving a gap. The Settings slot switches to the settings mode, the others start
their app through the existing queued path, and the table is static for now.

**Blocked by:** 03.

**Status:** ready-for-agent

- [ ] The dock appears in both the apps and settings modes, below the page dots.
- [ ] The order is Wi-Fi, Settings, Files, Grok Bot; missing or incompatible entries are omitted
      with no empty slot, so the dock holds four, three or fewer without gaps.
- [ ] The buttons are icon-only and use the shared icons; Settings toggles the mode and the other
      present entries start their app through the queued path.
- [ ] No new settings file, key or configuration screen is added; the table is static.
- [ ] Pure dock resolver tests cover four entries, three without the carousel, missing Wi-Fi or
      Files, and incompatible ids.
- [ ] The build is green for `es3c28p`.
- [ ] Owner check after flashing: each dock tap works, Settings toggles the mode, and the absent
      carousel leaves three evenly spaced icons.

## 06: The lock screen turns the idle display into a phone-style lock

**What to build:** `Lock` becomes a screensaver type in Display settings. On a fresh install of a
build with the mobile-home flag, it is the default with Auto screen off enabled at 30 s. At the
timeout the backlight goes off, any quick panel or recents overlay is hidden, and no lock widgets
exist. The first touch wakes the display and shows a lock overlay with the large clock (the existing
large text font until 07), date and swipe-up hint; the waking press cannot unlock. A subsequent
swipe up anywhere on the lock surface
unlocks and returns to the previous window. Other gestures and the quick panel cannot bypass the
lock; with a keypad or encoder, any key unlocks. Decorative screensavers and None keep their current
behaviour.

**Blocked by:** 01, 03.

**Status:** ready-for-agent

- [ ] On builds with the flag, `Lock` appears in Display settings as Lock Screen, round-trips
      through the settings file, and leaves the existing types in their current order and meaning.
- [ ] With the flag off, Lock is not offered in Display settings, and a settings file that contains
      it falls back safely instead of entering a broken state.
- [ ] Fresh installs with the flag default to Lock, Auto screen off on and a 30 s timeout; the
      flag-off build and existing saved settings keep their previous defaults.
- [ ] Pure state-machine tests cover boot as unlocked, timeout to locked off, activity to locked on,
      the waking press never arming unlock, an armed swipe unlocking, a second timeout while locked
      on, and no lock at boot.
- [ ] At timeout the backlight goes off, any window-manager overlay is hidden, and no lock widgets
      are allocated.
- [ ] On wake the backlight comes on and the lock overlay is built with the clock, date and swipe
      hint; the waking press cannot unlock.
- [ ] A subsequent swipe up anywhere on the lock surface (at least 40 px, mostly vertical) unlocks;
      home, back, recents, the hold timer and the quick panel are unavailable while locked.
- [ ] With a keypad or encoder present, any key unlocks.
- [ ] Unlocking deletes the overlay, returns to the previous window, restores the backlight if
      needed and resets display activity; the window stack is untouched.
- [ ] A second timeout while locked on deletes the overlay and stays locked.
- [ ] Decorative types and None keep their behaviour; a device without a backlight never enters the
      lock.
- [ ] The build is green with the flag on and off.
- [ ] Owner check after flashing: the 30 s timeout, wake, no one-press unlock, an anywhere swipe
      unlocking, returning to the previous app, no bypass, and any-key unlock.

## 07: The Home and lock clocks get a dedicated large font

**What to build:** A clock-only generated font (digits, colon, hyphen and space) at about 2.5x the
device's base text size, shared by the Home and the lock screen. It is built into the firmware only
for mobile-home builds, participates in the existing font cache and size configuration, and falls
back to the large text font if it cannot be loaded. The date, AM/PM label and swipe hint keep the
normal fonts.

**Blocked by:** 03, 06.

**Status:** ready-for-agent

- [ ] The build generates a clock font with only the clock glyphs, at about 35 px on `es3c28p` and
      the device's bits-per-pixel.
- [ ] The clock font is embedded only when `TT_MOBILE_HOME` is on; the flag-off image does not grow
      by it.
- [ ] The Home and lock clocks use the clock font; the date, AM/PM and hint keep the normal fonts.
- [ ] A missing or failed clock font falls back to the large text font and still renders.
- [ ] Changing the appearance font size regenerates and caches the same glyph subset through the
      existing cache path.
- [ ] The build is green with the flag on and off.
- [ ] Owner check after flashing: the clock is visibly large on both screens, does not clip at
      240x320, and the 12-hour AM/PM label fits.

## 08: Footprint measured and the owner verification checklist recorded

**What to build:** Measure the final feature against the starting build: application image size and
static DRAM/IRAM from the same build's map and binary, plus an estimate of the runtime heap for the
new Home and lock widgets. Record the numbers and the owner's on-device checklist. No push, PR or
merge is part of this ticket.

**Blocked by:** 01–07.

**Status:** ready-for-agent

- [ ] Baseline and after numbers are recorded from the same device build: `Tactility.bin`, `.data`,
      `.bss`, IRAM and DIRAM usage. The bin delta is at most +20 KB, and static DRAM and IRAM each
      grow by 0 B.
- [ ] The runtime heap estimate for the new Home and lock widgets is at most 4 KB steady state, and
      the analysis finds no allocation that outlives the lock overlay or grows per cycle.
- [ ] No new task, thread or dependency is introduced.
- [ ] The flag-off build keeps the old launcher and lock behaviour; the only shared user-visible
      changes are the quick-panel volume row and the enabled ES3C28P backlight.
- [ ] The owner's on-device checklist is recorded: backlight brightness/off, the 30 s timeout, wake
      and lock, no one-press unlock, anywhere swipe, no bypass, any-key unlock, return to the
      previous app, volume persistence, Home layout, and gesture regression.
- [ ] The build is green for `es3c28p`.
- [ ] No push, PR or merge.

---

## Notes on verification

- There is no automated UI harness in this repository, and this workflow does not flash a device.
  "Verified" therefore means the pure-logic tests pass and the device firmware builds and links with
  the feature enabled. The owner flashes the branch and runs the on-device checklist; the feel,
  timing and backlight behaviour are deliberately left for that session.
- The pure seams follow the spec: home item aggregation and dock resolution, the lock state machine
  (including the press-arming rule), and time/date formatting. They sit with the existing doctest
  tests, next to the gesture geometry test, and must compile as a standalone translation unit when
  the full local test target is unavailable.
- The size baseline is the build before ticket 01: `Tactility.bin` 3,885,008 B, static DRAM
  65,007 B, IRAM 16,384 B. The previous gesture feature cost +3,936 B of flash and +32 B of static
  DRAM. The clock font is expected to be the largest single item in this feature (roughly 7–9 KB at
  4 bpp).
- The branch is not pushed. Tickets 01–03 can start in parallel; 04 and 05 follow 03; the lock
  follows the backlight and the Home clock formatter; the clock font follows both consumers; the
  footprint ticket closes the work.
