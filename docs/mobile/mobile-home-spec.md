# Mobile home: phone-style launcher, quick-panel volume and lock screen

Status: ready-for-agent (branch `feat/mobile-home`, device `es3c28p`, 240x320)

## Problem Statement

On a touch device, Tactility still presents the launcher as a utility screen: a paged app grid,
a bottom bar with page arrows and a separate settings mode, and no clock, date or dock. It works,
but it does not read as the home screen of a handheld. The quick panel has brightness but no volume,
so changing the volume means leaving the panel and opening the Audio settings app.

The only device this work targets, the ES3C28P, also leaves its panel backlight disabled in the
device tree. The display is therefore always lit: the quick panel correctly hides the brightness
slider, and the idle service has no way to dim or switch the screen off. The existing screen-timeout
settings do nothing on this board, and there is no moment where the device is deliberately asleep,
so there is no wake step and no at-a-glance clock.

The result is a handheld that behaves like a development board: no volume where sound is adjusted,
no real screen-off, no lock screen, and a launcher that does not match the way the device is held
and used.

## Solution

The launcher becomes a phone-style **Home**: the existing statusbar stays at the top, followed by a
large clock and date, a three-column paged app grid with labels and page dots, and a fixed bottom
dock of up to four shortcuts. The grid is ordered with Settings, the Grok Bot carousel (when installed)
and Wi-Fi first, then the remaining apps alphabetically. Settings is the existing settings mode,
shown in place of the app grid; the Settings tile becomes Apps. The existing favourite long-press
and badge behaviour is preserved.

The quick panel gains a volume slider directly under brightness. Both controls are rows with a
small icon and a slider that shows its value. The volume row appears only on devices with audio
output hardware and uses the existing audio service, so the chosen level survives a reboot without
a new settings file or key.

The ES3C28P backlight is enabled in the device tree. That makes the existing brightness control and
screen timeout real, and it is the mechanism the lock screen relies on.

The lock screen is a new **screensaver type** in Display settings, so all idle-screen behaviour is
chosen in one place. On a fresh install of a build with the mobile-home feature, the default is Lock
with a 30 s timeout. At the timeout the backlight goes off and no lock widgets exist. The first
touch wakes the display and shows the lock screen: a large clock, the date, and a swipe-up hint.
A swipe up anywhere on the lock surface unlocks and returns to the previous screen. There is no PIN,
no lock at boot, and no other system gesture or quick-panel entry can bypass the lock. Decorative
screensavers remain available for anyone who selects one instead.

The whole Home and lock presentation is behind a Kconfig flag, default off, and is enabled for the
ES3C28P by that device's configuration. Builds without the flag keep the existing launcher and idle
behaviour. The quick-panel volume control is independent of the flag and applies to every device
with audio output hardware.

## User Stories

1. As a touch-device user, I want the device to start on a home screen with the time and date, so
   that I can see when it is without opening an app.
2. As a touch-device user, I want app icons in a labelled grid, so that I can recognise apps at a
   glance.
3. As a touch-device user, I want to swipe left and right between home pages, so that I can reach
   more apps without scrolling.
4. As a touch-device user, I want page dots below the grid, so that I know when there is more than
   one page.
5. As a touch-device user, I want Settings to be the first icon, so that I can find it without
   searching.
6. As a user with the Grok Bot carousel installed, I want it next to Settings, so that the app this
   device is built around is one tap away.
7. As a user without the Grok Bot carousel, I want the home screen to look complete without it, so
   that a missing optional app leaves no gap or placeholder.
8. As a touch-device user, I want Wi-Fi in the pinned group, so that I can manage networks quickly.
9. As a touch-device user, I want every other installed app in alphabetical order, so that I can
   predict where it is.
10. As a touch-device user, I want the Settings icon to open the settings list, so that I do not
    need a separate settings app.
11. As a touch-device user, I want the Settings icon to become Apps while the settings list is
    shown, so that going back is obvious.
12. As a touch-device user, I want a fixed dock at the bottom, so that my most-used apps are always
    in the same place.
13. As a touch-device user, I want the dock to omit an app that is not installed, so that it never
    shows an empty or broken slot.
14. As a touch-device user, I want a dock tap to start the app, so that it behaves like the grid.
15. As a touch-device user, I want a long press on a grid icon to keep marking an app as a
    favourite, so that the existing behaviour is not lost.
16. As a touch-device user, I want the existing statusbar with time, Wi-Fi and battery, so that
    system status remains visible on the Home.
