# v2 icon sources

The 35 SVGs here are the source of the one-colour icon masks in
`Tactility/Source/lvgl/MobileIcons.cpp`.

- 27 are copied from the approved mockup (`design/redesign-mockups` at `5e5e06d0`,
  `docs/design-v2/src/index.html`). Do not redraw them.
- 8 are additions in the same style for the Settings rows that exist on the ES3C28P:
  appearance, location, calendar, usb, target, code, chip and info.

Every icon uses a 24 viewBox, integer coordinates, one colour, a real 2 px stroke and round caps
and joins. The 16 px instances (`wifis`, `chev`, `close`, `lock`) use stroke-width 3 in the 24
viewBox, which is exactly 2 px at 16.

## Regenerating the masks

The build does not run this step. It is only needed when an SVG changes.

1. Render the SVGs with Chrome at 1x on a transparent background:
   - 24 px icons: cells of 32x32 (8 columns), icon centred.
   - 16 px icons: cells of 24x24 (4 columns), icon at 16x16 with `stroke-width="3"`.
2. Capture each icon's alpha channel with Pillow and crop it to 24x24 or 16x16.
3. Emit an A8 `lv_image_dsc_t` per icon (stride = width, data_size = width x height) and the
   `MobileIcon` enum order used by `Tactility/Source/lvgl/MobileIcons.cpp`.
4. Keep the enum order stable: adding or reordering icons changes the enum values. The app id
   table in the same file is the only other place that names an icon.

The app id table maps the launcher and Settings ids to a mask; unknown apps fall back to `hub`.
