# Redesign v2: the mobile UI at 240x320

Status: ready-for-agent (branch `feat/redesign-v2`, device `es3c28p`, 240x320 portrait)

Design source: branch `design/redesign-mockups`, commit `5e5e06d0`, `docs/design-v2/`
(`SPEC.md`, `src/index.html`, `src/styles.css`, `png/1x/*.png`, `png/2x/*.png`, `checks.json`).
On this machine that branch is checked out at `/Users/helder/esp32/tactility-mobile-design`.

The mockup is the source of truth for pixels, type and colour. Where the SPEC prose and the mockup
CSS disagree, the CSS wins and the difference is recorded in Further Notes. This spec replaces the
mobile presentation decisions in `docs/mobile/mobile-home-spec.md`; that document's lock state
machine, backlight work, boot-crash findings and footprint method still apply.

## Problem Statement

The ES3C28P is held like a phone, but its interface is still a development-board interface. The Home
is a three-column grid with a centred clock and a mode toggle that switches between apps and a
settings grid. The quick panel is a row of square icon tiles and a slider box with minus and plus
buttons. Recent apps open as a translucent strip of 66%-wide cards. Files shows a grid of large
tiles. Wi-Fi is a stack of stock list rows, and choosing a network opens a generic form rather than
a keyboard screen. Every screen inherits whatever text size and theme colour the user picked, so the
same screen is 11 px on one device and 18 px on another.

The approved v2 mockups define a single quiet visual language: one blue used only for state and the
primary action, Inter at six fixed sizes, line icons with a real 2 px stroke at 24 and 16 px, a
44 px toolbar, and nine specific screens. None of that exists in the firmware. The previous mobile
work also produced two failures that must not repeat: labels and icons drifted from the design
because they were derived from a scalable font size, and the first Home build overflowed the
launcher task's stack and crashed on every boot.

There is also no Settings screen at all: the settings root is a grid of app icons inside the
launcher, so there is nowhere to show current values (brightness, volume, SSID, Bluetooth, screen
timeout) at a glance.

## Solution

The firmware gets the v2 mobile presentation across nine screens: Home, Lock, Quick panel, Recents,
Settings, Wi-Fi, Files, Dialog and the Wi-Fi password keyboard. The presentation is the existing
`CONFIG_TT_MOBILE_HOME` feature, rebuilt rather than extended: the v1 mobile Home, its mode toggle
and its three-column grid are deleted, and the new presentation is the only mobile one behind that
flag.

The design is expressed as a small token layer (palette, radii, bands, type roles, icon sizes) plus
a handful of shared widgets (card, list row, tile, gesture bar, primary/secondary button, switch).
Screens are built from those tokens in raw pixels at 240x320. The system theme is forced to the v2
palette at boot, so apps that are not redesigned no longer disagree with the chrome; their internal
depth is out of scope and becomes a follow-up.

Inter replaces Adwaita in v2 builds at six fixed sizes (12 Regular; 13, 14, 15, 22 and 48 Medium).
The user's font-size setting no longer scales this build: the size is part of the design. Fonts and
icons are generated at build time from committed subsets and live in flash, so they cost no RAM.

The 27 mockup icons become A8 alpha masks at 24 and 16 px, recoloured at draw time, because the
LVGL software renderer has no A4 image path. Every screen is checked against `png/1x` in a 240x320
simulator before anything is flashed; the hardware checklist is the owner's after he approves a
flash.

The v2 Home is guarded: it is active only when the logical display is 240x320 after rotation. On
another resolution or orientation the launcher falls back to the legacy layout inside the v2 chrome,
so the absolute layout can never be stretched.

## User Stories

1. As a handheld user, I want the device to open on a Home with a large clock and the date, so that
   I can read the time at a glance.
2. As a handheld user, I want the clock on the left and the date stacked on its right, so that the
   header uses one line and leaves room for apps.
3. As a handheld user, I want a four-column grid of labelled app icons, so that I can recognise and
   open apps the way I do on a phone.
4. As a handheld user, I want each app's icon on a 44 px rounded surface with a 12 px label under
   it, so that the grid reads as a consistent set rather than as buttons.
5. As a handheld user, I want long app names cut with an ellipsis at a fixed width, so that the
   grid stays aligned.
6. As a handheld user, I want eight apps per page and dots that appear only when there is more, so
   that the Home shows no chrome I do not need.
7. As a handheld user, I want to swipe sideways between pages, so that I can reach every installed
   app without a drawer.
8. As a handheld user, I want my current page preserved when I come back from an app, so that I do
   not lose my place.
9. As a handheld user, I want a dock of four shortcuts in the same columns as the grid, so that the
   most-used apps are always in the same place.
10. As a handheld user, I want a dot under an open app in the dock, so that I can see what is
    running.
11. As a handheld user, I want Settings to be a real app with rows and current values, so that I can
    see the brightness, volume, network and timeout without opening each one.
12. As a handheld user, I want each Settings row to open its screen, so that the hub is a map, not a
    dead summary.
13. As a handheld user, I want the other settings below the first five, so that nothing disappears.
14. As a handheld user, I want the lock screen to be calm and unmistakable, with a clock, a date and
    a swipe hint, so that I know the device is locked and how to leave it.
15. As a handheld user, I want the lock to have the same app behind it when I unlock, so that the
    lock does not reset my work.
16. As a handheld user, I want the waking press not to unlock, so that picking the device up does
    not open apps by accident.
17. As a handheld user, I want any key to unlock on a keypad device, so that the lock never traps
    me.
