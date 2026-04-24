"""OpenAI helpers for simple Cardputer voice/text turns."""

from __future__ import annotations

import json
from typing import Optional

import requests

from grounding_extract import strip_for_live_tts


OPENAI_API_BASE = "https://api.openai.com/v1"


def _auth_headers(api_key: str) -> dict[str, str]:
    key = str(api_key or "").strip()
    if not key:
        raise RuntimeError("OpenAI API key is not configured")
    return {
        "Authorization": f"Bearer {key}",
    }


def _raise_openai_error(resp: requests.Response, prefix: str) -> None:
    detail: Optional[str] = None
    try:
        data = resp.json()
        if isinstance(data, dict):
            err = data.get("error")
            if isinstance(err, dict):
                detail = str(err.get("message") or err.get("code") or "").strip()
            elif err:
                detail = str(err).strip()
    except (ValueError, json.JSONDecodeError):
        detail = None
    if not detail:
        try:
            detail = (resp.text or "").strip()[:300]
        except Exception:
            detail = ""
    if detail:
        raise RuntimeError(f"{prefix} ({resp.status_code}): {detail}")
    raise RuntimeError(f"{prefix} ({resp.status_code})")


def transcribe_openai_audio(
    *,
    api_key: str,
    audio_bytes: bytes,
    filename: str = "cardputer.wav",
    model: str = "gpt-4o-mini-transcribe",
    timeout_sec: float = 180.0,
) -> str:
    if not audio_bytes:
        return ""

    with requests.post(
        f"{OPENAI_API_BASE}/audio/transcriptions",
        headers=_auth_headers(api_key),
        data={"model": model},
        files={"file": (filename, audio_bytes, "audio/wav")},
        timeout=(10.0, timeout_sec),
    ) as resp:
        if not resp.ok:
            _raise_openai_error(resp, "OpenAI transcription failed")
        data = resp.json()
        if isinstance(data, dict):
            return str(data.get("text") or "").strip()
        return ""


def generate_openai_chat_reply(
    *,
    api_key: str,
    user_text: str,
    history: list[dict[str, str]],
    system_prompt: str,
    model: str = "gpt-4o-mini",
    timeout_sec: float = 180.0,
) -> str:
    cleaned = str(user_text or "").strip()
    if not cleaned:
        return ""

    messages: list[dict[str, str]] = [{"role": "system", "content": system_prompt}]
    for item in history:
        role = str(item.get("role") or "").strip()
        content = str(item.get("content") or "").strip()
        if role in {"user", "assistant"} and content:
            messages.append({"role": role, "content": content})
    messages.append({"role": "user", "content": cleaned})

    with requests.post(
        f"{OPENAI_API_BASE}/chat/completions",
        headers={**_auth_headers(api_key), "Content-Type": "application/json"},
        json={
            "model": model,
            "messages": messages,
            "temperature": 0.7,
        },
        timeout=(10.0, timeout_sec),
    ) as resp:
        if not resp.ok:
            _raise_openai_error(resp, "OpenAI chat completion failed")
        data = resp.json()
        if not isinstance(data, dict):
            return ""
        choices = data.get("choices")
        if not isinstance(choices, list) or not choices:
            return ""
        message = choices[0].get("message") if isinstance(choices[0], dict) else {}
        if not isinstance(message, dict):
            return ""
        content = message.get("content")
        if isinstance(content, str):
            return content.strip()
        if isinstance(content, list):
            parts: list[str] = []
            for item in content:
                if isinstance(item, dict) and item.get("type") == "text":
                    txt = str(item.get("text") or "").strip()
                    if txt:
                        parts.append(txt)
            return "\n".join(parts).strip()
        return ""


def synthesize_openai_tts_pcm(
    *,
    api_key: str,
    text: str,
    model: str = "gpt-4o-mini-tts",
    voice: str = "alloy",
    timeout_sec: float = 180.0,
) -> bytes:
    cleaned = strip_for_live_tts(text or "")
    if not cleaned:
        return b""

    with requests.post(
        f"{OPENAI_API_BASE}/audio/speech",
        headers={**_auth_headers(api_key), "Content-Type": "application/json"},
        json={
            "model": model,
            "voice": voice,
            "input": cleaned,
            "response_format": "pcm",
        },
        stream=True,
        timeout=(10.0, timeout_sec),
    ) as resp:
        if not resp.ok:
            _raise_openai_error(resp, "OpenAI TTS failed")
        audio_parts: list[bytes] = []
        for chunk in resp.iter_content(chunk_size=8192):
            if chunk:
                audio_parts.append(chunk)
        return b"".join(audio_parts)
