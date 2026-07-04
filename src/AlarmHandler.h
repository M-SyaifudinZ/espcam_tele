#pragma once
#include <Arduino.h>

class AlarmHandler {
public:
    AlarmHandler(uint8_t pin_alarm, uint8_t pin_led);
    void begin();
    void activate();
    void deactivate();
    bool isActive() const { return _active; }

private:
    uint8_t _pin_alarm;
    uint8_t _pin_led;
    bool _active;
};