18. As a handheld user, I want the quick panel to slide from the top with brightness, volume and
    three toggles, so that the controls I use most are one gesture away.
19. As a handheld user, I want the panel to show the value next to each slider, so that I can set a
    level precisely.
20. As a handheld user, I want Wi-Fi, Bluetooth and Recents as large labelled tiles, so that I can
    hit them with a thumb.
21. As a handheld user, I want the panel to close with a swipe up, so that the gesture that opened
    it closes it.
22. As a handheld user, I want Recents as a full screen of rows, so that switching apps does not
    depend on reading a thumbnail.
23. As a handheld user, I want an X on each recent app, so that I can close exactly one app.
24. As a handheld user, I want the current app marked in Recents, so that I know where I am.
25. As a handheld user, I want a swipe up on a recent card to close it, so that the list behaves
    like a phone's task switcher.
26. As a handheld user, I want an empty state when nothing is open, so that the screen is never a
    blank void.
27. As a handheld user, I want the Wi-Fi screen to show the connected network in a highlighted card,
    so that I can see what I am on before the list.
28. As a handheld user, I want available networks with a signal icon, a lock for protected ones and
    a single tap to connect, so that joining is one decision.
29. As a handheld user, I want a scanning state as text, not a spinner, so that the screen does not
    animate for no reason.
30. As a handheld user, I want to forget the current network from its card, so that I can remove
    credentials without a hidden menu.
31. As a handheld user, I want the forget confirmation anchored at the bottom, so that the buttons
    are under my thumb and I can still see the network above.
32. As a handheld user, I want the same dialog style in every app, so that a confirmation looks the
    same wherever it appears.
33. As a handheld user, I want the Wi-Fi password screen to show the network name, a password field,
    an eye and a Join button, so that joining is one screen.
34. As a handheld user, I want a keyboard with letters, shift, delete, numbers, space and Go, so that
    I can type any password without a hardware keyboard.
35. As a handheld user, I want no key-popup bubble, so that typing is snappy on this panel.
36. As a handheld user, I want a wrong password explained in words, so that I know what to do next.
37. As a handheld user, I want Files as a list with names, sizes and item counts, so that I can find
    a file without decoding a grid of icons.
38. As a handheld user, I want the current path above the list and a count of items and folders
    below it, so that I always know where I am.
39. As a handheld user, I want the file actions in a menu, so that the toolbar stays quiet.
40. As a handheld user, I want every app toolbar to be the same 44 px band with a 22 px title, so
    that screens do not jump when I move between apps.
41. As a handheld user, I want the status bar to show the time on the left and live status icons on
    the right, so that I can see the network and the card state without opening anything.
42. As a handheld user, I want the app content to start at the same y on every screen, so that the
    layout feels stable.
43. As a handheld user, I want one blue used only for state and the main action, so that my eye goes
    to the thing that is on.
44. As a handheld user, I want pressed states to be a colour step, not a shadow or a scale, so that
    the panel feels immediate.
45. As a handheld user, I want the same type scale on every screen, so that the interface reads as
    one product.
46. As a handheld user, I want tap targets of 40–44 px wherever the design allows (the 23 px keys
    are the known exception), so that I stop missing buttons.
47. As a handheld user, I want the light-on-dark contrast of the mockup, so that the panel is
    readable outdoors and indoors.
48. As a handheld user with the 12-hour setting, I want AM/PM next to the clock, so that I do not
    have to convert the time.
49. As a keypad user, I want the grid to keep arrow-key navigation and a visible focus ring, so that
    the redesign does not lock me out.
50. As a user who turned animations off, I want every transition to become instant, so that the
    setting is honoured.
51. As a user who set a favourite app, I want the favourite to keep its ordering priority, so that
    my habits survive the redesign.
52. As a user who rotates the display, I want the legacy launcher instead of a broken layout, so
    that the device stays usable outside portrait 240x320.
53. As a user of a non-redesigned settings app, I want it to open with the new toolbar and palette,
    so that it does not look like a different operating system.
54. As a maintainer, I want one token module and one icon set, so that a future screen cannot drift
    from the design.
55. As a maintainer, I want the v1 mobile code deleted rather than kept behind a second flag, so
    that there is only one mobile presentation to reason about.
56. As a maintainer, I want fonts and icons generated at build time from committed sources, so that
    the build stays reproducible and offline.
57. As a maintainer, I want the Home build path to leave a measured 25% stack margin, so that the
    boot crash cannot return through a bigger Home.
58. As a maintainer, I want the lock built on the LVGL task, so that it stops sharing the timer
    daemon's stack.
59. As a maintainer, I want a flash/RAM report before and after, so that the redesign's cost is
    known rather than assumed.
60. As a maintainer, I want every screen compared against `png/1x` in the simulator, so that
    typography and geometry errors are found before a flash.

## Implementation Decisions

### Feature flag, guard and identity

- `CONFIG_TT_MOBILE_HOME` stays the only flag; the ES3C28P keeps enabling it through its device
  properties. Builds without the flag are unchanged, including their fonts, theme and launcher.
- A single runtime predicate decides whether the **v2 Home** is active: the flag is on **and** the
  logical display is 240x320 after rotation. It is evaluated when the launcher builds its widgets
  (after the saved rotation is applied) and re-evaluated after a rotation change.
- A v2 build always uses the v2 tokens, type scale, palette and chrome (status bar, toolbar,
  buttons); that is what makes the non-designed apps consistent. Only the Home layout is absolute,
  so only the Home falls back: on another resolution or orientation the legacy launcher layout is
  shown inside the v2 chrome, with the v2 type scale.
