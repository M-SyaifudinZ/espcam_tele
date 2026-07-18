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
    http.addHeader("X-API-Key", CLOUD_API_KEY);   
    http.setTimeout(3000);

    int code = http.GET();

    if (code == 200) {
        String payload = http.getString();
        
        StaticJsonDocument<256> doc; 
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            const char* triggeredDeviceId = doc["device"]; 

            if (triggeredDeviceId != nullptr) {
                if (strcmp(triggeredDeviceId, _myDeviceId) != 0) {
                    Serial.println("[EMG] Bahaya dari device LAIN! Menyalakan buzzer...");
                    _alarm.activate(); 
                } else {
                    Serial.println("[EMG] Bahaya dari device INI. Abaikan buzzer.");
                }
            } 
            // Bagian else dihapus biar nggak nyepam saat nggak ada data emergency
            
        } else {
            // Hanya nge-print kalau format JSON-nya benar-benar rusak
            Serial.printf("[EMG] Gagal parsing JSON: %s\n", error.c_str());
        }
    } else if (code < 0) {
        Serial.printf("[EMG] Connection error: %s\n", http.errorToString(code).c_str());
    } else {
        // Bisa di-comment juga kalau API servermu sering balikin 404/401 dan kamu gak mau lihat log-nya
        Serial.printf("[EMG] HTTP error: %d\n", code);   
    }

    http.end();
}