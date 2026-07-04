#include "CameraHandler.h"

#define CAM_PWDN  32
#define CAM_RESET -1
#define CAM_XCLK   0
#define CAM_SIOD  26
#define CAM_SIOC  27
#define CAM_D7    35
#define CAM_D6    34
#define CAM_D5    39
#define CAM_D4    36
#define CAM_D3    21
#define CAM_D2    19
#define CAM_D1    18
#define CAM_D0     5
#define CAM_VSYNC 25
#define CAM_HREF  23
#define CAM_PCLK  22

CameraHandler::CameraHandler() : _ready(false) {}

bool CameraHandler::begin() {
    camera_config_t cfg;
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.pin_d0 = CAM_D0; cfg.pin_d1 = CAM_D1;
    cfg.pin_d2 = CAM_D2; cfg.pin_d3 = CAM_D3;
    cfg.pin_d4 = CAM_D4; cfg.pin_d5 = CAM_D5;
    cfg.pin_d6 = CAM_D6; cfg.pin_d7 = CAM_D7;
    cfg.pin_xclk     = CAM_XCLK;
    cfg.pin_pclk     = CAM_PCLK;
    cfg.pin_vsync    = CAM_VSYNC;
    cfg.pin_href     = CAM_HREF;
    cfg.pin_sscb_sda = CAM_SIOD;
    cfg.pin_sscb_scl = CAM_SIOC;
    cfg.pin_pwdn     = CAM_PWDN;
    cfg.pin_reset    = CAM_RESET;
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_JPEG;

    if (psramFound()) {
        cfg.frame_size   = FRAMESIZE_UXGA;
        cfg.jpeg_quality = 10;
        cfg.fb_count     = 2;
    } else {
        cfg.frame_size   = FRAMESIZE_SVGA;
        cfg.jpeg_quality = 12;
        cfg.fb_count     = 1;
    }

    if (esp_camera_init(&cfg) != ESP_OK) {
        Serial.println("[CAM] Init failed");
        return false;
    }
    _ready = true;
    Serial.println("[CAM] Ready");
    return true;
}

camera_fb_t* CameraHandler::capture() {
    if (!_ready) return nullptr;
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) Serial.println("[CAM] Capture failed");
    return fb;
}

void CameraHandler::releaseFrame(camera_fb_t* fb) {
    if (fb) esp_camera_fb_return(fb);
}
