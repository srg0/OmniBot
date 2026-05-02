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

void stopRealtimeRecordingForVad() {
  if (!gRealtimeRecording) {
    return;
  }
  gRealtimeRecording = false;
  drainRealtimeRecording();
  stopMic();
  gRealtimeInflightChunks.clear();
  gRealtimeAwaiting = true;
  gRealtimeAwaitingStartedMs = millis();
  gRealtimeRestartAfterPlayback = false;
  setFaceMode(FaceMode::Thinking);
  setStatus("Realtime thinking...");
  setVoiceDiag("realtime: vad stopped");
  appendRuntimeLog("RT", "vad stop record", false);
  logf("[RT] vad stop record free=%u largest=%u",
       static_cast<unsigned>(ESP.getFreeHeap()),
       static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
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
  gRealtimeHandsFreeActive = false;
  gRealtimeRestartAfterPlayback = false;
  gRealtimeNextListenMs = 0;
  gRealtimeSmokeAudioPending = false;
  gRealtimeSmokeAudioStreaming = false;
  gRealtimeSmokeAudioChunkIndex = 0;
  gRealtimeSmokeAudioNextMs = 0;
  gRealtimeAwaiting = false;
  gRealtimeGotAudio = false;
  gRealtimeSessionId = "";
  setVoiceDiag("realtime: " + reason);
}
