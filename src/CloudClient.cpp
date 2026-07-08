#include "CloudClient.h"
#include "config.h"
#include <ArduinoJson.h>
#include <WiFi.h>

CloudClient::CloudClient(const char* status_url, const char* image_url, const char* api_key)
    : _status_url(status_url), _image_url(image_url), _api_key(api_key) {}

void CloudClient::_addAuth(HTTPClient& http) {
    http.addHeader("X-API-Key", _api_key);
    http.setTimeout(HTTP_TIMEOUT_MS);
}

bool CloudClient::postStatus(bool pir, bool door, const char* status) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.begin(_status_url);
    http.addHeader("Content-Type", "application/json");
    _addAuth(http);

    StaticJsonDocument<128> doc;
    doc["cloud_status"] = status;
    doc["pir_status"]   = pir ? "triggered" : "idle";
    doc["door_status"]  = door ? "open" : "closed";

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    if (code <= 0) {
        Serial.printf("[CLOUD] status POST failed: %s\n", http.errorToString(code).c_str());
    }
    http.end();
    Serial.printf("[CLOUD] status POST: %d\n", code);
    return code >= 200 && code < 300;
}

bool CloudClient::postImage(camera_fb_t* fb) {
    if (!fb || WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.begin(_image_url);
    http.addHeader("Content-Type", "image/jpeg");
    _addAuth(http);

    int code = http.POST(fb->buf, fb->len);
    if (code <= 0) {
        Serial.printf("[CLOUD] image POST failed: %s\n", http.errorToString(code).c_str());
    }
    http.end();
    Serial.printf("[CLOUD] image POST: %d (%u bytes)\n", code, fb->len);
    return code >= 200 && code < 300;
}
