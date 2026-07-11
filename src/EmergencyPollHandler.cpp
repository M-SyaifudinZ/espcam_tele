#include "EmergencyPollHandler.h"
#include "config_public.h"
#ifdef __has_include
#if __has_include("config.h")
#include "config.h"
#endif
#endif
#include <ArduinoJson.h>

EmergencyPollHandler::EmergencyPollHandler(const char* url, const char* myDeviceId, AlarmHandler& alarm)
    : _url(url), _myDeviceId(myDeviceId), _alarm(alarm) {}

void EmergencyPollHandler::begin() {
    Serial.println("[EMG] Emergency polling aktif (plain HTTP, no TLS)");
}

void EmergencyPollHandler::loop() {
    unsigned long now = millis();
    if (now - _lastPoll < POLL_INTERVAL_MS) return;
    _lastPoll = now;

    if (WiFi.status() != WL_CONNECTED) return;
    checkEmergency();
}

void EmergencyPollHandler::checkEmergency() {
    HTTPClient http;
    http.begin(_url);
    http.addHeader("X-API-Key", CLOUD_API_KEY );   
    http.setTimeout(3000);

    int code = http.GET();

    if (code == 200) {
        // ... (sama seperti sebelumnya)
    } else if (code < 0) {
        Serial.printf("[EMG] Connection error: %s\n", http.errorToString(code).c_str());
    } else {
        Serial.printf("[EMG] HTTP error: %d\n", code);   // ← tambahan, biar 401/404/dll kelihatan di log, gak silent
    }

    http.end();
}