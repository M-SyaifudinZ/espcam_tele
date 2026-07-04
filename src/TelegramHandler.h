#pragma once
#include <functional>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "CameraHandler.h"
#include "SensorHandler.h"
#include "AlarmHandler.h"

class TelegramHandler {
public:
    TelegramHandler(const char* token, CameraHandler& cam,
                    SensorHandler& sensor, AlarmHandler& alarm);
    void begin(SemaphoreHandle_t cam_mutex);
    void handle();

    void sendPersonal(const String& text);
    void sendPhotoPersonal(camera_fb_t* fb, const String& caption = "📸");
    void broadcastAll(const String& text);

    std::function<void()> onCancelAlarm;

private:
    WiFiClientSecure _tls;
    UniversalTelegramBot _bot;
    CameraHandler& _cam;
    SensorHandler& _sensor;
    AlarmHandler& _alarm;
    SemaphoreHandle_t _camMutex = nullptr;

    void _process(telegramMessage& msg);
    void _cmdMatiAlarm(const String& chat_id);
    void _cmdStatus(const String& chat_id);
    void _cmdFoto(const String& chat_id);
    void _cmdRestart(const String& chat_id);
    void _sendPhoto(const String& chat_id, camera_fb_t* fb, const String& caption);
    void _forEachGroupId(std::function<void(const String&)> fn);
};
