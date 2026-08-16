#include "api_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "data_model.h"

ApiClient g_apiClient;

void ApiClient::begin(const char *baseUrl) {
    _baseUrl = baseUrl;
}

void ApiClient::setBaseUrl(const String &url) {
    if (url == _baseUrl) return;
    _baseUrl = url;
    _lastFetch = 0;
}

void ApiClient::loop() {
    unsigned long now = millis();
    if (now - _lastFetch < FETCH_INTERVAL_MS) return;
    _lastFetch = now;
    fetchDashboard();
}

void ApiClient::fetchDashboard() {
    if (WiFi.status() != WL_CONNECTED) return;

    String url = _baseUrl + "/dashboard";
    HTTPClient http;
    http.begin(url);
    http.setTimeout(3000);
    int code = http.GET();

    if (code != HTTP_CODE_OK) {
        // Negative codes are HTTPClient's own transport errors (refused,
        // timeout, DNS) rather than HTTP statuses -- worth distinguishing.
        Serial.printf("[api] GET %s -> %d (%s)\n", url.c_str(), code,
                      HTTPClient::errorToString(code).c_str());
        http.end();
        g_dashboard.last_fetch_ok = false;
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();

    if (err) {
        Serial.printf("[api] JSON parse failed: %s\n", err.c_str());
        g_dashboard.last_fetch_ok = false;
        return;
    }
    Serial.println("[api] fetch OK");

    JsonObject system = doc["system"];
    if (!system.isNull()) {
        g_dashboard.hostname = system["hostname"] | "";
        g_dashboard.version = system["version"] | "";
        g_dashboard.uptime = system["uptime"] | "";
        JsonArray load = system["load_avg"];
        for (size_t i = 0; i < 3 && i < load.size(); i++) {
            g_dashboard.load_avg[i] = load[i];
        }
        g_dashboard.system_valid = true;
    }

    JsonObject resources = doc["resources"];
    if (!resources.isNull()) {
        g_dashboard.cpu_pct = resources["cpu_pct"] | 0.0f;
        g_dashboard.mem_pct = resources["mem_pct"] | 0.0f;
        g_dashboard.disk_pct = resources["disk_pct"] | 0.0f;
        g_dashboard.mem_used_mb = resources["mem_used_mb"] | 0;
        g_dashboard.mem_total_mb = resources["mem_total_mb"] | 0;
        g_dashboard.disk_used_gb = resources["disk_used_gb"] | 0.0f;
        g_dashboard.disk_total_gb = resources["disk_total_gb"] | 0.0f;
        g_dashboard.cpu_history.push(g_dashboard.cpu_pct);

        g_dashboard.cpu_temp_count = 0;
        float hottest = 0;
        for (JsonVariant v : resources["temps_c"].as<JsonArray>()) {
            if (g_dashboard.cpu_temp_count >= MAX_CPU_TEMPS) break;
            float t = v.as<float>();
            g_dashboard.cpu_temps[g_dashboard.cpu_temp_count++] = t;
            if (t > hottest) hottest = t;
        }
        if (g_dashboard.cpu_temp_count > 0) g_dashboard.temp_history.push(hottest);

        g_dashboard.zone_temp_count = 0;
        for (JsonVariant v : resources["zone_temps_c"].as<JsonArray>()) {
            if (g_dashboard.zone_temp_count >= MAX_ZONE_TEMPS) break;
            g_dashboard.zone_temps[g_dashboard.zone_temp_count++] = v.as<float>();
        }

        g_dashboard.resources_valid = true;
    }

    // "items" here is an object keyed by device name, not an array.
    JsonObject interfaces = doc["interfaces"]["items"];
    if (!interfaces.isNull()) {
        g_dashboard.interface_count = 0;
        for (JsonPair kv : interfaces) {
            if (g_dashboard.interface_count >= MAX_INTERFACES) break;
            InterfaceEntry &e = g_dashboard.interfaces[g_dashboard.interface_count++];
            e.device = kv.key().c_str();
            e.name = kv.value().as<const char *>();
        }
        g_dashboard.interfaces_valid = true;
    }

    JsonArray gateways = doc["gateways"]["items"];
    if (!gateways.isNull()) {
        g_dashboard.gateway_count = 0;
        for (JsonObject g : gateways) {
            if (g_dashboard.gateway_count >= MAX_GATEWAYS) break;
            GatewayEntry &e = g_dashboard.gateways[g_dashboard.gateway_count++];
            e.name = g["name"] | "";
            e.address = g["gateway"] | "";
            e.interface = g["interface"] | "";
        }
        g_dashboard.gateways_valid = true;
    }

    JsonArray services = doc["services"]["items"];
    if (!services.isNull()) {
        g_dashboard.service_count = 0;
        g_dashboard.services_running = 0;
        g_dashboard.services_total = 0;
        for (JsonObject s : services) {
            bool running = s["running"] | false;
            g_dashboard.services_total++;
            if (running) g_dashboard.services_running++;
            if (g_dashboard.service_count >= MAX_SERVICES) continue;
            ServiceEntry &e = g_dashboard.services[g_dashboard.service_count++];
            e.name = s["name"] | "";
            e.running = running;
        }
        g_dashboard.services_valid = true;
    }

    JsonObject traffic = doc["traffic"];
    if (!traffic.isNull()) {
        g_dashboard.wan_in_bps = traffic["wan_in_bps"] | 0;
        g_dashboard.wan_out_bps = traffic["wan_out_bps"] | 0;
        g_dashboard.lan_in_bps = traffic["lan_in_bps"] | 0;
        g_dashboard.lan_out_bps = traffic["lan_out_bps"] | 0;
        g_dashboard.wan_today_in_bytes = traffic["wan_today_in_bytes"] | (uint64_t)0;
        g_dashboard.wan_today_out_bytes = traffic["wan_today_out_bytes"] | (uint64_t)0;
        g_dashboard.wan_month_in_bytes = traffic["wan_month_in_bytes"] | (uint64_t)0;
        g_dashboard.wan_month_out_bytes = traffic["wan_month_out_bytes"] | (uint64_t)0;
        g_dashboard.traffic_in_history.push((float)g_dashboard.wan_in_bps);
        g_dashboard.traffic_out_history.push((float)g_dashboard.wan_out_bps);
        g_dashboard.traffic_valid = true;
    }

    JsonObject firewall = doc["firewall"];
    if (!firewall.isNull()) {
        g_dashboard.fw_blocks_per_min = firewall["blocks_per_min"] | 0;
        g_dashboard.fw_window_seconds = firewall["window_seconds"] | 0;

        JsonArray top = firewall["top_src"];
        if (!top.isNull() && top.size() > 0) {
            g_dashboard.fw_top_src = top[0]["ip"] | "";
            g_dashboard.fw_top_src_count = top[0]["count"] | 0;
        } else {
            g_dashboard.fw_top_src = "";
            g_dashboard.fw_top_src_count = 0;
        }

        g_dashboard.fw_recent_count = 0;
        for (JsonObject e : firewall["recent"].as<JsonArray>()) {
            if (g_dashboard.fw_recent_count >= MAX_FW_RECENT) break;
            FirewallEntry &f = g_dashboard.fw_recent[g_dashboard.fw_recent_count++];
            f.time = e["time"] | "";
            f.src = e["src"] | "";
            f.dst_port = e["dst_port"] | "";
            f.proto = e["proto"] | "";
            f.interface = e["interface"] | "";
            f.label = e["label"] | "";
            f.blocked = (strcmp(e["action"] | "block", "pass") != 0);
        }
        g_dashboard.firewall_valid = true;
    }

    JsonObject clients = doc["clients"];
    if (!clients.isNull()) {
        g_dashboard.clients_online = clients["online"] | 0;
        g_dashboard.clients_total = clients["total"] | 0;
        g_dashboard.client_count = 0;
        for (JsonObject c : clients["items"].as<JsonArray>()) {
            if (g_dashboard.client_count >= MAX_CLIENTS) break;
            ClientEntry &e = g_dashboard.clients[g_dashboard.client_count++];
            e.ip = c["ip"] | "";
            e.name = c["name"] | "";
            e.interface = c["interface"] | "";
            e.online = c["online"] | false;
        }
        g_dashboard.clients_valid = true;
    }

    JsonObject updates = doc["updates"];
    if (!updates.isNull()) {
        g_dashboard.update_available = updates["available"] | false;
        g_dashboard.updates_valid = true;
    }

    JsonObject cs = doc["crowdsec"];
    if (!cs.isNull()) {
        g_dashboard.crowdsec_available = cs["available"] | false;
        g_dashboard.crowdsec_running = cs["running"] | false;
        g_dashboard.cs_active_decisions = cs["active_decisions"] | 0;
        g_dashboard.cs_alerts_total = cs["alerts_total"] | 0;
        g_dashboard.cs_recent_count = 0;
        for (JsonObject e : cs["recent"].as<JsonArray>()) {
            if (g_dashboard.cs_recent_count >= MAX_CS_RECENT) break;
            CrowdsecEntry &r = g_dashboard.cs_recent[g_dashboard.cs_recent_count++];
            r.ip = e["ip"] | "";
            r.country = e["country"] | "";
            r.as_name = e["as"] | "";
            r.reason = e["reason"] | "";
            r.decision = e["decision"] | "";
        }
        g_dashboard.crowdsec_valid = true;
    }

    g_dashboard.has_data = true;
    g_dashboard.last_fetch_ok = true;
    g_dashboard.last_fetch_ms = millis();
}
