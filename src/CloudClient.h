#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include "esp_camera.h"

class CloudClient {
public:
    CloudClient(const char* status_url, const char* image_url, const char* api_key);
    bool postStatus(bool pir, bool door, const char* status);
    bool postImage(camera_fb_t* fb);

private:
    const char* _status_url;
    const char* _image_url;
    const char* _api_key;
    void _addAuth(HTTPClient& http);
};
