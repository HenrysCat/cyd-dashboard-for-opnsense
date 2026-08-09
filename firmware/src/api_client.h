#pragma once
#include <Arduino.h>

class ApiClient {
public:
    void begin(const char *baseUrl);
    void loop();  // non-blocking; call every main loop iteration
    const String &baseUrl() const { return _baseUrl; }

private:
    String _baseUrl;
    unsigned long _lastFetch = 0;
    static const unsigned long FETCH_INTERVAL_MS = 2000;

    void fetchDashboard();
};

extern ApiClient g_apiClient;
