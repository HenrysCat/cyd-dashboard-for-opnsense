#pragma once
#include <Arduino.h>

static const size_t SPARKLINE_SAMPLES = 60;

// How long since the last *successful* fetch before the on-screen values are
// treated as stale. The poll interval is 2s, so this tolerates several missed
// polls before flagging -- long enough to ride out a brief blip, short enough
// that you're never reading minutes-old numbers believing they're current.
static const unsigned long DATA_STALE_MS = 15000;

// Fixed-size ring buffer for sparkline history -- avoids any heap churn on a
// board with no PSRAM.
struct RingBuffer {
    float values[SPARKLINE_SAMPLES] = {0};
    size_t head = 0;
    size_t count = 0;

    void push(float v) {
        values[head] = v;
        head = (head + 1) % SPARKLINE_SAMPLES;
        if (count < SPARKLINE_SAMPLES) count++;
    }

    // Oldest-first access, index 0..count()-1
    float at(size_t i) const {
        size_t start = (head + SPARKLINE_SAMPLES - count) % SPARKLINE_SAMPLES;
        return values[(start + i) % SPARKLINE_SAMPLES];
    }
};

// Fixed caps rather than dynamic containers -- the middleware's payload is
// small and bounded, and this keeps allocation predictable on a board with no
// PSRAM. Anything beyond these counts is simply not displayed.
static const size_t MAX_CPU_TEMPS = 8;
static const size_t MAX_ZONE_TEMPS = 4;
static const size_t MAX_INTERFACES = 8;
static const size_t MAX_GATEWAYS = 6;
static const size_t MAX_SERVICES = 22;
static const size_t MAX_FW_RECENT = 10;
static const size_t MAX_CLIENTS = 22;
static const size_t MAX_CS_RECENT = 8;

struct InterfaceEntry {
    String device;  // e.g. "em0_vlan10"
    String name;    // e.g. "LAN"
};

struct GatewayEntry {
    String name;       // e.g. "WAN_DHCP"
    String address;    // e.g. "92.29.96.1" (may be empty)
    String interface;  // e.g. "wan"
};

struct ServiceEntry {
    String name;
    bool running = false;
};

struct FirewallEntry {
    String time;       // HH:MM:SS, as the firewall reported it
    String src;        // source IP
    String dst_port;   // destination port
    String proto;      // tcp/udp/icmp
    String interface;  // friendly name, e.g. "WAN"
    String label;      // matching rule, e.g. "GeoIP"
    bool blocked = true;
};

struct ClientEntry {
    String ip;
    String name;       // hostname, description, or NIC manufacturer
    String interface;  // e.g. "LAN"
    bool online = false;
};

struct CrowdsecEntry {
    String ip;
    String country;  // 2-letter code
    String as_name;  // owning network, e.g. "BLOOM-HOST"
    String reason;   // scenario, e.g. "pf-scan-multi_ports"
    String decision; // e.g. "ban:1"
};

struct DashboardData {
    // system (slow poll)
    String hostname;
    String version;
    String uptime;
    float load_avg[3] = {0, 0, 0};
    bool system_valid = false;

    // resources (fast poll)
    float cpu_pct = 0;
    float mem_pct = 0;
    float disk_pct = 0;
    uint32_t mem_used_mb = 0;
    uint32_t mem_total_mb = 0;
    float disk_used_gb = 0;
    float disk_total_gb = 0;
    float cpu_temps[MAX_CPU_TEMPS] = {0};
    size_t cpu_temp_count = 0;
    float zone_temps[MAX_ZONE_TEMPS] = {0};
    size_t zone_temp_count = 0;
    bool resources_valid = false;
    RingBuffer cpu_history;
    RingBuffer temp_history;  // hottest CPU core, for the thermal trend chart

    // traffic (fast poll)
    uint32_t wan_in_bps = 0;
    uint32_t wan_out_bps = 0;
    uint32_t lan_in_bps = 0;
    uint32_t lan_out_bps = 0;
    bool traffic_valid = false;
    RingBuffer traffic_in_history;
    RingBuffer traffic_out_history;

    // interfaces / gateways / services (slow poll)
    InterfaceEntry interfaces[MAX_INTERFACES];
    size_t interface_count = 0;
    bool interfaces_valid = false;

    GatewayEntry gateways[MAX_GATEWAYS];
    size_t gateway_count = 0;
    bool gateways_valid = false;

    ServiceEntry services[MAX_SERVICES];
    size_t service_count = 0;
    size_t services_running = 0;
    size_t services_total = 0;  // real total, even if it exceeds MAX_SERVICES
    bool services_valid = false;

    // firewall log (own cadence on the middleware side)
    uint32_t fw_blocks_per_min = 0;
    uint32_t fw_window_seconds = 0;
    String fw_top_src;
    uint32_t fw_top_src_count = 0;
    FirewallEntry fw_recent[MAX_FW_RECENT];
    size_t fw_recent_count = 0;
    bool firewall_valid = false;

    // DHCP/ARP clients (slow poll)
    ClientEntry clients[MAX_CLIENTS];
    size_t client_count = 0;      // how many are held for display
    size_t clients_online = 0;    // real totals, even if more than fit above
    size_t clients_total = 0;
    bool clients_valid = false;

    // firmware update availability (slow poll)
    bool update_available = false;
    bool updates_valid = false;

    // CrowdSec -- an optional OPNsense plugin. crowdsec_available stays false
    // when it isn't installed, and the page is then left out of the rotation
    // entirely rather than shown empty.
    bool crowdsec_available = false;
    bool crowdsec_running = false;
    uint32_t cs_active_decisions = 0;
    uint32_t cs_alerts_total = 0;
    CrowdsecEntry cs_recent[MAX_CS_RECENT];
    size_t cs_recent_count = 0;
    bool crowdsec_valid = false;

    bool has_data = false;       // at least one successful fetch
    unsigned long last_fetch_ms = 0;
    bool last_fetch_ok = false;
};

extern DashboardData g_dashboard;