17. As a touch-device user, I want the large clock to follow the system 12/24-hour setting, so that
    every clock on the device agrees.
18. As a touch-device user, I want the date in a short readable form, so that it does not crowd the
    240x320 screen.
19. As a touch-device user, I want the home, back and recents gestures to keep working from the
    Home, so that navigation does not regress.
20. As a touch-device user, I want going home to keep my apps running, so that returning to one
    does not lose my place.
21. As a touch-device user, I want the Home to be the default launcher at boot, so that I do not
    have to configure anything.
22. As a user, I want the Home to use the same app identity as the launcher, so that boot, the home
    gesture and the recents switcher keep working without a second concept.
23. As a touch-device user, I want a volume slider in the quick panel, so that I can adjust sound
    without opening the Audio settings app.
24. As a touch-device user, I want the volume slider directly under brightness, so that the panel
    reads as display and then sound.
25. As a touch-device user, I want the slider to show its current value, so that I can see the
    level I am choosing.
26. As a touch-device user, I want the volume I chose to survive a reboot, so that I do not have to
    set it again.
27. As a user of a device without audio output, I want no volume row, so that I never see a control
    that cannot do anything.
28. As a touch-device user, I want the panel to show the volume that is already set when I open it,
    so that the slider does not lie about the current level.
29. As a user with a dimmable backlight, I want brightness and volume rows to look consistent, so
    that the panel is easy to scan.
30. As a touch-device user, I want volume zero to mean silence without a separate mute state, so
    that raising the slider brings the sound back.
31. As an ES3C28P owner, I want the panel backlight to be controlled by the firmware, so that
    brightness and screen-off work on my device.
32. As an ES3C28P owner, I want the brightness slider to appear in the quick panel, so that I can
    dim the screen without opening Display settings.
33. As a touch-device user, I want the screen to turn off after 30 seconds of inactivity on a fresh
    install, so that the device behaves like a phone and saves power.
34. As a touch-device user, I want the first touch after screen-off to wake the display and show the
    lock screen, so that I do not unlock by accident while only meaning to wake it.
35. As a touch-device user, I want a large clock and the date on the lock screen, so that I can
    check the time at a glance.
36. As a touch-device user, I want a swipe-up hint on the lock screen, so that I know how to
    unlock.
37. As a touch-device user, I want to swipe up anywhere on the lock surface, so that unlocking is
    easy with one hand.
38. As a touch-device user, I want unlocking to return me to the app I was using, so that I do not
    lose my context.
39. As a touch-device user, I want no lock screen at boot, so that startup still goes straight to
    the Home.
40. As a touch-device user, I want the quick panel, recents and home/back gestures to be
    unavailable while locked, so that the lock cannot be bypassed.
41. As a touch-device user, I want the lock widgets to be deleted while the screen is off, so that a
    sleeping device uses no CPU or memory on a lock screen nobody can see.
42. As a user of a keypad or encoder device, I want any key to unlock, so that I cannot get trapped
    on the lock screen.
43. As a touch-device user, I want the decorative screensavers to remain available, so that I can
    choose one instead of the lock screen.
44. As a touch-device user, I want the lock screen to be a choice in the existing Display settings,
    so that there is one place for idle-screen behaviour.
45. As an existing user, I want my saved display settings to be respected, so that upgrading does
    not silently change my timeout or screensaver.
46. As a touch-device user, I want no PIN, password or pattern, so that unlocking stays quick.
47. As a user of another board, I want my launcher and idle behaviour unchanged when the feature is
    off, so that the fork does not impose this UI on every device.
48. As a device integrator, I want to enable the Home and lock at build time, so that the code is
    absent from firmware that does not need it.
49. As a device integrator, I want the feature to stay within +20 KB of flash and add no static DRAM
    or IRAM, so that the firmware stays predictable.
50. As a developer, I want the lock state machine, home ordering and time formatting to be testable
    without a display, so that their edge cases stay covered as the UI changes.
51. As a developer, I want a missing clock font to fall back to the large text font, so that the
    Home and lock screen never fail to render.
52. As a user, I want the Home clock to advance on the minute without polling, so that it is
    accurate and cheap.

## Implementation Decisions

### Feature flag and home identity

- The Home is the existing `tactility.launcher` app in its mobile-home presentation, not a new app.
  The app id, the boot default and the home gesture target stay unchanged; the recents entry keeps
  the same app identity and is labelled Home when the flag is on.
