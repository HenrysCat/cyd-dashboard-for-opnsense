"""Thin client around the OPNsense REST API.

Endpoint paths below were verified against a live OPNsense 26.7.1 instance
during development (see middleware/README.md for how, if you need to
re-verify against a different version/plugin set). Each metric group is a
single function/path here on purpose, so a wrong guess on a different version
is a one-line fix rather than a scavenger hunt.

CPU usage is not a pollable REST resource -- OPNsense only exposes it as a
live Server-Sent Events stream (`/diagnostics/cpu_usage/stream`, one event/sec).
`stream_cpu_usage()` is a long-lived async generator for that; the aggregator
runs it as its own background task rather than polling it.
"""
import json

import httpx

from config import Config

# One path per metric group -- the seam to correct against a live instance.
ENDPOINTS = {
    "system_info": "/diagnostics/system/systemInformation",
    "system_time": "/diagnostics/system/systemTime",
    "memory": "/diagnostics/system/systemResources",
    "disk": "/diagnostics/system/systemDisk",
    "thermal": "/diagnostics/system/systemTemperature",
    "traffic": "/diagnostics/traffic/interface",
    "interfaces": "/diagnostics/interface/getInterfaceNames",
    "gateways": "/routing/settings/searchGateway",
    "services": "/core/service/search",
    "cpu_stream": "/diagnostics/cpu_usage/stream",
    "firewall_log": "/diagnostics/firewall/log",
    "dhcp_leases": "/dhcpv4/leases/searchLease",
    "arp": "/diagnostics/interface/getArp",
    "firmware_status": "/core/firmware/status",
    # Optional plugin -- these 404 when CrowdSec isn't installed, which the
    # aggregator uses to detect absence rather than treating it as an error.
    "crowdsec_status": "/crowdsec/service/status",
    "crowdsec_alerts": "/crowdsec/alerts/search",
    "crowdsec_decisions": "/crowdsec/decisions/search",
}


class OpnsenseClient:
    def __init__(self, config: Config):
        self._client = httpx.AsyncClient(
            base_url=config.base_url,
            auth=(config.api_key, config.api_secret),
            verify=config.verify_ssl,
            timeout=10.0,
        )

    async def aclose(self):
        await self._client.aclose()

    async def _get(self, path: str) -> dict:
        response = await self._client.get(path)
        response.raise_for_status()
        return response.json()

    async def get_system_info(self) -> dict:
        return await self._get(ENDPOINTS["system_info"])

    async def get_system_time(self) -> dict:
        return await self._get(ENDPOINTS["system_time"])

    async def get_memory(self) -> dict:
        return await self._get(ENDPOINTS["memory"])

    async def get_disk(self) -> dict:
        return await self._get(ENDPOINTS["disk"])

    async def get_thermal(self) -> dict:
        return await self._get(ENDPOINTS["thermal"])

    async def get_traffic(self) -> dict:
        return await self._get(ENDPOINTS["traffic"])

    async def get_interfaces(self) -> dict:
        return await self._get(ENDPOINTS["interfaces"])

    async def get_gateways(self) -> dict:
        return await self._get(ENDPOINTS["gateways"])

    async def get_services(self) -> dict:
        return await self._get(ENDPOINTS["services"])

    async def get_dhcp_leases(self) -> dict:
        return await self._get(ENDPOINTS["dhcp_leases"])

    async def get_arp(self) -> list:
        return await self._get(ENDPOINTS["arp"])

    async def get_firmware_status(self) -> dict:
        """Cached status only -- this does NOT contact OPNsense's mirrors. It
        reports whatever the firewall's own last update check found, so
        `status` stays "none" until a check has actually been run there."""
        return await self._get(ENDPOINTS["firmware_status"])

    async def get_crowdsec_status(self) -> dict:
        return await self._get(ENDPOINTS["crowdsec_status"])

    async def get_crowdsec_alerts(self) -> dict:
        return await self._get(ENDPOINTS["crowdsec_alerts"])

    async def get_crowdsec_decisions(self) -> dict:
        return await self._get(ENDPOINTS["crowdsec_decisions"])

    async def get_firewall_log(self, limit: int = 50) -> list:
        """Most recent firewall log entries. The endpoint defaults to 1000
        entries (~700KB) which is far too heavy to poll repeatedly, so a limit
        is always passed. Note `/log/{n}` does NOT limit -- only `?limit=`."""
        return await self._get(f"{ENDPOINTS['firewall_log']}?limit={limit}")

    async def stream_cpu_usage(self):
        """Yields one dict per SSE event, e.g. {"total": 6, "user": 6, ...,
        "idle": 94} -- "total" is the current overall CPU busy percentage.
        Runs until the connection drops; caller is responsible for reconnecting."""
        async with self._client.stream("GET", ENDPOINTS["cpu_stream"]) as response:
            response.raise_for_status()
            async for line in response.aiter_lines():
                if line.startswith("data:"):
                    yield json.loads(line[len("data:"):].strip())