- The type scale and palette of a v2 build are fixed for the whole build, even if the orientation
  guard falls back to the legacy layout. A v2 build does not carry both the old 11/14/18/35 fonts
  and the new six roles.
- The Home keeps the launcher app identity and remains the boot app and the home-gesture target.
  The new Settings hub is a separate visible system app.
- The v1 mobile code is deleted: the centred header, three-column grid, Apps/Settings mode toggle,
  mode tile and mode-aware resolver are gone. The non-mobile launcher path is untouched.

### Design tokens and shared widgets

- A mobile design module owns the palette, radii, bands and component sizes as constant values in
  raw pixels. Nothing in the v2 screens is expressed in DPX or in a multiple of a font size.
- Palette: `bg #0E1116`, `surface #1A1F27`, `surface-2 #242A33`, `line #242A33`,
  `line-soft #1F2530`, `text #E8ECF1`, `text-2 #9AA4B2`, `text-3 #8A94A3`, `accent #4C8DFF`,
  `accent-text #9CC0FF`, `accent-soft #1A2B45`, `accent-line #2B4C7E`, `on-accent #0B1118`.
- Radii: 12 for cards, dock, list cards, panel, dialog and the connected network; 8 for buttons,
  fields, keys and panel tiles; 10 for switch and slider thumb; 2 for the gesture bar.
- Bands: status bar 0–24, toolbar 24–68, content starts at 68; cards and free text use a 12 px side
  margin, full-width lists use 16 px for text.
- Component sizes: toolbar action 44x44; list row 44; grid cell 60x64 with a 44x44 surface; dock
  224x56 at (8,244); panel tile 68x52 with a 14 px label; dialog button 40; key 40; switch 36x20
  inside a 44x44 target; X target 40x44 in Recents; eye target 40x40.
- Shared widgets: card, list row (leading icon, label, optional value, optional chevron, optional
  trailing target), app tile, gesture bar, primary and secondary button, switch. The list row
  draws its divider inside its own 44 px height, inset to 48 or 52, so adding rows never changes
  the band.
- The system theme is configured from the v2 palette at boot on v2 builds: dark, primary
  `#4C8DFF`, secondary and error kept from the theme defaults, surface `#0E1116`. The vendored
  theme is not edited.
- The Appearance app hides the sections that stop having an effect on a v2 build: theme mode,
  monochrome, theme colours and font size. The animations toggle stays and still turns every
  transition into an instant change. Density stays because it still affects the internals of apps
  that are not redesigned. The system-bars section is hidden because the v2 chrome is always a top
  split.

### Typography

