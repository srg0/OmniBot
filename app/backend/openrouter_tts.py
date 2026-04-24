"""OpenRouter text-to-speech helpers for hub/browser playback."""

from __future__ import annotations

import json
from typing import Optional

import requests

from grounding_extract import strip_for_live_tts


OPENROUTER_TTS_URL = "https://openrouter.ai/api/v1/audio/speech"
OPENROUTER_APP_REFERER = "https://github.com/nazirlouis/OmniBot"
OPENROUTER_APP_TITLE = "OmniBot"


def synthesize_openrouter_tts_pcm(
    *,
    api_key: str,
    text: str,
    model: str,
    voice: str,
    timeout_sec: float = 180.0,
) -> bytes:
    """Render full PCM audio via OpenRouter TTS."""
    api_key = str(api_key or "").strip()
    model = str(model or "").strip()
    voice = str(voice or "").strip()
    cleaned = strip_for_live_tts(text or "")
    if not api_key:
        raise RuntimeError("OpenRouter API key is not configured")
    if not model:
        raise RuntimeError("OpenRouter TTS model is not configured")
    if not voice:
        raise RuntimeError("OpenRouter TTS voice is not configured")
    if not cleaned:
        return b""

    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json",
        "HTTP-Referer": OPENROUTER_APP_REFERER,
        "X-Title": OPENROUTER_APP_TITLE,
    }
    payload = {
        "input": cleaned,
        "model": model,
        "voice": voice,
        "response_format": "pcm",
    }

    with requests.post(
        OPENROUTER_TTS_URL,
        headers=headers,
        json=payload,
        stream=True,
        timeout=(10.0, timeout_sec),
    ) as resp:
        if not resp.ok:
            detail: Optional[str] = None
            try:
                data = resp.json()
                if isinstance(data, dict):
                    detail = str(
                        data.get("error") or data.get("message") or data.get("detail") or ""
                    ).strip()
            except (ValueError, json.JSONDecodeError):
                detail = None
            if not detail:
                try:
                    detail = (resp.text or "").strip()[:300]
                except Exception:
                    detail = ""
            if detail:
                raise RuntimeError(f"OpenRouter TTS failed ({resp.status_code}): {detail}")
            raise RuntimeError(f"OpenRouter TTS failed with status {resp.status_code}")

        audio_parts: list[bytes] = []
        for chunk in resp.iter_content(chunk_size=8192):
            if chunk:
                audio_parts.append(chunk)
        return b"".join(audio_parts)
