# Firmware

The display half of the project: the code that runs on the ESP32 "Cheap Yellow Display" and
renders your OPNsense firewall's status.

It talks to the [middleware](../middleware/) over your LAN, so **set that up first** - the display
needs its address during setup.

## What you need

- An **ESP32-2432S028R** board, commonly sold as the "Cheap Yellow Display" or CYD
  (2.8in, 320x240, resistive touch). Other CYD variants usually work - see
  [Troubleshooting](#troubleshooting) if the screen looks wrong.
- A USB cable, for the first flash only. After that you can update over Wi-Fi.
- Either [esptool](https://pypi.org/project/esptool/) (`pip install esptool`) to flash a
  prebuilt binary, or [PlatformIO](https://platformio.org/install/ide?install=vscode) to build
  from source. You do not need both.

## Flashing it

Nothing needs editing first, whichever route you take. Wi-Fi details and the middleware address
are entered on the device itself, so no passwords are baked into the firmware.

### Option A: flash a release binary (no PlatformIO)

Download **`firmware-merged.bin`** from the
[latest release](https://github.com/HenrysCat/cyd-dashboard-for-opnsense/releases/latest) and
write it to the board:

```
esptool.py --chip esp32 --port COM5 write_flash 0x0 firmware-merged.bin
```

Replace `COM5` with your board's port -- `/dev/ttyUSB0` on Linux, `/dev/cu.usbserial-*` on macOS.

> **Use `firmware-merged.bin`, not `firmware.bin`.** The merged image contains the bootloader,
> partition table and app together and is written at `0x0`. `firmware.bin` is the app on its own,
> for over-the-air updates only -- flashing it to a blank board over USB leaves an unbootable
> device.

### Option B: build from source

```
cd firmware
pio run -t upload
```

In VS Code you can instead press the PlatformIO **Upload** button in the status bar.

> **Tip:** if the upload cannot connect, hold the **BOOT** button, tap **RST**, then release BOOT
> just as the upload starts. Do not keep holding BOOT once the device reboots -- ten seconds of it
> triggers the factory reset.

### Which binary do I need?

**Cable to a blank board: merged. Over Wi-Fi: plain.**

The two release assets are not alternatives -- one contains the other. `firmware-merged.bin` is
`firmware.bin` plus the 64KB of flash that sits in front of it:

| Offset | Contents | In `firmware.bin`? |
|---|---|---|
| `0x1000` | Second-stage bootloader | No |
| `0x8000` | Partition table | No |
| `0xe000` | `boot_app0` -- which app slot to boot | No |
| `0x10000` | **The application itself** | This is the whole file |

So `firmware.bin` is the compiled program and nothing else. It has no idea where in flash it
belongs; the bootloader and partition table are what put it there. From `0x10000` onward, the two
files are byte for byte identical.

That leads to two failure modes worth avoiding:

- **`firmware.bin` written to `0x0` over USB** puts the application where the bootloader should
  be. Nothing is left to start it and no table describes the layout, so the board does not boot --
  silently, with a blank screen that looks like dead hardware.
- **`firmware-merged.bin` uploaded as an OTA update** is rejected. The device flashes the
  *inactive* app slot and then points `boot_app0` at it, so it expects an application image alone;
  a bootloader and partition table are not valid in that slot.

Once the merged image is on the board you never need it again -- every later update is the plain
`firmware.bin` through the settings page, with no cable involved.

## First-time setup

On first boot the screen explains what to do, and the device creates its own Wi-Fi network:

1. On your phone or laptop, join the Wi-Fi network **`CYD-Dashboard-Setup`**
   (password: `dashboard123`).
2. Open **http://192.168.4.1** in a browser.
3. Pick your home Wi-Fi and enter its password.
4. In **Middleware host:port**, enter the address of the machine running the middleware, for
   example `192.168.1.50:8098`. No `http://` needed.
5. Save. The device joins your Wi-Fi and the dashboard appears.

That's it - it remembers everything and goes straight to the dashboard on future restarts.

## Using it

**Tap the left half** of the screen to go back a page, **the right half** to go forward. The dots
along the bottom show where you are.

| Page | Shows |
|---|---|
| Overview | Hostname, uptime, load, CPU/RAM/disk gauges and a CPU trend |
| Traffic | WAN in/out with a live chart, plus LAN throughput |
| CPU & Thermal | CPU load, per-core temperatures and a temperature trend |
| Interfaces & Gateways | Your interfaces and gateways, with live WAN rates |
| Services | Every service, green for running and red for stopped |
| Firewall | Blocks per minute, worst offender, and recent blocked connections |
| Clients | Devices on your network, online or offline |
| CrowdSec | Bans and recent alerts - only if you run the CrowdSec plugin |

**The BOOT button** (on the back of the board) does two things:

- **Short press** - shows a System Info screen with the device's IP address (handy for finding the
  settings page), Wi-Fi signal, memory use and uptime. Press again or tap the screen to close it.
- **Hold for 10 seconds** - forgets your Wi-Fi and middleware settings and returns to first-time
  setup. Your display corrections are kept.

## Settings

Once it is on your network, open **http://cyd-dash.local/** in a browser, or use the IP
address shown on the System Info screen.

Everything applies straight away - no restart:

| Setting | What it is for |
|---|---|
| Backlight brightness | Dim the screen (5-100%) |
| Cycle pages automatically | Rotate through pages on a timer, for unattended wall use |
| Seconds per page | How long each page is shown when cycling |
| Swap red/blue channels | Red looks blue, yellow looks cyan |
| Rotate display 90 degrees | Screen is portrait and cut off |
| Flip display 180 degrees | Screen is upside down |
| Mirror display | Text appears back to front |
| Invert display colours | Colours look like a photo negative |
| Swap left/right navigation | Tapping left goes forwards instead of back |

The display corrections exist because CYD boards ship with several different panel types. If yours
looks wrong out of the box, one of those toggles almost certainly fixes it.

## Updating later

You do not need to take it off the wall, or plug it in. On the settings page, scroll to
**Firmware**, choose a `firmware.bin` and upload it -- the device flashes itself and restarts.

Get that file from either:

- the [latest release](https://github.com/HenrysCat/cyd-dashboard-for-opnsense/releases/latest) --
  the plain **`firmware.bin`** asset, which is exactly what this expects, or
- your own build, at `firmware/.pio/build/esp32dev/firmware.bin` after `pio run`.

> Over-the-air updates need two app partitions to flash between, which is why
> [`platformio.ini`](platformio.ini) sets `min_spiffs.csv`. A single-app partition table breaks
> this feature.

## Is it working?

A **small red dot** in the bottom-left corner is the only health indicator you need. If it is
showing, one of these is true:

- Wi-Fi has dropped
- the middleware cannot be reached
- the readings on screen are more than 15 seconds old

No dot means everything on screen is current. It recovers by itself: Wi-Fi reconnects
automatically, and a watchdog restarts the device if it ever locks up.

## Troubleshooting

| Symptom | Fix |
|---|---|
| Screen is blank | Check the USB cable supplies enough power; try another cable or port |
| Colours look wrong | Try *Swap red/blue channels* or *Invert display colours* in settings |
| Screen upside down or mirrored | Try *Flip display 180* or *Mirror display* |
| Portrait and cut off | Try *Rotate display 90 degrees* |
| Taps page the wrong way | Try *Swap left/right navigation* |
| Cannot find the settings page | Short-press BOOT to see the device's IP address |
| Stuck on "Wi-Fi setup needed" | Your Wi-Fi password may be wrong; hold BOOT 10s and start over |
| Dashboard shows dashes and a red dot | The middleware address is wrong or the container is not running |

Nothing on the screen? Connect USB and run `pio device monitor` - the device logs what it is doing,
including its IP address and any failed connections.

## Technical notes

Details that matter if you plan to modify this.

**TFT_eSPI is configured entirely with `-D` flags in [`platformio.ini`](platformio.ini), not a
`User_Setup.h`.** A `-include` forced header does not reliably reach the library's own source
files, so settings written there are silently ignored by TFT_eSPI while still looking correct in
your code. In practice that left the library using its internal 240x320 default, so `fillScreen`
only ever addressed 240 of the panel's 320 columns and about 80px of the screen showed permanent
noise. Please do not move this config back into a header.

**Display and touch are on separate SPI buses.** The display uses HSPI (12/13/14/15), the XPT2046
touch controller has its own pins on VSPI (25/32/39/33). Calling the global `SPI.begin()`
reconfigures VSPI to its default pins and corrupts the display.

**Orientation is applied by writing the panel's MADCTL register directly**, not through
`setRotation()`. That is what lets the settings take effect without a reboot. The touch mapping is
transformed to match, so taps keep landing where they look.

**There is no touch calibration step.** Fixed panel bounds are used instead. Resistive CYD panels
are consistent enough that per-unit calibration added a failure mode - an uncalibratable screen
could lock you out of the device entirely - without improving accuracy for half-screen tap zones.

**LVGL uses the system heap** (`LV_MEM_CUSTOM 1`) rather than a fixed static pool. A pool large
enough for every page did not fit in static DRAM, and when a smaller pool ran out, LVGL's
allocation-failure assert is an infinite loop - which presents as the device hard-freezing with no
crash output at all.

**Pages only redraw when new data arrives** or you change page. Refreshing on every loop iteration
issues hundreds of label and style writes per second, each invalidating the object, which starves
LVGL's rendering until the UI stops responding.
