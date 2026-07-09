#include "TelegramHandler.h"
#include "config_public.h"
#ifdef __has_include
#if __has_include("config.h")
#include "config.h"
#endif
#endif
#include <WiFi.h>

namespace {
camera_fb_t* sPhotoFb = nullptr;
size_t sPhotoOffset = 0;

bool photoMoreDataAvailable() {
    return sPhotoFb != nullptr && sPhotoOffset < sPhotoFb->len;
}

byte photoGetNextByte() {
    if (!sPhotoFb || sPhotoOffset >= sPhotoFb->len) return 0;
    return sPhotoFb->buf[sPhotoOffset++];
}
}  // namespace

TelegramHandler::TelegramHandler(const char* token, CameraHandler& cam,
                                  SensorHandler& sensor, AlarmHandler& alarm)
    : _bot(token, _tls), _cam(cam), _sensor(sensor), _alarm(alarm) {}

void TelegramHandler::begin(SemaphoreHandle_t cam_mutex) {
    _tls.setInsecure();
    _tls.setTimeout(5000);
    _camMutex = cam_mutex;
    Serial.println("[TG] Ready");
}

void TelegramHandler::handle() {
    int n = _bot.getUpdates(_bot.last_message_received + 1);
    while (n) {
        for (int i = 0; i < n; i++) {
            _process(_bot.messages[i]);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        n = _bot.getUpdates(_bot.last_message_received + 1);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ─── Direct send ─────────────────────────────────────────────────────────────

void TelegramHandler::sendPersonal(const String& text) {
    _bot.sendMessage(PERSONAL_CHAT_ID, text, "");
    vTaskDelay(pdMS_TO_TICKS(1));
}

void TelegramHandler::sendPhotoPersonal(camera_fb_t* fb, const String& caption) {
    if (!fb) return;
    _sendPhoto(PERSONAL_CHAT_ID, fb, caption);
}

void TelegramHandler::broadcastAll(const String& text) {
    _bot.sendMessage(PERSONAL_CHAT_ID, text, "");
    _forEachGroupId([&](const String& id) {
        _bot.sendMessage(id, text, "");
        vTaskDelay(pdMS_TO_TICKS(1));
    });
    vTaskDelay(pdMS_TO_TICKS(1));
}

// ─── Commands ────────────────────────────────────────────────────────────────

void TelegramHandler::_process(telegramMessage& msg) {
    String text = msg.text;
    String id   = msg.chat_id;
    Serial.printf("[TG] cmd: '%s' from %s\n", text.c_str(), id.c_str());

    if (text == "/matialarm")  _cmdMatiAlarm(id);
    else if (text == "/status") _cmdStatus(id);
    else if (text == "/foto")   _cmdFoto(id);
    else if (text == "/restart") _cmdRestart(id);
}

void TelegramHandler::_cmdMatiAlarm(const String& chat_id) {
    if (onCancelAlarm) onCancelAlarm();
    _bot.sendMessage(chat_id, "✅ Alarm ESP32 dimatikan", "");
    vTaskDelay(pdMS_TO_TICKS(1));
}

void TelegramHandler::_cmdStatus(const String& chat_id) {
    SensorState s = _sensor.read();
    String msg = "📊 STATUS ESP32-CAM\n";
    msg += "📶 WiFi RSSI  : " + String(WiFi.RSSI()) + " dBm\n";
    msg += "🚨 PIR        : "; msg += s.pir_active ? "TRIGGERED" : "Idle"; msg += "\n";
    msg += "🚪 Pintu      : "; msg += s.door_open ? "Terbuka" : "Tertutup"; msg += "\n";
    msg += "🚗 Kendaraan  : "; msg += s.vehicle_active ? "TERDETEKSI" : "Kosong"; msg += "\n";
    msg += "🔊 Alarm      : "; msg += _alarm.isActive() ? "AKTIF" : "OFF";
    _bot.sendMessage(chat_id, msg, "");
    vTaskDelay(pdMS_TO_TICKS(1));
}

void TelegramHandler::_cmdFoto(const String& chat_id) {
    _bot.sendMessage(chat_id, "📸 Mengambil foto...", "");
    if (!_camMutex) { _bot.sendMessage(chat_id, "❌ Camera mutex error", ""); return; }

    if (xSemaphoreTake(_camMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        camera_fb_t* fb = _cam.capture();
        if (fb) {
            _sendPhoto(chat_id, fb, "📸 Foto manual");
            _cam.releaseFrame(fb);
        } else {
            _bot.sendMessage(chat_id, "❌ Gagal capture", "");
        }
        xSemaphoreGive(_camMutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    } else {
        _bot.sendMessage(chat_id, "⏳ Kamera sedang digunakan, coba lagi", "");
    }
}

void TelegramHandler::_cmdRestart(const String& chat_id) {
    _bot.sendMessage(chat_id, "🔄 Restarting...", "");
    delay(300);
    ESP.restart();
}

void TelegramHandler::_sendPhoto(const String& chat_id, camera_fb_t* fb,
                                  const String& caption) {
    sPhotoFb = fb;
    sPhotoOffset = 0;

    _bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len,
                           photoMoreDataAvailable, photoGetNextByte,
                           nullptr, nullptr);
    vTaskDelay(pdMS_TO_TICKS(1));

    sPhotoFb = nullptr;
    sPhotoOffset = 0;

    if (caption.length() > 0) {
        _bot.sendMessage(chat_id, caption, "");
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void TelegramHandler::_forEachGroupId(std::function<void(const String&)> fn) {
    String ids = GROUP_CHAT_IDS;
    if (ids.length() == 0) return;
    int start = 0;
    while (true) {
        int comma = ids.indexOf(',', start);
        String id = (comma == -1) ? ids.substring(start) : ids.substring(start, comma);
        id.trim();
        if (id.length() > 0) fn(id);
        if (comma == -1) break;
        start = comma + 1;
    }
}
