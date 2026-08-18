bool realtimePayloadContains(uint8_t* payload, size_t length, const char* needle) {
  if (!payload || !needle) {
    return false;
  }
  size_t needleLen = strlen(needle);
  if (needleLen == 0 || length < needleLen) {
    return false;
  }
  for (size_t i = 0; i + needleLen <= length; ++i) {
    if (memcmp(payload + i, needle, needleLen) == 0) {
      return true;
    }
  }
  return false;
}

bool realtimeFindJsonStringField(uint8_t* payload, size_t length, const char* key,
                                 const char*& valueStart, size_t& valueLen) {
  valueStart = nullptr;
  valueLen = 0;
  if (!payload || !key) {
    return false;
  }
  String needle = String("\"") + key + "\":\"";
  size_t needleLen = needle.length();
  if (needleLen == 0 || length < needleLen) {
    return false;
  }
  for (size_t i = 0; i + needleLen <= length; ++i) {
    if (memcmp(payload + i, needle.c_str(), needleLen) != 0) {
      continue;
    }
    size_t start = i + needleLen;
    size_t end = start;
    bool escaped = false;
    while (end < length) {
      char ch = static_cast<char>(payload[end]);
      if (escaped) {
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        valueStart = reinterpret_cast<const char*>(payload + start);
        valueLen = end - start;
        return true;
      }
      ++end;
    }
    return false;
  }
  return false;
}

uint32_t readRealtimeLe32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

bool acceptRealtimePcmBytes(const uint8_t* pcm, size_t pcmBytes) {
  if (!pcm || pcmBytes == 0 || (pcmBytes & 1U) != 0) {
    return false;
  }
  const uint32_t now = millis();
  const bool firstAudio = gRealtimeFirstAudioDeltaMs == 0;
  if (firstAudio) {
    gRealtimeFirstAudioDeltaMs = now;
    const uint32_t fromOrigin = gRealtimeLatencyOriginMs == 0
      ? 0 : now - gRealtimeLatencyOriginMs;
    logf("[RT][LAT] first_audio_delta from_stop_ms=%u bytes=%u transport=%s",
         static_cast<unsigned>(fromOrigin), static_cast<unsigned>(pcmBytes),
         gRealtimeBinaryAudio ? "binary-v1" : "json");
  }
  gRealtimeLastAudioDeltaMs = now;
  gRealtimeGotAudio = true;

  // OpenAI can deliver generated audio much faster than its 24 kHz playback
  // clock.  Do not treat that normal burst as a fatal ring overflow.  While
  // this websocket callback is applying TCP backpressure, the speaker DMA
  // keeps running; feed each freed playback slot and copy the frame in parts.
  size_t accepted = 0;
  const uint32_t waitStartedMs = millis();
  while (accepted < pcmBytes && gRealtimeActive) {
    serviceRealtimePlayback();
    if (!gRealtimeActive) {
      return false;
    }
    const size_t freeBytes = kRealtimePlaybackRingBytes - gRealtimePlaybackBufferedBytes;
    if (freeBytes > 0) {
      const size_t chunk = min(pcmBytes - accepted, freeBytes);
      accepted += writeRealtimePlaybackRing(pcm + accepted, chunk);
      continue;
    }
    if (millis() - waitStartedMs > kRealtimeStreamStallTimeoutMs) {
      logf("[RT] playback backpressure timeout buffered=%u accepted=%u frame=%u",
           static_cast<unsigned>(gRealtimePlaybackBufferedBytes),
           static_cast<unsigned>(accepted),
           static_cast<unsigned>(pcmBytes));
      closeRealtimeSession("playback backpressure timeout");
      return false;
    }
    delay(1);
  }
  if (accepted != pcmBytes) {
    return false;
  }
  gRealtimeReplyBytes += accepted;
  if (firstAudio) {
    setFaceMode(FaceMode::Speaking);
    setStatus("Realtime audio...", 700);
  }
  serviceRealtimePlayback();
  return true;
}

