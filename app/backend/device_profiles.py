"""Known OmniBot device profiles and capability defaults."""

from __future__ import annotations

from typing import Any


DEVICE_PROFILE_DEFAULT = "pixel"


DEVICE_PROFILES: dict[str, dict[str, Any]] = {
    "pixel": {
        "display_name": "Pixel",
        "capabilities": {
            "text_input": False,
            "voice_input": True,
            "audio_output": True,
            "vision": True,
            "face_animation": True,
            "presence_scan": True,
            "face_enrollment": True,
            "ble_provisioning": True,
        },
    },
    "cardputer_adv": {
        "display_name": "ADV Cardputer",
        "capabilities": {
            "text_input": True,
            "voice_input": False,
            "audio_output": True,
            "vision": False,
            "face_animation": False,
            "presence_scan": False,
            "face_enrollment": False,
            "ble_provisioning": True,
        },
    },
}


def normalize_device_type(value: Any) -> str:
    raw = str(value or "").strip().lower().replace("-", "_").replace(" ", "_")
    if raw in DEVICE_PROFILES:
        return raw
    if raw in {"cardputer", "cardputeradv", "adv_cardputer"}:
        return "cardputer_adv"
    return DEVICE_PROFILE_DEFAULT


def default_display_name_for_device_type(device_type: Any) -> str:
    dtype = normalize_device_type(device_type)
    return str(DEVICE_PROFILES[dtype]["display_name"])


def capabilities_for_device_type(
    device_type: Any, overrides: Any = None
) -> dict[str, bool]:
    dtype = normalize_device_type(device_type)
    caps = dict(DEVICE_PROFILES[dtype]["capabilities"])
    if isinstance(overrides, dict):
        for key, value in overrides.items():
            if key in caps:
                caps[key] = bool(value)
    return caps
