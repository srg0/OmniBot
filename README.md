# 🤖 OmniBot

An open-source platform for building and managing ESP32-based AI robots connected to your PC via Wi-Fi.

## Structure

```
OmniBot/
├── app/                  ← Central Brain Application
│   ├── backend/          ← Python (FastAPI) server with Gemini AI
│   └── frontend/         ← React dashboard (Vite)
│
└── bots/                 ← ESP32 Robot Firmware
    └── Pixel/            ← First bot: Pixel (Seeed XIAO ESP32S3 Sense + Round Display)
```

## Quick Start

### Backend
```bash
cd app/backend
conda activate desktop-bot
python app.py
```

### Frontend
```bash
cd app/frontend
npm install
npm run dev
```

### Bot Firmware
Open `bots/Pixel/` in PlatformIO and flash to your device. See [`bots/Pixel/README.md`](bots/Pixel/README.md) for full setup details.

## Architecture

```
[Pixel Bot]  ──WebSocket──▶  [FastAPI Backend]  ──▶  [Gemini AI]
                                     │
                             [React Dashboard]
```

The **backend** receives binary audio and video streams from bots over WebSocket, sends them to Gemini, and returns AI responses. The **frontend** lets you manage connected bots, configure their AI models and system instructions, and monitor live activity.

## Bots

| Name | Board | Description |
|------|-------|-------------|
| [**Pixel**](bots/Pixel/README.md) | Seeed XIAO ESP32S3 Sense + Round Display | Desktop AI companion with camera, mic, and animated round screen |

## Backend — Key Endpoints

| Endpoint | Type | Description |
|----------|------|-------------|
| `/ws/stream` | WebSocket | Binary audio/video stream from bots |
| `/api/bots` | REST | List connected bots |
| `/api/bots/{id}/settings` | REST | Get/set per-bot AI model and system instructions |
