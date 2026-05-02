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

void handleRealtimeAudioDeltaPayload(uint8_t* payload, size_t length) {
  const char* audioStart = nullptr;
  size_t audioLen = 0;
  if (!realtimeFindJsonStringField(payload, length, "audio", audioStart, audioLen) || audioLen == 0) {
    return;
  }
  if (!ensureRealtimeReplyFile()) {
    appendRuntimeLog("RT", "reply file unavailable", true);
    return;
  }
  base64_decodestate state;
  base64_init_decodestate(&state);

  uint8_t decoded[768];
  bool wroteAudio = false;
  size_t offset = 0;
  while (offset < audioLen) {
    const size_t remaining = audioLen - offset;
    const size_t inputLen = min<size_t>(1024, remaining);
    int decodedLen = base64_decode_block(audioStart + offset,
                                         static_cast<int>(inputLen),
                                         reinterpret_cast<char*>(decoded),
                                         &state);
    if (decodedLen > 0) {
      size_t written = gRealtimeReplyFile.write(decoded, decodedLen);
      gRealtimeReplyBytes += written;
      wroteAudio = wroteAudio || written > 0;
    }
    offset += inputLen;
  }

  if (wroteAudio) {
    gRealtimeGotAudio = true;
    setFaceMode(FaceMode::Speaking);
    setStatus("Realtime audio...", 700);
  }
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
    gRealtimeSessionId = doc["session_id"] | "";
    appendRuntimeLog("RT", "ready " + gRealtimeSessionId, false);
    logf("[RT] ready session=%s sample_rate=%u",
         gRealtimeSessionId.c_str(),
         static_cast<unsigned>(doc["sample_rate"] | 0));
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
      bool sent = gRealtimeWs.sendTXT(out);
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
      if (!wasSmoke && gRealtimeHandsFreeActive && gRealtimeActive && gRealtimeConnected) {
        gRealtimeRestartAfterPlayback = true;
        gRealtimeNextListenMs = millis() + 1800;
      }
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
  if (type == "audio.delta") {
    return;
  }
  if (type == "audio.done") {
    closeRealtimeReplyFile();
    if (gRealtimeReplyBytes > 0) {
      appendRuntimeLog("RT", "audio bytes=" + String(static_cast<unsigned>(gRealtimeReplyBytes)), false);
      startPlaybackStreamFromRawPcmFile(kRealtimeReplyPcmPath,
                                        kRealtimeModelSampleRate,
                                        gRealtimeReplyBytes,
                                        "realtime reply");
    } else {
      appendRuntimeLog("RT", "audio done empty", true);
    }
    return;
  }
  if (type == "realtime.error" || type == "protocol_error") {
    const String message = doc["message"] | "realtime error";
    appendRuntimeLog("RT", "error " + message, true);
    logf("[RT] error type=%s message=%s", type.c_str(), message.c_str());
    pushError("Realtime: " + message);
    setFaceMode(FaceMode::Error);
    setStatus("Realtime error", 1800);
    gRealtimeAwaiting = false;
    return;
  }
  if (type == "realtime.closed") {
    const String reason = doc["reason"] | "closed";
    gRealtimeActive = false;
    gRealtimeConnected = false;
    gRealtimeAwaiting = false;
    closeRealtimeReplyFile();
    setStatus("Realtime closed", 1000);
    appendRuntimeLog("RT", "closed " + reason, false);
    logf("[RT] closed reason=%s", reason.c_str());
  }
}

void realtimeWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      gRealtimeConnected = false;
      if (gRealtimeActive) {
        appendRuntimeLog("RT", "disconnected", true);
        setStatus("Realtime disconnected", 1200);
        logf("[RT] ws disconnected active=1");
        if (!gRealtimeConnected && !gRealtimeRecording && !gRealtimeAwaiting) {
          bool wasSmoke = gRealtimeSmokeActive;
          gRealtimeActive = false;
          gRealtimeSmokeActive = false;
          gRealtimeSmokeTextPending = false;
          closeRealtimeReplyFile();
          resumeHubWebSocketAfterVoice();
          if (wasSmoke) {
            logf("[SMOKE] realtime fail stage=connect_disconnect verify=%s",
                 gRealtimeTlsVerify ? "ca" : "insecure");
          }
        }
      }
      break;
    case WStype_CONNECTED: {
      logf("[RT] ws connected host=%s port=%u path=%s",
           gHubHost.c_str(), gHubPort, kRealtimeWsPath);
      DynamicJsonDocument doc(768);
      doc["type"] = "connect";
      doc["device_id"] = gDeviceId;
      doc["device_token"] = gDeviceToken;
      doc["topic_key"] = currentConversationKey();
      doc["sample_rate"] = kRealtimeModelSampleRate;
      String out;
      serializeJson(doc, out);
      bool sent = gRealtimeWs.sendTXT(out);
      logf("[RT] auth sent=%d topic_len=%u",
           static_cast<int>(sent),
           static_cast<unsigned>(currentConversationKey().length()));
      setStatus("Realtime auth...", 700);
      break;
    }
    case WStype_TEXT: {
      logf("[RT] ws text bytes=%u", static_cast<unsigned>(length));
      if (realtimePayloadContains(payload, length, "\"type\":\"audio.delta\"")) {
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
    case WStype_ERROR:
      appendRuntimeLog("RT", "ws error", true);
      setStatus("Realtime WS error", 1200);
      logf("[RT] ws error bytes=%u", static_cast<unsigned>(length));
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
  pauseHubWebSocketForVoice();
  resetRealtimeTurnState();
  gRealtimeActive = true;
  gRealtimeConnected = false;
  gRealtimeStartAfterReady = startAfterReady;
  gRealtimeAwaiting = false;
  gRealtimeRestartAfterPlayback = false;
  gRealtimeSessionStartedMs = millis();
  gRealtimeWs.setExtraHeaders();
  if (hubUsesTls() && gRealtimeTlsVerify) {
    gRealtimeWs.beginSslWithCA(gHubHost.c_str(), gHubPort, kRealtimeWsPath, kTlsRootCaPem, "");
  } else if (hubUsesTls()) {
    gRealtimeWs.beginSSL(gHubHost.c_str(), gHubPort, kRealtimeWsPath, "", "");
  } else {
    gRealtimeWs.begin(gHubHost.c_str(), gHubPort, kRealtimeWsPath, "");
  }
  gRealtimeWs.onEvent(realtimeWsEvent);
  gRealtimeWs.setReconnectInterval(0);
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
    stopPlaybackStream();
    gPlaybackActive = false;
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
  startMic();
  serviceRealtimeRecording(true);
  setFaceMode(FaceMode::Listening);
  setStatus("Realtime listening...");
  setVoiceDiag("realtime: recording");
  appendRuntimeLog("RT", "record start", false);
  gRealtimeAwaitingStartedMs = 0;
  logf("[RT] record start free=%u largest=%u",
       static_cast<unsigned>(ESP.getFreeHeap()),
       static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}
