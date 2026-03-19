# 🤖 Pixel

Pixel is a desktop AI companion bot built on the Seeed XIAO ESP32S3 Sense and the XIAO Round Display. It has a camera, microphone, animated round face, and connects to the OmniBot backend over Wi-Fi to talk to Gemini AI.

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | Seeed XIAO ESP32S3 Sense | — |
| Display | 1.28" Round TFT (GC9A01, 240×240) | SPI |
| Touch Controller | CST816S (gesture + tap detection) | I2C |
| RTC | PCF8563 (CR1220 battery-backed) | I2C |
| Microphone | PDM MEMS mic (onboard Sense module) | I2S |
| Camera | OV2640 (onboard Sense module) | DVP |

## Features

- 🤖 **Animated eyes** — idle blinking face with randomized blink timing
- 🎙️ **Voice + vision input** — tap to record audio and stream video frames live
- 🧠 **Gemini AI** — streams audio/video to backend; Gemini processes and responds
- 📡 **BLE Wi-Fi provisioning** — no hardcoded credentials; set up via the OmniBot web app
- ⚙️ **Per-bot settings** — AI model and system instructions configurable from the dashboard
- 🕐 **Clock & Timer** *(planned)* — swipe gestures to show NTP-synced time (PCF8563-backed) and a stopwatch

## Interaction

| Gesture | Action |
|---------|--------|
| **Tap & hold** | Start recording (audio + video streamed to Gemini) |
| **Tap & hold again** | Stop recording and trigger AI response |
| **Swipe right** *(planned)* | Cycle: Idle → Clock → Timer |
| **Swipe left** *(planned)* | Go back a screen |

## OmniBot Binary WebSocket Protocol

Pixel communicates with the backend over a single WebSocket connection. Each binary message is prefixed with a 1-byte type header:

| Prefix | Meaning |
|--------|---------|
| `0x01` | Audio chunk (raw PCM, 16-bit, 16kHz) |
| `0x02` | Video frame (JPEG) |
| `0x03` | Stop signal — tell backend to run Gemini |

## Flashing

1. Open `bots/Pixel/` in **PlatformIO**
2. Set your backend IP in `src/main.cpp`:
   ```cpp
   const char* backend_ip = "YOUR_PC_LOCAL_IP";
   ```
3. Upload to your XIAO ESP32S3 Sense
4. On first boot, Pixel will advertise over BLE as `"Pixel"` — open the OmniBot dashboard to provision Wi-Fi credentials

## Pin Reference

| Signal | Pin |
|--------|-----|
| TFT SCK | D8 |
| TFT MOSI | D10 |
| TFT MISO | D9 |
| TFT CS | D1 |
| TFT DC | D3 |
| TFT Backlight | D6 |
| Touch Interrupt | D7 |
| I2S WS (Mic) | GPIO 42 |
| I2S SD (Mic) | GPIO 41 |