- Six absolute type sizes across seven roles, in both weights of Inter (the grid label is the 12 px
  caption on a 16 px box):

  | Role | Size / box height / weight | Used by |
  |---|---|---|
  | Caption | 12 / 14 / Regular | hints, status caption, "Available networks", footer |
  | Grid label | 12 / 16 / Regular | Home app labels (the mockup CSS overrides the SPEC's 12/14) |
  | Secondary | 13 / 16 / Medium | status-bar time, values, dates, path, percentages |
  | Dense | 14 / 16 / Medium | keys, panel tile labels |
  | Body | 15 / 20 / Medium | list names, buttons, dialog body, weekday |
  | Title | 22 / 26 / Medium | toolbars, Recents, dialog title, dialogue/page titles |
  | Clock | 48 / 48 / Medium | Home and lock clock |

- Title carries -0.2 px letter spacing; the clock carries -1 px. No other role is tracked.
- Labels use the role's box height as their height and centre the text vertically, so the mockup's
  y positions are label tops, not baselines.
- Fonts: Inter 400 and 500, subset per role as committed TTFs, rasterised at build time by the
  existing pipeline at 4 bpp. 12, 13, 14 and 22 are ASCII; 15 includes Latin-1 (user text: SSIDs,
  file names); 48 contains digits, colon, hyphen and space.
- The 12-hour clock keeps AM/PM as a separate 13 px Medium label next to the 48 px digits. The 48
  px subset has no letters.
- Clock digits must be tabular. If the subset's default figures are not, the clock subset is
  regenerated with the tabular variants before the device build; the simulator is the check.
- The three legacy roles map to the new scale on v2 builds: small = 12, default = 15, large = 22.
  Every existing app therefore adopts the v2 type scale without per-app work.
- The LVGL built-in default font stays as the fallback for symbol glyphs. v2 screens use images for
  icons, so no v2 label depends on a fallback glyph.
- The font-size setting and its Appearance row are gone on v2 builds. The 10–32 scaling path stays
  for non-v2 builds.

### Icon system

- The 23 24 px and 4 16 px mockup icons are rendered from the mockup SVGs into A8 alpha masks at
  exactly 24 and 16 px. The real stroke is 2 px at both sizes (2 units in the 24 viewBox; the 16 px
  instances use 3 units, which scales to 2 px).
- A8 is the only viable mask format: the LVGL version in the tree has the A4 format but its
  software image renderer has no A4 path. The cost is about 15 KB for the 27 masks, not the 7.7 KB
  the SPEC estimated.
- Icons are recoloured at draw time with the image recolour style at full opacity; no per-colour
  copies.
- The icon set is named by an enum, not by glyph strings. The Home grid, Recents, Settings rows,
  toolbars and Wi-Fi list use image widgets sized 24 or 16.
- Extra 24 px glyphs in the same style are drawn only for the Settings rows that exist on this
  board and have no mockup glyph (Appearance, Location, Time & Date, USB, Touch Calibration,
  Development, Kernel) plus System Info, about eight glyphs and 5 KB. If the flash budget is
  exceeded, the remaining rows fall back to their current Material icon and the report records it.
- App id to icon is a table, with the mockup's eight Home apps mapped explicitly and a generic
  fallback for unknown or user-installed apps. No 32 px icon is introduced.
- Generated mask sources are committed; regeneration is a documented developer step, not a build
  dependency. Material Symbols is not removed: it remains the icon source for non-v2 builds and
  for the internals of apps that still use glyph icons.

### Chrome: status bar, toolbar, overlays

- On v2 builds the status bar is 24 px: the time at the left in 13 Medium with 12 px padding, and
  the live status icons at the right in 16 px with 12 px padding. The existing status services and
  icon slots are unchanged; only the height, fonts, sizes and padding change.
- The toolbar is a fixed 44 px band starting at y=24: back/menu targets 44x44, title 22 Medium,
  action targets 44x44. Content starts at y=68; screens that the mockup starts at 76 add their own
  8 px.
- All apps in a v2 build inherit the toolbar band and the palette. The non-designed settings screens
  (Display, Audio, Bluetooth, Power, and the rest) therefore get the new chrome and colours while
  keeping their internal layout; their deep redesign is a follow-up.
- Windows and system overlays (quick panel, Recents, dialog) share one full-screen black scrim at
  60% opacity. It is one object, not one per overlay, and there is no other transparency. The
  overlay backdrop used by the window manager becomes 60% on v2 builds.
- Buttons are 40 px high with radius 8: primary is `accent` with `on-accent` text, secondary is
  `surface-2` with `text`. Pressed states follow the mockup's colour-step rule; focus is a 2 px
  accent outline offset by 2 px for keypad/encoder input.
- Dividers are 1 px `line` inside cards and 1 px `line-soft` in full-width lists.

### Home

- Layout: status bar 0–24; clock 48 at (12,28); date block right-aligned at x=228, weekday 15 in a
  20 px box at y=38, date 13 in a 16 px box at y=58; grid from y=86 in four 60 px columns, rows of
  64 with an 8 px gap, two rows per page (86–222); page dots in the 22 px gap at y≈232; dock
  224x56 at (8,244) in the same four columns; nothing below y=300.
- The grid cell is 60x64, transparent and fully tappable. Inside it, a 44x44 `surface` square with
  radius 12 holds the 24 px icon; the label is 12/16, 4 px below the square, maximum 52 px wide,
  one line with an ellipsis. The favourite badge is not drawn in v2.
- Eight slots per page; paging is horizontal, page dots appear only when there is more than one
  page, and the page survives raising the Home. There is no vertical scroll.
- Pinning order: the Settings hub first, then Grok Bot when installed, then Wi-Fi, then the other
  user/system apps with favourites first and then by name. Settings-category apps, hidden apps,
  headless apps and apps the device cannot run are excluded, as today. The old mode entries are
  gone.
- The dock is a fixed table: Grok Bot, Files, Camera, Settings. Missing or incompatible apps are
  omitted without leaving a gap, as today. The open-app dot is a 4 px accent dot 6 px above the
  dock's bottom edge, aligned to the app's column; it is computed from the window stack when the
  Home is raised, so no listener or poll runs while an app is in front.
- Long-press keeps toggling the favourite flag through the existing file-operations path; ordering
  changes on the next populate.
- Clock and date use the existing formatter and the 12/24-hour setting; the minute timer is one
  shot, rearmed to the next minute, and rewritten only when the day changes. The timer and all
  widgets are destroyed with the Home.
- App start remains queued on the file-operations task. Keypad grid navigation stays.
- Grid labels for arbitrary installed app names truncate at 52 px; that is accepted and not treated
  as a defect.

### Settings hub

- A new visible app in the system category, id `tactility.settings`, named "Settings", is the first
  Home tile and the last dock slot. It is excluded from its own list.
- The hub lists every visible settings-category app, plus the mockup's synthetic Power row. The
  first five rows use the mockup's labels: Display, Sound, Wi-Fi, Bluetooth and Power (Sound opens
  the Audio app). The remaining rows follow alphabetically.
- Values are read when the hub is shown and refreshed when it is shown again; no polling, no
  listeners and no background timer:
  - Display: backlight duty as an integer percentage, e.g. "70%".
  - Audio: output volume as an integer percentage, e.g. "40%".
  - Wi-Fi: the connected SSID, "Off" when the radio is disabled, "Not connected" otherwise.
  - Bluetooth: "On" or "Off" from the radio state.
  - Power: the auto screen-off timeout ("30 s", "5 min", "Never"); the row opens Display, which owns
    that setting.
- Each row has a 24 px icon in `text-2`, a 15 px label, an optional 13 px `text-3` value and a 16 px
  `text-3` chevron. The divider is inset to 48. The card is 216 wide at x=12, 44 px rows, radius 12.
- Rows beyond the fifth continue inside the same card, which is a fixed 216x220 viewport at
  (12,76): the five designed rows are exactly the mockup's first paint, and the rest scroll inside
  the card. The scrollbar is auto-hidden and only appears while scrolling, 2 px in `text-3`.
- The hub's own icon is the mockup's settings glyph.

### Lock screen

- The lock stays the `Lock` screensaver type and the existing state machine; only its presentation
  and build task change.
- Presentation: full-screen two-stop vertical gradient from `bg` at 0–45% to `accent-soft` at 100%;
  no status bar; a 16 px `text-2` Wi-Fi icon at the top right when the radio is on; the 48 px clock
  centred at y=100; the date 15 `text-2` at y=154; the caption "Swipe up to unlock" 12 at y=276;
  and the 36x4 gesture bar centred at y=302.
