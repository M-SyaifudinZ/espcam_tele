#include "SensorHandler.h"

SensorHandler* SensorHandler::_inst = nullptr;

void IRAM_ATTR SensorHandler::_pirISR()     { if (_inst) _inst->_pir_flag = true; }
void IRAM_ATTR SensorHandler::_vehicleISR() { if (_inst) _inst->_vehicle_flag = true; }
void IRAM_ATTR SensorHandler::_bypassISR()  { if (_inst) _inst->_bypass_flag = true; }

SensorHandler::SensorHandler(uint8_t pin_pir, uint8_t pin_door,
                              uint8_t pin_vehicle, uint8_t pin_bypass)
    : _pin_pir(pin_pir), _pin_door(pin_door),
      _pin_vehicle(pin_vehicle), _pin_bypass(pin_bypass),
      _pir_flag(false), _vehicle_flag(false), _bypass_flag(false) {
    _inst = this;
}

void SensorHandler::begin() {
    pinMode(_pin_pir,     INPUT);
    pinMode(_pin_door,    INPUT_PULLUP);
    pinMode(_pin_vehicle, INPUT_PULLUP);
    pinMode(_pin_bypass,  INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(_pin_pir),     _pirISR,     RISING);
    attachInterrupt(digitalPinToInterrupt(_pin_vehicle), _vehicleISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(_pin_bypass),  _bypassISR,  FALLING);

    Serial.println("[SENSOR] Ready: PIR, door, vehicle switch, bypass button");
}

SensorState SensorHandler::read() {
    return {
        .pir_active     = digitalRead(_pin_pir) == HIGH,
        .door_open      = digitalRead(_pin_door) == HIGH,
        .vehicle_active = digitalRead(_pin_vehicle) == LOW
    };
}

bool SensorHandler::consumePirTrigger() {
    if (_pir_flag) { _pir_flag = false; return true; }
    return false;
}

bool SensorHandler::consumeVehicleTrigger() {
    if (_vehicle_flag) { _vehicle_flag = false; return true; }
    return false;
}

bool SensorHandler::consumeBypassTrigger() {
    if (_bypass_flag) { _bypass_flag = false; return true; }
    return false;
}
