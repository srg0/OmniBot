  DynamicJsonDocument doc(8192);
  auto err = deserializeJson(doc, body);
  if (err || !doc["items"].is<JsonArray>()) {
    gContextError = err ? String(err.c_str()) : "Bad history";
    setStatus("History parse failed", 1400);
    return false;
  }

  String responseTitle = doc["topic"]["display_title"] | doc["topic"]["title"] | doc["title"] | "";
  String responseEmoji = doc["topic"]["emoji"] | doc["topic"]["icon"] | "";
  if (!responseEmoji.isEmpty() && responseTitle.indexOf(responseEmoji) < 0) {
    responseTitle = responseEmoji + " " + responseTitle;
  }
  if (!responseTitle.isEmpty()) {
    responseTitle = normalizeTopicTitleForDisplay(responseTitle, false);
    if (!responseTitle.isEmpty() && responseTitle != "Topic") {
      gContexts[gSelectedContext].label = responseTitle;
      gContexts[gSelectedContext].emoji = responseEmoji.isEmpty()
                                              ? fallbackTopicEmojiForTitle(responseTitle, gContexts[gSelectedContext].threadId)
                                              : responseEmoji;
      gContexts[gSelectedContext].shortName = makeTopicShortName(responseTitle, gContexts[gSelectedContext].threadId);
    }
  }

  std::vector<ChatLine> historyChatLines;
  historyChatLines.reserve(kMaxContextPreviewLines);
  gContextPreview.clear();
  for (JsonObject item : doc["items"].as<JsonArray>()) {
    if (gContextPreview.size() >= kMaxContextPreviewLines) {
      break;
    }
    JsonObject sender = item["sender"].as<JsonObject>();
    JsonObject content = item["content"].as<JsonObject>();
    String kind = content["kind"] | "";
    ContextPreviewLine line;
    line.role = sender["kind"] | "";
    line.voice = kind == "voice";
    if (line.role.isEmpty()) {
      line.role = "msg";
    }
    if (kind == "voice") {
      line.text = content["transcript"] | "";
      if (line.text.isEmpty()) {
        uint32_t durationMs = content["voice"]["duration_ms"] | 0;
        line.text = "Voice " + String(durationMs / 1000) + "s";
      }
    } else {
      line.text = content["text"] | "";
    }
    line.text = trimPreview(line.text, 92);
    if (!line.text.isEmpty()) {
      gContextPreview.push_back(line);
      historyChatLines.push_back({
          topicHistoryPrefix(line.role, line.voice),
          trimPreview(line.text, 180),
          topicHistoryLineKind(line.role),
      });
    }
  }
  gContextHistoryKey = topicKey;
  gContextHistoryFetchedAtMs = millis();
  gContextError = "";
  replaceChatWithTopicHistory(gContexts[gSelectedContext].label, historyChatLines);
  appendRuntimeLog("CTX", "history " + gContexts[gSelectedContext].shortName +
                            " lines=" + String(gContextPreview.size()), true);
  setStatus("History loaded", 800);
  return true;
}

bool saveVoiceNoteMetadata(const VoiceNote& note) {
  fs::FS* fs = activeVoiceFs();
  if (!fs) {
    return false;
  }
  DynamicJsonDocument doc(768);
  doc["id"] = note.id;
  doc["audio_path"] = note.audioPath;
  doc["title"] = note.title;
  doc["preview"] = note.preview;
  doc["sample_rate"] = note.sampleRate;
  doc["duration_ms"] = note.durationMs;
  doc["assistant"] = note.assistant;

  File meta = fs->open(note.metaPath, "w");
  if (!meta) {
    return false;
  }
  serializeJson(doc, meta);
  meta.close();
  return true;
}

bool persistWavFile(const String& path, const uint8_t* header, size_t headerLen, const uint8_t* audioBytes,
                    size_t audioLen) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return false;
  }
  File file = fs->open(path, "w");
  if (!file) {
    return false;
  }
  size_t writtenHeader = file.write(header, headerLen);
  size_t writtenBody = file.write(audioBytes, audioLen);
  file.close();
  return writtenHeader == headerLen && writtenBody == audioLen;
}

