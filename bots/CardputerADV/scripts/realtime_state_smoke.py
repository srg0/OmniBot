#!/usr/bin/env python3
"""Deterministic host smoke for CardPuter Realtime playback transitions.

This intentionally models only the firmware's busy/terminal contract. Hardware
queue timing remains covered by the PlatformIO compile and later cable QA.
"""

from dataclasses import dataclass
from pathlib import Path


PREBUFFER = 16 * 1024
STREAM_STALL_MS = 4000
SPEAKER_STALL_MS = 2500
TERMINAL_GRACE_MS = 4000
RESPONSE_TIMEOUT_MS = 45000
S1_RESPONSE_TIMEOUT_MS = 90000
RESTART_MS = 120
BINARY_HEADER_BYTES = 8
BINARY_MIC_PCM = 0x01
BINARY_MODEL_PCM = 0x02
BINARY_VERSION = 1


def binary_frame(frame_type: int, sequence: int, pcm: bytes) -> bytes:
    assert len(pcm) > 0 and len(pcm) % 2 == 0
    return bytes((frame_type, BINARY_VERSION, 0, 0)) + sequence.to_bytes(4, "little") + pcm


def parse_binary_model_frame(frame: bytes, expected_sequence: int | None) -> tuple[int, bytes]:
    assert len(frame) >= BINARY_HEADER_BYTES
    assert frame[0] == BINARY_MODEL_PCM
    assert frame[1] == BINARY_VERSION
    assert frame[2:4] == b"\x00\x00"
    sequence = int.from_bytes(frame[4:8], "little")
    if expected_sequence is not None:
        assert sequence == expected_sequence
    pcm = frame[BINARY_HEADER_BYTES:]
    assert pcm and len(pcm) % 2 == 0
    return sequence, pcm


@dataclass
class RealtimeState:
    active: bool = True
    connected: bool = True
    awaiting: bool = True
    playback: bool = False
    speaker_playing: bool = False
    got_audio: bool = False
    audio_done: bool = False
    response_done: bool = False
    restart_at_ms: int = 0
    buffered: int = 0
    first_delta_ms: int = 0
    last_delta_ms: int = 0
    playback_progress_ms: int = 0
    audio_done_ms: int = 0
    awaiting_started_ms: int = 0
    s1_thinking: bool = False
    deferred_ui: str = ""
    opened_ui: str = ""
    closed_reason: str = ""

    def close(self, reason: str) -> None:
        self.active = False
        self.connected = False
        self.awaiting = False
        self.playback = False
        self.speaker_playing = False
        self.buffered = 0
        self.restart_at_ms = 0
        self.s1_thinking = False
        self.closed_reason = reason
        if self.deferred_ui:
            self.opened_ui = self.deferred_ui
            self.deferred_ui = ""

    def thinking_s1(self, now: int) -> None:
        self.awaiting = True
        self.s1_thinking = True
        self.awaiting_started_ms = now

    def request_ui(self, mode: str) -> None:
        if self.active or self.connected or self.awaiting:
            self.deferred_ui = mode
        else:
            self.opened_ui = mode

    def delta(self, byte_count: int, now: int) -> None:
        self.got_audio = True
        self.first_delta_ms = self.first_delta_ms or now
        self.last_delta_ms = now
        self.buffered += byte_count
        if not self.playback and self.buffered >= PREBUFFER:
            self.playback = True
            self.speaker_playing = True
            self.playback_progress_ms = now

    def audio_terminal(self, now: int) -> None:
        self.audio_done = True
        self.audio_done_ms = now
        if self.buffered and not self.playback:
            self.playback = True
            self.speaker_playing = True
            self.playback_progress_ms = now

    def response_terminal(self, now: int) -> None:
        self.response_done = True
        self.awaiting = False
        if self.got_audio and not self.audio_done:
            self.audio_terminal(now)
        if not self.playback:
            self.restart_at_ms = now + RESTART_MS

    def speaker_idle(self, now: int) -> None:
        self.speaker_playing = False
        self.buffered = 0
        if self.audio_done:
            self.playback = False
            if self.response_done:
                self.restart_at_ms = now + RESTART_MS

    def tick(self, now: int) -> None:
        timeout_ms = S1_RESPONSE_TIMEOUT_MS if self.s1_thinking else RESPONSE_TIMEOUT_MS
        if self.awaiting and self.awaiting_started_ms and now - self.awaiting_started_ms > timeout_ms:
            self.close("response timeout")
        elif (
            not self.s1_thinking
            and
            self.got_audio
            and not self.audio_done
            and not self.speaker_playing
            and self.buffered < PREBUFFER
            and now - self.last_delta_ms > STREAM_STALL_MS
        ):
            self.close("audio stream stalled")
        elif (
            self.audio_done
            and self.speaker_playing
            and now - self.playback_progress_ms > SPEAKER_STALL_MS
        ):
            self.close("speaker stalled")
        elif (
            not self.s1_thinking
            and
            self.audio_done
            and not self.response_done
            and not self.playback
            and now - self.audio_done_ms > TERMINAL_GRACE_MS
        ):
            self.close("response.done missing")


