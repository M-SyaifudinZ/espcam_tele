#pragma once
#include <Arduino.h>
#include "esp_camera.h"

class CameraHandler {
public:
    CameraHandler();
    bool begin();
    camera_fb_t* capture();
    void releaseFrame(camera_fb_t* fb);
    bool isReady() const { return _ready; }

private:
    bool _ready;
};