bool persistFileToFile(File& src, File& dst, size_t bytesToCopy) {
  uint8_t buffer[1024];
  size_t copied = 0;
  while (copied < bytesToCopy) {
    size_t chunk = bytesToCopy - copied;
    if (chunk > sizeof(buffer)) {
      chunk = sizeof(buffer);
    }
    size_t readBytes = src.read(buffer, chunk);
    if (readBytes == 0) {
      return false;
    }
    size_t written = dst.write(buffer, readBytes);
    if (written != readBytes) {
      return false;
    }
    copied += readBytes;
  }
  return copied == bytesToCopy;
}

bool persistWavFromPcmFile(const String& outPath, const String& pcmPath, uint32_t sampleRate, size_t audioLen) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return false;
  }
  File src = fs->open(pcmPath, "r");
  if (!src) {
    return false;
  }
  File dst = fs->open(outPath, "w");
  if (!dst) {
    src.close();
    return false;
  }

  uint8_t header[44];
  writeWavHeader(header, static_cast<uint32_t>(audioLen), sampleRate);
  bool ok = dst.write(header, sizeof(header)) == sizeof(header) && persistFileToFile(src, dst, audioLen);
  src.close();
  dst.close();
  if (!ok) {
    fsRemove(*fs, outPath);
  }
  return ok;
}

void rememberVoiceNote(const String& title, const String& preview, const uint8_t* header, size_t headerLen,
                       const uint8_t* audioBytes, size_t audioLen, uint32_t sampleRate, bool assistant) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return;
  }
  uint64_t totalBytes = activeStorageTotalBytes();
  uint64_t usedBytes = activeStorageUsedBytes();
  size_t requiredBytes = headerLen + audioLen + 2048;
  if (totalBytes > 0 && usedBytes + requiredBytes >= totalBytes) {
    logf("[VOICE] skip note storage used=%llu total=%llu required=%u",
         static_cast<unsigned long long>(usedBytes),
         static_cast<unsigned long long>(totalBytes),
         static_cast<unsigned>(requiredBytes));
    return;
  }
  VoiceNote note;
  note.id = makeVoiceNoteId(assistant);
  note.audioPath = voiceAudioPath(note.id);
  note.metaPath = voiceMetaPath(note.id);
  note.title = makeVoiceNoteTitle(assistant, audioLen, sampleRate);
  note.preview = trimPreview(preview);
  note.sampleRate = sampleRate;
  note.durationMs = sampleRate ? static_cast<uint32_t>((audioLen / 2U) * 1000ULL / sampleRate) : 0;
  note.assistant = assistant;

  if (!persistWavFile(note.audioPath, header, headerLen, audioBytes, audioLen)) {
    return;
  }
  if (!saveVoiceNoteMetadata(note)) {
    fsRemove(*fs, note.audioPath);
    return;
  }
  gVoiceNotes.push_back(note);
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
  gSelectedVoiceNote = 0;
}

void rememberVoiceNoteFromPcmFile(const String& title, const String& preview, const String& pcmPath,
                                  size_t audioLen, uint32_t sampleRate, bool assistant) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage() || !fs || !fsExists(*fs, pcmPath)) {
    return;
  }
  uint64_t totalBytes = activeStorageTotalBytes();
  uint64_t usedBytes = activeStorageUsedBytes();
  size_t requiredBytes = 44 + audioLen + 2048;
  if (totalBytes > 0 && usedBytes + requiredBytes >= totalBytes) {
    logf("[VOICE] skip note storage used=%llu total=%llu required=%u",
         static_cast<unsigned long long>(usedBytes),
         static_cast<unsigned long long>(totalBytes),
         static_cast<unsigned>(requiredBytes));
    return;
  }

  VoiceNote note;
  note.id = makeVoiceNoteId(assistant);
  note.audioPath = voiceAudioPath(note.id);
  note.metaPath = voiceMetaPath(note.id);
  note.title = makeVoiceNoteTitle(assistant, audioLen, sampleRate);
  note.preview = trimPreview(preview);
  note.sampleRate = sampleRate;
  note.durationMs = sampleRate ? static_cast<uint32_t>((audioLen / 2U) * 1000ULL / sampleRate) : 0;
  note.assistant = assistant;

  if (!persistWavFromPcmFile(note.audioPath, pcmPath, sampleRate, audioLen)) {
    return;
  }
  if (!saveVoiceNoteMetadata(note)) {
    fsRemove(*fs, note.audioPath);
    return;
  }
  gVoiceNotes.push_back(note);
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
  gSelectedVoiceNote = 0;
}

uint16_t readLe16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