- A new Kconfig option `TT_MOBILE_HOME` selects the presentation. It defaults to off for upstream
  builds and is enabled for the ES3C28P through that device's configuration properties, so
  `device.py es3c28p` regenerates the build with it on. No manual `sdkconfig` edit is part of the
  workflow.
- With the flag on, the manifest name is `Home`; the recents switcher therefore shows "Home". With
  the flag off, the manifest name stays `Launcher` and the existing UI, idle behaviour, screensaver
  list and font set are unchanged.
- The old launcher's `Apps` and `Settings` modes remain the two contents of the Home. The mode and
  page survive a raise exactly as they do today; the widgets are rebuilt through the launcher's
  existing `create_widgets` callback.
- The quick-panel volume slider is not behind this flag: it is available on every device with
  audio output hardware.

### Home layout

- The app area is a column: header, grid that grows, page dots, dock. The global statusbar is not
  part of the Home and is not modified; it remains above the app area.
- The header has a large centred clock and a smaller date below it. Before the clock is set, both
  show the same placeholder rule the statusbar and quick panel use (a year before 2025 means the
  time is not meaningful). With 12-hour format, a small `AM`/`PM` label sits next to the clock.
- The header timer is a one-shot timer rearmed to the next minute, so the clock changes on the
  minute boundary instead of polling. The date label is only rewritten when the day changes. The
  timer is created with the Home widgets and deleted when the window manager destroys them, so no
  timer runs while another app is showing.
- The grid reuses the existing paged tile grid in its no-arrow form: three columns at 240 px,
  icon above label, swipe navigation, centred page dots that hide when there is one page, and the
  existing keypad/encoder grid navigation. The page arrows and the old bottom bar are not shown.
- The dock is a visually distinct row using the existing theme surface/card treatment; no new image
  assets are introduced. It is icon-only.
- No new window, app or service is created for the Home; it remains one resident app whose widgets
  are rebuilt on raise.

### Home content and order

- The grid is ordered by a pure resolver:
  1. the Settings tile (a mode switch, not an app);
  2. the Grok Bot carousel, only when it is installed and compatible with the device;
  3. Wi-Fi (`tactility.wifimanage`);
  4. every other app that the current launcher shows, sorted with favourites first and then by
     name.
- Settings-category apps are not in the main grid; they remain the content of the Settings mode.
  The current filters stay: user and system categories only, no hidden or headless manifests, and
  no packages the device cannot run.
- The Settings tile is not launched as an app. It toggles the existing mode in place and relabels
  itself to Apps with the apps icon; the existing icon-colour behaviour for the two modes is kept.
- The Grok Bot carousel appears with the shared `FORUM` icon. When it is absent, the resolver simply
  omits it; nothing else moves into a placeholder.
- Long-press on a grid tile still toggles the existing favourite state and the badge still renders.
  The favourites file and its loading path are unchanged.
- Starting an app still goes through the launcher's existing queued file-operations path, so app
  loading never blocks the LVGL task.

### Dock

- The dock has four fixed slots, in this order: Wi-Fi (`tactility.wifimanage`), Settings
  (the settings mode), Files (`tactility.files`), Grok Bot carousel
  (`one.grokbot.carousel`).
- An app id that is not installed or not compatible is omitted; the remaining icons keep their
  order and no empty slot is drawn. Absent the carousel, the dock is the first three.
- The Settings slot is a mode switch like the Settings tile. The other three start their app from
  the same queued path the grid uses. The dock is visible in both the apps and settings modes.
- The dock is deliberately not configurable in this work: it is a static table, not a new
  favourites file or a settings screen. A later change can replace the table without changing the
  Home's structure.

### Quick-panel volume

- The quick panel's card gains a volume row after the brightness row. Each control is a horizontal
  row with a small leading icon (lightbulb for brightness, music note for volume) and a slider box
  that fills the rest of the row. The slider box already shows its value and has `-`/`+` buttons.
- The volume row is shown when the audio service reports output hardware, even if output is
  currently disabled. The brightness row keeps its current rule: only when the backlight is
  dimmable.
- The slider range is 0..100 in steps of 10, matching the Audio settings app. Its initial value is
  the current output volume. Each change calls the existing output-volume API directly on the LVGL
  task, exactly as the Audio settings app does. No audio event subscription is added; reopening the
  panel reads the current value.
- Volume 0 is the lowest codec level, not a separate mute state. Muting and input controls stay in
  the Audio settings app.
- Persistence is already handled by the audio service's debounce: the slider writes no file and
  adds no settings key. When no stream is open, the codec layer caches the level and applies it when
  a stream opens, so adjusting before playback works.
