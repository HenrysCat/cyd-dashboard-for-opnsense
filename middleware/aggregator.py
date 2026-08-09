"""Background polling loop that builds and caches the slim dashboard JSON.

Three concurrent tasks: fast poll (memory/disk/thermal/traffic, feel "live"),
slow poll (system info, interfaces, gateways, services, which change rarely),
and a persistent CPU usage stream (OPNsense only exposes CPU as a live SSE
feed, not a pollable resource -- see opnsense_client.stream_cpu_usage). Each
poll is wrapped so a single failed OPNsense call logs and keeps the
last-known-good value rather than taking the whole snapshot down; each section
carries its own `updated_at` so a consumer can tell if a value has gone stale.
"""
import asyncio
import logging

import httpx
import time
from collections import Counter
from datetime import datetime, timezone

# Enough entries to estimate a rate without hammering the firewall: the log
# endpoint returns ~700 bytes per entry, and this is polled continuously.
FIREWALL_LOG_LIMIT = 50
FIREWALL_RECENT_SHOWN = 10
MAX_CLIENTS_SHOWN = 22
CROWDSEC_RECENT_SHOWN = 8

from config import Config
from opnsense_client import OpnsenseClient

logger = logging.getLogger("aggregator")


def _now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _parse_gb(value: str) -> float:
    """"108G" -> 108.0, "4.3G" -> 4.3. OPNsense's systemDisk always reports in G."""
    return float(value.rstrip("G"))


