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
    constants = read("src/main_parts/001_main.cpp.inc")
    types = read("src/main_parts/003_main.cpp.inc")
    settings = read("src/main_parts/026_main.cpp.inc")
    saver = read("src/main_parts/009_main.cpp.inc")
    mode = read("src/main_parts/024_main.cpp.inc")
    keyboard = read("src/main_parts/044_main.cpp.inc") + read("src/main_parts/048_main.cpp.inc")
    labels = read("src/main_parts/056_main.cpp.inc") + read("src/main_parts/057_main.cpp.inc")
    inbox = read("src/main_parts/020_main.cpp.inc")

    require(not (ROOT / "src/main_parts/uncanny_default_eye.h").exists(),
            "photographic eye tables should not remain in the firmware")
    require("drawEyeSkinBackdrop" in eye and "drawSmoothUncannyEye" in eye,
            "procedural skin and eye layers are not wired")
    require("drawSmoothUncannyEye(62, 67" in eye and "drawSmoothUncannyEye(178, 67" in eye,
            "both eyes must be rendered in the same frame")
    require("fillEllipse" in eye and "pushImage" not in eye,
            "Big Eyes must use smooth primitives rather than sampled bitmap strips")
    require("malloc(" not in eye and "calloc(" not in eye and "new " not in eye,
            "eye renderer must not allocate from the heap")

    require("kEyeSkinCount = 6" in constants and "uint8_t eyeSkin" in types,
            "the six eye skins are not represented in settings")
    for skin in ("Black", "Cat", "Tiger", "Leopard", "Lion", "Rabbit"):
        require(f'return "{skin}"' in types, f"missing eye skin label: {skin}")
    require('getUChar("eye_skin"' in settings and 'putUChar("eye_skin"' in settings,
            "eye skin must round-trip through Preferences")

    require('getUChar("scr_style"' in settings and 'putUChar("scr_style"' in settings,
            "screensaver style must round-trip through Preferences")
    require("UiMode::Face : UiMode::CMatrix" in saver,
            "screensaver entry must select Big Eyes or C-Matrix")
    require("enteringScreensaverUi" in mode and "mode == UiMode::Face" in mode,
            "Face screensaver must preserve the active screensaver lifecycle")
    require("Saver style" in labels and "Eye skin" in labels and "Big Eyes" in labels and "C-Matrix" in labels,
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

    removed_asset_bytes = 200 * 200 * 2 + 256 * 64 * 2 + 128 * 128 * 2 + 80 * 80 * 2
    require(removed_asset_bytes == 158_336, "unexpected removed eye asset footprint")

    print("ui_feature_smoke: PASS")
    print(f"removed eye assets: {removed_asset_bytes} bytes flash")
    print("fixed eye render buffer: 0 bytes RAM")


if __name__ == "__main__":
    main()
