# Pixel

Pixel is a desktop companion robot based on the **Seeed XIAO ESP32S3 Sense** and the **XIAO 1.28″ round display**. It captures microphone audio and (optionally) camera frames, shows animated eyes and UI on the round TFT, and streams each turn to the OmniBot hub over Wi‑Fi so **Gemini** can reply.

Firmware lives in this folder as a **PlatformIO** project (`platformio.ini` + `src/main.cpp`).

## Using Pixel

### Before you start

1. **Run the OmniBot hub** on a PC on the same LAN as Pixel (see the [main README](../../README.md)).
2. **Set the hub address in firmware** — In `src/main.cpp`, `backend_ip` and `backend_port` must point at that machine. Use the PC’s **LAN IP** (not `127.0.0.1`), and the port the hub listens on (`8000` for local dev; **8080** on the host if you use Docker’s default mapping). Rebuild and upload after any change.
3. **Provision Wi‑Fi** — First boot (or after erasing Wi‑Fi) Pixel shows **BLE SETUP**. Use the dashboard **Setup** flow on the PC to scan Bluetooth, select **Pixel**, and send SSID/password.

When Wi‑Fi is saved and the hub is reachable, Pixel opens a WebSocket to `ws://<backend_ip>:<backend_port>/ws/stream`. The dashboard **Dashboard** view shows when the bot is connected.

### Voice and vision

- From the **idle** face, **tap** once to **start** recording (mic + optional camera frames if **VIDEO** is on).
- **Tap** again to **stop** and send the turn to the hub. Pixel shows a short “thinking” animation while Gemini runs, then the reply (and any weather/map overlays the hub sends).
- **Vision** is controlled on the device: **swipe down** from idle → **SETTINGS** → **VIDEO: ON/OFF**. When the hub is connected, that choice stays in sync with **Pixel bot settings** in the UI.

If the hub WebSocket is **down** when you finish a recording, Pixel **does not** upload audio/video for that turn; it clears buffers and returns to the idle face. Wait until the bot is connected again (or fix Wi‑Fi / hub), then try another turn.

### On-device settings (swipe down from idle)

| Control | What it does |
|--------|----------------|
| **VIDEO: ON/OFF** | Include JPEG frames in each turn when ON (matches hub “vision”). |
| **BT SETUP** | Disconnects from the hub, turns **Wi‑Fi off**, and advertises **BLE** so you can send a **new** SSID/password from the dashboard (same flow as first-time setup). |
| **CLR WIFI** | Erases saved SSID/password from flash and **reboots**. Next boot uses BLE setup if nothing is stored. |

**Swipe up** closes the settings screen.

### Clock screen

**Swipe left** from idle opens the time screen (RTC). **Swipe right** or **up** to leave it. The hub can push a timezone rule over the WebSocket when connected so the clock matches **Hub / application** settings.

---

## Wi‑Fi: first connect, retries, and recovery

1. **Flash** firmware with the correct `backend_ip` / `backend_port` in `src/main.cpp`.
2. **Boot** with no saved credentials → screen shows **BLE SETUP**; the device advertises as **`Pixel`**.
3. On the PC, open the hub dashboard **Setup**: scan BLE, choose Pixel, send Wi‑Fi credentials. Pixel stores them, joins Wi‑Fi, then connects to the hub WebSocket.

**If Wi‑Fi fails at boot** (wrong password, AP offline, etc.):

- The firmware tries up to **three** association attempts per boot (~15 s each).
- It tracks a **failure counter** across boots. After **3** failed boots in a row, Pixel falls back to **BLE SETUP** so you can reprovision from the dashboard without re-flashing.
- After **8** failed boots, stored Wi‑Fi credentials are **cleared automatically** as a last-resort recovery; the next boot opens BLE setup.

**Change Wi‑Fi while Pixel already works:** **SETTINGS** → **BT SETUP**, then on the PC use **Setup** (e.g. **Add New Bot**) to scan and send the new network.

**Erase Wi‑Fi only:** **SETTINGS** → **CLR WIFI** (or wait for the automatic clear above). You will need BLE provisioning again.

---

## Hub connection and reconnecting

- **WebSocket to the hub** uses the library’s automatic reconnect: about every **5 seconds** while disconnected, Pixel retries `ws://<backend_ip>:<backend_port>/ws/stream` as long as Wi‑Fi stays up. You do not need to reboot Pixel when the hub was temporarily stopped—start the hub again and wait for the link to come back.
- **Hub IP or port changed:** update `backend_ip` / `backend_port` in `src/main.cpp`, rebuild, and flash. There is no runtime config for the hub address on the device.
- **PC firewall:** allow inbound TCP to the hub port from Pixel’s IP on the LAN.

