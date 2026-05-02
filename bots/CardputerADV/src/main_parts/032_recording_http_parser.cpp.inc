
void serviceRecordingToFile(bool refill) {
  if (!gRecording || !gRecordingToFile) {
    return;
  }
  size_t micQueued = M5Cardputer.Mic.isRecording();
  if (gRecordInflightChunks.size() > micQueued) {
    flushCompletedRecordChunksToFile(gRecordInflightChunks.size() - micQueued);
  }
  if (!refill) {
    return;
  }
  while (gRecordInflightChunks.size() < 2) {
    uint8_t chunkIndex = gRecordNextChunkIndex;
    gRecordNextChunkIndex = (gRecordNextChunkIndex + 1) % kRecordChunkBufferCount;
    bool alreadyInflight = false;
    for (uint8_t inflight : gRecordInflightChunks) {
      if (inflight == chunkIndex) {
        alreadyInflight = true;
        break;
      }
    }
    if (alreadyInflight) {
      continue;
    }
    if (!queueRecordChunk(chunkIndex)) {
      break;
    }
    gRecordInflightChunks.push_back(chunkIndex);
  }
}

void drainRecordingToFile() {
  unsigned long deadline = millis() + kRecordDrainTimeoutMs;
  while (millis() < deadline) {
    serviceRecordingToFile(false);
    if (M5Cardputer.Mic.isRecording() == 0 && gRecordInflightChunks.empty()) {
      return;
    }
    yield();
    delay(4);
  }
  serviceRecordingToFile(false);
}

void startRecording() {
  if (gRecording || gSubmitting || gThinking || gPlaybackActive) {
    logf("[VOICE] start ignored rec=%d submit=%d think=%d play=%d", gRecording, gSubmitting,
         gThinking, gPlaybackActive);
    return;
  }
  if (!gWifiReady || gHubHost.isEmpty()) {
    pushError("Hub is not configured for voice.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice unavailable", 1500);
    setVoiceDiag("voice: no wifi/hub");
    logf("[VOICE] start blocked wifi=%d hub=%s", gWifiReady, gHubHost.c_str());
    return;
  }
  if (!gPlaybackActive && gTtsBuffer) {
    releaseTtsBuffer();
  }
  gRecordedSamples = 0;
  gRecording = true;
  gRecordStartMs = millis();
  gVoiceLevel8 = 36;
  pulseButtonPress();

  gRecordingToFile = gStorageReady && ensureVoiceStorage();
  if (gRecordingToFile) {
    fs::FS* fs = activeVoiceFs();
    fsRemove(*fs, kActiveRecordPcmPath);
    gActiveRecordFile = fs->open(kActiveRecordPcmPath, "w");
    if (!gActiveRecordFile) {
      gRecordingToFile = false;
    } else {
      gActiveRecordPath = kActiveRecordPcmPath;
      gActiveRecordBytes = 0;
      gActiveRecordCommittedBytes = 0;
      gActiveRecordExpectedBytes = 0;
      gRecordInflightChunks.clear();
      gRecordNextChunkIndex = 0;
    }
  }

  if (!gRecordingToFile) {
    logHeapStats("voice-start");
    if (!ensureRecordBuffer()) {
      pushError("Audio buffer allocation failed.");
      setFaceMode(FaceMode::Error);
      setStatus("No audio buffer", 1500);
      setVoiceDiag("voice: record buffer failed");
      logf("[VOICE] audio buffer allocation failed");
      gRecording = false;
      return;
    }
  }

  startMic();
  if (gRecordingToFile) {
    serviceRecordingToFile();
    setVoiceDiag("voice: recording fs");
  } else {
    M5Cardputer.Mic.record(gRecordBuffer, gRecordCapacitySamples, kMicSampleRate);
    setVoiceDiag("voice: recording");
  }
  setFaceMode(FaceMode::Listening);
  setStatus("Listening...");
  render();
  logf("[VOICE] recording started mode=%s", gRecordingToFile ? "file" : "ram");
}

void writeWavHeader(uint8_t* h, uint32_t dataSize, uint32_t sampleRate) {
  uint32_t fileSize = 36 + dataSize;
  uint32_t byteRate = sampleRate * 2;
  uint16_t blockAlign = 2;

  h[0] = 'R'; h[1] = 'I'; h[2] = 'F'; h[3] = 'F';
  h[4] = fileSize & 0xFF;
  h[5] = (fileSize >> 8) & 0xFF;
  h[6] = (fileSize >> 16) & 0xFF;
  h[7] = (fileSize >> 24) & 0xFF;
  h[8] = 'W'; h[9] = 'A'; h[10] = 'V'; h[11] = 'E';
  h[12] = 'f'; h[13] = 'm'; h[14] = 't'; h[15] = ' ';
  h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0;
  h[20] = 1; h[21] = 0;
  h[22] = 1; h[23] = 0;
  h[24] = sampleRate & 0xFF;
  h[25] = (sampleRate >> 8) & 0xFF;
  h[26] = (sampleRate >> 16) & 0xFF;
  h[27] = (sampleRate >> 24) & 0xFF;
  h[28] = byteRate & 0xFF;
  h[29] = (byteRate >> 8) & 0xFF;
  h[30] = (byteRate >> 16) & 0xFF;
  h[31] = (byteRate >> 24) & 0xFF;
  h[32] = blockAlign & 0xFF;
  h[33] = (blockAlign >> 8) & 0xFF;
  h[34] = 16; h[35] = 0;
  h[36] = 'd'; h[37] = 'a'; h[38] = 't'; h[39] = 'a';
  h[40] = dataSize & 0xFF;
  h[41] = (dataSize >> 8) & 0xFF;
  h[42] = (dataSize >> 16) & 0xFF;
  h[43] = (dataSize >> 24) & 0xFF;
}

bool parseHttpStatusAndHeaders(
    WiFiClient& client,
    int& statusCode,
    int& contentLength,
    uint32_t& sampleRate,
    uint32_t timeoutMs = 30000) {
  statusCode = 0;
  contentLength = -1;
  sampleRate = kTtsDefaultSampleRate;

  unsigned long deadline = millis() + timeoutMs;
  String line;
  auto readLine = [&](String& out) -> bool {
    out = "";
    while (millis() < deadline) {
      while (client.available() > 0) {
        int raw = client.read();
        if (raw < 0) {
          break;
        }
        char c = static_cast<char>(raw);
        if (c == '\n') {
          return true;
        }
        if (out.length() < 512) {
          out += c;
        }
      }
      if (!client.connected()) {
        return out.length() > 0;
      }
      pumpBlockingNetworkUi();
    }
    return out.length() > 0;
  };

  while (millis() < deadline) {
    if (!readLine(line)) {
      break;
    }
    line.trim();
    if (line.isEmpty() || !line.startsWith("HTTP/")) {
      continue;
    }

    int firstSpace = line.indexOf(' ');
