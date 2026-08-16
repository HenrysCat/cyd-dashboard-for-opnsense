#pragma once
#include <Arduino.h>

class ApiClient {
public:
    void begin(const char *baseUrl);

    // Repoints at a different middleware without a reboot, and pulls the next
    // fetch forward so the change is visible on the display immediately rather
    // than up to one poll interval later.
    void setBaseUrl(const String &url);
    void loop();  // non-blocking; call every main loop iteration
    const String &baseUrl() const { return _baseUrl; }

private:
    String _baseUrl;
    unsigned long _lastFetch = 0;
    static const unsigned long FETCH_INTERVAL_MS = 2000;

    void fetchDashboard();
};

extern ApiClient g_apiClient;
