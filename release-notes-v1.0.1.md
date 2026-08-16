WAN usage totals on the Traffic page, and the middleware address is now editable from the device's settings UI.

## Traffic usage totals

The Traffic page now shows how much has actually been used, not just how fast it is moving:

```
Today  in 4.21 GB   out 0.83 GB
Month  in 212.60 GB   out 38.40 GB
```

OPNsense reports cumulative interface byte counters, and the middleware already compared them between polls to produce the live bits-per-second figure. Those same per-poll differences now also accumulate into day- and month-to-date totals.

Counting differences rather than reading the firewall's absolute counters is what makes the totals survive an **OPNsense reboot**: a counter reset reads as a negative difference and is discarded, instead of the total lurching backwards. They also survive a **container restart**, being written to `data/traffic_totals.json` on the existing Docker volume. Traffic passing while the middleware is down is not counted, as there is no way to attribute it to a particular day afterwards.

To make room, the In/Out captions moved up onto the title's row. The trend chart is now *taller* than it was in v1.0.0 despite the two extra lines.

### Set `TZ`, or your day rolls over at the wrong midnight

Day and month boundaries follow the container's local time, which is UTC unless you say otherwise:

```yaml
environment:
  TZ: Europe/London
```

Without it, "today" ends at UTC midnight — an hour early anywhere observing daylight saving. If you deploy through Portainer, set it in the stack's environment variables, since Portainer does not read `.env` from a repository.

## Middleware address in the settings UI

Previously the middleware address could only be set through the Wi-Fi captive portal, so moving the container to a different host or port meant a 10-second BOOT hold — which also wiped your Wi-Fi credentials.

There is now a **Middleware** section in the device's settings web UI. It applies on the next poll, with no reboot, and leaves Wi-Fi alone. The 10-second hold remains a full network reset for when the device cannot get onto the network at all.

## Upgrading from v1.0.0

Both halves need updating; the totals only appear when the new firmware and new middleware are both running.

1. **Middleware** — re-pull `ghcr.io/henryscat/cyd-dashboard-middleware:latest` and add `TZ` to the environment. Your existing volume, API credentials and settings are kept.
2. **Firmware** — flash by any route below.

Totals begin at zero when the new middleware first runs, so the first day's figure covers only part of a day.

## Flashing

### Web installer

The simplest route, straight from a Chrome or Edge browser with the board plugged in:

**https://henryscat.github.io/cyd-dashboard-for-opnsense/**

Leave *Erase device* unticked to keep your Wi-Fi settings and middleware address when upgrading. Tick it for a first-time install.

### Over Wi-Fi

Already running v1.0.0? Upload **`firmware.bin`** on the device's settings page under *Firmware*. No cable needed.

### USB, single file

Download **`firmware-merged.bin`** and write it at offset `0x0`:

```
esptool.py --chip esp32 --port COM5 write_flash 0x0 firmware-merged.bin
```

### USB, individual files

The same image as four separate parts, for anyone who prefers to write them individually:

| Offset | File | What it is |
|---|---|---|
| `0x1000` | `bootloader.bin` | Second-stage bootloader |
| `0x8000` | `partitions.bin` | Partition table |
| `0xe000` | `boot_app0.bin` | OTA slot selector |
| `0x10000` | `firmware.bin` | The application |

```
esptool.py --chip esp32 --port COM5 write_flash \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000  bootloader.bin \
  0x8000  partitions.bin \
  0xe000  boot_app0.bin \
  0x10000 firmware.bin
```

`boot_app0.bin` is easy to dismiss but matters on any board that has taken an over-the-air update. The partition table has two application slots and no factory slot, so `otadata` decides which one boots, and an OTA leaves the board running from the second slot. Writing only `firmware.bin` would rewrite the *first* slot while the bootloader still starts the second, leaving a board running the old firmware and appearing not to have updated. `boot_app0.bin` resets that choice.

Replace `COM5` with your board's port — `/dev/ttyUSB0` on Linux, `/dev/cu.usbserial-*` on macOS.

> **`firmware.bin` and `firmware-merged.bin` are not alternatives.** Writing `firmware.bin` at `0x0` leaves a board that will not boot; uploading `firmware-merged.bin` as an over-the-air update is rejected. See the [firmware README](https://github.com/HenrysCat/cyd-dashboard-for-opnsense/tree/main/firmware#which-binary-do-i-need).

---

OPNsense® is a registered trademark of Deciso B.V. This is an unofficial, independent project, not affiliated with or endorsed by Deciso B.V.
