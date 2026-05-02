
void startHubWebSocket() {
  if (!kUseHubWebSocket) {
    gWsReady = false;
    setStatus("Hub HTTP mode", 1200);
    return;
  }
  if (gHubHost.isEmpty()) {
    pushError("Hub endpoint is empty. Use BLE provisioning.");
    return;
  }

  gWs.begin(gHubHost.c_str(), gHubPort, kWsPath);
  gWs.onEvent(wsEvent);
  gWs.setReconnectInterval(5000);
  gWs.enableHeartbeat(15000, 3000, 2);
  gLastWsReconnectMs = millis();
  setStatus("Connecting to hub...");
}

void pauseHubWebSocketForVoice() {
  if (!kUseHubWebSocket) {
    gResumeWsAfterVoice = false;
    return;
  }
  if (gBleActive || gHubHost.isEmpty()) {
    return;
  }
  gResumeWsAfterVoice = true;
  if (gWsReady) {
    logf("[WS] pause for voice");
    gWs.disconnect();
    gWsReady = false;
  }
}

void resumeHubWebSocketAfterVoice() {
  if (!kUseHubWebSocket) {
    gResumeWsAfterVoice = false;
    return;
  }
  if (!gResumeWsAfterVoice) {
    return;
  }
  gResumeWsAfterVoice = false;
  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (!gBleActive && gWifiReady && !gHubHost.isEmpty()) {
    logf("[WS] resume after voice");
    startHubWebSocket();
  }
}

bool queueRealtimeChunk(uint8_t chunkIndex) {
  return M5Cardputer.Mic.record(gRealtimeInputBuffers[chunkIndex],
                                kRealtimeInputChunkSamples,
                                kRealtimeInputSampleRate);
}

void upsampleRealtimeChunk(const int16_t* input) {
  for (size_t i = 0; i < kRealtimeInputChunkSamples; ++i) {
    const int16_t sample = input[i];
    const size_t out = i * kRealtimeUpsampleRatio;
    for (size_t r = 0; r < kRealtimeUpsampleRatio; ++r) {
      gRealtimeUpsampleBuffer[out + r] = sample;
    }
  }
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

