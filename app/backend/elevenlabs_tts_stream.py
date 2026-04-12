"""ElevenLabs WebSocket stream-input TTS: PCM s16le mono 24 kHz from API for hub clients.

Uses ``output_format=pcm_24000`` on the WebSocket URL so frames are raw PCM (base64 in JSON),
avoiding MP3 + ffmpeg decode. Follows the official real-time TTS flow (init with ``text`` space +
``xi_api_key``, stream deltas, ``flush`` on end of turn, ``{"text": ""}`` to close). See:
https://elevenlabs.io/docs/eleven-api/guides/how-to/websockets/realtime-tts

Auth is via ``xi_api_key`` in the first JSON message only (no WebSocket headers), per their Python example.
"""

from __future__ import annotations

import asyncio
import base64
import contextlib
import json
import logging
import os
from typing import Awaitable, Callable, Optional

import websockets
from websockets.exceptions import ConnectionClosed

logger = logging.getLogger(__name__)

# Send before ElevenLabs' 20s input timeout (docs: keepalive is a single space " ").
_KEEPALIVE_SEC = 12.0

# Character thresholds before synthesizing audio (see ElevenLabs "buffering" / chunk_length_schedule).
# Smaller first values → earlier first audio (lower latency), often at some quality cost.
# Default matches doc example for lower TTFB vs [120, 160, 250, 290].
_DEFAULT_CHUNK_LENGTH_SCHEDULE: tuple[int, ...] = (50, 120, 160, 290)
_ENV_CHUNK_SCHEDULE = "OMNIBOT_ELEVENLABS_CHUNK_SCHEDULE"


def chunk_length_schedule() -> list[int]:
    """Parse OMNIBOT_ELEVENLABS_CHUNK_SCHEDULE=e.g. 50,120,160,290 or use default."""
    raw = (os.environ.get(_ENV_CHUNK_SCHEDULE) or "").strip()
    if not raw:
        return list(_DEFAULT_CHUNK_LENGTH_SCHEDULE)
    out: list[int] = []
    for part in raw.split(","):
        part = part.strip()
        if not part:
            continue
        try:
            n = int(part)
        except ValueError:
            logger.warning(
                "[elevenlabs] invalid %s entry %r; using default %s",
                _ENV_CHUNK_SCHEDULE,
                part,
                list(_DEFAULT_CHUNK_LENGTH_SCHEDULE),
            )
            return list(_DEFAULT_CHUNK_LENGTH_SCHEDULE)
        if n < 1:
            logger.warning(
                "[elevenlabs] %s values must be >= 1; using default",
                _ENV_CHUNK_SCHEDULE,
            )
            return list(_DEFAULT_CHUNK_LENGTH_SCHEDULE)
        out.append(n)
    if not out:
        return list(_DEFAULT_CHUNK_LENGTH_SCHEDULE)
    return out


def _el_debug_enabled() -> bool:
    return (os.environ.get("OMNIBOT_ELEVENLABS_DEBUG") or "").strip().lower() in (
        "1",
        "true",
        "yes",
        "on",
    )


def _live_audio_debug() -> bool:
    """Same as gemini_live_session — set OMNIBOT_LIVE_AUDIO_DEBUG=1 for end-to-end audio tracing."""
    return (os.environ.get("OMNIBOT_LIVE_AUDIO_DEBUG") or "").strip().lower() in (
        "1",
        "true",
        "yes",
        "on",
    )


def _dlog(msg: str, *args: object) -> None:
    if _el_debug_enabled():
        logger.info("[elevenlabs] " + msg, *args)


def _alog(msg: str, *args: object) -> None:
    if _live_audio_debug():
        logger.info("[elevenlabs-audio] " + msg, *args)


ELEVENLABS_MODEL_ID = "eleven_flash_v2_5"
ELEVENLABS_VOICE_PIXEL_MALE = "KJnoleF17m24tnkdd9Jx"
ELEVENLABS_VOICE_PIXEL_FEMALE = "857KGbdfUgZoaVgOCZFz"

PCM_CHUNK_BYTES = 4800  # 100 ms at 24 kHz mono s16le


def elevenlabs_stream_url(voice_id: str) -> str:
    return (
        f"wss://api.elevenlabs.io/v1/text-to-speech/{voice_id}/stream-input"
        f"?model_id={ELEVENLABS_MODEL_ID}&output_format=pcm_24000"
    )


def voice_id_for_pixel_tts_mode(mode: str) -> str:
    m = (mode or "").strip()
    if m == "elevenlabs_pixel_male":
        return ELEVENLABS_VOICE_PIXEL_MALE
    if m == "elevenlabs_pixel_female":
        return ELEVENLABS_VOICE_PIXEL_FEMALE
    raise ValueError(f"Not an ElevenLabs Pixel mode: {mode!r}")


