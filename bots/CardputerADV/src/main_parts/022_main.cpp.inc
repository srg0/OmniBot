bool startPlaybackStreamFromRawPcmFile(const String& path, uint32_t sampleRate, size_t audioLen, const String& diag) {
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, path)) {
    setStatus("Voice file missing", 1200);
    return false;
  }
  stopPlaybackStream();
  gPlaybackStreamFile = fs->open(path, "r");
  if (!gPlaybackStreamFile) {
    setStatus("Voice file missing", 1200);
    return false;
  }
  gPlaybackStreamPath = path;
  gPlaybackStreamRemainingBytes = audioLen;
  gPlaybackStreamTotalBytes = audioLen;
  gPlaybackStreamSampleRate = sampleRate;
  gPlaybackStreamNextBuffer = 0;
  gPlaybackStreamRawPcm = true;
  gPlaybackStartedMs = 0;
  gPlaybackDurationMs = playbackDurationMsForBytes(audioLen, sampleRate);
  resetPlaybackAnalyzer();
  gPlaybackStreaming = true;
  gPlaybackActive = true;
  pulseButtonPress();
  setFaceMode(FaceMode::Speaking);
  setStatus("Playing voice...", 1200);
  setVoiceDiag(diag);
  return true;
}

bool startPlaybackStreamFromWavFile(const String& path, const String& diag) {
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, path)) {
    setStatus("Voice file missing", 1200);
    return false;
  }
  stopPlaybackStream();
  File file = fs->open(path, "r");
  if (!file) {
    setStatus("Voice file missing", 1200);
    return false;
  }
  WavInfo info;
  if (!parseWavInfo(file, info)) {
    String err = info.error.isEmpty() ? String("invalid wav") : info.error;
    file.close();
    setStatus("WAV " + err, 1600);
    setVoiceDiag("playback: " + err);
    appendRuntimeLog("AUDIO", "wav parse failed " + path + ": " + err, true);
    return false;
  }
  if (!file.seek(info.dataOffset)) {
    file.close();
    setStatus("WAV seek failed", 1400);
    setVoiceDiag("playback: seek failed");
    appendRuntimeLog("AUDIO", "wav data seek failed " + path, true);
    return false;
  }
  gPlaybackStreamFile = file;
  gPlaybackStreamPath = path;
  gPlaybackStreamRemainingBytes = info.dataBytes;
  gPlaybackStreamTotalBytes = info.dataBytes;
  gPlaybackStreamSampleRate = info.sampleRate;
  gPlaybackStreamNextBuffer = 0;
  gPlaybackStreamRawPcm = false;
  gPlaybackStartedMs = 0;
  gPlaybackDurationMs = playbackDurationMsForBytes(info.dataBytes, info.sampleRate);
  resetPlaybackAnalyzer();
  gPlaybackStreaming = true;
  gPlaybackActive = true;
  pulseButtonPress();
  setFaceMode(FaceMode::Speaking);
  setStatus("Playing voice...", 1200);
  setVoiceDiag(diag);
  appendRuntimeLog("AUDIO", "play " + path + " " + String(info.sampleRate) + "Hz " +
                            String(static_cast<unsigned>(info.dataBytes / 1024)) + "k", false);
  return true;
}