- The panel's close path and its brightness-save-on-close behaviour are unchanged.

### Backlight on the ES3C28P

- The existing PWM backlight node for the ES3C28P is enabled in its device tree; no driver or
  kernel API is added. The node drives GPIO45, whose hardware documentation describes it as the
  backlight FET gate with PWM support.
- Enabling it makes the existing brightness slider appear (the backlight now reports a dimmable
  0..255 range), makes the boot-time saved brightness apply, and gives the idle service a real
  backlight to turn off and restore.
- No display sleep/wake command is added; screen-off remains a backlight operation. Other boards
  are untouched by this change.

### Idle lock

- `Lock` is appended to the screensaver types (before the count sentinel), with the display name
  `Lock Screen`. The Display settings list and the property-file serialisation learn the new value.
  The existing values keep their order and meaning.
- On builds with `TT_MOBILE_HOME`, a fresh install defaults to `Lock`, enables auto screen-off and
  sets the timeout to 30 s. Builds without the flag, and existing saved settings, keep the previous
  defaults. There is no migration of an existing display settings file; the owner selects Lock and
  enables Auto screen off once after flashing.
- The idle service owns the lock state. It is a small state machine with three states:
  - **Unlocked**: normal operation; the backlight may be on or off according to the timeout.
  - **Locked off**: the screen has timed out; the backlight is off and no lock widgets exist.
  - **Locked on**: the screen has been woken while locked; the lock overlay is visible.
- Events and transitions:
  - Boot starts **Unlocked**. There is no lock at boot.
  - **Unlocked + Timeout** hides any window-manager overlay (quick panel or recents), turns the
    backlight off and enters **Locked off**. No lock widgets are created.
  - **Locked off + Activity** turns the backlight on and enters **Locked on**, building the lock
    overlay. The press that caused the activity does not arm unlocking.
  - **Locked on + a press that starts on the visible lock** arms unlocking. A release that is a
    mostly vertical upward swipe of at least 40 px unlocks.
  - **Locked on + Timeout** turns the backlight off again, deletes the lock widgets and returns to
    **Locked off**.
  - **Unlock** deletes the lock widgets, clears the locked state, restores the backlight if needed,
    resets display activity and leaves the window stack untouched, so the previous app is shown.
- The arming rule is part of the state machine: a gesture whose press began before the lock became
  visible can never unlock. This covers the waking press and a press that started while the screen
  was still off.
- The lock overlay is a top-layer overlay, not a window-manager overlay: it must cover the
  statusbar and any window-manager overlay, and it must not use the overlay backdrop's
  tap-outside-to-dismiss or ESC-to-dismiss behaviour. It is opaque in the theme's background
  colour and accepts no click that dismisses it.
- Content: a large centred clock, the date below it, and a bottom hint (an upward chevron with
  "Swipe up"). No status icons, no PIN field, no dismiss button. The clock uses the same formatter
  and font as the Home. A minute timer runs only while the overlay is visible and is deleted with
  the overlay.
- Gestures: while locked, the system gesture layer treats a swipe up anywhere on the lock surface
  as unlock and ignores home, back, recents and the hold-to-recents timer. The statusbar cannot be
  reached for the quick panel because the top layer covers it. With a keypad or encoder present,
  any key unlocks, so the lock cannot trap a device without a pointer.
- The lock state machine runs only when the selected type is `Lock`. The existing decorative
  screensaver types and `None` keep their current behaviour: `None` still switches the backlight
  off at the timeout, and the animated types keep their five-minute backlight-off phase. `Lock`
  also switches the backlight off at the timeout, but shows the lock screen on wake. On a device
  without a backlight the idle path does not run, so `Lock` never appears and the display settings
  row stays hidden as today.

### Clock font and time formatting

- A new generated text font contains only the glyphs the clock needs: digits, colon, hyphen and
  space. It is about 2.5x the device's base text size (35 px on the ES3C28P), uses the same
  TrueType source and bits-per-pixel as the other generated fonts, and is built into the firmware
  only for `TT_MOBILE_HOME` builds. This keeps other devices' flash unchanged.
- The clock font participates in the existing font cache and size configuration, so changing the
  appearance font size regenerates it (with the same glyph subset) and caches it like the other
  fonts. If it cannot be loaded, the clock falls back to the existing large text font and still
  renders.
- The date, the AM/PM label and the swipe hint use the normal text fonts; only the clock glyphs are
  large. The placeholder uses the same glyphs (`--:--`).