- Unlock behaviour is unchanged: a swipe up anywhere, any key on a keypad, and the waking press
  never unlocks. No quick panel or Recents can be opened over the lock.
- The overlay is created and destroyed on the LVGL task; the timer callback only decides the
  transition and schedules the work (BC-08). The timer daemon no longer creates widgets.
- The lock is not shown at boot, is freed when the screen goes off, and returns to the previous app.
- If the 45% gradient bands on the RGB565 panel, the fallback in the SPEC applies: move the gradient
  to the bottom third. The banding is checked on the device, not in the simulator.

### Quick panel

- The panel is 240x230 at the top, `surface`, with 12 px rounded bottom corners and a 1 px `line`
  bottom border. It is built 12 px above the screen (y=-12, height 242) so the visible top corners
  are square without per-corner radius support.
- One full-screen scrim at 60% sits behind it.
- Header: time 22 at (12,12) and the short date 13 `text-2` right-aligned at x=228, same band.
- Two slider rows, 40 px high, 216 wide at x=12, at y=48 and y=88: a 24 px `text-2` icon (sun,
  volume), a 6 px track in `surface-2` with an `accent` fill, a 20 px `text` thumb, and a 13 px
  `text-2` value 32 px wide at the right. The whole 40 px row is the touch target.
- Three tiles at y=136, 68x52, radius 8, at x=12/86/160 with 6 px gaps, each with a 14 px label 6 px
  below: Wi-Fi, Bluetooth and Recents. On = `accent` fill with an `on-accent` icon; off =
  `surface-2` with a `text-2` icon. Recents opens the switcher. There is no power, location, mesh or
  notification tile.
- The 36x4 gesture bar sits at y=218; a swipe up closes the panel. A device without a pointer
  closes it with the back/Escape key; no extra tile is drawn.
- Opening is 160 ms (the panel travels from y=-242 to y=-12, scrim opacity 0 to 153); closing is
  120 ms, reversed. Only translate and the scrim's opacity animate; the app behind is not redrawn.
- Brightness persistence on close and the volume service path are unchanged. The panel refuses to
  open while locked.

### Recents

- Recents is an opaque `bg` screen that also covers the status bar; nothing of the app behind is
  visible and there is no transparency.
- Header: "Recents" 22 at x=16, 40 px band starting at y=28; "Swipe up to close" 12 `text-2` at the
  right on the same line.
- The list starts at y=76: up to five cards, 208x44 at x=16, 4 px gaps, ending at y=312. Each card
  is `surface` with radius 12, a 24 px icon, the app name 15, and a 16 px `text-2` X inside a 40x44
  target at the right. The current window has a 3x20 accent bar, radius 2, inset 4 px from the
  card's left edge and vertically centred.
- The card opens the app; only the X closes it. Closing animates the card up and the cards below it
  up by 48 px in 120 ms. Swipe up on a card closes it with the same animation.
- Opening is 150 ms, entering from 24 px below with no fade. No thumbnails are drawn.
- Empty state: "No open apps" 15 `text-2`, centred.
- Recents is a full-screen window-manager overlay, opaque and covering the status bar, not a
  translucent card row. On a device without a pointer, the back/Escape key closes the screen;
  pointer devices see the mockup exactly.

### Wi-Fi

- Toolbar: back 44, title "Wi-Fi" 22, and a 36x20 switch inside a 44x44 target at the right. On =
  accent track with the `text` thumb at the right; off = `surface-2` track with the `text-2` thumb
  at the left; 100 ms.
- Connected network card at (12,76), 216x56, `accent-soft` with a 1 px `accent-line` border, radius
  12: a 24 px Wi-Fi icon in `accent-text`, the network name 15 and "Connected" 13, both
  `accent-text`, and a 16 px chevron in `accent-text` at the right. Tapping the card opens the
  forget dialog; on the connected network that is the only management action.
- "Available networks" caption 12 `text-2` at (16,144); while scanning, the caption is replaced by
  "Scanning…" 13 `text-2` in the same band.
- The list card starts at y=162, 216 wide at x=12 and a fixed 132 px tall viewport (three rows).
  Each row is 44 px: a 24 px signal icon (`sig3`/`sig2`/`sig1`), the name 15 and a 16 px
  `text-3` lock when the network is protected; dividers inset to 48. The current network stays
  fixed above; the list scrolls inside its viewport when there are more than three, with the same
  auto-hidden 2 px scrollbar as the Settings hub.
- Signal buckets are pure: RSSI at least -60 is three bars, at least -75 is two, anything else is
  one.
- There is no spinner. An empty scan shows "No networks found" 15 centred. With the switch off, the
  connected card and list are replaced by "Wi-Fi is off" 15 centred.
- Tapping a network uses the existing connect path and opens the password screen for a protected
  network. Connect failures appear as a 13 px `accent-text` line under the caption. There is no red.
- Forget flow: the dialog confirms, then the app disconnects with auto-connect suppressed and
  removes the saved settings through the existing Wi-Fi settings API. Errors show inline; the
  connected card stays visible behind the scrim.
- The hidden-SSID form stays as it is and inherits chrome, buttons and text roles.

### Dialog

- On v2 builds the shared alert dialog is a bottom-anchored card: 216 wide, 12 px side margins,
  16 px padding, radius 12, `surface` with a 1 px `line` border, 12 px above the bottom edge of the
  screen. Behind it, the 60% scrim.
- Title 22 on one line; body 15 `text-2`; buttons 40 px high with an 8 px gap: Cancel in `surface-2`
  and the confirming action in `accent` with `on-accent` text. A destructive action is still accent,
  because the palette has no error colour.
