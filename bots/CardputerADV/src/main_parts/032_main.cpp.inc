bool handlePetKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    return applyPetCare(suggestedPetAction());
  }
  if (lowerKey == 'f') {
    return applyPetCare(PetCareAction::Feed);
  }
  if (lowerKey == 'p') {
    return applyPetCare(PetCareAction::Play);
  }
  if (lowerKey == 'c') {
    return applyPetCare(PetCareAction::Clean);
  }
  if (lowerKey == 's') {
    return applyPetCare(PetCareAction::Sleep);
  }
  if (lowerKey == 'm') {
    return applyPetCare(PetCareAction::Medicine);
  }
  if (lowerKey == 'd') {
    return applyPetCare(PetCareAction::Discipline);
  }
  if (lowerKey == 'h') {
    return applyPetCare(PetCareAction::Hunt);
  }
  if (lowerKey == 'e') {
    return applyPetCare(PetCareAction::Explore);
  }
  if (lowerKey == 'r') {
    updatePetMood();
    setStatus(String(petMoodName(gPet.mood)) + " " + petAgeText(), 1000);
    return true;
  }
  return false;
}

bool copyFileBetweenFs(fs::FS& fromFs, fs::FS& toFs, const String& path) {
  if (!fsExists(fromFs, path)) {
    return false;
  }
  if (fsExists(toFs, path)) {
    return true;
  }
  File src = fromFs.open(path, "r");
  if (!src) {
    return false;
  }
  File dst = toFs.open(path, "w");
  if (!dst) {
    src.close();
    return false;
  }
  uint8_t buffer[1024];
  bool ok = true;
  while (src.available()) {
    size_t readBytes = src.read(buffer, sizeof(buffer));
    if (readBytes == 0) {
      break;
    }
    if (dst.write(buffer, readBytes) != readBytes) {
      ok = false;
      break;
    }
  }
  src.close();
  dst.close();
  if (!ok) {
    fsRemove(toFs, path);
  }
  return ok;
}

void migrateLittleFsVoiceNotesToSd() {
  if (!gSdReady || !gLittleFsReady || !ensureVoiceStorageOn(SD) || !LittleFS.exists(kVoiceDir)) {
    return;
  }
  File dir = LittleFS.open(kVoiceDir);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  uint16_t copied = 0;
  while (true) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }
    String path = file.path();
    bool regular = !file.isDirectory();
    file.close();
    if (!regular || path.indexOf("__active_") >= 0 ||
        (!path.endsWith(kVoiceMetaExt) && !path.endsWith(kVoiceAudioExt))) {
      continue;
    }
    if (copyFileBetweenFs(LittleFS, SD, path)) {
      ++copied;
    }
  }
  if (copied > 0) {
    logf("[SD] migrated %u voice files from LittleFS", static_cast<unsigned>(copied));
  }
}

void initStorage() {
  Serial.println("[BOOT] storage: LittleFS begin");
  gLittleFsReady = LittleFS.begin(true);
  if (!gLittleFsReady) {
    pushError("LittleFS init failed.");
  }

  Serial.println("[BOOT] storage: SD begin");
  gSdSpi.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  gSdReady = SD.begin(kSdCsPin, gSdSpi, kSdFrequencyHz);
  if (gSdReady && SD.cardType() == CARD_NONE) {
    SD.end();
    gSdReady = false;
  }
  if (gSdReady) {
    ensureVoiceStorageOn(SD);
    ensurePomodoroStorageOn(SD);
    logf("[SD] ready type=%u total=%lluMB used=%lluMB pins=sck%u miso%u mosi%u cs%u",
         static_cast<unsigned>(SD.cardType()),
         static_cast<unsigned long long>(SD.totalBytes() / (1024ULL * 1024ULL)),
         static_cast<unsigned long long>(SD.usedBytes() / (1024ULL * 1024ULL)),
         static_cast<unsigned>(kSdSckPin),
         static_cast<unsigned>(kSdMisoPin),
         static_cast<unsigned>(kSdMosiPin),
         static_cast<unsigned>(kSdCsPin));
    // Keep boot deterministic: migration can stall on corrupted LittleFS/SD metadata.
    logf("[SD] LittleFS voice migration skipped on boot");
  } else {
    logf("[SD] not mounted; using LittleFS fallback");
  }

  if (gOtaBootApplyMode) {
    Serial.println("[BOOT] storage: OTA minimal mode");
    gStorageReady = gSdReady || gLittleFsReady;
    gStorageLabel = activeVoiceStorageLabel();
    if (!gStorageReady) {
      setStatus("Storage unavailable", 1000);
    }
    return;
  }

  Serial.println("[BOOT] storage: scan emoji");
  scanEmojiAssets();
  gStorageReady = gSdReady || gLittleFsReady;
  gStorageLabel = activeVoiceStorageLabel();
  if (!gStorageReady) {
    setStatus("Storage unavailable", 1000);
    return;
  }
  Serial.println("[BOOT] storage: ensure voice");
  ensureVoiceStorage();
  if (!gSdReady && gLittleFsReady) {
    ensurePomodoroStorageOn(LittleFS);
  }
  Serial.println("[BOOT] storage: load voice notes");
  loadVoiceNotes();
  Serial.println("[BOOT] storage: defer audio scan");
  appendRuntimeLog("LIB", "audio scan deferred", false);
  Serial.println("[BOOT] storage: load manifests");
  loadAssetManifestSummary();
  loadOtaManifestSummary();
  Serial.println("[BOOT] storage: load contexts");
  loadConversationContexts();
  pushSystem("Storage: " + gStorageLabel);
  if (gEmojiAssetCount > 0) {
    pushSystem("Emoji SD: " + String(gEmojiAssetCount));
  }
  Serial.println("[BOOT] storage: ready");
}

