void handleRealtimeJson(const JsonDocument& doc) {
  const String type = doc["type"] | "";
  if (type == "connected") {
    setStatus("Realtime connected", 700);
    return;
  }
  if (type == "realtime.ready") {
    gRealtimeConnected = true;
    gRealtimeSessionId = doc["session_id"] | "";
    appendRuntimeLog("RT", "ready " + gRealtimeSessionId, false);
    setStatus("Realtime ready", 700);
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
    setStatus("Realtime heard", 500);
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
      gRealtimeAwaiting = false;
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
    const String audio = doc["audio"] | "";
    if (audio.isEmpty()) {
      return;
    }
    if (!ensureRealtimeReplyFile()) {
      appendRuntimeLog("RT", "reply file unavailable", true);
      return;
    }
    const int maxDecoded = static_cast<int>((audio.length() * 3U) / 4U + 8U);
    uint8_t* decoded = static_cast<uint8_t*>(malloc(maxDecoded));
    if (!decoded) {
      appendRuntimeLog("RT", "audio decode oom", true);
      setStatus("Realtime OOM", 1200);
      return;
    }
    int decodedLen = base64_decode_chars(audio.c_str(), static_cast<int>(audio.length()),
                                         reinterpret_cast<char*>(decoded));
    if (decodedLen > 0) {
      size_t written = gRealtimeReplyFile.write(decoded, decodedLen);
      gRealtimeReplyBytes += written;
      gRealtimeGotAudio = true;
      setFaceMode(FaceMode::Speaking);
      setStatus("Realtime audio...", 700);
    }
    free(decoded);
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
  }
}

void realtimeWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      gRealtimeConnected = false;
      if (gRealtimeActive) {
        appendRuntimeLog("RT", "disconnected", true);
        setStatus("Realtime disconnected", 1200);
      }
      break;
    case WStype_CONNECTED: {
      DynamicJsonDocument doc(768);
      doc["type"] = "connect";
      doc["device_id"] = gDeviceId;
      doc["device_token"] = gDeviceToken;
      doc["topic_key"] = currentConversationKey();
      doc["sample_rate"] = kRealtimeModelSampleRate;
      String out;
      serializeJson(doc, out);
      gRealtimeWs.sendTXT(out);
      setStatus("Realtime auth...", 700);
      break;
    }
    case WStype_TEXT: {
      DynamicJsonDocument doc(8192);
      auto err = deserializeJson(doc, payload, length);
      if (err) {
        appendRuntimeLog("RT", "json parse failed", true);
        setStatus("Realtime JSON failed", 1200);
        break;
      }
      handleRealtimeJson(doc);
      break;
    }
    case WStype_ERROR:
      appendRuntimeLog("RT", "ws error", true);
      setStatus("Realtime WS error", 1200);
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
  resetRealtimeTurnState();
  gRealtimeActive = true;
  gRealtimeConnected = false;
  gRealtimeStartAfterReady = startAfterReady;
  gRealtimeAwaiting = false;
  if (hubUsesTls()) {
    gRealtimeWs.beginSSL(gHubHost.c_str(), gHubPort, kRealtimeWsPath);
  } else {
    gRealtimeWs.begin(gHubHost.c_str(), gHubPort, kRealtimeWsPath);
  }
  gRealtimeWs.onEvent(realtimeWsEvent);
  gRealtimeWs.setReconnectInterval(0);
  gRealtimeWs.enableHeartbeat(15000, 3000, 2);
  setStatus("Realtime connecting...");
  appendRuntimeLog("RT", "connect host=" + gHubHost + ":" + String(gHubPort), false);
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
  gRealtimeStartedMs = millis();
  gVoiceLevel8 = 36;
  pulseButtonPress();
  startMic();
  serviceRealtimeRecording(true);
  setFaceMode(FaceMode::Listening);
  setStatus("Realtime listening...");
  setVoiceDiag("realtime: recording");
  appendRuntimeLog("RT", "record start", false);
}

void stopRealtimeRecordingAndCommit() {
  if (!gRealtimeRecording) {
    return;
  }
  gRealtimeRecording = false;
  pulseButtonPress();
  drainRealtimeRecording();
  stopMic();
  gRealtimeInflightChunks.clear();
  DynamicJsonDocument doc(128);
  doc["type"] = "audio.commit";
  String payload;
  serializeJson(doc, payload);
  bool sent = gRealtimeConnected && gRealtimeWs.sendTXT(payload);
  if (!sent) {
    appendRuntimeLog("RT", "commit send failed", true);
    setStatus("Realtime commit failed", 1500);
    setFaceMode(FaceMode::Error);
    gRealtimeAwaiting = false;
    return;
  }
  gRealtimeAwaiting = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Realtime thinking...");
  setVoiceDiag("realtime: committed");
  appendRuntimeLog("RT", "record commit", false);
}
