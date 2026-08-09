Live OPNsense firewall stats on a "Cheap Yellow Display" — the inexpensive ESP32-2432S028R touchscreen board (320×240, ILI9341, resistive touch).

![The dashboard cycling through its pages](https://raw.githubusercontent.com/HenrysCat/cyd-dashboard-for-opnsense/main/assets/dashboard-demo.gif)

## What it does

Eight pages of firewall status on a wall-mountable screen, tapped through on the touchscreen:

| Page | Shows |
|---|---|
| Overview | Hostname, uptime, load, CPU/RAM/disk gauges, CPU trend, update badge |
| Traffic | WAN in/out with trend chart, plus LAN throughput |
| CPU & Thermal | CPU %, load, per-core and ACPI zone temperatures |
| Interfaces & Gateways | Interfaces with device names, gateways, live WAN rates |
| Services | Every service, colour-coded running/stopped |
| Firewall | Blocks/min, top source, recent log entries with rule labels |
| Clients | DHCP/ARP devices, online/offline, by hostname or NIC vendor |
| CrowdSec | Ban decisions and recent alerts — optional, hidden if not installed |

## How it's built

Two halves. A **FastAPI middleware** polls the OPNsense REST API and republishes everything as one compact JSON document; **PlatformIO + LVGL firmware** renders it.

That split is the point. The ESP32 has ~320KB of RAM and no PSRAM, so TLS to OPNsense, large payloads (the firewall log endpoint alone defaults to ~700KB) and rate calculations all happen upstream of the device. The display fetches a few kilobytes of ready-made numbers.

## Getting started

1. [Set up the middleware](https://github.com/HenrysCat/cyd-dashboard-for-opnsense/tree/main/middleware) on an always-on machine with Docker. It needs an OPNsense API key.
2. [Flash the display](https://github.com/HenrysCat/cyd-dashboard-for-opnsense/tree/main/firmware) and point it at the middleware's address.

The middleware runs from a prebuilt image (`ghcr.io/henryscat/cyd-dashboard-middleware`), built for amd64 and arm64, so there's nothing to compile on your NAS or Pi.

## No compiled-in secrets

Wi-Fi and the middleware address are entered on the device itself through a captive portal on first boot. Backlight, orientation, colour corrections, page cycling and OTA firmware updates are all adjustable from a small web UI the device serves on your LAN.

## Built to be left alone

- A red dot appears if Wi-Fi drops, data goes stale, or nothing arrives — on-screen values are never silently out of date.
- Wi-Fi reconnects by itself after an AP reboot.
- A task watchdog reboots the device if the main loop stalls.
- The middleware has a Docker healthcheck, so a wedged container is restarted, not just a crashed one.
- Failed polls keep the last known good values rather than blanking the display.

---

OPNsense® is a registered trademark of Deciso B.V. This is an unofficial, independent project, not affiliated with or endorsed by Deciso B.V.