- The card grows upward with its content, so it never cuts the background text in half and the
  buttons stay at thumb height.
- Every app that uses the shared alert dialog gets this presentation on v2 builds. Input dialogs
  keep their layout but inherit chrome, buttons and type.

### Files

- The mobile Files screen is a list, not a grid. Toolbar: back 44, "Files" 22, and a 44x44 menu
  target with the mockup's more-vertical icon. The existing file actions move into that menu.
- Path 13 `text-2` at (16,76). The list starts at y=96 and ends at y=288; only the list scrolls.
- Rows are 44 px, full width, with 16 px left and 12 px right padding: a 24 px folder/file icon, the
  name 15, and a 13 `text-3` meta at the right (size for a file, item count for a folder). The
  divider is 1 px `line-soft` at the row's bottom, from x=52.
- The footer is fixed at y=288–320 with a 1 px `line` top border and the caption "N items · M
  folders" 12 centred, with correct singular forms.
- Empty folder: "This folder is empty" 15 `text-2` centred. Navigation stays by row taps, the back
  gesture and the toolbar's back target.
- The non-mobile Files view keeps its tile grid.

### Keyboard and Wi-Fi password screen

- The software keyboard is rebuilt once for the whole system on v2 builds: a single button matrix,
  four rows of 40 px keys with 1 px gaps, radius 8, no popup bubble, 14/16 letter labels, 168 px
  tall starting at y=152, a 1 px `line-soft` top border and 4 px of padding above the first row.
- Row 1 is QWERTYUIOP (ten keys of equal width); row 2 is ASDFGHJKL inset by half a key; row 3 is
  Shift (1.5), ZXCVBNM, Delete (1.5); row 4 is 123 (2), space (5.5) and Go (2.5).
- Letter keys are `surface-2` with `text`; modifiers (Shift, Delete, 123) are `surface` with
  `text-2`; Go is `accent` with `on-accent`. Pressed states are the colour step from the mockup.
- Layers: lower case, upper case through Shift, and a numbers/symbols layer through 123, all in the
  same four-row geometry. Go sends Enter.
- The keyboard is shown on textarea focus and hidden on blur as today, with a 140 ms slide from
  y=320 to y=152.
- The Wi-Fi password screen: toolbar back and the SSID as the 22 px title; a 160x40 field at (12,76)
  in `surface`, radius 8, with an `accent` 1 px border while focused, 15 px password dots, a static
  2x18 `accent` caret (no blinking) and a 24 px eye inside a 40x40 target at the field's right; a
  48x40 Join button at (180,76) with `on-accent` text, shown as unavailable (`surface-2` with
  `text-3`) while the field is empty; the caption "WPA2 · case-sensitive" 12 `text-2` at y=124; and
  a 13 `accent-text` error line under the field. The keyboard occupies y=152–320.
- A wrong password keeps the screen open and shows the error; Join closes the screen on success and
  returns to the Wi-Fi list.

### Gestures and animations

- Edge strips become 16 px (from 20). Top down opens the quick panel; bottom up goes Home; bottom up
  and hold opens Recents; left drag right goes back. The lock intercepts all of them.
- No control sits in the bottom 20 px of the Home; the dock ends at y=300.
- The keyboard's last row touches the bottom edge. A tap works there; only a drag longer than the
  gesture threshold becomes a gesture. This exception is accepted.
- The gesture bar is only drawn where an edge swipe acts: the lock (unlock) and the quick panel
  (close).
- Transitions follow the mockup: lock to app 180 ms up; panel open 160 ms and close 120 ms; Recents
  open 150 ms and close 120 ms; app open/back 140 ms; dialog 120 ms; switch/tile 100 ms; pressed
  0 ms. Only translate and scrim opacity animate. With animations disabled in Appearance, all of
  them are instant. Nothing loops, pulses or blinks.

### Memory, CPU and stack

- No large object, string or container is created on a widget-build path. Lists are built in a loop
  from fixed buffers; no `std::vector` or `std::format` temporary lives in an event callback; the
  Home's transient collections are freed before the build returns.
- Widgets are created only when a screen or overlay is shown and destroyed when it is hidden. The
  Home, lock, panel, Recents and dialogs own no permanent timer beyond the Home's minute timer and
  the existing idle timer.
- No polling: Settings values refresh on show; dock dots refresh on raise; scanning updates through
  existing events; the clock updates on the minute.
- No new image assets beyond the icon masks, no thumbnails, no wallpaper, no blur and no second
  transparency layer. The single partial draw buffer (15,360 B) stays; double buffering is not
  enabled.
- Fonts and icon masks are `const` data read from flash; none is loaded into RAM at runtime.
- BC-08 is implemented in this work: the lock overlay is built and destroyed on the LVGL task.
- Every new build path must leave at least a 25% margin against its task's stack:
  launcher 6,144 B, LVGL task 9,120 B, app tasks 8,192 B by default, timer daemon 5,120 B. A
  temporary probe records the high-water mark of the launcher, the LVGL task, the timer daemon and
  the Settings, Files and Wi-Fi app tasks, including the boot peak; the probe is removed before the
  final commit and its numbers go in the footprint report.
- Static DRAM must stay at 65,039 B (+0). IRAM stays at 100% of 16,384 B (no new IRAM). The steady
  UI heap increase is capped at 6 KB and the internal-heap increase at 2 KB.

### Footprint and budget

- Flash budget for the whole redesign: at most +100 KB of `Tactility.bin`. The expected split is
  about +20 KB net of text fonts, about +15 KB of base icon masks, up to +5 KB of extra glyphs and
  the rest in code.
