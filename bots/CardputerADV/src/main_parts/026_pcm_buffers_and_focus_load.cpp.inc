bool ensurePcmBuffer(
    int16_t*& buffer,
    size_t& capacityBytes,
    size_t& capacitySamples,
    size_t targetBytes,
    size_t minBytes,
    size_t heapFallbackCapBytes,
    const char* tag) {
  if (buffer) {
    return true;
  }
  buffer = static_cast<int16_t*>(
      heap_caps_malloc(targetBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (buffer) {
    capacityBytes = targetBytes;
    capacitySamples = targetBytes / sizeof(int16_t);
    logf("[%s] buffer allocated in PSRAM bytes=%u", tag, static_cast<unsigned>(capacityBytes));
    return true;
  }

  size_t heapTargetBytes = targetBytes;
  if (heapFallbackCapBytes > 0 && heapTargetBytes > heapFallbackCapBytes) {
    heapTargetBytes = heapFallbackCapBytes & ~static_cast<size_t>(1);
  }
  size_t freeHeap = ESP.getFreeHeap();
  if (freeHeap > kHeapReserveBytes && heapTargetBytes > (freeHeap - kHeapReserveBytes)) {
    heapTargetBytes = (freeHeap - kHeapReserveBytes) & ~static_cast<size_t>(1);
  }
  if (heapTargetBytes >= minBytes) {
    buffer = static_cast<int16_t*>(heap_caps_malloc(heapTargetBytes, MALLOC_CAP_8BIT));
    if (buffer) {
      capacityBytes = heapTargetBytes;
      capacitySamples = heapTargetBytes / sizeof(int16_t);
      logf("[%s] buffer allocated in heap bytes=%u target=%u",
           tag,
           static_cast<unsigned>(capacityBytes),
           static_cast<unsigned>(targetBytes));
      return true;
    }
  }

  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t fallbackBytes = 0;
  size_t fallbackLimit = largest;
  if (freeHeap > kHeapReserveBytes) {
    size_t freeHeapLimit = freeHeap - kHeapReserveBytes;
    if (fallbackLimit > freeHeapLimit) {
      fallbackLimit = freeHeapLimit;
    }
  } else {
    fallbackLimit = 0;
  }
  if (fallbackLimit > 8192) {
    fallbackBytes = (fallbackLimit - 4096) & ~static_cast<size_t>(1);
  }
  if (heapFallbackCapBytes > 0 && fallbackBytes > heapFallbackCapBytes) {
    fallbackBytes = heapFallbackCapBytes & ~static_cast<size_t>(1);
  }
  if (fallbackBytes < minBytes) {
    logf("[%s] buffer alloc failed target=%u largest=%u",
         tag,
         static_cast<unsigned>(targetBytes), static_cast<unsigned>(largest));
    return false;
  }

  buffer = static_cast<int16_t*>(heap_caps_malloc(fallbackBytes, MALLOC_CAP_8BIT));
  if (!buffer) {
    logf("[%s] fallback alloc failed bytes=%u largest=%u",
         tag,
         static_cast<unsigned>(fallbackBytes), static_cast<unsigned>(largest));
    return false;
  }

  capacityBytes = fallbackBytes;
  capacitySamples = fallbackBytes / sizeof(int16_t);
  logf("[%s] buffer fallback bytes=%u samples=%u",
       tag,
       static_cast<unsigned>(capacityBytes),
       static_cast<unsigned>(capacitySamples));
  return true;
}

bool ensureRecordBuffer() {
  return ensurePcmBuffer(
      gRecordBuffer,
      gRecordCapacityBytes,
      gRecordCapacitySamples,
      kRecordBufferBytes,
      kMinRecordBufferBytes,
      kMaxHeapRecordFallbackBytes,
      "REC");
}

bool ensureTtsBuffer() {
  return ensurePcmBuffer(
      gTtsBuffer,
      gTtsCapacityBytes,
      gTtsCapacitySamples,
      kTtsBufferBytes,
      kMinTtsBufferBytes,
      kMaxHeapTtsFallbackBytes,
      "TTS");
}

void releasePcmBuffer(int16_t*& buffer, size_t& capacityBytes, size_t& capacitySamples, const char* tag) {
  if (!buffer) {
    return;
  }
  free(buffer);
  buffer = nullptr;
  capacityBytes = 0;
  capacitySamples = 0;
  logf("[%s] buffer released", tag);
}

void releaseRecordBuffer() {
  releasePcmBuffer(gRecordBuffer, gRecordCapacityBytes, gRecordCapacitySamples, "REC");
}

void releaseTtsBuffer() {
  releasePcmBuffer(gTtsBuffer, gTtsCapacityBytes, gTtsCapacitySamples, "TTS");
}

void logHeapStats(const char* tag) {
  logf("[HEAP] %s free=%u largest=%u",
       tag,
       static_cast<unsigned>(ESP.getFreeHeap()),
       static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}

uint32_t currentRecordLimitMs() {
  if (gRecordingToFile && gStorageReady) {
    uint64_t total = activeStorageTotalBytes();
    uint64_t used = activeStorageUsedBytes();
    uint64_t availableBytes = 0;
    if (total > used + 8192) {
      availableBytes = total - used - 8192;
    }
    uint32_t storageMs = static_cast<uint32_t>((availableBytes * 1000ULL) / (kMicSampleRate * sizeof(int16_t)));
    uint32_t configuredMs = static_cast<uint32_t>(kMaxRecordSeconds * 1000.0f);
    return storageMs < configuredMs ? storageMs : configuredMs;
  }
  if (!gRecordCapacitySamples) {
    return static_cast<uint32_t>(kMaxRecordSeconds * 1000.0f);
  }
  uint32_t capacityMs = static_cast<uint32_t>((gRecordCapacitySamples * 1000ULL) / kMicSampleRate);
  uint32_t configuredMs = static_cast<uint32_t>(kMaxRecordSeconds * 1000.0f);
  return capacityMs < configuredMs ? capacityMs : configuredMs;
}

void initSpeaker() {
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(gSpeakerVolume);
}

const char* focusStateName(FocusState state) {
  switch (state) {
    case FocusState::Focus:
      return "FOCUS";
    case FocusState::ShortBreak:
      return "BREAK";
    case FocusState::LongBreak:
      return "LONG";
    case FocusState::Reflect:
      return "REFLECT";
    case FocusState::Paused:
      return "PAUSED";
    case FocusState::Stopped:
    default:
      return "READY";
  }
}

bool focusStateRuns(FocusState state) {
  return state == FocusState::Focus || state == FocusState::ShortBreak ||
         state == FocusState::LongBreak || state == FocusState::Reflect;
}

uint32_t focusDurationForState(FocusState state) {
  switch (state) {
    case FocusState::Focus:
      return gFocusSettings.focusSec;
    case FocusState::ShortBreak:
      return gFocusSettings.shortBreakSec;
    case FocusState::LongBreak:
      return gFocusSettings.longBreakSec;
    case FocusState::Reflect:
      return 60;
    case FocusState::Paused:
    case FocusState::Stopped:
    default:
      return gFocusSettings.focusSec;
  }
}

String focusTimeString(uint32_t seconds) {
  uint32_t minutes = seconds / 60;
  uint8_t secs = seconds % 60;
  char buf[10];
  snprintf(buf, sizeof(buf), "%02lu:%02u", static_cast<unsigned long>(minutes), secs);
  return String(buf);
}

void focusTone(uint16_t freq, uint16_t durationMs) {
  if (gRecording || gPlaybackActive) {
    return;
  }
  M5Cardputer.Speaker.tone(freq, durationMs);
}

bool playFocusCue(FocusState state) {
  if (gRecording || gSubmitting || gThinking || gPlaybackActive) {
    return false;
  }
  const char* path = nullptr;
  switch (state) {
    case FocusState::Focus:
      path = kFocusCuePath;
      break;
    case FocusState::ShortBreak:
    case FocusState::LongBreak:
      path = kBreakCuePath;
      break;
    case FocusState::Reflect:
      path = kReflectionCuePath;
      break;
    case FocusState::Paused:
    case FocusState::Stopped:
    default:
      return false;
  }
  fs::FS* fs = activeVoiceFs();
  if (fs && fsExists(*fs, path)) {
    return startPlaybackStreamFromWavFile(path, String("focus cue: ") + focusStateName(state));
  }
  return false;
}

void loadFocusSettings() {
  gPrefs.begin(kPrefsFocusNs, true);
  gFocusSettings.focusSec = gPrefs.getUInt("focus", kFocusDefaultSec);
  gFocusSettings.shortBreakSec = gPrefs.getUInt("short", kFocusShortBreakDefaultSec);
  gFocusSettings.longBreakSec = gPrefs.getUInt("long", kFocusLongBreakDefaultSec);
  gFocusSettings.cyclesPerRound = static_cast<uint8_t>(gPrefs.getUChar("cycles", kFocusDefaultCycles));
  gFocusSettings.autoStart = gPrefs.getBool("auto", false);
  gFocusSettings.metronome = gPrefs.getBool("metro", false);
  gFocusSettings.bpm = static_cast<uint16_t>(gPrefs.getUShort("bpm", 60));
  gPrefs.end();

  gFocusSettings.focusSec = clampValue<uint32_t>(gFocusSettings.focusSec, kFocusMinDurationSec, kFocusMaxDurationSec);
  gFocusSettings.shortBreakSec = clampValue<uint32_t>(gFocusSettings.shortBreakSec, kFocusMinDurationSec, kFocusMaxDurationSec);
  gFocusSettings.longBreakSec = clampValue<uint32_t>(gFocusSettings.longBreakSec, kFocusMinDurationSec, kFocusMaxDurationSec);
  gFocusSettings.cyclesPerRound = clampValue<uint8_t>(gFocusSettings.cyclesPerRound, 1, 9);
  gFocusSettings.bpm = clampValue<uint16_t>(gFocusSettings.bpm, kFocusMinBpm, kFocusMaxBpm);
  gFocus.remainingSec = gFocusSettings.focusSec;
  gFocus.sessionTotalSec = gFocusSettings.focusSec;
}
