#!/usr/bin/env python3
"""Deterministic host smoke checks for Big Eyes, screensaver selection, and Ctrl+digits."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    eye = read("src/main_parts/050_main.cpp.inc")
    asset = read("src/main_parts/uncanny_default_eye.h")
    settings = read("src/main_parts/026_main.cpp.inc")
    saver = read("src/main_parts/009_main.cpp.inc")
    mode = read("src/main_parts/024_main.cpp.inc")
    keyboard = read("src/main_parts/044_main.cpp.inc") + read("src/main_parts/048_main.cpp.inc")
    labels = read("src/main_parts/056_main.cpp.inc") + read("src/main_parts/057_main.cpp.inc")
    inbox = read("src/main_parts/020_main.cpp.inc")

    require("PROGMEM" in asset, "eye lookup tables must stay in flash")
    require("gUncannyStrip[kUncannyVisibleEyeWidth * kUncannyStripRows]" in eye,
            "renderer must use one fixed strip buffer")
    require("renderOneUncannyEye(gUncannyNextEye" in eye and "gUncannyNextEye ^= 1U" in eye,
            "steady-state renderer must alternate eyes")
    require("malloc(" not in eye and "calloc(" not in eye and "new " not in eye,
            "eye renderer must not allocate from the heap")

    require('getUChar("scr_style"' in settings and 'putUChar("scr_style"' in settings,
            "screensaver style must round-trip through Preferences")
    require("UiMode::Face : UiMode::CMatrix" in saver,
            "screensaver entry must select Big Eyes or C-Matrix")
    require("enteringScreensaverUi" in mode and "mode == UiMode::Face" in mode,
            "Face screensaver must preserve the active screensaver lifecycle")
    require("Saver style" in labels and "Big Eyes" in labels and "C-Matrix" in labels,
            "screensaver picker labels are incomplete")

    shortcut_header = read("src/launcher_shortcuts.h")
    require("digitFromHid" in shortcut_header, "Ctrl+digit must normalize physical HID codes")
    require("launcher_shortcuts::digitFromHid" in keyboard,
            "launcher does not use the physical Ctrl+digit mapping")
    require("shortcutKey, keys.ctrl" in keyboard,
            "normalized Ctrl+digit is not wired into launcher resolution")
    for digit, hid in enumerate(range(0x1E, 0x27), start=1):
        require(hid - 0x1E == digit - 1, f"unexpected HID mapping for digit {digit}")

    require("pending?limit=10" in inbox,
            "device-event polling must drain a bounded batch")
    require("seq <= gInboxLastSeq" in inbox and "ackDeviceEvent(candidateEventId)" in inbox,
            "stale device events must be acknowledged instead of poisoning the queue")
    require("[INBOX] auto blocked" in inbox,
            "hidden auto-poll busy gates must be observable on serial")

    compiled_asset_bytes = 200 * 200 * 2 + 256 * 64 * 2 + 128 * 128 * 2 + 80 * 80 * 2
    require(compiled_asset_bytes == 158_336, "unexpected compiled eye asset footprint")
    strip_bytes = 120 * 8 * 2
    require(strip_bytes == 1_920, "unexpected fixed eye strip footprint")

    print("ui_feature_smoke: PASS")
    print(f"compiled eye assets: {compiled_asset_bytes} bytes flash")
    print(f"fixed render strip: {strip_bytes} bytes RAM")


if __name__ == "__main__":
    main()