void handleRealtimeAudioDeltaPayload(uint8_t* payload, size_t length) {
  const char* audioStart = nullptr;
  size_t audioLen = 0;
  if (!realtimeFindJsonStringField(payload, length, "audio", audioStart, audioLen)) {
    realtimeFindJsonStringField(payload, length, "delta", audioStart, audioLen);
  }
  if (!audioStart || audioLen == 0) {
    return;
  }
  base64_decodestate state;
  base64_init_decodestate(&state);

  uint8_t decoded[768];
  size_t offset = 0;
  while (offset < audioLen) {
    const size_t remaining = audioLen - offset;
    const size_t inputLen = min<size_t>(1024, remaining);
    int decodedLen = base64_decode_block(audioStart + offset,
                                         static_cast<int>(inputLen),
                                         reinterpret_cast<char*>(decoded),
                                         &state);
    if (decodedLen > 0) {
      if (!acceptRealtimePcmBytes(decoded, static_cast<size_t>(decodedLen))) {
        return;
      }
    }
    offset += inputLen;
  }
}

void handleRealtimeBinaryFrame(uint8_t* payload, size_t length) {
  if (!gRealtimeBinaryAudio || !payload || length < kRealtimeBinaryHeaderBytes) {
    closeRealtimeSession("unexpected binary audio");
    return;
  }
  if (payload[0] != kRealtimeBinaryModelPcmType ||
      payload[1] != kRealtimeBinaryVersion || payload[2] != 0 || payload[3] != 0) {
    closeRealtimeSession("invalid binary audio header");
    return;
  }
  const size_t pcmBytes = length - kRealtimeBinaryHeaderBytes;
  if (pcmBytes == 0 || (pcmBytes & 1U) != 0) {
    closeRealtimeSession("invalid binary pcm length");
    return;
  }
  const uint32_t sequence = readRealtimeLe32(payload + 4);
  if (gRealtimeBinaryModelSequenceSeen && sequence != gRealtimeExpectedModelSequence) {
    ++gRealtimeBinaryRxSequenceErrors;
    logf("[RT] binary sequence error expected=%u got=%u errors=%u",
         static_cast<unsigned>(gRealtimeExpectedModelSequence),
         static_cast<unsigned>(sequence),
         static_cast<unsigned>(gRealtimeBinaryRxSequenceErrors));
    closeRealtimeSession("binary sequence error");
    return;
  }
  gRealtimeBinaryModelSequenceSeen = true;
  gRealtimeExpectedModelSequence = sequence + 1;
  acceptRealtimePcmBytes(payload + kRealtimeBinaryHeaderBytes, pcmBytes);
}

String sanitizeRealtimeHeaderValue(const String& value) {
  String out = value;
  out.replace("\r", "");
  out.replace("\n", "");
  return out;
}

void configureRealtimeAuthHeaders() {
  gRealtimeExtraHeaders = "";
  const String deviceId = sanitizeRealtimeHeaderValue(gDeviceId);
  const String token = sanitizeRealtimeHeaderValue(gDeviceToken);
  if (!deviceId.isEmpty()) {
    gRealtimeExtraHeaders += "X-Device-Id: ";
    gRealtimeExtraHeaders += deviceId;
    gRealtimeExtraHeaders += "\r\n";
  }
  if (!token.isEmpty()) {
    gRealtimeExtraHeaders += "Authorization: Bearer ";
    gRealtimeExtraHeaders += token;
    gRealtimeExtraHeaders += "\r\n";
    gRealtimeExtraHeaders += "X-Device-Token: ";
    gRealtimeExtraHeaders += token;
    gRealtimeExtraHeaders += "\r\n";
  }
  // WebSocketsClient appends its own CRLF after extraHeaders. Leaving a CRLF
  // here creates an empty line before its User-Agent header, so the server
  // upgrades early and then sees that trailing HTTP header as a WS frame.
  if (gRealtimeExtraHeaders.endsWith("\r\n")) {
    gRealtimeExtraHeaders.remove(gRealtimeExtraHeaders.length() - 2);
  }
  if (gRealtimeExtraHeaders.isEmpty()) {
    gRealtimeWs.setExtraHeaders();
  } else {
    gRealtimeWs.setExtraHeaders(gRealtimeExtraHeaders.c_str());
  }
  logf("[RT] auth headers device=%d bearer=%d token_len=%u",
       static_cast<int>(!deviceId.isEmpty()),
       static_cast<int>(!token.isEmpty()),
       static_cast<unsigned>(token.length()));
}

