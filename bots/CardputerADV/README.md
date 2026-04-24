# Cardputer ADV

`CardputerADV` is a separate OmniBot firmware target for `M5Stack Cardputer ADV`.

It is intentionally not a fork of `bots/Pixel`:

- `Pixel` is a round-display, camera-first companion bot.
- `Cardputer ADV` is a keyboard-first device with a rectangular LCD.
- This port keeps OmniBot hub compatibility, but changes the device UX and hardware assumptions.

## What Works

- BLE provisioning with the same OmniBot service/characteristic UUIDs as `Pixel`
- Wi-Fi + hub endpoint storage in NVS
- WebSocket registration to `/ws/stream`
- `device_hello` handshake as `cardputer_adv`
- OpenAI-backed typed chat turns via `POST /api/device-text-turn`
- OpenAI-backed push-to-talk voice turns via `POST /api/device-audio-turn`
- OpenAI PCM TTS playback via `POST /api/device-tts`
- On-screen eyes/chat UI for recent messages, status, and input
- `BtnA` or `Fn` push-to-talk trigger on the device

## Current Scope

This ADV port is intentionally camera-free and optimized for a simple voice agent loop.

Not implemented yet:

- camera / vision
- wake-word / hands-free capture
- presence scan / face enrollment

The hub advertises these capability limits so the UI can hide unsupported controls.

## Build

```bash
cd bots/CardputerADV
pio run
```

Flash:

```bash
pio run --target upload
```

Monitor:

```bash
pio run --target monitor
```

## Provisioning

On first boot without Wi-Fi credentials, the firmware enters BLE setup mode automatically.

From the OmniBot hub:

1. Open `Add New Bot`
2. Scan over Bluetooth
3. Send Wi-Fi credentials
4. The hub also sends its LAN IP and port

The device stores both and reboots.

## Local Commands

Type these on the Cardputer and press `Enter`:

- `/help`
- `/status`
- `/btsetup`
- `/clearwifi`

`Tab` is a shortcut for `/help`.

## Voice Flow

1. Hold `BtnA` or `Fn`
2. Speak into the Cardputer mic
3. Release the trigger
4. The hub transcribes speech with OpenAI, generates a reply, and streams PCM back
5. The Cardputer shows transcript + reply and plays the spoken answer through its speaker
