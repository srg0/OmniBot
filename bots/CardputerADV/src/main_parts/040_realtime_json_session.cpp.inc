}

bool sendRealtimeAudioChunk(const int16_t* samples, size_t sampleCount) {
  if (!gRealtimeActive || !gRealtimeConnected) {
    return false;
  }
  String encoded = base64::encode(reinterpret_cast<const uint8_t*>(samples),
                                  sampleCount * sizeof(int16_t));
  encoded.replace("\n", "");
  encoded.replace("\r", "");
  DynamicJsonDocument doc(768 + encoded.length());
  doc["type"] = "audio.append";
  doc["audio"] = encoded;
  String payload;
  serializeJson(doc, payload);
  return gRealtimeWs.sendTXT(payload);
}

void flushCompletedRealtimeChunks(size_t completedCount) {
  for (size_t i = 0; i < completedCount && !gRealtimeInflightChunks.empty(); ++i) {
    uint8_t chunkIndex = gRealtimeInflightChunks.front();
    gRealtimeInflightChunks.pop_front();
    updateVoiceLevelFromSamples(gRealtimeInputBuffers[chunkIndex], kRealtimeInputChunkSamples);
    upsampleRealtimeChunk(gRealtimeInputBuffers[chunkIndex]);
    if (!sendRealtimeAudioChunk(gRealtimeUpsampleBuffer, kRealtimeOutputChunkSamples)) {
      setVoiceDiag("realtime: audio send failed");
      appendRuntimeLog("RT", "audio send failed", true);
      setFaceMode(FaceMode::Error);
      setStatus("Realtime send failed", 1600);
      break;
    }
  }
}

void serviceRealtimeRecording(bool refill = true) {
  if (!gRealtimeRecording && gRealtimeInflightChunks.empty()) {
    return;
  }
  size_t micQueued = M5Cardputer.Mic.isRecording();
  if (gRealtimeInflightChunks.size() > micQueued) {
    flushCompletedRealtimeChunks(gRealtimeInflightChunks.size() - micQueued);
  }
  if (!refill || !gRealtimeRecording) {
    return;
  }
  while (gRealtimeInflightChunks.size() < 2) {
    uint8_t chunkIndex = gRealtimeNextChunkIndex;
    gRealtimeNextChunkIndex = (gRealtimeNextChunkIndex + 1) % kRealtimeChunkBufferCount;
    bool alreadyInflight = false;
    for (uint8_t inflight : gRealtimeInflightChunks) {
      if (inflight == chunkIndex) {
        alreadyInflight = true;
        break;
      }
    }
    if (alreadyInflight) {
      continue;
    }
    if (!queueRealtimeChunk(chunkIndex)) {
      break;
    }
    gRealtimeInflightChunks.push_back(chunkIndex);
  }
}

void drainRealtimeRecording() {
  unsigned long deadline = millis() + kRecordDrainTimeoutMs;
  while (millis() < deadline) {
    serviceRealtimeRecording(false);
    if (M5Cardputer.Mic.isRecording() == 0 && gRealtimeInflightChunks.empty()) {
      return;
    }
    yield();
    delay(4);
  }
  serviceRealtimeRecording(false);
}

void closeRealtimeReplyFile() {
  if (gRealtimeReplyFile) {
    gRealtimeReplyFile.flush();
    gRealtimeReplyFile.close();
  }
}

bool ensureRealtimeReplyFile() {
  if (gRealtimeReplyFile) {
    return true;
  }
  if (!gStorageReady || !ensureVoiceStorage()) {
    setVoiceDiag("realtime: no storage");
    return false;
  }
  fs::FS* fs = activeVoiceFs();
  if (!fs) {
    setVoiceDiag("realtime: no fs");
    return false;
  }
  fsRemove(*fs, kRealtimeReplyPcmPath);
  gRealtimeReplyFile = fs->open(kRealtimeReplyPcmPath, "w");
  if (!gRealtimeReplyFile) {
    setVoiceDiag("realtime: reply file failed");
    return false;
  }
  gRealtimeReplyPath = kRealtimeReplyPcmPath;
  gRealtimeReplyBytes = 0;
  return true;
}

void resetRealtimeTurnState() {
  gRealtimeText = "";
  gRealtimeGotAudio = false;
  gRealtimeReplyBytes = 0;
  closeRealtimeReplyFile();
  fs::FS* fs = activeVoiceFs();
  if (fs) {
    fsRemove(*fs, kRealtimeReplyPcmPath);
  }
}

void closeRealtimeSession(const String& reason) {
  if (gRealtimeRecording) {
    gRealtimeRecording = false;
    drainRealtimeRecording();
    stopMic();
  }
  closeRealtimeReplyFile();
  gRealtimeInflightChunks.clear();
  if (gRealtimeActive || gRealtimeConnected) {
    DynamicJsonDocument doc(128);
    doc["type"] = "close";
    String payload;
    serializeJson(doc, payload);
    gRealtimeWs.sendTXT(payload);
    gRealtimeWs.disconnect();
  }
  gRealtimeActive = false;
  gRealtimeConnected = false;
  gRealtimeStartAfterReady = false;
  gRealtimeAwaiting = false;
  gRealtimeGotAudio = false;
  gRealtimeSessionId = "";
  setVoiceDiag("realtime: " + reason);
}

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
