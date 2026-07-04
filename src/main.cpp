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
static AlarmHandler  alarm(PIN_ALARM, PIN_STATUS_LED);
static CloudClient   cloud(CLOUD_STATUS_URL, CLOUD_IMAGE_URL, CLOUD_API_KEY);
static TelegramHandler* tg = nullptr;

// ─── Shared state (cross-task, volatile) ─────────────────────────────────────
static SemaphoreHandle_t g_camMutex = nullptr;

static volatile bool     g_doorTimerRunning = false;
static volatile bool     g_alarmActive      = false;
static unsigned long     g_doorOpenTime      = 0;   // hanya ditulis dari sensorTask

static volatile bool g_notifHuman     = false;   // sensor set, TG clear
static volatile bool g_notifVehicle   = false;
static volatile bool g_notifDoorAlarm = false;

static unsigned long g_lastHumanMs = 0;           // hanya ditulis dari sensorTask

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
            if (now - lastPoll >= TG_POLL_MS) {
                lastPoll = now;
                tg->handle();
            }

            // Notif manusia — kirim teks + foto ke personal
            if (g_notifHuman) {
                g_notifHuman = false;
                tg->sendPersonal("⚠️ TERDETEKSI MANUSIA");
                if (xSemaphoreTake(g_camMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                    camera_fb_t* fb = camera.capture();
                    if (fb) {
                        tg->sendPhotoPersonal(fb, "📸 Deteksi Manusia");
                        camera.releaseFrame(fb);
                    }
                    xSemaphoreGive(g_camMutex);
                }
            }

            // Notif kendaraan — teks saja ke personal
            if (g_notifVehicle) {
                g_notifVehicle = false;
                tg->sendPersonal("🚗 TERDETEKSI KENDARAAN");
            }

            // Notif alarm pintu — broadcast ke semua chat
            if (g_notifDoorAlarm) {
                g_notifDoorAlarm = false;
                tg->broadcastAll("🚨 ALARM! Pintu terbuka lebih dari 1 menit tanpa respons!");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ─── Task: Sensor + Cloud + Door Alarm (Core 1) ───────────────────────────────
static void sensorTask(void*) {
    unsigned long lastCheck = 0;
    for (;;) {
        unsigned long now = millis();
        if (now - lastCheck < SENSOR_INTERVAL_MS) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        lastCheck = now;

        SensorState state = sensor.read();

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
                alarm.activate();
                g_notifDoorAlarm = true;
                Serial.println("[DOOR] ALARM AKTIF — timeout 1 menit");
            }
        }

        // ── Bypass button fisik ───────────────────────────────────────────────
        if (sensor.consumeBypassTrigger()) {
            g_doorTimerRunning = false;
            g_alarmActive = false;
            alarm.deactivate();
            Serial.println("[BYPASS] Alarm dimatikan via tombol fisik");
        }

        // ── PIR — deteksi manusia ─────────────────────────────────────────────
        if (sensor.consumePirTrigger()) {
            cloud.postStatus(true, state.door_open, "motion_detected");
            if (xSemaphoreTake(g_camMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
                camera_fb_t* fb = camera.capture();
                if (fb) { cloud.postImage(fb); camera.releaseFrame(fb); }
                xSemaphoreGive(g_camMutex);
            }
            // Notif Telegram dengan cooldown
            if (millis() - g_lastHumanMs >= HUMAN_COOLDOWN_MS) {
                g_lastHumanMs = millis();
                g_notifHuman = true;
            }
        }

        // ── Vehicle limit switch ──────────────────────────────────────────────
        if (sensor.consumeVehicleTrigger()) {
            cloud.postStatus(false, state.door_open, "vehicle_detected");
            g_notifVehicle = true;
            Serial.println("[SENSOR] Kendaraan terdeteksi via limit switch");
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ─── Setup & Loop ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n[BOOT] ESP32-CAM Security Unit");

    g_camMutex = xSemaphoreCreateMutex();

    camera.begin();
    sensor.begin();
    alarm.begin();

    wifiConnect();

    tg = new TelegramHandler(TG_BOT_TOKEN, camera, sensor, alarm);
    tg->begin(g_camMutex);

    tg->onCancelAlarm = []() {
        g_doorTimerRunning = false;
        g_alarmActive = false;
        alarm.deactivate();
        Serial.println("[TG] /matialarm — alarm dibatalkan");
    };

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