- The report compares the current head build with the final build: `Tactility.bin`, `.text`,
  `.rodata`, static DRAM, DIRAM and IRAM, plus the boot heap (internal and PSRAM free/total) and
  the high-water marks above.
- The icon masks are expected to make the report's asset line larger than the mockup's estimate
  (7.7 KB); the difference is the A4-to-A8 decision and is not a regression.
- The draw-buffer line in the mockup (2 x 30 KB) is not adopted: the board keeps its 15 KB single
  partial buffer.

### Verification

- A simulator profile at 240x320, RGB565, with the v2 flag on is used to render every screen before
  a device flash. The nine states are captured through the existing screenshot path (the screenshot
  app or a development-only snapshot hook) and compared with `png/1x` for geometry, colour and band
  positions; text anti-aliasing differs from Chrome and is not pixel-diffed.
- The simulator's profile and flag changes are development-only and do not ship in firmware.
- The owner's hardware checklist covers what the simulator cannot: touch targets, edge gestures,
  gradient banding, brightness, volume, Wi-Fi connect/forget, keyboard feel and the absence of the
  boot crash. It starts with **Display → Orientation = Portrait**: the v2 Home only builds at
  240x320, and the board is natively landscape.
- Build verification for every ticket is `source ~/esp/esp-idf-v6.1/export.sh &&
  python3 device.py es3c28p && idf.py build` exiting 0. No ticket flashes, pushes, opens a PR or
  merges.

## Testing Decisions

Good tests exercise externally visible behaviour through pure functions, not LVGL widget internals,
object counts or private state. The feature has four pure seams; the Home resolver already has a
home in the existing doctest suite, and the other three are new pure modules in the same style.

- **Home resolver (existing seam, extended).** Pure input: app descriptors (id, name, category,
  flags, compatibility), favourites and the installed set. Output: the ordered grid items and dock
  entries. New cases: the Settings hub first; Grok then Wi-Fi; settings-category apps excluded;
  more than eight items returned in order (the 8-per-page slicing is a UI concern, checked in the
  simulator); the fixed dock table with missing apps collapsing; favourites ordering; no legacy
  mode entries. Prior art: `HomeModelTest`.
- **Mobile formatting (new seam, one pure module).** Pure input: the current display duty, audio
  volume, Wi-Fi state and SSID, Bluetooth state and screen-off timeout. Output: the five Settings
  value strings with the documented "Off"/"Not connected"/"Never" cases and percentages. Also
  covers the file meta and footer strings ("N items · M folders", singular forms) and the RSSI to
  one/two/three-bar bucket. This is the highest seam for the new value UI and keeps the LVGL side
  free of formatting branches.
- **Presentation guard (new seam, pure).** Input: flag, logical width and height, orientation.
  Output: v2 active or legacy. Cases: 240x320 portrait; 320x240 landscape; the same after a
  flipped rotation; another resolution entirely.
- **App icon table (new seam, pure).** Input: app id. Output: the v2 icon enum or the fallback, for
  the eight mockup apps, the settings rows, an installed third-party app and an unknown id.
- Tests compile standalone with doctest as the previous mobile tests did, because the full local
  test target remains awkward on a stock macOS checkout. The CI target must stay green when it can
  run.
- Not unit-tested, because no harness exists: LVGL layout and rendering, swipes and edge gestures,
  animations, the keyboard matrix, the panel and Recents overlays, the lock layering and the
  backlight. These are covered by the simulator comparison and the owner checklist.
- What makes a good test here: it would fail if a future change reorders the Home, drops the
  Settings hub, changes a value string, lets a landscape display into the absolute layout, or maps
  an app to the wrong icon. It does not assert object counts, colours or pixel positions.

## Glossary

**Home**: The default screen: status bar, clock and date, paged app grid and dock.
_Avoid_: Launcher, desktop, start screen

**Dock**: The fixed row of four shortcuts at the bottom of the Home, in the grid's columns.
_Avoid_: Favourites, pinned apps, taskbar

**Quick panel**: The 230 px sheet that slides from the top with brightness, volume and three tiles.
_Avoid_: Notification shade, control centre

**Recents**: The opaque full screen listing up to five open apps, each closable with an X.
_Avoid_: Task switcher, overview, recent apps overlay

**Lock screen**: The gradient screen shown when the panel wakes after a locked timeout, dismissed by
a swipe up.
_Avoid_: Screensaver, login, PIN screen

**Settings hub**: The new Settings app: rows for every settings-category app, with live values for
Display, Sound, Wi-Fi, Bluetooth and Power.
_Avoid_: Settings mode, settings grid

**v2**: The fixed mobile design of a v2 build: tokens, type scale, palette and chrome everywhere,
and the 240x320 Home layout when the portrait guard allows it.
_Avoid_: New UI, redesign (without the version)

**Token**: A named design value (colour, radius, band, type role, component size) that screens
consume instead of hardcoding.
_Avoid_: Style, constant

**Type role**: One of the seven fixed roles across six Inter sizes and two weights (caption, grid
label, secondary, dense, body, title, clock).
_Avoid_: Font size, scale

**Icon mask**: A one-colour A8 alpha image rendered at 24 or 16 px from a mockup SVG and recoloured
at draw time.
_Avoid_: Icon font, glyph

**Gesture bar**: The 36x4 rounded bar drawn only where a bottom-edge swipe acts (lock, quick panel).
_Avoid_: Handle, indicator, home bar

**Scrim**: The single full-screen black layer at 60% behind the quick panel, Recents or a dialog.
_Avoid_: Backdrop, dimmer, overlay