class Aggregator:
    def __init__(self, config: Config, client: OpnsenseClient):
        self._config = config
        self._client = client
        self._lock = asyncio.Lock()
        self._snapshot: dict = {
            "system": None,
            "resources": None,
            "traffic": None,
            "interfaces": None,
            "gateways": None,
            "services": None,
            "firewall": None,
            "clients": None,
            "updates": None,
            "crowdsec": None,
        }
        # None = not yet probed, False = plugin absent (stop trying).
        self._crowdsec_available: bool | None = None
        self._iface_names: dict = {}  # device -> friendly name, for log display
        self._latest_cpu_pct: float | None = None
        self._prev_traffic_counters: dict | None = None
        self._prev_traffic_time: float | None = None
        self._tasks: list[asyncio.Task] = []

    def start(self):
        self._tasks = [
            asyncio.create_task(self._loop(self._config.poll_interval_fast, self._poll_fast)),
            asyncio.create_task(self._loop(self._config.poll_interval_slow, self._poll_slow)),
            # Its own cadence: heavier than the other calls, but needs to feel
            # closer to live than the 15s slow poll.
            asyncio.create_task(self._loop(5.0, self._poll_firewall)),
            asyncio.create_task(self._cpu_stream_loop()),
        ]

    async def stop(self):
        for task in self._tasks:
            task.cancel()
        await self._client.aclose()

    async def get_snapshot(self) -> dict:
        async with self._lock:
            return dict(self._snapshot)

    async def _loop(self, interval: float, poll_fn):
        while True:
            try:
                await poll_fn()
            except Exception:
                logger.exception("poll failed for %s", poll_fn.__name__)
            await asyncio.sleep(interval)

    async def _cpu_stream_loop(self):
        # Long-lived SSE connection, not a periodic poll -- reconnect with a
        # backoff if OPNsense drops it or it's unreachable.
        while True:
            try:
                async for event in self._client.stream_cpu_usage():
                    self._latest_cpu_pct = float(event.get("total", 0))
            except Exception:
                logger.exception("cpu usage stream dropped, reconnecting in 5s")
            await asyncio.sleep(5)

    async def _set_section(self, name: str, value: dict):
        async with self._lock:
            self._snapshot[name] = value

    async def _poll_fast(self):
        memory_raw = await self._client.get_memory()
        memory = memory_raw.get("memory", {})
        mem_total = float(memory.get("total", 0) or 0)
        mem_used = float(memory.get("used", 0) or 0)

        disk_raw = await self._client.get_disk()
        root_disk = next(
            (d for d in disk_raw.get("devices", []) if d.get("mountpoint") == "/"),
            disk_raw.get("devices", [{}])[0] if disk_raw.get("devices") else {},
        )

        # The same endpoint returns both per-core CPU temps and motherboard/chipset
        # ACPI "thermal zone" sensors (type "zone", e.g. hw.acpi.thermal.tz0/tz1) --
        # OPNsense's own dashboard shows these as separate "CPU N" / "Zone N" rows.
        thermal_raw = await self._client.get_thermal()

        def _temps_for(sensor_type: str) -> list[float]:
            entries = sorted(
                (entry.get("device_seq", 0), float(entry.get("temperature", 0)))
                for entry in thermal_raw
                if entry.get("type") == sensor_type
            )
            return [temp for _, temp in entries]

        await self._set_section("resources", {
            "cpu_pct": self._latest_cpu_pct,
            "mem_pct": round(mem_used / mem_total * 100, 1) if mem_total else None,
            "mem_used_mb": round(mem_used / 1024 / 1024),
            "mem_total_mb": round(mem_total / 1024 / 1024),
            "disk_pct": root_disk.get("used_pct"),
            "disk_used_gb": _parse_gb(root_disk["used"]) if root_disk.get("used") else None,
            "disk_total_gb": _parse_gb(root_disk["blocks"]) if root_disk.get("blocks") else None,
            "temps_c": _temps_for("cpu"),
            "zone_temps_c": _temps_for("zone"),
            "updated_at": _now(),
        })

        traffic_raw = await self._client.get_traffic()
        await self._set_section("traffic", self._compute_traffic_bps(traffic_raw))

    async def _poll_slow(self):
        system_raw = await self._client.get_system_info()
        time_raw = await self._client.get_system_time()
        load_avg = [float(x) for x in time_raw.get("loadavg", "0,0,0").split(",")]
        await self._set_section("system", {
            "hostname": system_raw.get("name"),
            "version": system_raw.get("versions", [None])[0],
            "uptime": time_raw.get("uptime"),
            "load_avg": load_avg,
            "updated_at": _now(),
        })

        interfaces_raw = await self._client.get_interfaces()
        # Kept for the firewall log, which reports raw device names.
        self._iface_names = dict(interfaces_raw)
        await self._set_section("interfaces", {
            "items": interfaces_raw,
            "updated_at": _now(),
        })

        gateways_raw = await self._client.get_gateways()
        await self._set_section("gateways", {
            "items": [
                {"name": row.get("name"), "gateway": row.get("gateway"), "interface": row.get("interface")}
                for row in gateways_raw.get("rows", [])
            ],
            "updated_at": _now(),
        })

        await self._poll_clients()
        await self._poll_updates()
        await self._poll_crowdsec()

        services_raw = await self._client.get_services()
        await self._set_section("services", {
            "items": [
                {"name": row.get("name"), "running": bool(row.get("running"))}
                for row in services_raw.get("rows", [])
            ],
            "updated_at": _now(),
        })

    @staticmethod
    def _ip_sort_key(ip: str):
        try:
            return tuple(int(part) for part in ip.split("."))
        except ValueError:
            return (999, 999, 999, 999)

    async def _poll_clients(self):
        leases = await self._client.get_dhcp_leases()
        arp = await self._client.get_arp()

        # ARP fills the gaps: DHCP leases often carry no hostname, but the ARP
        # table usually knows the NIC vendor, which is the next best label.
        arp_by_ip = {e.get("ip", ""): e for e in arp}

        items = []
        online = 0
        for row in leases.get("rows", []):
            ip = row.get("address", "")
            is_online = row.get("status") == "online"
            if is_online:
                online += 1
            a = arp_by_ip.get(ip, {})
            name = (
                row.get("hostname")
                or row.get("descr")
                or a.get("hostname")
                or row.get("man")
                or a.get("manufacturer")
                or row.get("mac", "")
            )
            items.append({
                "ip": ip,
                "name": name,
                "interface": row.get("if_descr") or a.get("intf_description", ""),
                "online": is_online,
            })

        # Online first, then by IP -- the machines that are actually up are
        # what you want to see without scrolling.
        items.sort(key=lambda i: (not i["online"], self._ip_sort_key(i["ip"])))
        total = len(items)

        await self._set_section("clients", {
            "online": online,
            "offline": total - online,
            "total": total,
            "items": items[:MAX_CLIENTS_SHOWN],
            "updated_at": _now(),
        })

    async def _poll_updates(self):
        fw = await self._client.get_firmware_status()
        status = fw.get("status", "none")
        await self._set_section("updates", {
            # "none" means OPNsense hasn't run an update check, not that the
            # system is up to date -- so this is only ever true once a check
            # has actually happened on the firewall itself.
            "available": status in ("update", "upgrade"),
            "status": status,
            "current": fw.get("product", {}).get("product_version", ""),
            "updated_at": _now(),
        })

    async def _poll_crowdsec(self):
        # CrowdSec is an optional OPNsense plugin. Its endpoints 404 when it
        # isn't installed, so the first 404 marks it absent and we stop asking
        # -- otherwise every poll would log an exception on most installs.
        if self._crowdsec_available is False:
            return

        try:
            status = await self._client.get_crowdsec_status()
            decisions = await self._client.get_crowdsec_decisions()
            alerts = await self._client.get_crowdsec_alerts()
        except httpx.HTTPStatusError as exc:
            if exc.response.status_code == 404:
                logger.info("CrowdSec plugin not installed -- skipping that section")
                self._crowdsec_available = False
                await self._set_section("crowdsec", {"available": False, "updated_at": _now()})
                return
            raise

        self._crowdsec_available = True

        recent = []
        for row in alerts.get("rows", [])[:CROWDSEC_RECENT_SHOWN]:
            # "Ip:1.2.3.4" -> "1.2.3.4"
            value = str(row.get("value", ""))
            ip = value.split(":", 1)[1] if ":" in value else value
            # "firewallservices/pf-scan-multi_ports" -> "pf-scan-multi_ports"
            reason = str(row.get("reason", "")).rsplit("/", 1)[-1]
            recent.append({
                "ip": ip,
                "country": row.get("country", ""),
                "as": row.get("as", ""),
                "reason": reason,
                "decision": row.get("decisions", ""),
            })

        await self._set_section("crowdsec", {
            "available": True,
            "running": status.get("status") == "running",
            "active_decisions": decisions.get("total", 0),
            "alerts_total": alerts.get("total", 0),
            "recent": recent,
            "updated_at": _now(),
        })

    async def _poll_firewall(self):
        entries = await self._client.get_firewall_log(FIREWALL_LOG_LIMIT)

        def ts(entry):
            try:
                return datetime.fromisoformat(entry.get("__timestamp__", ""))
            except ValueError:
                return None

        # Newest first, so "recent" is genuinely the latest activity.
        entries = [e for e in entries if ts(e) is not None]
        entries.sort(key=ts, reverse=True)

        blocked = [e for e in entries if e.get("action") == "block"]

        # Rate is derived from the spread of the returned entries rather than
        # by comparing against this process's clock: the log's __timestamp__ is
        # in the firewall's local timezone (BST here) while this container runs
        # UTC, so any direct comparison would be an hour out.
        rate = 0
        window = 0
        if len(entries) >= 2:
            newest, oldest = ts(entries[0]), ts(entries[-1])
            window = max(0, int((newest - oldest).total_seconds()))
            if window > 0:
                rate = round(len(blocked) / window * 60)
        top_src = [
            {"ip": ip, "count": n}
            for ip, n in Counter(e.get("src", "") for e in blocked).most_common(3)
            if ip
        ]

        # All actions, not just blocks: the display colours pass/block
        # differently. (OPNsense only logs blocks unless pass rules have
        # logging enabled, so in a default setup these are all blocks.)
        recent = [
            {
                # Time is passed through as the firewall reported it, so it
                # matches what the OPNsense UI shows.
                "time": (e.get("__timestamp__") or "")[11:19],
                "action": e.get("action", ""),
                "interface": self._iface_names.get(e.get("interface", ""), e.get("interface", "")),
                "src": e.get("src", ""),
                "dst_port": e.get("dstport", ""),
                "proto": e.get("protoname", ""),
                # Which rule matched, e.g. "GeoIP" or "Spamhaus and dshield".
                "label": e.get("label", ""),
            }
            for e in entries[:FIREWALL_RECENT_SHOWN]
        ]

        await self._set_section("firewall", {
            "blocks_per_min": rate,
            # The rate is an average over window_seconds, which is however
            # long the returned entries happen to span -- short when blocks are
            # frequent, longer when they're sparse.
            "window_seconds": window,
            "blocked_in_window": len(blocked),
            "top_src": top_src,
            "recent": recent,
            "updated_at": _now(),
        })

    def _compute_traffic_bps(self, traffic_raw: dict) -> dict:
        interfaces = traffic_raw.get("interfaces", {})
        counters = {
            "wan_in": int(interfaces.get("wan", {}).get("bytes received", 0) or 0),
            "wan_out": int(interfaces.get("wan", {}).get("bytes transmitted", 0) or 0),
            "lan_in": int(interfaces.get("lan", {}).get("bytes received", 0) or 0),
            "lan_out": int(interfaces.get("lan", {}).get("bytes transmitted", 0) or 0),
        }
        now = time.monotonic()

        bps = {f"{key}_bps": 0 for key in counters}
        if self._prev_traffic_counters is not None and self._prev_traffic_time is not None:
            dt = now - self._prev_traffic_time
            if dt > 0:
                for key, value in counters.items():
                    delta_bytes = value - self._prev_traffic_counters[key]
                    bps[f"{key}_bps"] = max(0, int(delta_bytes * 8 / dt)) if delta_bytes >= 0 else 0

        self._prev_traffic_counters = counters
        self._prev_traffic_time = now

        return {**bps, "updated_at": _now()}
