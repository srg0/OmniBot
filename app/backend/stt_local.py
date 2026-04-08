"""Optional local STT (faster-whisper) for logging transcripts alongside Gemini audio."""

from __future__ import annotations

import os
import threading
from typing import Any, Optional

# 16 kHz mono s16le PCM (matches Pixel / hub WAV)
SAMPLE_RATE = 16000

_model_lock = threading.Lock()
_model: Any = None
_model_name: Optional[str] = None
_init_error: Optional[str] = None


def _desired_model_name() -> str:
    return (os.getenv("OMNIBOT_WHISPER_MODEL") or "tiny").strip() or "tiny"


def transcribe_pcm_s16le(pcm: bytes) -> Optional[tuple[str, Optional[float]]]:
    """
    Transcribe raw PCM int16 mono at 16 kHz. Returns (text, avg_logprob) or None on failure/skip.
    """
    global _model, _model_name, _init_error
    if not pcm or len(pcm) < 3200:  # ~0.1s
        return None

    try:
        import numpy as np
    except ImportError:
        return None

    try:
        from faster_whisper import WhisperModel
    except ImportError:
        return None

    name = _desired_model_name()
    with _model_lock:
        if _model is None or _model_name != name:
            device = (os.getenv("OMNIBOT_WHISPER_DEVICE") or "cpu").strip() or "cpu"
            compute_type = (os.getenv("OMNIBOT_WHISPER_COMPUTE") or "int8").strip() or "int8"
            try:
                _model = WhisperModel(name, device=device, compute_type=compute_type)
                _model_name = name
                _init_error = None
            except Exception as e:
                _model = None
                _model_name = None
                _init_error = str(e)
                print(f"[Omnibot/stt] faster-whisper init failed: {e}")
                return None

    audio = np.frombuffer(pcm, dtype=np.int16).astype(np.float32) / 32768.0
    if audio.size == 0:
        return None

    model = _model
    try:
        segments, info = model.transcribe(
            audio,
            language=(os.getenv("OMNIBOT_WHISPER_LANGUAGE") or "").strip() or None,
            vad_filter=True,
            beam_size=1,
        )
        parts: list[str] = []
        logprobs: list[float] = []
        for seg in segments:
            t = (seg.text or "").strip()
            if t:
                parts.append(t)
            if getattr(seg, "avg_logprob", None) is not None:
                logprobs.append(float(seg.avg_logprob))
        text = " ".join(parts).strip()
        if not text:
            return None
        avg_lp = sum(logprobs) / len(logprobs) if logprobs else None
        return text, avg_lp
    except Exception as e:
        print(f"[Omnibot/stt] transcribe error: {e}")
        return None
