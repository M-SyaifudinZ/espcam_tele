#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "CameraHandler.h"
#include "SensorHandler.h"
#include "AlarmHandler.h"
#include "CloudClient.h"
#include "TelegramHandler.h"

// ─── Komponen ─────────────────────────────────────────────────────────────────
static CameraHandler camera;
static SensorHandler sensor(PIN_PIR, PIN_DOOR, PIN_VEHICLE_SWITCH, PIN_BYPASS_BUTTON);
static AlarmHandler  alarmHandler(PIN_ALARM, PIN_STATUS_LED);
static CloudClient   cloud(CLOUD_STATUS_URL, CLOUD_IMAGE_URL, CLOUD_API_KEY);
static TelegramHandler* tg = nullptr;

// ─── Shared state (cross-task, volatile) ─────────────────────────────────────
static SemaphoreHandle_t g_camMutex = nullptr;

static volatile bool     g_doorTimerRunning = false;
static volatile bool     g_alarmActive      = false;
static unsigned long     g_doorOpenTime      = 0;   // hanya ditulis dari sensorTask

static volatile bool g_notifDoorAlarm = false;

static bool g_lastPirState = false;
static bool g_lastPirStateInitialized = false;
static String g_serialLine;

static void handleSerialDebugCommands();

// ─── WiFi ─────────────────────────────────────────────────────────────────────
static void wifiConnect() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[WiFi] Connecting");
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_TIMEOUT_MS) {
        delay(500); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Timeout — restart");
        ESP.restart();
    }
}

// ─── Task: Telegram polling + notifikasi (Core 0) ────────────────────────────
static void tgTask(void*) {
    unsigned long lastPoll = 0;
    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            unsigned long now = millis();

            // Poll perintah bot
            if (now - lastPoll >= 5000UL) {
                lastPoll = now;
                tg->handle();
            }

            // Notif alarm pintu — broadcast ke semua chat
            if (g_notifDoorAlarm) {
                g_notifDoorAlarm = false;
                tg->broadcastAll("🚨 ALARM! Pintu terbuka lebih dari 1 menit tanpa respons!");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ─── Task: Sensor + Cloud + Door Alarm (Core 1) ───────────────────────────────
static void sensorTask(void*) {
    unsigned long lastCheck = 0;
    for (;;) {
        handleSerialDebugCommands();

        unsigned long now = millis();
        if (now - lastCheck < SENSOR_INTERVAL_MS) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        lastCheck = now;

        SensorState state = sensor.read();

        if (!g_lastPirStateInitialized || state.pir_active != g_lastPirState) {
            g_lastPirState = state.pir_active;
            g_lastPirStateInitialized = true;
            Serial.printf("[PIR] State: %s (%s)\n",
                          state.pir_active ? "HIGH/TRIGGERED" : "LOW/IDLE",
                          sensor.pirSimulationEnabled() ? "SIM" : "HW");
        }

        // ── Door alarm timer ──────────────────────────────────────────────────
        if (state.door_open && !g_doorTimerRunning && !g_alarmActive) {
            g_doorTimerRunning = true;
            g_doorOpenTime = millis();
            Serial.println("[DOOR] Terbuka — timer 60s dimulai");

        } else if (!state.door_open && g_doorTimerRunning && !g_alarmActive) {
            g_doorTimerRunning = false;
            Serial.println("[DOOR] Tertutup — timer dibatalkan");

        } else if (g_doorTimerRunning && !g_alarmActive) {
            if (millis() - g_doorOpenTime >= DOOR_ALARM_TIMEOUT_MS) {
                g_doorTimerRunning = false;
                g_alarmActive = true;
                alarmHandler.activate();
                g_notifDoorAlarm = true;
                Serial.println("[DOOR] ALARM AKTIF — timeout 1 menit");
            }
        }

        // ── Bypass button fisik ───────────────────────────────────────────────
        if (sensor.consumeBypassTrigger()) {
            g_doorTimerRunning = false;
            g_alarmActive = false;
            alarmHandler.deactivate();
            Serial.println("[BYPASS] Alarm dimatikan via tombol fisik");
        }

        // ── PIR — deteksi manusia ─────────────────────────────────────────────
        if (sensor.consumePirTrigger()) {
            Serial.println("[SENSOR] PIR trigger");
            cloud.postStatus(true, state.door_open, "motion_detected");

            if (xSemaphoreTake(g_camMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
                camera_fb_t* fb = camera.capture();
                if (fb) {
                    cloud.postImage(fb);
                    camera.releaseFrame(fb);
                }
                xSemaphoreGive(g_camMutex);
            }
        }

        // ── Vehicle limit switch ──────────────────────────────────────────────
        if (sensor.consumeVehicleTrigger()) {
            cloud.postStatus(false, state.door_open, "vehicle_detected");
            Serial.println("[SENSOR] Kendaraan terdeteksi via limit switch");
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void handleSerialDebugCommands() {
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (g_serialLine.length() == 0) {
                continue;
            }

            g_serialLine.trim();
            if (g_serialLine.equalsIgnoreCase("pir on") || g_serialLine.equalsIgnoreCase("pir trigger") || g_serialLine.equalsIgnoreCase("pir pulse")) {
                sensor.triggerPirSimulation();
                Serial.println("[SIM] PIR = HIGH/TRIGGERED");
            } else if (g_serialLine.equalsIgnoreCase("pir off") || g_serialLine.equalsIgnoreCase("pir low")) {
                sensor.clearPirSimulation();
                Serial.println("[SIM] PIR = LOW/IDLE");
            } else if (g_serialLine.equalsIgnoreCase("pir status")) {
                SensorState state = sensor.read();
                Serial.printf("[SIM] PIR status: %s (%s)\n",
                              state.pir_active ? "HIGH/TRIGGERED" : "LOW/IDLE",
                              sensor.pirSimulationEnabled() ? "SIM" : "HW");
            } else {
                Serial.printf("[SIM] Unknown command: %s\n", g_serialLine.c_str());
                Serial.println("[SIM] Commands: pir on | pir off | pir status");
            }

            g_serialLine = "";
        } else {
            if (g_serialLine.length() < 64) {
                g_serialLine += c;
            } else {
                g_serialLine = "";
            }
        }
    }
}

// ─── Setup & Loop ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n[BOOT] ESP32-CAM Security Unit");
    Serial.printf("[BOOT] Free heap: %u\n", ESP.getFreeHeap());

    g_camMutex = xSemaphoreCreateMutex();

    Serial.println("[BOOT] Camera init...");
    camera.begin();
    Serial.println("[BOOT] Sensor init...");
    sensor.begin();
    Serial.println("[BOOT] Alarm init...");
    alarmHandler.begin();

    Serial.println("[BOOT] WiFi connect...");
    wifiConnect();

    Serial.println("[BOOT] Telegram init...");
    tg = new TelegramHandler(TG_BOT_TOKEN, camera, sensor, alarmHandler);
    tg->begin(g_camMutex);

    tg->onCancelAlarm = []() {
        g_doorTimerRunning = false;
        g_alarmActive = false;
        alarmHandler.deactivate();
        Serial.println("[TG] /matialarm — alarm dibatalkan");
    };

    Serial.println("[BOOT] Creating tasks...");
    xTaskCreatePinnedToCore(tgTask,     "TGTask",     10240, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(sensorTask, "SensorTask",  4096, nullptr, 1, nullptr, 1);

    Serial.println("[BOOT] Semua task berjalan");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Reconnecting...");
        wifiConnect();
    }
    delay(5000);
}
