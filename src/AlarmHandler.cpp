#include "AlarmHandler.h"

AlarmHandler::AlarmHandler(uint8_t pin_alarm, uint8_t pin_led)
    : _pin_alarm(pin_alarm), _pin_led(pin_led), _active(false) {}

void AlarmHandler::begin() {
    pinMode(_pin_alarm, OUTPUT);
    pinMode(_pin_led, OUTPUT);
    deactivate();
}

void AlarmHandler::activate() {
    _active = true;
    digitalWrite(_pin_alarm, HIGH);
    digitalWrite(_pin_led, LOW);  // active LOW
    Serial.println("[ALARM] Activated");
}

void AlarmHandler::deactivate() {
    _active = false;
    digitalWrite(_pin_alarm, LOW);
    digitalWrite(_pin_led, HIGH);
    Serial.println("[ALARM] Deactivated");
}
