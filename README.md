# OmniBot

Open-source stack for ESP32-based AI robots that talk to Google Gemini over Wi‑Fi, with a FastAPI “brain” and a React control room on your PC.

## Repository layout

```
OmniBot/
├── app/
│   ├── backend/          # FastAPI + Google GenAI (Gemini), BLE provisioning, WebSockets
│   └── frontend/         # React + Vite dashboard
└── bots/
    └── Pixel/            # First bot: Seeed XIAO ESP32S3 Sense + round display (PlatformIO)
```

## What it does

- **Bots** open a WebSocket to the backend and stream **PCM audio** (16-bit, 16 kHz) and optional **JPEG video** frames. A stop packet triggers Gemini to answer using the buffered turn.
- **Gemini** runs in a **per-device chat session** with **Google Search** and tools that drive Pixel’s display (`show_weather`, `face_animation`).
- **Dashboard** connects to a separate monitor WebSocket for live logs, streamed AI text, audio/video previews, and tool telemetry.
- **BLE provisioning** sends Wi‑Fi credentials from the PC to Pixel (no credentials baked into firmware beyond first-hop backend IP/port).

## Prerequisites

- **Python 3** with `pip`
- **Node.js** (for the frontend)
- **Google AI API key** for Gemini ([Google AI Studio](https://aistudio.google.com/))
- **Bluetooth** on the PC (for provisioning real hardware)
- **Windows** for automatic Wi‑Fi SSID scan in the setup UI (`netsh`). On other OSes, enter SSID manually or extend `setup_wifi_networks` in `app/backend/app.py`.

## Configuration

Create `app/backend/.env`:

```env
GEMINI_API_KEY=your_key_here
```

Per-bot settings (model, system instruction, timezone, vision on/off) are stored in `app/backend/bot_settings.json` when you save from the UI or when the device reports vision changes.

## Quick start

### Backend

```bash
cd app/backend
python -m venv .venv
.venv\Scripts\activate          # Windows; on macOS/Linux: source .venv/bin/activate
pip install -r requirements.txt
python app.py
```

The server listens on **http://0.0.0.0:8000** (all interfaces). If you use Conda, activate your environment instead of `venv` before `pip install` / `python app.py`.

### Frontend

```bash
cd app/frontend
npm install
npm run dev
```

The UI expects the API at **http://localhost:8000** and the monitor socket at **ws://localhost:8000/ws/monitor** (see `app/frontend/src/App.jsx` and `setupService.js`). Change these if you run the backend on another host.

### Pixel firmware

Open `bots/Pixel` in **PlatformIO**, set `backend_ip` / `backend_port` in `src/main.cpp`, build, and upload. Details: [`bots/Pixel/README.md`](bots/Pixel/README.md).

## Architecture

```
[Pixel]  ──WebSocket /ws/stream──▶  [FastAPI backend]  ──▶  [Gemini + Google Search + tools]
                                           │
                                           ├── bot_settings.json
                                           │
[React dashboard]  ◀──WebSocket /ws/monitor──┘
        │
        └── REST: setup, settings, text commands, runtime toggles
```

## Frontend (dashboard)

Modes from the sidebar:

- **Dashboard** — intelligence feed, typed commands to Gemini (`POST /api/text-command`), connection status.
- **Setup** — BLE scan (`GET /setup/scan`), Wi‑Fi list (`GET /setup/wifi-networks`), provision (`POST /setup/provision`).
- **Settings** — load/save `GET`/`POST /api/settings/{device_id}` (default target id in UI: `default_bot`).

## Backend API summary

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/ping` | Health check; also broadcasts an `esp32_connected`-style event to monitor clients |
| `GET` | `/setup/scan` | BLE scan; lists devices whose name contains `Pixel` |
| `GET` | `/setup/wifi-networks` | Wi‑Fi SSIDs (Windows `netsh`) |
| `POST` | `/setup/provision` | Write JSON `{"ssid","password"}` to Pixel’s BLE Wi‑Fi characteristic |
| `GET` | `/api/settings/{device_id}` | Read model, system instruction, timezone rule, vision flag |
| `POST` | `/api/settings/{device_id}` | Save the same fields; pushes runtime vision/timezone to the bot if connected |
| `POST` | `/api/text-command` | Body: `{ "message", "device_id" }` — one Gemini turn, stream to UI, reply to bot WebSocket |
| `POST` | `/api/text-command/reset/{device_id}` | Clear in-memory chat session for that device |
| `GET` / `POST` | `/api/runtime/{device_id}/vision` | Get/set whether JPEG frames are assembled into video for the model |
| `GET` / `POST` | `/api/runtime/{device_id}/timezone` | Get/set POSIX TZ string for the bot’s clock sync |
| WebSocket | `/ws/stream` | Binary AV protocol from ESP32; JSON control/response both ways |
| WebSocket | `/ws/monitor` | JSON events for the React dashboard |

### Bot binary protocol (`/ws/stream`)

Each binary message: **1 byte type** + payload.

| Byte | Meaning |
|------|---------|
| `0x01` | PCM audio chunk (16-bit LE, mono, 16 kHz) |
| `0x02` | JPEG video frame |
| `0x03` | End of turn — assemble WAV (+ optional MP4), call Gemini, reply with JSON `{"status","reply"}` or error |

### Monitor WebSocket message types (representative)

Examples the UI handles: `esp32_connected`, `esp32_disconnected`, `processing_started`, `audio_captured`, `video_captured`, `ai_response_stream_*`, `error`, `tool_call`, `vision_changed`, `timezone_changed`.

## Bots

| Bot | Board | Doc |
|-----|-------|-----|
| **Pixel** | Seeed XIAO ESP32S3 Sense + round display | [`bots/Pixel/README.md`](bots/Pixel/README.md) |

## Python dependencies

See `app/backend/requirements.txt` (FastAPI, uvicorn, `google-genai`, Bleak, OpenCV/imageio for video assembly, etc.).

## License / contributing

Add your preferred license and contribution notes here if you publish the repo publicly.
