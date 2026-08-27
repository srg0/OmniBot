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
- `/rfid` — open RFID Lab for a connected M5Stack Unit RFID/RFID2 / MFRC522 I2C reader
- `/sonar` — open the HC-SR04 Sonar / Proximity screen

`Tab` is a shortcut for `/help`.

## HC-SR04 Sonar wiring

Sonar uses the Cardputer ADV EXT 2.54 mm header and samples at about 10 Hz:

- `VCC` → `5VOUT` (third contact on the lower row)
- `GND` → `GND` (second contact on the lower row)
- `TRIG` → `G4` (second contact on the upper row)
- `ECHO` → voltage divider → `G6` (third contact on the upper row)

The HC-SR04 ECHO signal is 5 V and must not go directly into the ESP32-S3.
Use `ECHO → 1 kOhm → G6`, with `2 kOhm` from the G6 node to GND.
`4.7 kOhm / 10 kOhm` is also suitable.

On the Sonar screen, press `M` to switch between the large Proximity view and
the animated Radar view. Radar plots the measured range on the fixed forward
bearing; a single stationary HC-SR04 cannot determine left/right angle.

## RFID Lab wiring

RFID Lab is intentionally read-only: it detects the reader and shows card UID,
SAK, UID length, and PICC type. It does not copy hotel/access cards and does not
write protected MIFARE sectors.

Default supported module: M5Stack Unit RFID/RFID2 or compatible MFRC522 I2C unit
at address `0x28` on the Cardputer HY2.0/Grove Port A:

- red → 5V
- black → GND
- yellow/G2 → SDA, GPIO2
- white/G1 → SCL, GPIO1

Generic SPI RC522 boards are not enabled by this firmware profile; use an I2C
Unit RFID/RFID2 adapter for the default Cardputer wiring.

## Voice Flow

1. Hold `BtnA` or `Fn`
2. Speak into the Cardputer mic
3. Release the trigger
4. The hub transcribes speech with OpenAI, generates a reply, and streams PCM back
5. The Cardputer shows transcript + reply and plays the spoken answer through its speaker

## Emoji SD Pack

The firmware renders color emoji from the SD card only. Put PNG assets under
`/emoji/uHEX.png` on the card root, for example `/emoji/u1F600.png`.

See `docs/emoji-sd-handoff.md` for the transfer handoff.
