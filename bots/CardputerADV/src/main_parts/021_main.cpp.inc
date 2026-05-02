uint32_t readLe32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

struct WavInfo {
  uint32_t dataOffset = 0;
  uint32_t dataBytes = 0;
  uint32_t sampleRate = 0;
  uint16_t channels = 0;
  uint16_t bitsPerSample = 0;
  uint16_t blockAlign = 0;
  uint16_t audioFormat = 0;
  String error;
};

bool readWavBytes(File& file, uint8_t* out, size_t len) {
  return static_cast<size_t>(file.read(out, len)) == len;
}

bool parseWavInfo(File& file, WavInfo& info) {
  info = WavInfo();
  if (!file || file.size() < 12) {
    info.error = "small file";
    return false;
  }
  if (!file.seek(0)) {
    info.error = "seek failed";
    return false;
  }
  uint8_t riff[12];
  if (!readWavBytes(file, riff, sizeof(riff)) ||
      memcmp(riff, "RIFF", 4) != 0 ||
      memcmp(riff + 8, "WAVE", 4) != 0) {
    info.error = "not RIFF/WAVE";
    return false;
  }

  bool haveFmt = false;
  const uint32_t fileSize = static_cast<uint32_t>(file.size());
  while (file.position() + 8 <= fileSize) {
    uint8_t chunkHeader[8];
    if (!readWavBytes(file, chunkHeader, sizeof(chunkHeader))) {
      info.error = "chunk read";
      return false;
    }
    char id[5] = {
        static_cast<char>(chunkHeader[0]),
        static_cast<char>(chunkHeader[1]),
        static_cast<char>(chunkHeader[2]),
        static_cast<char>(chunkHeader[3]),
        0};
    uint32_t chunkSize = readLe32(chunkHeader + 4);
    uint32_t chunkStart = static_cast<uint32_t>(file.position());
    uint32_t nextChunk = chunkStart + chunkSize + (chunkSize & 1U);
    if (chunkStart > fileSize || chunkSize > fileSize - chunkStart) {
      info.error = String(id) + " size";
      return false;
    }

    if (memcmp(chunkHeader, "fmt ", 4) == 0) {
      if (chunkSize < 16) {
        info.error = "fmt small";
        return false;
      }
      uint8_t fmt[16];
      if (!readWavBytes(file, fmt, sizeof(fmt))) {
        info.error = "fmt read";
        return false;
      }
      info.audioFormat = readLe16(fmt);
      info.channels = readLe16(fmt + 2);
      info.sampleRate = readLe32(fmt + 4);
      info.blockAlign = readLe16(fmt + 12);
      info.bitsPerSample = readLe16(fmt + 14);
      haveFmt = true;
    } else if (memcmp(chunkHeader, "data", 4) == 0) {
      info.dataOffset = chunkStart;
      info.dataBytes = chunkSize;
      break;
    }

    if (!file.seek(nextChunk)) {
      info.error = String(id) + " seek";
      return false;
    }
  }

  if (!haveFmt) {
    info.error = "fmt missing";
    return false;
  }
  if (info.dataBytes == 0) {
    info.error = "data missing";
    return false;
  }
  if (info.audioFormat != 1 || info.bitsPerSample != 16 || info.channels != 1 || info.sampleRate < 8000) {
    info.error = "unsupported wav";
    return false;
  }
  if (info.blockAlign == 0) {
    info.blockAlign = static_cast<uint16_t>((info.bitsPerSample / 8U) * info.channels);
  }
  return true;
}

uint32_t wavDurationMs(const String& path, uint32_t* sampleRateOut = nullptr) {
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, path)) {
    return 0;
  }
  File file = fs->open(path, "r");
  WavInfo info;
  bool ok = parseWavInfo(file, info);
  file.close();
  if (!ok) {
    appendRuntimeLog("AUDIO", "wav duration failed " + path + ": " + info.error, false);
    return 0;
  }
  if (sampleRateOut) {
    *sampleRateOut = info.sampleRate;
  }
  uint32_t bytesPerSampleFrame = max<uint32_t>(1, info.blockAlign);
  return static_cast<uint32_t>((static_cast<uint64_t>(info.dataBytes) * 1000ULL) /
                               (static_cast<uint64_t>(info.sampleRate) * bytesPerSampleFrame));
}

bool registerDownloadedVoiceNote(const String& wavPath, const String& titleHint, const String& preview, bool assistant) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage() || !fs || !fsExists(*fs, wavPath)) {
    return false;
  }

  uint32_t sampleRate = 16000;
  uint32_t durationMs = wavDurationMs(wavPath, &sampleRate);
  VoiceNote note;
  note.id = filenameFromPath(wavPath);
  int dot = note.id.lastIndexOf('.');
  if (dot > 0) {
    note.id = note.id.substring(0, dot);
  }
  note.audioPath = wavPath;
  note.metaPath = voiceMetaPath(note.id);
  note.title = titleHint.isEmpty() ? makeVoiceNoteTitle(assistant, max<uint32_t>(1, durationMs) * sampleRate / 500U,
                                                        sampleRate)
                                   : trimPreview(titleHint, 24);
  note.preview = trimPreview(preview);
  note.sampleRate = sampleRate;
  note.durationMs = durationMs;
  note.assistant = assistant;
  if (!saveVoiceNoteMetadata(note)) {
    return false;
  }
  bool existsInList = false;
  for (auto& existing : gVoiceNotes) {
    if (existing.audioPath == note.audioPath) {
      existing = note;
      existsInList = true;
      break;
    }
  }
  if (!existsInList) {
    gVoiceNotes.push_back(note);
  }
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
  gSelectedVoiceNote = 0;
  return true;
}

bool downloadAndPlayResponseAudio(const String& url, const String& localPath, const String& sha256,
                                  uint32_t expectedSize, const String& title, const String& preview) {
  if (url.isEmpty()) {
    if (!localPath.isEmpty() && registerDownloadedVoiceNote(localPath, title, preview, true)) {
      return startPlaybackStreamFromWavFile(localPath, "reply: " + title);
    }
    return false;
  }
  if (!gStorageReady || !ensureVoiceStorage()) {
    return false;
  }
  String id = makeVoiceNoteId(true);
  String outPath = voiceAudioPath(id);
  if (!downloadUrlToFile(url, outPath, sha256, expectedSize)) {
    appendRuntimeLog("AUDIO", "reply audio download failed", true);
    return false;
  }
  if (!registerDownloadedVoiceNote(outPath, title, preview, true)) {
    return false;
  }
  return startPlaybackStreamFromWavFile(outPath, "reply: " + title);
}

void stopPlaybackStream() {
  if (gPlaybackStreamFile) {
    gPlaybackStreamFile.close();
  }
  gPlaybackStreamPath = "";
  gPlaybackStreaming = false;
  gPlaybackStreamRemainingBytes = 0;
  gPlaybackStreamTotalBytes = 0;
  gPlaybackStreamSampleRate = 0;
  gPlaybackStreamNextBuffer = 0;
  gPlaybackStreamRawPcm = false;
  gPlaybackStartedMs = 0;
  gPlaybackDurationMs = 0;
  resetPlaybackAnalyzer();
  if (gDynamicPlaybackBuffer) {
    free(gDynamicPlaybackBuffer);
    gDynamicPlaybackBuffer = nullptr;
  }
  M5Cardputer.Speaker.stop();
}

