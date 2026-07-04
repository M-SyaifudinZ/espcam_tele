# espcam_tele — ESP32-CAM Security Unit

Sistem keamanan secondary unit berbasis ESP32-CAM AI Thinker dengan PIR, limit switch kendaraan, magnetic switch pintu, door alarm lokal, HTTP POST ke cloud, dan Telegram Bot non-blocking (FreeRTOS dual-core).

## Hardware Wiring

| Komponen              | GPIO | Keterangan                             |
|-----------------------|------|----------------------------------------|
| PIR BTE16-19 OUT      | 13   | Interrupt RISING                       |
| Magnetic Switch       | 14   | INPUT_PULLUP, HIGH = terbuka           |
| Limit Switch Kendaraan| 15   | INPUT_PULLUP, FALLING = kendaraan      |
| Bypass Button         | 2    | INPUT_PULLUP, FALLING = ditekan        |
| Relay/Alarm OUT       | 12   | HIGH = aktif (jaga LOW saat boot!)     |
| LED Status            | 33   | On-board, active LOW                   |
| Kamera OV3660         | —    | Built-in AI-Thinker                    |

> **Penting:** GPIO 12 harus LOW saat boot. Pastikan relay tidak menarik HIGH sebelum ESP32 selesai booting.
> GPIO 2 (bypass button) memiliki on-board LED — normal digunakan sebagai input.

## Yang Diubah di `src/config.h`

| Define | Isi |
|--------|-----|
| `WIFI_SSID` | Nama WiFi |
| `WIFI_PASSWORD` | Password WiFi |
| `TG_BOT_TOKEN` | Token dari @BotFather |
| `PERSONAL_CHAT_ID` | ID chat pribadi |
| `GROUP_CHAT_IDS` | `"id1,id2"` atau `""` |
| `CLOUD_STATUS_URL` | Endpoint HTTP POST JSON |
| `CLOUD_IMAGE_URL` | Endpoint HTTP POST image |
| `CLOUD_API_KEY` | API key server |
| `PIN_*` | Sesuaikan wiring fisik |
| `DOOR_ALARM_TIMEOUT_MS` | Default 60000 (60 detik) |

## Instalasi PlatformIO

```bash
pip install platformio

git clone https://github.com/M-SyaifudinZ/espcam_tele
cd espcam_tele

# Edit konfigurasi
nano src/config.h

# Mode flash: hubungkan GPIO0 ke GND, tekan reset
pio run --target upload

# Monitor serial
pio device monitor
```

## Payload Cloud (HTTP POST JSON)

```json
{
  "cloud_status": "motion_detected",
  "pir_status": "triggered",
  "door_status": "open"
}
```

Gambar dikirim sebagai raw JPEG ke `CLOUD_IMAGE_URL` dengan header `Content-Type: image/jpeg`.

## Perintah Telegram Bot

| Perintah | Fungsi |
|----------|--------|
| `/matialarm` | Matikan alarm lokal ESP32 |
| `/status` | WiFi RSSI, PIR, pintu, kendaraan, alarm |
| `/foto` | Capture + kirim foto ke chat |
| `/restart` | `ESP.restart()` |

## Logika Sistem

- **PIR trigger** → HTTP POST status + image ke cloud + notif "TERDETEKSI MANUSIA" ke Telegram pribadi (cooldown 30 detik)
- **Limit switch kendaraan** → HTTP POST status ke cloud + notif "TERDETEKSI KENDARAAN" ke Telegram pribadi
- **Pintu terbuka > 1 menit** → Sirine ON + broadcast ke SEMUA Telegram
- **Bypass** (tombol fisik atau `/matialarm`) → Sirine OFF, timer reset
- **FreeRTOS** → Core 0: Telegram polling, Core 1: sensor + cloud + door alarm
- **Camera mutex** → kamera aman diakses dari dua core tanpa konflik