void servicePlaybackStream() {
  if (!gPlaybackStreaming) {
    return;
  }
  constexpr int kPlaybackChannel = 0;
  while (gPlaybackStreamRemainingBytes > 0 && M5Cardputer.Speaker.isPlaying(kPlaybackChannel) < 2) {
    size_t chunk = gPlaybackStreamRemainingBytes;
    if (chunk > kPlaybackChunkBytes) {
      chunk = kPlaybackChunkBytes;
    }
    if (chunk > 1 && (chunk & 1U)) {
      --chunk;
    }
    if (chunk < sizeof(int16_t)) {
      gPlaybackStreamRemainingBytes = 0;
      break;
    }
    uint8_t* buffer = gPlaybackStreamBuffers[gPlaybackStreamNextBuffer];
    size_t readBytes = gPlaybackStreamFile.read(buffer, chunk);
    if (readBytes == 0) {
      gPlaybackStreamRemainingBytes = 0;
      break;
    }
    updatePlaybackAnalyzerFromSamples(reinterpret_cast<const int16_t*>(buffer),
                                      readBytes / sizeof(int16_t),
                                      gPlaybackStreamSampleRate);
    bool firstChunk = M5Cardputer.Speaker.isPlaying(kPlaybackChannel) == 0;
    bool queued = M5Cardputer.Speaker.playRaw(reinterpret_cast<const int16_t*>(buffer),
                                              readBytes / sizeof(int16_t),
                                              gPlaybackStreamSampleRate,
                                              false,
                                              1,
                                              kPlaybackChannel,
                                              firstChunk);
    if (!queued) {
      setVoiceDiag("playback: queue fail");
      setFaceMode(FaceMode::Error);
      setStatus("Playback failed", 1500);
      stopPlaybackStream();
      gPlaybackActive = false;
      return;
    }
    if (gPlaybackStartedMs == 0) {
      gPlaybackStartedMs = millis();
    }
    gPlaybackStreamRemainingBytes -= readBytes;
    gPlaybackStreamNextBuffer = (gPlaybackStreamNextBuffer + 1) % kPlaybackBufferCount;
  }

  if (gPlaybackStreamRemainingBytes == 0 && M5Cardputer.Speaker.isPlaying(kPlaybackChannel) == 0) {
    bool removeTempPcm = gPlaybackStreamPath == kActiveTtsPcmPath;
    fs::FS* fs = activeVoiceFs();
    stopPlaybackStream();
    gPlaybackActive = false;
    setFaceMode(FaceMode::Idle);
    setStatus("Reply finished", 1000);
    if (removeTempPcm && fs) {
      fsRemove(*fs, kActiveTtsPcmPath);
    }
  }
}

bool playVoiceNoteAt(int index) {
  if (index < 0 || index >= static_cast<int>(gVoiceNotes.size())) {
    return false;
  }
  const auto& note = gVoiceNotes[index];
  return startPlaybackStreamFromWavFile(note.audioPath, String("note: ") + note.title);
}

String normalizeActionText(String value) {
  value.trim();
  value.toLowerCase();
  value.replace("_", ".");
  value.replace("-", "_");
  return value;
}

bool actionBool(JsonVariantConst value, bool fallback = false) {
  if (value.is<bool>()) {
    return value.as<bool>();
  }
  if (value.is<int>()) {
    return value.as<int>() != 0;
  }
  String text = value.as<String>();
  text.trim();
  text.toLowerCase();
  if (text == "true" || text == "on" || text == "yes" || text == "1") {
    return true;
  }
  if (text == "false" || text == "off" || text == "no" || text == "0") {
    return false;
  }
  return fallback;
}

int actionInt(JsonObjectConst action, const char* key, int fallback, int minValue, int maxValue) {
  if (action[key].isNull()) {
    return fallback;
  }
  return clampValue<int>(action[key] | fallback, minValue, maxValue);
}

bool mapActionUiMode(String mode, UiMode& out) {
  mode.trim();
  mode.toLowerCase();
  mode.replace("-", "_");
  mode.replace(" ", "_");
  if (mode == "home" || mode == "pulse") {
    out = UiMode::Home;
  } else if (mode == "chat" || mode == "chat_eyes") {
    out = UiMode::ChatFace;
  } else if (mode == "chat_full" || mode == "full_chat") {
    out = UiMode::ChatFull;
  } else if (mode == "face" || mode == "eyes") {
    out = UiMode::Face;
  } else if (mode == "hero" || mode == "pet" || mode == "tama" ||
             mode == "tamagotchi" || mode == "companion") {
    out = UiMode::Hero;
  } else if (mode == "voice") {
    out = UiMode::Voice;
  } else if (mode == "library") {
    out = UiMode::Library;
  } else if (mode == "folders") {
    out = UiMode::AudioFolders;
  } else if (mode == "notes" || mode == "note" || mode == "notepad") {
    out = UiMode::Notes;
  } else if (mode == "assets") {
    out = UiMode::Assets;
  } else if (mode == "focus") {
    out = UiMode::Focus;
  } else if (mode == "topics" || mode == "contexts") {
    out = UiMode::Contexts;
  } else if (mode == "logs") {
    out = UiMode::Logs;
  } else if (mode == "settings") {
    out = UiMode::Settings;
  } else if (mode == "calculator" || mode == "calc") {
    out = UiMode::Calculator;
  } else if (mode == "battery") {
    out = UiMode::Battery;
  } else if (mode == "ota" || mode == "firmware") {
    out = UiMode::Ota;
  } else {
    return false;
  }
  return true;
}

bool actionLocalPathAllowed(const String& path) {
  return path.startsWith("/audio/") || path.startsWith("/pomodoro/audio/") ||
         path.startsWith("/voice/") || path.startsWith("/reply/");
}