bool remountSdStorage(const char* reason) {
  if (!gSdReady) {
    return false;
  }
  logf("[SD] remount reason=%s", reason ? reason : "-");
  SD.end();
  delay(120);
  gSdSpi.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  gSdReady = SD.begin(kSdCsPin, gSdSpi, kSdFrequencyHz);
  if (gSdReady && SD.cardType() == CARD_NONE) {
    SD.end();
    gSdReady = false;
  }
  if (gSdReady) {
    ensureVoiceStorageOn(SD);
    ensurePomodoroStorageOn(SD);
    gStorageReady = true;
    gStorageLabel = "sd";
    logf("[SD] remount ok freq=%u", static_cast<unsigned>(kSdFrequencyHz));
    return true;
  }
  gStorageReady = gLittleFsReady;
  gStorageLabel = activeVoiceStorageLabel();
  logf("[SD] remount failed");
  return false;
}

void startMic() {
  M5Cardputer.Speaker.end();
  auto micCfg = M5Cardputer.Mic.config();
  micCfg.sample_rate = kMicSampleRate;
  micCfg.magnification = 64;
  micCfg.noise_filter_level = 64;
  micCfg.task_priority = 1;
  M5Cardputer.Mic.config(micCfg);
  M5Cardputer.Mic.begin();
}

void stopMic() {
  M5Cardputer.Mic.end();
  initSpeaker();
}

void flushCompletedRecordChunksToFile(size_t completedCount) {
  for (size_t i = 0; i < completedCount && !gRecordInflightChunks.empty(); ++i) {
    uint8_t chunkIndex = gRecordInflightChunks.front();
    gRecordInflightChunks.pop_front();
    updateVoiceLevelFromSamples(gRecordChunkBuffers[chunkIndex], kRecordChunkSamples);
    if (gActiveRecordFile) {
      size_t written = gActiveRecordFile.write(reinterpret_cast<const uint8_t*>(gRecordChunkBuffers[chunkIndex]),
                                               kRecordChunkBytes);
      gActiveRecordBytes += written;
      gActiveRecordCommittedBytes += written;
      if (written != kRecordChunkBytes) {
        gRecordFileWriteFailed = true;
        gRecording = false;
        setHttpDiag("record fs short " + String(static_cast<unsigned>(written)) + "/" +
                    String(static_cast<unsigned>(kRecordChunkBytes)));
        logf("[VOICE] record fs short write=%u expected=%u",
             static_cast<unsigned>(written),
             static_cast<unsigned>(kRecordChunkBytes));
        setStatus("Record FS failed", 1400);
        setFaceMode(FaceMode::Error);
        return;
      }
    }
  }
}

bool queueRecordChunk(uint8_t chunkIndex) {
  return M5Cardputer.Mic.record(gRecordChunkBuffers[chunkIndex], kRecordChunkSamples, kMicSampleRate);
}

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
