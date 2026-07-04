#include "TelegramHandler.h"
#include "config.h"
#include <WiFi.h>

TelegramHandler::TelegramHandler(const char* token, CameraHandler& cam,
                                  SensorHandler& sensor, AlarmHandler& alarm)
    : _bot(token, _tls), _cam(cam), _sensor(sensor), _alarm(alarm) {}

void TelegramHandler::begin(SemaphoreHandle_t cam_mutex) {
    _tls.setInsecure();
    _camMutex = cam_mutex;
    Serial.println("[TG] Ready");
}

void TelegramHandler::handle() {
    int n = _bot.getUpdates(_bot.last_message_received + 1);
    while (n) {
        for (int i = 0; i < n; i++) _process(_bot.messages[i]);
        n = _bot.getUpdates(_bot.last_message_received + 1);
    }
}

// ─── Direct send ─────────────────────────────────────────────────────────────

void TelegramHandler::sendPersonal(const String& text) {
    _bot.sendMessage(PERSONAL_CHAT_ID, text, "");
}

void TelegramHandler::sendPhotoPersonal(camera_fb_t* fb, const String& caption) {
    if (!fb) return;
    _sendPhoto(PERSONAL_CHAT_ID, fb, caption);
}

void TelegramHandler::broadcastAll(const String& text) {
    _bot.sendMessage(PERSONAL_CHAT_ID, text, "");
    _forEachGroupId([&](const String& id) {
        _bot.sendMessage(id, text, "");
    });
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
    uint8_t* data  = fb->buf;
    size_t   total = fb->len;
    size_t   sent  = 0;

    _bot.sendPhotoByBinary(
        chat_id, "image/jpeg", total,
        [data, &sent, total](WiFiClient* client) -> bool {
            const size_t CHUNK = 1024;
            while (sent < total) {
                size_t n = min(CHUNK, total - sent);
                if (client->write(data + sent, n) != n) return false;
                sent += n;
            }
            return true;
        },
        caption, ""
    );
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
