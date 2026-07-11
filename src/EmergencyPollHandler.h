#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include "AlarmHandler.h"

class EmergencyPollHandler {
public:
    EmergencyPollHandler(const char* url, const char* myDeviceId, AlarmHandler& alarm);

    void begin();
    void loop();   // panggil berkala dari task

private:
    static const unsigned long POLL_INTERVAL_MS = 2000;

    String        _url;
    const char*   _myDeviceId;
    AlarmHandler& _alarm;
    unsigned long _lastPoll = 0;

    void checkEmergency();
};