**Legacy presentation**: The pre-v2 launcher layout, used when the flag is off or the display is not
240x320 portrait; on a v2 build it keeps the v2 chrome, palette and type.
_Avoid_: Old UI, fallback mode

## ADR Candidates

1. **A fixed 240x320 layout instead of a responsive one.** The mockups are absolute and the
   acceptance test is a comparison against 240x320 PNGs; a responsive layout would reintroduce the
   drift the redesign exists to remove. The cost is that the presentation needs a runtime guard and
   falls back to the legacy launcher elsewhere.

2. **A fixed six-size Inter scale, ignoring the user font size.** The design's typography is part
   of the product and the previous scalable size is what made screens drift. The cost is that the
   Appearance font-size control and the 10–32 scaling path no longer apply on v2 builds.

3. **A8 icon masks instead of an icon font.** The mockup's SVG shapes are the source of truth;
   rendering them to masks keeps the 2 px stroke exact at 24 and 16. The LVGL software renderer has
   no A4 path, so A8 costs about 15 KB, roughly twice the mockup estimate. Reusing Material Symbols
   would be cheaper but would not match the design.

4. **A separate Settings hub app instead of the launcher's settings mode.** A row list with live
   values cannot live in a launcher mode without making the launcher own five settings services and
   its own window state. A normal app keeps the launcher as a launcher, makes Settings appear in
   Recents like any app, and lets the mode machinery be deleted.

5. **The lock is built on the LVGL task.** The timer daemon's 5,120 B stack is shared with every
   software timer in the firmware; the v2 lock is larger than the v1 one. Moving construction to
   the LVGL task removes the shared-stack risk instead of growing the daemon stack.

## Out of Scope

- Deep redesign of the settings screens that were not drawn (Display, Audio, Bluetooth, Power,
  Appearance, Location, Time & Date, USB, Touch Calibration, Development, Kernel). They get the v2
  chrome, theme, fonts and buttons; their internal layout is a follow-up.
- The remaining non-designed apps' internals beyond inheriting chrome, palette and type.
- Notifications, notification badges, media controls or a notification list.
- PIN, password, pattern, biometric or secure lock.
- Wallpapers, widgets, folders, app drawer beyond paging, or a customisable dock.
- Recents thumbnails, "clear all" or persistent recents across reboots.
- Thumbnail/screenshot generation for any screen.
- Multiple themes, light mode, monochrome, user colours or user font sizes on v2 builds.
- Other resolutions, landscape v2, other boards, or upstream-compatible layouts.
- Changes to existing app manifests, app autostart or the boot app. The new Settings hub is the only
  new manifest.
- New settings features (for example a new timeout control) or changes to the Wi-Fi stack.
- Flashing, pushing, pull requests or merges. The owner approves any device write.

## Further Notes

- Mockup discrepancies resolved here, CSS first: the grid label is 12/16 (the SPEC prose says
  12/14) and the 22 px title is tracked -0.2. The Settings card's five rows are 5 x 44 inside the
  card, so the card ends at y=296 as drawn. The 16 px icons use stroke 3 in the 24 viewBox, which
  is exactly 2 px at 16.
- The mockup's fifth Settings row, "Power 5 min", does not map to an app on this board: the Power
  app is a battery monitor and this board has no battery. The row therefore shows the auto
  screen-off timeout and opens Display. This is the only row whose label and target differ.
- Only the 27 base icons are in the mockup. The Settings hub needs eight more 24 px glyphs for
  rows that exist on this board; they are drawn in the same style and capped at 5 KB. If the budget
  is exceeded, the fallback is the row's current Material icon, recorded in the report.
- The Home grid's names are user data. Labels longer than 52 px truncate; that is by design and is
  not a bug to fix by shrinking type.
- The ES3C28P is natively 320x240 and defaults to landscape; the v2 Home requires the saved
  orientation to be Portrait (240x320). On a fresh install that is a one-time change in Display
  settings; a first-boot portrait default for this board is a follow-up, not part of this work. In
  landscape the legacy launcher is used.
- The status bar keeps its live status icons (SD, Bluetooth, battery, GPS, memory) at 16 px on the
  right. The mockup shows only Wi-Fi because nothing else was active in the capture; hiding real
  warnings would be a regression. The Wi-Fi indicator is the v2 `wifis` mask; the other status
  icons keep the existing Material statusbar glyphs at 16, because the mockup defines no
  replacement and the set is state-driven.
- The fonts are Inter (SIL Open Font License). The subset TTFs are committed like the existing
  Adwaita subsets, and the license notice is added to the third-party notices. No other dependency
  or license changes.
- Clock digits are expected to be tabular. If the subset's default figures are proportional, the
  clock font is regenerated from the tabular variants before the device build; the simulator check
  is what decides.
- The lock gradient is the one place where RGB565 may band. It is validated on the panel; the SPEC
  already provides the shortened-gradient fallback.
- The Wi-Fi connected card's chevron is the affordance for managing the connected network; the only
  management action is Forget, so it opens the confirmation dialog. There is no network details
  screen.
- The previous mobile work's footprint method, the boot-crash stack budget and the owner-checklist
  pattern still apply. The final report must include the binary size, `.text`, `.rodata`, static
  DRAM, DIRAM/IRAM, boot heap (internal and PSRAM) and the measured high-water marks.
- The mockup branch is not part of this repository. Implementers read it from the design worktree or
  a checkout of `design/redesign-mockups` at `5e5e06d0`; the PNG contact sheet and `checks.json`
  are the acceptance references.
