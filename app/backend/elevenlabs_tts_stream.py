"""ElevenLabs WebSocket stream-input TTS: MP3 from API, decode to PCM s16le mono 24 kHz for hub clients.

Follows the official real-time TTS flow (init with ``text`` space + ``xi_api_key``, stream deltas,
``flush`` on end of turn, ``{"text": ""}`` to close). See:
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

import imageio_ffmpeg
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


def _dlog(msg: str, *args: object) -> None:
    if _el_debug_enabled():
        logger.info("[elevenlabs] " + msg, *args)


ELEVENLABS_MODEL_ID = "eleven_flash_v2_5"
ELEVENLABS_VOICE_PIXEL_MALE = "KJnoleF17m24tnkdd9Jx"
ELEVENLABS_VOICE_PIXEL_FEMALE = "857KGbdfUgZoaVgOCZFz"

PCM_CHUNK_BYTES = 4800  # 100 ms at 24 kHz mono s16le


def elevenlabs_stream_url(voice_id: str) -> str:
    return (
        f"wss://api.elevenlabs.io/v1/text-to-speech/{voice_id}/stream-input"
        f"?model_id={ELEVENLABS_MODEL_ID}"
    )


def voice_id_for_pixel_tts_mode(mode: str) -> str:
    m = (mode or "").strip()
    if m == "elevenlabs_pixel_male":
        return ELEVENLABS_VOICE_PIXEL_MALE
    if m == "elevenlabs_pixel_female":
        return ELEVENLABS_VOICE_PIXEL_FEMALE
    raise ValueError(f"Not an ElevenLabs Pixel mode: {mode!r}")


async def _mp3_bytes_to_pcm_s16le_24k_mono(mp3: bytes) -> bytes:
    if not mp3:
        return b""
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    proc = await asyncio.create_subprocess_exec(
        ffmpeg,
        "-i",
        "pipe:0",
        "-f",
        "s16le",
        "-acodec",
        "pcm_s16le",
        "-ar",
        "24000",
        "-ac",
        "1",
        "pipe:1",
        stdin=asyncio.subprocess.PIPE,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.DEVNULL,
    )
    if proc.stdin is None or proc.stdout is None:
        return b""
    proc.stdin.write(mp3)
    await proc.stdin.drain()
    proc.stdin.close()
    out = await proc.stdout.read()
    await proc.wait()
    return out


async def stream_elevenlabs_turn_pcm(
    *,
    api_key: str,
    voice_id: str,
    text_queue: "asyncio.Queue[Optional[tuple[str, bool]]]",
    emit_pcm: Callable[[bytes], Awaitable[None]],
) -> None:
    """Drain text_queue: each item is (delta_text, finished). None ends input.

    Sends chunks to ElevenLabs stream-input, collects MP3, decodes to PCM, emits in small chunks.

    While waiting for the next Gemini transcription chunk, sends periodic ``{"text": " "}``
    keepalives so ElevenLabs' 20s input timeout is not hit (see realtime-tts docs).
    """
    uri = elevenlabs_stream_url(voice_id)
    mp3_acc = bytearray()
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
                                mp3_acc.extend(dec)
                                _dlog("recv audio mp3_chunk=%s total_mp3=%s", len(dec), len(mp3_acc))
                            except Exception:
                                pass
                        if data.get("isFinal"):
                            _dlog("recv isFinal")
                            break
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

            await ws.send(json.dumps({"text": ""}))

            try:
                await asyncio.wait_for(recv_task, timeout=120.0)
            except asyncio.TimeoutError:
                logger.warning("[elevenlabs] recv timeout voice_id=%s", voice_id)
                recv_task.cancel()
                with contextlib.suppress(asyncio.CancelledError):
                    await recv_task

            logger.info(
                "[elevenlabs] recv done; chunks_sent=%s keepalives=%s total_mp3=%s bytes",
                chunks_sent,
                keepalives_sent,
                len(mp3_acc),
            )

    except Exception as e:
        logger.warning("[elevenlabs] stream failed: %s", e)
        raise

    pcm = await _mp3_bytes_to_pcm_s16le_24k_mono(bytes(mp3_acc))
    logger.info("[elevenlabs] ffmpeg decoded PCM len=%s bytes; emitting to hub clients", len(pcm))
    n_emit = 0
    for i in range(0, len(pcm), PCM_CHUNK_BYTES):
        await emit_pcm(pcm[i : i + PCM_CHUNK_BYTES])
        n_emit += 1
    _dlog("emit_pcm calls=%s", n_emit)