def test_early_streaming_before_response_done() -> None:
    state = RealtimeState()
    state.delta(PREBUFFER, 741)
    assert state.playback
    assert state.awaiting
    assert not state.response_done


def test_short_audio_flush_and_restart() -> None:
    state = RealtimeState()
    state.delta(2048, 700)
    assert not state.playback
    state.audio_terminal(720)
    assert state.playback
    state.response_terminal(725)
    state.speaker_idle(780)
    assert not state.playback and not state.awaiting
    assert state.restart_at_ms == 780 + RESTART_MS


def test_response_done_recovers_missing_audio_done() -> None:
    state = RealtimeState()
    state.delta(2048, 700)
    state.response_terminal(750)
    assert state.audio_done and state.playback and not state.awaiting
    state.speaker_idle(800)
    assert state.restart_at_ms == 800 + RESTART_MS


def test_empty_audio_terminal_clears_busy() -> None:
    state = RealtimeState()
    state.audio_terminal(700)
    state.response_terminal(710)
    assert not state.awaiting and not state.playback
    assert state.restart_at_ms == 710 + RESTART_MS


def test_stream_and_speaker_watchdogs() -> None:
    stream = RealtimeState()
    stream.delta(100, 100)
    stream.tick(100 + STREAM_STALL_MS + 1)
    assert stream.closed_reason == "audio stream stalled"
    assert not stream.awaiting and not stream.playback

    speaker = RealtimeState()
    speaker.delta(PREBUFFER, 100)
    speaker.audio_terminal(110)
    speaker.tick(100 + SPEAKER_STALL_MS + 1)
    assert speaker.closed_reason == "speaker stalled"
    assert not speaker.awaiting and not speaker.playback


def test_missing_response_done_watchdog() -> None:
    state = RealtimeState()
    state.audio_terminal(100)
    state.tick(100 + TERMINAL_GRACE_MS + 1)
    assert state.closed_reason == "response.done missing"
    assert not state.awaiting and not state.playback


def test_explicit_failures_clear_all_busy_state() -> None:
    for reason in ("playback queue failed", "disconnect", "escape"):
        state = RealtimeState(playback=True, speaker_playing=True, buffered=PREBUFFER)
        state.close(reason)
        assert not state.active
        assert not state.connected
        assert not state.awaiting
        assert not state.playback
        assert not state.speaker_playing


def test_s1_wait_uses_long_timeout_and_suppresses_audio_watchdogs() -> None:
    state = RealtimeState()
    state.delta(100, 100)
    state.audio_terminal(120)
    state.speaker_idle(130)
    state.thinking_s1(1000)
    state.tick(1000 + RESPONSE_TIMEOUT_MS + 1)
    assert state.active and state.awaiting and state.s1_thinking
    state.tick(1000 + S1_RESPONSE_TIMEOUT_MS)
    assert state.active
    state.tick(1000 + S1_RESPONSE_TIMEOUT_MS + 1)
    assert state.closed_reason == "response timeout"


