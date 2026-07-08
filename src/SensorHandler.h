#pragma once
#include <Arduino.h>

struct SensorState {
    bool pir_active;
    bool door_open;
    bool vehicle_active;
};

class SensorHandler {
public:
    SensorHandler(uint8_t pin_pir, uint8_t pin_door,
                  uint8_t pin_vehicle, uint8_t pin_bypass);
    void begin();
    SensorState read();
    bool consumePirTrigger();
    bool consumeVehicleTrigger();
    bool consumeBypassTrigger();
    void triggerPirSimulation();
    void clearPirSimulation();
    bool pirSimulationEnabled() const;

private:
    uint8_t _pin_pir, _pin_door, _pin_vehicle, _pin_bypass;
    volatile bool _pir_flag;
    volatile bool _vehicle_flag;
    volatile bool _bypass_flag;
    volatile bool _pir_sim_enabled;
    volatile bool _pir_sim_state;

    static SensorHandler* _inst;
    static void IRAM_ATTR _pirISR();
    static void IRAM_ATTR _vehicleISR();
    static void IRAM_ATTR _bypassISR();
};
