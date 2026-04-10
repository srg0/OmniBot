"""
Gemini 2.5 Flash Preview TTS for hub speaker playback after voice turns.
See https://ai.google.dev/gemini-api/docs/speech-generation
"""

from __future__ import annotations

import asyncio
import base64
import re
from functools import partial
from typing import AsyncIterator

from google.genai import types

from hub_config import get_genai_client

GEMINI_TTS_MODEL = "gemini-2.5-flash-preview-tts"
TTS_SAMPLE_RATE = 24000
TTS_CHANNELS = 1
DEFAULT_HUB_TTS_VOICE = "Kore"

# Prebuilt voices (Gemini TTS docs)
ALLOWED_TTS_VOICES = frozenset(
    {
        "Zephyr",
        "Puck",
        "Charon",
        "Kore",
        "Fenrir",
        "Leda",
        "Orus",
        "Aoede",
        "Callirrhoe",
        "Autonoe",
        "Enceladus",
        "Iapetus",
        "Umbriel",
        "Algieba",
        "Despina",
        "Erinome",
        "Algenib",
        "Rasalgethi",
        "Laomedeia",
        "Achernar",
        "Alnilam",
        "Schedar",
        "Gacrux",
        "Pulcherrima",
        "Achird",
        "Zubenelgenubi",
        "Vindemiatrix",
        "Sadachbia",
        "Sadaltager",
        "Sulafat",
    }
)

_MAX_TTS_CHARS = 12000
_WS_PCM_CHUNK_BYTES = 48_000  # ~1s at 24kHz mono s16le, keeps base64 under ~64KB


def normalize_hub_tts_voice(name: str) -> str:
    n = (name or "").strip()
    if not n:
        return DEFAULT_HUB_TTS_VOICE
    for v in ALLOWED_TTS_VOICES:
        if v.lower() == n.lower():
            return v
    return DEFAULT_HUB_TTS_VOICE


def build_tts_prompt(text: str) -> str:
    raw = (text or "").strip()
    if not raw:
        return ""
    # Light cleanup: collapse whitespace, strip markdown code fences
    raw = re.sub(r"```[\s\S]*?```", " ", raw)
    raw = re.sub(r"\s+", " ", raw).strip()
    if len(raw) > _MAX_TTS_CHARS:
        raw = raw[:_MAX_TTS_CHARS] + "…"
    return (
        "Read the following assistant reply aloud in a clear, conversational tone. "
        "Do not add a preamble or meta-commentary; speak only the content.\n\n"
        f"{raw}"
    )


def _blob_to_pcm(blob) -> bytes | None:
    if blob is None:
        return None
    data = getattr(blob, "data", None)
    if data is None:
        return None
    if isinstance(data, str):
        try:
            return base64.b64decode(data)
        except Exception:
            return None
    if isinstance(data, (bytes, bytearray)):
        return bytes(data)
    return None


def pcm_fragments_from_generate_response(response) -> list[bytes]:
    out: list[bytes] = []
    if response is None:
        return out
    cands = getattr(response, "candidates", None) or []
    for cand in cands:
        content = getattr(cand, "content", None)
        parts = getattr(content, "parts", None) if content else None
        if not parts:
            continue
        for part in parts:
            inline = getattr(part, "inline_data", None)
            if inline is None:
                continue
            mime = (getattr(inline, "mime_type", None) or "").lower()
            if mime and "audio" not in mime and "pcm" not in mime and "mpeg" not in mime:
                continue
            pcm = _blob_to_pcm(inline)
            if pcm:
                out.append(pcm)
    return out


def split_pcm_for_ws(pcm: bytes, max_chunk: int = _WS_PCM_CHUNK_BYTES) -> list[bytes]:
    if not pcm:
        return []
    if len(pcm) <= max_chunk:
        return [pcm]
    return [pcm[i : i + max_chunk] for i in range(0, len(pcm), max_chunk)]


def _tts_generate_config(voice_name: str) -> types.GenerateContentConfig:
    v = normalize_hub_tts_voice(voice_name)
    return types.GenerateContentConfig(
        response_modalities=["AUDIO"],
        speech_config=types.SpeechConfig(
            voice_config=types.VoiceConfig(
                prebuilt_voice_config=types.PrebuiltVoiceConfig(voice_name=v)
            )
        ),
    )


async def async_iter_tts_pcm(text: str, voice_name: str) -> AsyncIterator[bytes]:
    """Yield PCM s16le mono chunks from streaming TTS; unary fallback if stream yields no audio."""
    client = get_genai_client()
    if client is None:
        return
    prompt = build_tts_prompt(text)
    if not prompt:
        return
    config = _tts_generate_config(voice_name)
    loop = asyncio.get_running_loop()
    yielded = False

    try:
        stream_iter = client.models.generate_content_stream(
            model=GEMINI_TTS_MODEL,
            contents=prompt,
            config=config,
        )
    except Exception as e:
        print(f"[hub_tts] generate_content_stream failed: {e}")
        stream_iter = None

    if stream_iter is not None:
        while True:
            try:
                chunk = await loop.run_in_executor(
                    None, partial(next, stream_iter, None)
                )
            except Exception as e:
                print(f"[hub_tts] stream chunk error: {e}")
                break
            if chunk is None:
                break
            for frag in pcm_fragments_from_generate_response(chunk):
                yielded = True
                yield frag

    if not yielded:
        try:

            def _unary():
                return client.models.generate_content(
                    model=GEMINI_TTS_MODEL,
                    contents=prompt,
                    config=config,
                )

            resp = await loop.run_in_executor(None, _unary)
        except Exception as e:
            print(f"[hub_tts] generate_content failed: {e}")
            return
        for frag in pcm_fragments_from_generate_response(resp):
            yield frag
