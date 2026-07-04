#pragma once

// ─── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID               "YOUR_SSID"
#define WIFI_PASSWORD           "YOUR_PASSWORD"
#define WIFI_TIMEOUT_MS         15000UL

// ─── Telegram ────────────────────────────────────────────────────────────────
#define TG_BOT_TOKEN            "YOUR_BOT_TOKEN"
#define PERSONAL_CHAT_ID        "YOUR_PERSONAL_CHAT_ID"
#define GROUP_CHAT_IDS          ""   // "id1,id2" atau kosong
#define TG_POLL_MS              1000UL

// ─── Cloud ───────────────────────────────────────────────────────────────────
#define CLOUD_STATUS_URL        "https://your-server.com/api/event"
#define CLOUD_IMAGE_URL         "https://your-server.com/api/image"
#define CLOUD_API_KEY           "YOUR_API_KEY"

// ─── GPIO ────────────────────────────────────────────────────────────────────
#define PIN_PIR                 13   // PIR BTE16-19
#define PIN_DOOR                14   // Magnetic switch (INPUT_PULLUP, HIGH=terbuka)
#define PIN_VEHICLE_SWITCH      15   // Limit/trigger switch kendaraan (INPUT_PULLUP, LOW=trigger)
#define PIN_BYPASS_BUTTON       2    // Tombol bypass fisik (INPUT_PULLUP, LOW=ditekan)
#define PIN_ALARM               12   // Relay sirine (HIGH=aktif, pastikan LOW saat boot)
#define PIN_STATUS_LED          33   // On-board LED (active LOW)

// ─── Timing ──────────────────────────────────────────────────────────────────
#define DOOR_ALARM_TIMEOUT_MS   60000UL
#define HUMAN_COOLDOWN_MS       30000UL
#define SENSOR_INTERVAL_MS      500UL
#define HTTP_TIMEOUT_MS         10000
