# Redesign v2 screen checks

The 240x320 captures in this folder are made with the simulator, not on the device. They are
development evidence for the tickets in `redesign-v2-tickets.md`; the firmware does not contain the
capture aids.

## Simulator setup

`Devices/simulator` is built from the same tree with `CONFIG_TT_MOBILE_HOME` on and the simulator
profile set to `ES3C28P` (`Data/tactility/simulator_profile.txt`), which renders 240x320 RGB565.
`TACTILITY_SIM_SCREENSHOT` writes the active screen to a PPM after
`TACTILITY_SIM_SCREENSHOT_DELAY_MS`; `F12` does it on demand. Convert the PPM to PNG with PIL.

## Development navigation

`Tactility/Source/lvgl/SimulatorDevNavigation.cpp` is compiled only for the simulator. It adds an
`F1`-`F9` shortcut, and the same action can be selected headlessly with
`TACTILITY_SIM_ACTION=<1-9>`:

| Key | Screen |
|---|---|
| F1 | Home |
| F2 | Settings hub |
| F3 | Wi-Fi |
| F4 | Files |
| F5 | Quick panel over Files |
| F6 | Recents over four apps |
| F7 | Lock screen |
| F8 | Alert dialog over Wi-Fi |
| F9 | Wi-Fi password screen for `Cafe_5G` |

Every action except Home and Lock first joins the mock driver's `Home Wifi` fixture network, so the
captures show a connected state. `F7` uses the same fixture radio.

Example:

```sh
TACTILITY_SIM_ACTION=2 TACTILITY_SIM_SCREENSHOT=/tmp/settings.ppm \
  TACTILITY_SIM_SCREENSHOT_DELAY_MS=5000 ./build/Tactility/Tactility
```

The captures are taken at 1:1. Text anti-aliasing and the system clock differ from the Chrome
mockups by nature; geometry, colours and band positions are the comparison.

## Captures

| File | Screen | Mockup |
|---|---|---|
| `06-settings.png` | Settings hub | `png/1x/settings.png` |
| `07-home.png` | Home | `png/1x/home.png` |
| `08-lock.png` | Lock | `png/1x/lock.png` |
| `09-panel.png` | Quick panel | `png/1x/quick.png` |
| `10-recents.png` | Recents | `png/1x/recents.png` |
| `11-dialog.png` | Alert dialog | `png/1x/dialog.png` |
| `12-wifi.png` | Wi-Fi | `png/1x/wifi.png` |
| `13-files.png` | Files | `png/1x/files.png` |
| `14-keyboard.png` | Wi-Fi password + keyboard | `png/1x/keyboard.png` |