- Time formatting: local time, the existing 24-hour setting, a leading-zero-free hour in 24-hour
  mode (`9:05`), a correct 12-hour clock (midnight is 12, noon is 12) with an AM/PM label. Date:
  `%a %d %b` (for example `Sat 24 Oct`), which is short enough for 240 px. Before the clock is set,
  the clock shows the placeholder and the date is blank or a placeholder. The same formatter is
  used on the Home and the lock screen.

### Footprint and configuration

- No new dependency, task, thread or service. The Home is one resident app that already exists.
  The lock state lives in the existing idle service, whose idle tick already runs (50 ms by
  default).
- Static tables hold the pinned ids and the up-to-four dock slots. The lock overlay is a small
  fixed set of widgets; it is freed when the screen goes off and when the device unlocks. No
  per-frame allocations or polling are introduced. The Home's minute timer exists only while its
  widgets do.
- Budget: at most +20 KB in `Tactility.bin`, 0 B of additional static DRAM and 0 B of additional
  IRAM, and at most 4 KB of steady-state runtime heap for the new Home and lock widgets beyond the
  existing launcher grid. The clock font is expected to be the largest single item (roughly 7–9 KB
  at 4 bpp).
- Measurement plan: record the baseline from the current build's `build/Tactility.bin` and
  `build/Tactility.map` (bin size, `.data`, `.bss` and the IRAM/DIRAM region usage), then repeat
  after the change on the same device build. Estimate the runtime heap from the new object count
  (header labels, dock buttons, lock labels and hint, page dots) rather than guessing; if the owner
  provides a device run, check that the estimate holds and that repeated lock/unlock cycles do not
  grow the heap.
- The current baseline for comparison is 3,885,008 B `Tactility.bin`, 65,007 B static DRAM and
  16,384 B IRAM; the previous gesture feature cost +3,936 B of flash and +32 B of static DRAM, so
  this feature's larger UI has a materially larger, but bounded, budget.

## Testing Decisions

Good tests exercise externally visible behaviour through the smallest pure seam, not LVGL widget
internals, object counts or private state. The feature has three pure seams, all of which already
have a home in the existing doctest suite:

- **Home item aggregation and dock resolution.** A pure resolver takes app descriptors (id, name,
  category, flags), the incompatible ids, the favourite ids and the current mode, and returns the
  ordered grid items and the dock entries. Tests cover: the pinned order; a missing or incompatible
  carousel; hidden, headless and settings-category apps; alphabetical ordering; favourites first
  among the remaining apps; the Settings sentinel; and a dock that collapses from four to three
  without a gap.
- **Lock state machine.** A pure reducer takes a state and an event (boot, timeout, activity,
  unlock, timeout while locked on, press/release with the arming rule) and returns the next state
  and the required effects (backlight on/off, overlay show/hide, idle reset). Tests cover every
  transition, the no-lock-at-boot rule, the waking press never arming an unlock, a swipe that starts
  before the lock is visible never unlocking, and the screen-off case deleting the overlay.
- **Time and date formatting.** Pure formatters take a broken-down time and the 24-hour flag.
  Tests cover the pre-2025 placeholder, 24-hour and 12-hour rendering, midnight and noon, a single
  digit minute, and the short date string.

The prior art is the existing gesture geometry test: a pure header function tested with doctest
next to the system-bars layout tests. The repository's full local test target is still awkward to
run on a stock macOS checkout (see the previous retro); the new tests must be compilable as a
standalone translation unit with doctest, as the gesture test was.

What is not unit tested, because no harness exists for LVGL interaction: the grid layout and swipes,
the dock appearance, the lock overlay layering, the backlight/wake timing, the quick-panel slider's
effect on the codec, and the gestures themselves. Those are verified on the device by the owner
after flashing: backlight brightness and off, the 30 s timeout, wake showing the lock, the waking
press not unlocking, an anywhere swipe unlocking, returning to the previous app, no bypass through
the statusbar/quick panel/recents, any-key unlock, volume changing the codec and surviving a reboot,
the Home at 240x320, and no regression in the existing home/back/recents gestures.

Build verification for every ticket is the same as the previous mobile work:
`source ~/esp/esp-idf-v6.1/export.sh && python3 device.py es3c28p && idf.py build`, which must exit
0. The size and static-memory numbers come from the same build's map and binary, not from a second
`idf.py size` run. No flashing, PRs or merges are part of this workflow.

## Glossary

