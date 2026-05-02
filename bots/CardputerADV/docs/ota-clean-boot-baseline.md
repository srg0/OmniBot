# Cardputer ADV OTA Clean-Boot Baseline

Baseline firmware: `0.2.53-dev`.

This document is a regression guard for the Cardputer ADV firmware. Treat these points as invariants unless the next change deliberately replaces the OTA architecture and revalidates it on hardware.

## Current Architecture

- `src/main.cpp` is intentionally a thin index that includes `src/main_parts/*.cpp.inc` in order.
- `src/main_parts` is a single translation unit split, not a real multi-file C++ refactor.
- The split preserves anonymous namespace linkage, shared `g*` state, initialization order, and behavior.
- Do not move fragments to separate `.cpp` files without first untangling global state and proving the hardware flow again.

## OTA Invariants

- Normal app runtime must not download and flash firmware directly.
- User apply in runtime writes an NVS boot-apply request and reboots.
- Clean boot updater runs before WebSocket, audio, keyboard, topics, pet, asset scans, and other heap-heavy runtime systems.
- Clean boot updater refreshes the firmware manifest before applying to avoid stale SD manifest data.
- Firmware download is staged on SD at `/pomodoro/firmware.tmp.bin`.
- Staged firmware must pass size and SHA-256 checks before flashing.
- Flashing writes the inactive OTA slot and then switches boot partition.
- OTA flash uses `OTA_WITH_SEQUENTIAL_WRITES` to avoid long blocking erase windows.
- SD SPI stays at `8MHz` unless a faster setting is retested on the real 64GB card; `25MHz` caused token/read instability.
- If SD staging fails, the code may retry/remount SD or fall back to direct streaming, but the clean boot updater remains mandatory.

## Build And Release Rules

- Build and upload with `/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh`.
- Do not use bare `pio run` / `pio run -t upload` unless all required provisioning environment variables are explicitly set.
- Do not commit Wi-Fi passwords, device tokens, OpenAI/OpenRouter keys, or local bridge config secrets.
- Do not stage unrelated host snapshot files from sibling OpenClaw repos into firmware commits.

## Hardware Validation Evidence

The baseline OTA path was validated on hardware with `0.2.53-dev`:

- Runtime apply deferred to clean boot updater.
- Clean updater heap was about `156KB`, instead of the low-memory full-runtime apply path.
- SD stage completed: `sd stage download ok`.
- Flash completed: `staged flash 2390k/2390k`.
- Device rebooted into `version=0.2.53-dev`.
- Slot switch was confirmed after boot.

## Known Non-Blocking Residual

Full runtime can still log TLS allocation failures during non-critical boot-time manifest or asset fetches. This must not be confused with OTA failure: OTA apply is intentionally isolated into the clean boot updater to avoid that heap pressure.

## Regression Checklist

Before treating an OTA or release change as safe:

- Build succeeds with the wrapper.
- Version bump is visible in `platformio.ini` and fallback `APP_VERSION`.
- OTA candidate downloads to 100%.
- Clean boot updater logs high free heap before apply.
- SD stage SHA/size verification completes.
- Flash progress reaches full firmware size.
- Rebooted app shows the new version.