bool sendRealtimeConnectFrame() {
  if (!gRealtimeActive || !gRealtimeWs.isConnected()) {
    return false;
  }
  DynamicJsonDocument doc(768);
  doc["type"] = "connect";
  doc["device_id"] = gDeviceId;
  doc["device_token"] = gDeviceToken;
  doc["topic_key"] = currentConversationKey();
  doc["sample_rate"] = kRealtimeModelSampleRate;
  doc["binary_audio"] = true;
  String out;
  serializeJson(doc, out);
  const bool sent = sendRealtimeTextFrame(out);
  logf("[RT] auth sent=%d deferred=1 bytes=%u topic_len=%u",
       static_cast<int>(sent),
       static_cast<unsigned>(out.length()),
       static_cast<unsigned>(currentConversationKey().length()));
  setStatus(sent ? "Realtime auth..." : "Realtime auth failed", sent ? 700 : 1400);
  return sent;
}

void handleRealtimeJson(const JsonDocument& doc) {
  const String type = doc["type"] | "";
  if (type == "connected") {
    const String protocol = doc["protocol"] | "";
    logf("[RT] connected protocol=%s", protocol.c_str());
    setStatus("Realtime connected", 700);
    return;
  }
  if (type == "realtime.ready") {
    gRealtimeConnected = true;
    const char* audioTransport = doc["audio_transport"] | "json";
    gRealtimeBinaryAudio = strcmp(audioTransport, "binary-v1") == 0;
    gRealtimeMicSequence = 0;
    gRealtimeBinaryModelSequenceSeen = false;
    gRealtimeExpectedModelSequence = 0;
    gRealtimeSessionId = doc["session_id"] | "";
    appendRuntimeLog("RT", "ready " + gRealtimeSessionId, false);
    logf("[RT] ready session=%s sample_rate=%u transport=%s",
         gRealtimeSessionId.c_str(),
         static_cast<unsigned>(doc["sample_rate"] | 0),
         gRealtimeBinaryAudio ? "binary-v1" : "json");
    setStatus("Realtime ready", 700);
    if (gRealtimeSmokeAudioPending) {
      beginRealtimeSyntheticAudioSmoke();
      return;
    }
    if (gRealtimeSmokeTextPending) {
      gRealtimeSmokeTextPending = false;
      DynamicJsonDocument smoke(256);
      smoke["type"] = "text.send";
      smoke["text"] = "Say one short word: ready.";
      String out;
      serializeJson(smoke, out);
      bool sent = sendRealtimeTextFrame(out);
      gRealtimeAwaiting = sent;
      gRealtimeAwaitingStartedMs = millis();
      logf("[SMOKE] realtime text_send sent=%d", static_cast<int>(sent));
      if (!sent) {
        setStatus("Realtime smoke send failed", 1600);
        setFaceMode(FaceMode::Error);
      }
      return;
    }
    if (gRealtimeStartAfterReady) {
      gRealtimeStartAfterReady = false;
      startRealtimeRecording();
    }
    return;
  }
  if (type == "speech.started") {
    setStatus("Realtime speech...", 500);
    return;
  }
  if (type == "speech.stopped") {
    if (gRealtimeRecording) {
      stopRealtimeRecordingForVad();
    } else {
      setStatus("Realtime heard", 500);
    }
    return;
  }
  if (type == "realtime.thinking") {
    gRealtimeAwaiting = true;
    setFaceMode(FaceMode::Thinking);
    setStatus("Realtime thinking...");
    return;
  }
  if (type == "transcript.done") {
    const String text = doc["text"] | "";
    if (!text.isEmpty()) {
      logf("[RT] transcript chars=%u", static_cast<unsigned>(text.length()));
      pushUser(text);
      setStatus("Transcript ready", 700);
    }
    return;
  }
  if (type == "text.delta") {
    const String delta = doc["delta"] | "";
    if (!delta.isEmpty()) {
      gRealtimeText += delta;
    }
    return;
  }
  if (type == "text.done" || type == "response.done") {
    String text = doc["text"] | "";
    if (text.isEmpty()) {
      text = gRealtimeText;
    }
    text.trim();
    if (!text.isEmpty()) {
      appendAssistantReply(text);
    }
    gRealtimeText = "";
    if (type == "response.done") {
      bool wasSmoke = gRealtimeSmokeActive;
      logf("[RT] response.done text_chars=%u audio=%d",
           static_cast<unsigned>(text.length()),
           static_cast<int>(gRealtimeGotAudio));
      if (wasSmoke) {
        logf("[SMOKE] realtime ok text_chars=%u audio=%d",
             static_cast<unsigned>(text.length()),
             static_cast<int>(gRealtimeGotAudio));
        gRealtimeSmokeActive = false;
      }
      gRealtimeAwaiting = false;
      gRealtimeResponseComplete = true;
      if (gRealtimeGotAudio && !gRealtimeAudioComplete) {
        // response.done is terminal; flush a short final PCM block even if the
        // bridge-side audio.done event was lost.
        gRealtimeAudioComplete = true;
        gRealtimeAudioDoneMs = millis();
      }
      serviceRealtimePlayback();
      const uint32_t firstDeltaMs = gRealtimeLatencyOriginMs == 0 || gRealtimeFirstAudioDeltaMs == 0
        ? 0 : gRealtimeFirstAudioDeltaMs - gRealtimeLatencyOriginMs;
      const uint32_t playbackMs = gRealtimeLatencyOriginMs == 0 || gRealtimePlaybackStartMs == 0
        ? 0 : gRealtimePlaybackStartMs - gRealtimeLatencyOriginMs;
      logf("[RT][LAT] response_done first_delta_ms=%u playback_ms=%u received=%u dropped=%u underruns=%u",
           static_cast<unsigned>(firstDeltaMs), static_cast<unsigned>(playbackMs),
           static_cast<unsigned>(gRealtimeReplyBytes),
           static_cast<unsigned>(gRealtimePlaybackDroppedBytes),
           static_cast<unsigned>(gRealtimePlaybackUnderruns));
      scheduleRealtimeListenAfterPlayback();
      if (!gPlaybackActive && !gRealtimeGotAudio) {
        setFaceMode(FaceMode::Idle);
        setStatus("Realtime done", 1000);
      }
    }
    return;
  }
  if (type == "device_actions") {
    String actionsJson;
    serializeJson(doc["actions"], actionsJson);
    executeDeviceActionsJson(actionsJson);
    return;
  }
  if (type == "audio.delta" || type == "response.output_audio.delta") {
    return;
  }
  if (type == "audio.done" || type == "response.output_audio.done") {
    gRealtimeAudioComplete = true;
    gRealtimeAudioDoneMs = millis();
    if (gRealtimeReplyBytes > 0) {
      appendRuntimeLog("RT", "audio bytes=" + String(static_cast<unsigned>(gRealtimeReplyBytes)), false);
      serviceRealtimePlayback();
    } else {
      appendRuntimeLog("RT", "audio done empty", true);
      serviceRealtimePlayback();
    }
    return;
  }
  if (type == "realtime.error" || type == "protocol_error" || type == "auth_error") {
    const String message = doc["message"] | "realtime error";
    appendRuntimeLog("RT", "error " + message, true);
    logf("[RT] error type=%s message=%s", type.c_str(), message.c_str());
    pushError("Realtime: " + message);
    setFaceMode(FaceMode::Error);
    setStatus("Realtime error", 1800);
    closeRealtimeSession("protocol error");
    return;
  }
  if (type == "realtime.closed") {
    const String reason = doc["reason"] | "closed";
    if (gRealtimeRecording) {
      gRealtimeRecording = false;
      stopMic();
      gRealtimeInflightChunks.clear();
    }
    gRealtimeActive = false;
    gRealtimeConnected = false;
    gRealtimeAwaiting = false;
    gRealtimeRecording = false;
    gRealtimeHandsFreeActive = false;
    gRealtimeRestartAfterPlayback = false;
    leaveRealtimeExclusiveMode(reason);
    setStatus("Realtime closed", 1000);
    appendRuntimeLog("RT", "closed " + reason, false);
    logf("[RT] closed reason=%s", reason.c_str());
  }
}

void realtimeWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: {
      const bool wasReady = gRealtimeConnected;
      gRealtimeConnected = false;
      gRealtimeAuthPending = false;
      gRealtimeAuthDueMs = 0;
      if (gRealtimeActive) {
        // A short-lived TLS/upgrade failure must not eject the user from the
        // mode. WebSocketsClient will reconnect; keep the pending start until
        // the bounded connect watchdog expires. Explicit auth/protocol errors
        // are handled above and clear gRealtimeActive before this callback.
        const uint32_t now = millis();
        const bool stillConnecting = !wasReady && !gRealtimeRecording &&
                                     gRealtimeSessionStartedMs != 0 &&
                                     now - gRealtimeSessionStartedMs <= 15000U;
        if (stillConnecting) {
          appendRuntimeLog("RT", "connect interrupted; retrying", true);
          setStatus("Realtime retrying...");
          logf("[RT] ws connect interrupted; retrying elapsed_ms=%u reason_bytes=%u",
               static_cast<unsigned>(now - gRealtimeSessionStartedMs),
               static_cast<unsigned>(length));
          break;
        }
        bool wasSmoke = gRealtimeSmokeActive;
        appendRuntimeLog("RT", "disconnected", true);
        setStatus("Realtime disconnected", 1200);
        logf("[RT] ws disconnected active=1");
        if (gRealtimeRecording) {
          gRealtimeRecording = false;
          M5Cardputer.Mic.end();
          gRealtimeInflightChunks.clear();
          initSpeaker();
        }
        gRealtimeActive = false;
        gRealtimeAwaiting = false;
        gRealtimeStartAfterReady = false;
        gRealtimeHandsFreeActive = false;
        gRealtimeRestartAfterPlayback = false;
        gRealtimeNextListenMs = 0;
        gRealtimeSmokeActive = false;
        gRealtimeSmokeTextPending = false;
        gRealtimeSmokeAudioPending = false;
        gRealtimeSmokeAudioStreaming = false;
        gRealtimeBinaryAudio = false;
        gRealtimeBinaryModelSequenceSeen = false;
        gRealtimeSessionId = "";
        gRealtimeExtraHeaders = "";
        gRealtimeWs.setExtraHeaders();
        leaveRealtimeExclusiveMode("disconnect");
        if (wasSmoke) {
          logf("[SMOKE] realtime fail stage=connect_disconnect verify=%s",
               gRealtimeTlsVerify ? "ca" : "insecure");
        }
      }
      break;
    }
    case WStype_CONNECTED: {
      logf("[RT] ws connected host=%s port=%u path=%s",
           gHubHost.c_str(), gHubPort, kRealtimeWsPath);
      // Do not write from inside WStype_CONNECTED. With ESP32-S3 TLS this
      // re-enters WebSocketsClient while it is still unwinding the HTTP 101
      // parser and has produced malformed frames with RSV2/RSV3 set. Send
      // from the normal loop after the callback has returned instead.
      gRealtimeAuthPending = true;
      gRealtimeAuthDueMs = millis() + 40U;
      logf("[RT] auth deferred delay_ms=40");
      setStatus("Realtime auth queued", 500);
      break;
    }
    case WStype_TEXT: {
      logf("[RT] ws text bytes=%u", static_cast<unsigned>(length));
      if (realtimePayloadContains(payload, length, "\"type\":\"audio.delta\"") ||
          realtimePayloadContains(payload, length, "\"type\":\"response.output_audio.delta\"")) {
        handleRealtimeAudioDeltaPayload(payload, length);
        break;
      }
      DynamicJsonDocument doc(min<size_t>(4096, max<size_t>(1024, length + 768)));
      auto err = deserializeJson(doc, payload, length);
      if (err) {
        appendRuntimeLog("RT", "json parse failed", true);
        setStatus("Realtime JSON failed", 1200);
        logf("[RT] json parse failed err=%s bytes=%u free=%u largest=%u",
             err.c_str(),
             static_cast<unsigned>(length),
             static_cast<unsigned>(ESP.getFreeHeap()),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
        break;
      }
      handleRealtimeJson(doc);
      break;
    }
    case WStype_BIN:
      handleRealtimeBinaryFrame(payload, length);
      break;
    case WStype_ERROR:
      appendRuntimeLog("RT", "ws error", true);
      setStatus("Realtime WS error", 1200);
      logf("[RT] ws error bytes=%u", static_cast<unsigned>(length));
      closeRealtimeSession("ws error");
      break;
    default:
      break;
  }
}