**Home**: The default screen of the device: the large clock and date, the paged app grid, and the
dock.
_Avoid_: Launcher, desktop, start screen

**Dock**: The fixed row of always-visible app shortcuts at the bottom of the Home.
_Avoid_: Favourites, pinned apps, taskbar

**Lock screen**: The overlay shown when the screen is woken after a locked timeout, dismissed by a
swipe up to return to the previous screen.
_Avoid_: Screensaver, login, PIN screen

**Quick panel**: The sheet that slides down from the statusbar and holds the clock, system toggles,
brightness, volume and recents.
_Avoid_: Notification shade, control centre

**Launcher (legacy mode)**: The launcher app's original presentation: the paged grid with the bottom
bar, page arrows and the Settings mode. The app identity is the same as the Home; only the
presentation differs.
_Avoid_: Home, old app

**Screensaver type**: The choice in Display settings for what the display shows when it idles. Lock
is one type; the others are decorative, and None is a plain black screen.
_Avoid_: Lock screen (unless referring to the Lock type)

## ADR Candidates

These are the decisions worth recording as ADRs when the implementation is split from the spec. Each
is hard to reverse, surprising without context, and the result of a real alternative.

1. **Enable the ES3C28P panel backlight.** The device tree shipped the PWM backlight node disabled,
   so the panel was always lit and the idle service could neither dim nor switch it off. The Home
   and lock work needs a real screen-off and a brightness control, so the existing node is enabled
   on GPIO45 instead of adding a display sleep/wake API or faking screen-off with a black overlay.
   The hardware references describe GPIO45 as the backlight FET gate with PWM support; this still
   has to be confirmed on the device after flashing.

2. **The Home lives in the launcher app.** The Home keeps the launcher's app identity and stays
   behind a build flag. Boot, the home gesture and the recents switcher already treat that identity
   as home; a new app id would have to be threaded through all of them and through boot
   configuration. One app also keeps the old launcher available in builds where the flag is off.

3. **The lock screen is a screensaver type.** Lock is added to the existing idle-screen choices
   rather than getting its own switch, so there is one place to decide what happens when the display
   idles. A separate switch would need a second control and a precedence rule against the
   screensaver. Lock differs from the decorative types in timing: the backlight goes off at the
   timeout rather than after the five-minute animation phase, and waking shows the lock instead of
   dismissing the idle visual.

## Out of Scope

- Notifications, notification badges or a notification list in the quick panel.
- Changing app autostart, boot configuration or the default app beyond the launcher's existing
  behaviour.
- PIN, password, pattern or biometric unlock.
- Configuring, reordering or persisting the dock; this work uses a static default table.
- Home-screen widgets, folders, custom pages or live app content.
- Moving or renaming the launcher app identity, or maintaining two separate home apps.
- Redesigning the statusbar or removing the duplicate small time from the Home.
- Display sleep/wake through ILI9341 commands or any new panel power API.
- Audio input, mute or output-enable controls in the quick panel.
- Hardware backlight calibration, current-limit changes or other board tuning.
- Upstream pull requests, merges or flashing in this workflow.

## Further Notes

- The backlight node was disabled in the upstream device definition, and the previous mobile work
  recorded the missing brightness row and dead timeout as a known board limitation. This spec
  deliberately reverses that limitation because the lock screen cannot exist without a real
  screen-off.
- Existing display settings are authoritative. If the device already has a saved display settings
  file, the new defaults do not apply; the owner must select Lock and enable Auto screen off once
  after flashing. On a device with no saved file, the new defaults apply. The spec does not migrate
  or rewrite user settings.
- The statusbar only shows a Wi-Fi icon while the radio is on; the Home does not add a second
  Wi-Fi indicator. The quick panel remains the place that names the connected network.
- When the Grok Bot carousel is not installed, the dock is three slots and the grid simply omits
  the pinned carousel entry. Nothing in the Home depends on its presence.
- The launcher stays resident and its widgets are rebuilt when it is raised, as they are today; the
  page, mode and favourites survive because they live in the app's context, not in the widgets.
- The quick-panel volume control is not part of the build flag. Any device with audio output
  hardware gets it, which keeps the fork's change to the shared quick panel small and generally
  useful.
- All new source files carry the repository's SPDX license header; no dependency or license change
  is introduced. The feature is additive and leaves upstream-compatible builds without the flag
  functionally unchanged (apart from the volume row).
- The size and heap numbers are recorded in the implementation tickets at the start and end of the
  work, against the baseline above, so the +20 KB budget is checked with the same method the
  gesture feature used.