---

## Hardware

| Component | Part | Notes |
|-----------|------|--------|
| MCU | Seeed XIAO ESP32S3 Sense | PSRAM required for audio + camera pipelines |
| Display | 1.28″ round TFT (GC9A01, 240×240) | SPI |
| Touch | CST816S class controller (I2C) | Gestures processed in firmware |
| RTC | PCF8563 (battery-backed) | I2C; used for clock screen |
| Microphone | Onboard PDM MEMS (Sense) | I2S in firmware |
| Camera | OV2640 (Sense) | DVP pins as in `main.cpp` |

## Build notes (memory)

`platformio.ini` sets **OPI PSRAM** and related flags. Do not remove these if you want simultaneous audio buffering and JPEG capture:

- `board_build.arduino.memory_type = qio_opi`
- `-DBOARD_HAS_PSRAM` and `-mfix-esp32-psram-cache-issue`

## Features (summary)

- **Animated eyes** — idle, recording, uploading (“thinking”), emotion overlays  
- **Voice + optional vision** — tap-driven capture; JPEG only when vision is enabled  
- **Gemini** — hub builds WAV / optional MP4, runs multimodal chat, returns text and tool-driven UI (weather, **map** JPEG `0x04`, face animations)  
- **BLE provisioning** — Wi‑Fi credentials from the PC; no secrets in firmware beyond hub IP/port  

## Touch interaction

Gestures are **short taps** and **swipes** (see thresholds in `main.cpp`: ~20 px drift / 500 ms for tap, 30 px for swipe).

| Gesture | When | Action |
|---------|------|--------|
| **Tap** | Idle | Start recording (audio + optional JPEG frames) |
| **Tap** | Recording | Stop capture, upload turn (`0x03`), show processing animation |
| **Swipe left** | Idle | Open **clock / time** screen |
| **Swipe right** | Time screen open | Close time screen, return to idle eyes |
| **Swipe down** | Idle | Open **on-device settings** (vision, **BT SETUP**, **CLR WIFI**) |
| **Swipe up** | Settings | Return to idle eyes |
| **Swipe up** | Time screen | Close time screen |
| **Tap** | Settings | Toggle **vision**; **BT SETUP** (Wi‑Fi off + BLE); **CLR WIFI** (erase stored Wi‑Fi, reboot); tap outside to exit |

Long **press-and-hold** is not the primary model; recording is started/stopped with **quick taps**.

## WebSocket transport

### Binary frames (device → server)

| Prefix | Payload | Meaning |
|--------|---------|---------|
| `0x01` | Raw PCM | 16-bit little-endian, mono, **16 kHz** |
| `0x02` | JPEG | One frame; backend can assemble ~10 fps MP4 when vision is on |
| `0x03` | (empty) | **End of turn** — process buffers with Gemini |
| `0x04` | JPEG | **Map snapshot** — hub screenshot of the Maps contextual widget (240×240 target); shown while `face_animation` **map** overlay is active |

### JSON (server → device)

Examples sent by the backend:

- `runtime_vision` — `{ "type": "runtime_vision", "enabled": true|false }`
- `runtime_timezone` — `{ "type": "runtime_timezone", "timezone_rule": "..." }`
- `show_weather` — weather overlay duration and condition/temperature
- `face_animation` — speaking / happy / mad / weather / **map** (map mode uses empty `words`; hub may follow with binary `0x04` JPEG)
- `gemini_first_token` — optional UI hint that the model started streaming
- Success/error replies — e.g. `{ "status": "success", "reply": "..." }`

### JSON (device → server)

- `vision_changed` — when the user toggles vision on the settings screen so `bot_settings.json` stays in sync

## Pin reference (display / touch / mic)

| Signal | Pin |
|--------|-----|
| TFT SCK | D8 |
| TFT MOSI | D10 |
| TFT MISO | D9 |
| TFT CS | D1 |
| TFT DC | D3 |
| TFT Backlight | D6 |
| Touch INT | D7 |
| Touch SDA / SCL | D4 / D5 |
| I2S mic WS / SD | GPIO 42 / 41 |

Camera DVP pins are defined under **Camera Pins** in `main.cpp` for the Sense module.

## Libraries (PlatformIO)

From `platformio.ini`:

- `esp32-camera`
- `moononournation/GFX Library for Arduino`
- `bblanchon/ArduinoJson`
- `links2004/WebSockets`
- `adafruit/RTClib`
- `bitbank2/JPEGDEC` (decode hub map JPEGs)

ESP32 BLE and Wi‑Fi come from the Arduino-ESP32 core.

## Related docs

- **OmniBot hub** (backend + dashboard): [`../../README.md`](../../README.md)
