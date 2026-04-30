# Cardputer ADV Emoji SD Handoff

## Goal

Give CardputerADV readable color emoji without embedding the emoji pack into the firmware image.
The firmware loads emoji PNGs from the SD card at render time.

## Firmware Contract

- Storage: SD card only, no LittleFS fallback for emoji images.
- Directory on SD root: `/emoji/`.
- File naming: `u<uppercase hex codepoint>.png`.
- Example: `😄` (`U+1F604`) is `/emoji/u1F604.png`.
- Expected image format: small PNG, current known-good pack is `12x12` RGBA.
- Complex emoji sequences are degraded for readability: variation selectors, ZWJ, and skin-tone modifiers are suppressed before display, so the base emoji can render when present.

## Current Source Pack

Known local source:

```bash
/Users/s1z0v/kd-projects/adv_cardputer/worktrees/plai/emoji/
```

Measured pack:

- `1446` PNG files
- `730721` bytes payload, about `714 KiB`
- about `5.7 MiB` on FAT with `4 KiB` clusters because the pack is many small files

## Transfer To SD

If macOS mounted the SD card as `/Volumes/CARDPUTER`, copy the package like this:

```bash
mkdir -p /Volumes/CARDPUTER/emoji
rsync -av --delete \
  /Users/s1z0v/kd-projects/adv_cardputer/worktrees/plai/emoji/ \
  /Volumes/CARDPUTER/emoji/
sync
diskutil eject /Volumes/CARDPUTER
```

Then insert the SD card and reboot the Cardputer.

## Archive Handoff

For passing the pack to another machine:

```bash
tar -C /Users/s1z0v/kd-projects/adv_cardputer/worktrees/plai \
  -czf /tmp/cardputer-emoji-12x12.tar.gz emoji
```

The receiver should extract it at the SD card root so the result is:

```text
/emoji/u1F600.png
/emoji/u2764.png
/emoji/u1F44D.png
```

## Verification

On boot, serial logs should include one of:

```text
[EMOJI] SD assets count=1446 bytes=730721
[EMOJI] missing /emoji on SD
[EMOJI] SD not mounted; emoji assets disabled
```

The chat boot log also shows `Emoji SD: 1446` when the pack is present.

Send a short bridge/Telegram message such as:

```text
Привет 😄 ❤️ 👍
```

Expected result: Russian text remains readable and supported emoji are drawn as color PNGs from SD.
