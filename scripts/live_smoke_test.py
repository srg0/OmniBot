#!/usr/bin/env python3
"""
Optional smoke test for Gemini 3.1 Live (SDK). Requires GEMINI_API_KEY or hub-stored key.

Run from repo root:
  python scripts/live_smoke_test.py
"""
from __future__ import annotations

import asyncio
import os
import sys

BACKEND = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "app", "backend"))
if BACKEND not in sys.path:
    sys.path.insert(0, BACKEND)


async def main() -> None:
    os.chdir(BACKEND)
    from google.genai import types

    from hub_config import get_genai_client, get_gemini_api_key

    if not get_gemini_api_key():
        print("SKIP: no GEMINI_API_KEY / hub key configured.")
        return

    from gemini_live_session import LIVE_MODEL, build_pixel_live_tools

    gc = get_genai_client()
    assert gc is not None

    cfg = types.LiveConnectConfig(
        response_modalities=[types.Modality.AUDIO],
        system_instruction="Reply in one short English sentence.",
        tools=build_pixel_live_tools(),
        input_audio_transcription=types.AudioTranscriptionConfig(),
        output_audio_transcription=types.AudioTranscriptionConfig(),
        context_window_compression=types.ContextWindowCompressionConfig(
            sliding_window=types.SlidingWindow(),
        ),
    )

    print("Connecting model=", LIVE_MODEL)

    async def _one_turn() -> None:
        async with gc.aio.live.connect(model=LIVE_MODEL, config=cfg) as session:
            await session.send_realtime_input(text='Say only the word "check".')
            async for msg in session.receive():
                sc = msg.server_content
                if sc and sc.input_transcription and sc.input_transcription.text:
                    it = sc.input_transcription
                    print("input_transcription:", repr(it.text), "finished=", it.finished)
                if sc and sc.output_transcription and sc.output_transcription.text:
                    ot = sc.output_transcription
                    print("output_transcription:", repr(ot.text), "finished=", ot.finished)
                d = msg.data
                if d:
                    print("audio bytes (aggregated):", len(d))
                if msg.tool_call:
                    print("tool_call:", msg.tool_call)
                if msg.go_away:
                    print("go_away:", msg.go_away)
            print("First model turn complete.")

    try:
        await asyncio.wait_for(_one_turn(), timeout=120.0)
    except asyncio.TimeoutError:
        print("TIMEOUT: no turn_complete within 120s (network, key, or model issue).")

    print("Done.")


if __name__ == "__main__":
    asyncio.run(main())