void startRealtimeSession(bool startAfterReady) {
  if (gRealtimeActive) {
    if (gRealtimeConnected && startAfterReady) {
      startRealtimeRecording();
    } else {
      gRealtimeStartAfterReady = startAfterReady;
    }
    return;
  }
  if (!gWifiReady || gHubHost.isEmpty()) {
    pushError("Realtime needs Wi-Fi and bridge.");
    setStatus("Realtime unavailable", 1400);
    setFaceMode(FaceMode::Error);
    return;
  }
  if (!enterRealtimeExclusiveMode()) {
    return;
  }
  resetRealtimeTurnState();
  gRealtimeActive = true;
  gRealtimeConnected = false;
  gRealtimeStartAfterReady = startAfterReady;
  gRealtimeAwaiting = false;
  gRealtimeRestartAfterPlayback = false;
  gRealtimeBinaryAudio = false;
  gRealtimeBinaryModelSequenceSeen = false;
  gRealtimeMicSequence = 0;
  gRealtimeExpectedModelSequence = 0;
  gRealtimeAuthPending = false;
  gRealtimeAuthDueMs = 0;
  gRealtimeSessionStartedMs = millis();
  configureRealtimeAuthHeaders();
  if (hubUsesTls() && gRealtimeTlsVerify) {
    gRealtimeWs.beginSslWithCA(gHubHost.c_str(), gHubPort, kRealtimeWsPath, kTlsRootCaPem, "");
  } else if (hubUsesTls()) {
    gRealtimeWs.beginSSL(gHubHost.c_str(), gHubPort, kRealtimeWsPath, "", "");
  } else {
    gRealtimeWs.begin(gHubHost.c_str(), gHubPort, kRealtimeWsPath, "");
  }
  gRealtimeWs.onEvent(realtimeWsEvent);
  gRealtimeWs.setReconnectInterval(750);
  gRealtimeWs.enableHeartbeat(15000, 3000, 2);
  setStatus("Realtime connecting...");
  appendRuntimeLog("RT", "connect host=" + gHubHost + ":" + String(gHubPort), false);
  logf("[RT] connect host=%s port=%u tls=%d verify=%s free=%u largest=%u",
       gHubHost.c_str(),
       gHubPort,
       static_cast<int>(hubUsesTls()),
       hubUsesTls() ? (gRealtimeTlsVerify ? tlsVerifyMode() : "insecure") : "none",
       static_cast<unsigned>(ESP.getFreeHeap()),
       static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}

void startRealtimeRecording() {
  if (!gRealtimeActive || !gRealtimeConnected) {
    startRealtimeSession(true);
    return;
  }
  if (gRealtimeRecording || gRecording || gSubmitting || gThinking) {
    setStatus("Realtime busy", 800);
    return;
  }
  if (gPlaybackActive) {
    setStatus("Realtime speaking", 700);
    setVoiceDiag("realtime: half-duplex; wait for playback");
    return;
  }
  resetRealtimeTurnState();
  gRealtimeInflightChunks.clear();
  gRealtimeNextChunkIndex = 0;
  gRealtimeRecording = true;
  gRealtimeAwaiting = false;
  gRealtimeHandsFreeActive = true;
  gRealtimeRestartAfterPlayback = false;
  gRealtimeStartedMs = millis();
  gVoiceLevel8 = 36;
  pulseButtonPress();
  startRealtimeMic();
  serviceRealtimeRecording(true);
  if (gRealtimeInflightChunks.empty()) {
    gRealtimeRecording = false;
    M5Cardputer.Mic.end();
    initSpeaker();
    setFaceMode(FaceMode::Error);
    setStatus("Realtime mic failed", 1800);
    setVoiceDiag("realtime: mic queue failed");
    logf("[RT] record start failed mic_queue=0 free=%u largest=%u",
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    closeRealtimeSession("mic start failed");
    return;
  }
  setFaceMode(FaceMode::Listening);
  setStatus("Realtime listening...");
  setVoiceDiag("realtime: recording");
  appendRuntimeLog("RT", "record start", false);
  gRealtimeAwaitingStartedMs = 0;
  logf("[RT] record start rate=%u chunk_ms=%u transport=%s queued=%u free=%u largest=%u",
       static_cast<unsigned>(kRealtimeInputSampleRate),
       static_cast<unsigned>(kRealtimeInputChunkSamples * 1000 / kRealtimeInputSampleRate),
       gRealtimeBinaryAudio ? "binary-v1" : "json",
       static_cast<unsigned>(gRealtimeInflightChunks.size()),
       static_cast<unsigned>(ESP.getFreeHeap()),
       static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}