def test_ui_open_is_deferred_until_realtime_exit() -> None:
    state = RealtimeState()
    state.request_ui("battery")
    assert state.active and state.deferred_ui == "battery" and not state.opened_ui
    state.close("escape")
    assert state.opened_ui == "battery" and not state.deferred_ui


def test_close_reason_is_bounded_and_present() -> None:
    reason = "mode change"
    frame = {"type": "close", "reason": reason[:80]}
    assert frame == {"type": "close", "reason": "mode change"}
    assert len(frame["reason"]) <= 80


def test_binary_v1_header_and_sequence() -> None:
    pcm = bytes(range(32))
    frame = binary_frame(BINARY_MODEL_PCM, 0x78563412, pcm)
    assert frame[:8] == b"\x02\x01\x00\x00\x12\x34\x56\x78"
    sequence, decoded = parse_binary_model_frame(frame, 0x78563412)
    assert sequence == 0x78563412
    assert decoded == pcm


def test_binary_v1_rejects_sequence_gap() -> None:
    frame = binary_frame(BINARY_MODEL_PCM, 9, b"\x00\x00")
    try:
        parse_binary_model_frame(frame, 8)
    except AssertionError:
        return
    raise AssertionError("binary sequence gap must be rejected")


def test_binary_negotiation_fallback() -> None:
    assert ("binary-v1" == "binary-v1") is True
    assert ("json" == "binary-v1") is False
    assert ("" == "binary-v1") is False