async def stream_elevenlabs_turn_pcm(
    *,
    api_key: str,
    voice_id: str,
    text_queue: "asyncio.Queue[Optional[tuple[str, bool]]]",
    emit_pcm: Callable[[bytes], Awaitable[None]],
) -> None:
    """Drain text_queue: each item is (delta_text, finished). None ends input.

    Sends chunks to ElevenLabs stream-input, collects PCM from WebSocket frames, emits in small chunks.

    While waiting for the next Gemini transcription chunk, sends periodic ``{"text": " "}``
    keepalives so ElevenLabs' 20s input timeout is not hit (see realtime-tts docs).
    """
    uri = elevenlabs_stream_url(voice_id)
    pcm_acc = bytearray()
    chunks_sent = 0
    keepalives_sent = 0

    try:
        schedule = chunk_length_schedule()
        logger.info(
            "[elevenlabs] starting stream voice_id=%s model=%s chunk_length_schedule=%s",
            voice_id,
            ELEVENLABS_MODEL_ID,
            schedule,
        )
        _dlog("connect uri=%s", uri)

        # Auth via JSON `xi_api_key` on the init message (ElevenLabs docs). Do not pass
        # extra_headers here: some websockets/asyncio builds incorrectly forward it to
        # loop.create_connection(), which raises TypeError.
        async with websockets.connect(uri, max_size=None) as ws:
            logger.info("[elevenlabs] websocket open")
            await ws.send(
                json.dumps(
                    {
                        "text": " ",
                        "voice_settings": {
                            "stability": 0.5,
                            "similarity_boost": 0.8,
                            "use_speaker_boost": False,
                        },
                        "generation_config": {"chunk_length_schedule": schedule},
                        "xi_api_key": api_key,
                    }
                )
            )
            _dlog("init message sent (voice_settings + xi_api_key)")

            async def recv_loop() -> None:
                try:
                    async for raw in ws:
                        try:
                            s = raw if isinstance(raw, str) else raw.decode("utf-8")
                            data = json.loads(s)
                        except Exception:
                            continue
                        if data.get("error"):
                            logger.warning("[elevenlabs] error payload: %s", data)
                        aud = data.get("audio")
                        if aud:
                            try:
                                dec = base64.b64decode(aud)
                                pcm_acc.extend(dec)
                                _dlog(
                                    "recv audio pcm_chunk=%s total_pcm=%s",
                                    len(dec),
                                    len(pcm_acc),
                                )
                            except Exception:
                                pass
                        # Do not break on isFinal — ElevenLabs may emit multiple finals per stream; stopping
                        # recv early drops later PCM until we send {"text": ""} and the socket closes.
                        if data.get("isFinal"):
                            _dlog("recv isFinal")
                except ConnectionClosed as cc:
                    logger.info("[elevenlabs] websocket closed code=%s reason=%s", cc.code, cc.reason)
                except Exception as e:
                    logger.warning("[elevenlabs] recv_loop: %s", e)

            recv_task = asyncio.create_task(recv_loop())

            while True:
                try:
                    item = await asyncio.wait_for(text_queue.get(), timeout=_KEEPALIVE_SEC)
                except asyncio.TimeoutError:
                    await ws.send(json.dumps({"text": " "}))
                    keepalives_sent += 1
                    logger.info(
                        "[elevenlabs] keepalive #%s (no new Gemini text for %ss; resets 20s input timeout)",
                        keepalives_sent,
                        int(_KEEPALIVE_SEC),
                    )
                    continue
                if item is None:
                    _dlog("end sentinel received from queue")
                    _alog("queue sentinel (turn end) chunks_sent_so_far=%s", chunks_sent)
                    break
                text, finished = item
                payload: dict = {"text": text}
                if finished:
                    payload["flush"] = True
                await ws.send(json.dumps(payload))
                chunks_sent += 1
                preview = (text or "")[:80].replace("\n", "\\n")
                _dlog(
                    "sent chunk #%s len=%s finished=%s preview=%r",
                    chunks_sent,
                    len(text or ""),
                    finished,
                    preview,
                )
                _alog(
                    "ws sent chunk #%s text_len=%s finished=%s preview=%r",
                    chunks_sent,
                    len(text or ""),
                    finished,
                    preview,
                )

            await ws.send(json.dumps({"text": ""}))

            try:
                await asyncio.wait_for(recv_task, timeout=120.0)
            except asyncio.TimeoutError:
                logger.warning("[elevenlabs] recv timeout voice_id=%s", voice_id)
                recv_task.cancel()
                with contextlib.suppress(asyncio.CancelledError):
                    await recv_task

            logger.info(
                "[elevenlabs] recv done; chunks_sent=%s keepalives=%s total_pcm=%s bytes",
                chunks_sent,
                keepalives_sent,
                len(pcm_acc),
            )
            _alog(
                "recv_loop joined chunks_sent=%s total_pcm=%s",
                chunks_sent,
                len(pcm_acc),
            )

    except Exception as e:
        logger.warning("[elevenlabs] stream failed: %s", e)
        raise

    pcm = bytes(pcm_acc)
    logger.info(
        "[elevenlabs] accumulated PCM len=%s bytes; emitting to hub clients",
        len(pcm),
    )
    if not pcm and chunks_sent > 0:
        logger.warning(
            "[elevenlabs-audio] ZERO PCM after recv but chunks_sent=%s pcm_acc=%s bytes — check API / output_format",
            chunks_sent,
            len(pcm_acc),
        )
    if not pcm and chunks_sent == 0:
        logger.warning(
            "[elevenlabs-audio] ZERO PCM and chunks_sent=0 — no text reached ElevenLabs this turn"
        )
    n_emit = 0
    for i in range(0, len(pcm), PCM_CHUNK_BYTES):
        sl = pcm[i : i + PCM_CHUNK_BYTES]
        _alog("emit_pcm slice #%s bytes=%s", n_emit + 1, len(sl))
        await emit_pcm(sl)
        n_emit += 1
    _dlog("emit_pcm calls=%s", n_emit)
    logger.info("[elevenlabs-audio] emit_pcm completed n=%s total_pcm=%s", n_emit, len(pcm))