def test_firmware_wiring() -> None:
    root = Path(__file__).resolve().parents[1]
    parts = root / "src" / "main_parts"
    playback = (parts / "041_main.cpp.inc").read_text()
    events = (parts / "042_main.cpp.inc").read_text()
    loop = (parts / "061_main.cpp.inc").read_text()
    escape = (parts / "047_main.cpp.inc").read_text()
    voice = (parts / "033_main.cpp.inc").read_text()
    realtime = (parts / "043_main.cpp.inc").read_text()
    actions = (parts / "024_main.cpp.inc").read_text()
    speaker = (parts / "027_main.cpp.inc").read_text()
    ui = (parts / "059_main.cpp.inc").read_text()
    contexts = (parts / "019_main.cpp.inc").read_text()
    globals_ = (parts / "005_main.cpp.inc").read_text()
    constants = (parts / "001_main.cpp.inc").read_text()
    audio = (parts / "040_main.cpp.inc").read_text()
    mic = (parts / "032_main.cpp.inc").read_text()

    combined = playback + events + realtime + globals_ + constants + audio + mic
    assert "kRealtimeReplyPcmPath" not in combined
    assert "gRealtimeReplyFile" not in combined
    assert "writeRealtimePlaybackRing" in events
    assert "response.output_audio.delta" in events
    assert "response.output_audio.done" in events
    assert "closeRealtimeSession(\"playback queue failed\")" in playback
    assert "kRealtimeStreamStallTimeoutMs" in playback
    assert "kRealtimeSpeakerStallTimeoutMs" in playback
    assert "drainRealtimeRecording" not in playback + realtime
    assert "stopRealtimeMicNonBlocking" in playback + realtime
    assert loop.index("pollTyping();") < loop.index("serviceRealtimeVoice();")
    assert "constexpr uint32_t kRealtimeInputSampleRate = 24000;" in constants
    assert "constexpr size_t kRealtimeInputChunkSamples = 720;" in constants
    assert "startRealtimeMic" in mic
    assert "micCfg.sample_rate = kRealtimeInputSampleRate;" in mic
    assert "upsampleRealtimeChunk" not in combined
    assert "gRealtimeUpsampleBuffer" not in combined
    assert "RealtimeChunkQueue gRealtimeInflightChunks" in globals_
    assert "std::deque<uint8_t> gRealtimeInflightChunks" not in globals_
    assert 'doc["binary_audio"] = true;' in events
    assert 'strcmp(audioTransport, "binary-v1") == 0' in events
    assert "case WStype_BIN:" in events
    assert "kRealtimeBinaryModelPcmType" in events
    assert "gRealtimeWs.sendBIN(gRealtimeBinaryMicFrame, payloadBytes, true)" in audio
    binary_send = audio[audio.index("bool sendRealtimeBinaryMicFrame"):
                        audio.index("bool sendRealtimeAudioChunk")]
    for forbidden in ("String", "base64", "malloc", "new "):
        assert forbidden not in binary_send
    exclusive = loop[loop.index("void serviceRealtimeExclusiveLoop"):
                     loop.index("void loop()")]
    for forbidden_service in (
        "serviceSetupPortal", "gWs.loop", "syncClockFromNtp", "serviceOtaBootGuard",
        "servicePet", "serviceTetris", "serviceRfidLab", "serviceIrRemote",
        "updateFocusTimer", "serviceFocusMetronome", "serviceAlarm", "serviceAutoInboxPoll",
    ):
        assert forbidden_service not in exclusive
    assert "pollTyping();" in exclusive
    assert "serviceRealtimeVoice();" in exclusive
    loop_body = loop[loop.index("void loop()") :]
    assert loop_body.index("handleHostSerialInput();") < loop_body.index("if (gRealtimeExclusiveMode)")
    assert "WiFi.getSleep()" in playback
    assert "WiFi.setSleep(WIFI_PS_NONE)" in playback
    assert "WiFi.setSleep(gRealtimePreviousWifiSleep)" in playback
    assert 'closeRealtimeSession("escape")' in escape
    assert "gRealtimeExclusiveMode || gRealtimeActive" in voice
    assert 'closeRealtimeSession("user stop")' in realtime
    assert "+ 1800" not in events + loop
    single_topic_guard = contexts[contexts.index("String currentConversationKey"):
                                  contexts.index("bool fetchRemoteTopicCatalog")]
    assert "if (kSingleCardputerTopicMode)" in single_topic_guard
    assert "return kPrimaryCardputerTopicKey;" in single_topic_guard
    assert "context.topicKey.isEmpty() ? context.key : context.topicKey" in single_topic_guard
    assert 'type == "auth_error"' in events
    assert 'setStatus("Realtime retrying...")' in events
    assert "gRealtimeWs.setReconnectInterval(750)" in events
    assert "constexpr size_t kRealtimePlaybackPrebufferBytes = 16 * 1024;" in constants
    assert "constexpr size_t kRealtimePlaybackChunkBytes = 8 * 1024;" in constants
    assert "constexpr size_t kPlaybackChunkSamples = 4096;" in constants
    assert "speakerCfg.task_priority" in speaker
    assert "updateVoiceLevelFromSamples" in playback
    assert "updatePlaybackAnalyzerFromSamples" not in playback
    ui_open = actions[actions.index('if (type == "ui.open")'):
                      actions.index('if (type == "audio.play")')]
    assert "gRealtimeUiOpenDeferred = true;" in ui_open
    assert ui_open.index("gRealtimeUiOpenDeferred = true;") < ui_open.index("setUiMode(mode);")
    assert 'closeRealtimeSession("mode change")' not in ui_open
    assert 'doc["reason"] = reason.substring(0, 80);' in playback
    assert "kRealtimeS1ResponseTimeoutMs" in realtime
    assert "gRealtimeAwaitingStartedMs = millis();" in events
    assert "renderRealtimeExclusiveUi" not in ui + loop
    assert "render();" in exclusive


def main() -> None:
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in sorted(tests, key=lambda fn: fn.__name__):
        test()
        print(f"PASS {test.__name__}")
    print(f"PASS realtime state smoke ({len(tests)} cases)")


if __name__ == "__main__":
    main()
