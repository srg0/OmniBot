void updateTransferUi(uint32_t done, uint32_t total, const String& detail) {
  if (!gTransferUiActive) {
    return;
  }
  gTransferDone = done;
  if (total > 0) {
    gTransferTotal = total;
  }
  if (!detail.isEmpty()) {
    gTransferDetail = detail;
  }
  uint32_t now = millis();
  if (now - gTransferLastRenderMs >= 220 || done >= gTransferTotal) {
    gTransferLastRenderMs = now;
    renderTransferUi();
  }
}

void finishTransferUi(const String& statusText, bool ok) {
  if (!gTransferUiActive) {
    setStatus(statusText, ok ? 900 : 1600);
    return;
  }
  if (!statusText.isEmpty()) {
    gTransferDetail = statusText;
  }
  gTransferDone = gTransferTotal;
  renderTransferUi();
  gTransferUiActive = false;
  setStatus(statusText, ok ? 900 : 1600);
}

void setFaceMode(FaceMode mode) {
  gFaceMode = mode;
}

AssistantPulseState assistantPulseState() {
  if (gRecording) {
    return AssistantPulseState::Recording;
  }
  if (gSubmitting || gThinking) {
    return AssistantPulseState::Thinking;
  }
  if (gPlaybackActive) {
    return AssistantPulseState::Playback;
  }
  return AssistantPulseState::Ready;
}

void pulseButtonPress(uint32_t durationMs = 120) {
  gAssistantPulsePressUntilMs = millis() + durationMs;
}

void showTopicOverlay(int8_t direction, uint32_t ttlMs) {
  uint32_t now = millis();
  gTopicOverlayActionMs = now;
  gTopicOverlayUntilMs = now + ttlMs;
  if (direction != 0) {
    gContextAnimDir = direction;
    gContextAnimStartMs = now;
  }
}

void updateVoiceLevelFromSamples(const int16_t* samples, size_t sampleCount) {
  if (!samples || sampleCount == 0) {
    return;
  }
  uint32_t acc = 0;
  size_t reads = 0;
  for (size_t i = 0; i < sampleCount; i += 16) {
    int32_t v = samples[i];
    acc += static_cast<uint32_t>(v < 0 ? -v : v);
    reads++;
  }
  if (reads == 0) {
    return;
  }
  uint32_t avg = acc / reads;
  uint8_t level = static_cast<uint8_t>(min<uint32_t>(255, avg >> 4));
  gVoiceLevel8 = static_cast<uint8_t>((static_cast<uint16_t>(gVoiceLevel8) * 3 + level) / 4);
}

uint32_t playbackDurationMsForBytes(size_t bytes, uint32_t sampleRate) {
  if (bytes == 0 || sampleRate == 0) {
    return 0;
  }
  return static_cast<uint32_t>((static_cast<uint64_t>(bytes / sizeof(int16_t)) * 1000ULL) / sampleRate);
}

uint32_t playbackElapsedMs() {
  if (!gPlaybackActive || gPlaybackStartedMs == 0) {
    return 0;
  }
  return min<uint32_t>(millis() - gPlaybackStartedMs, gPlaybackDurationMs);
}

uint32_t playbackRemainingMs() {
  if (!gPlaybackActive || gPlaybackDurationMs == 0) {
    return 0;
  }
  uint32_t elapsed = playbackElapsedMs();
  return elapsed >= gPlaybackDurationMs ? 0 : gPlaybackDurationMs - elapsed;
}

uint8_t playbackProgressPct() {
  if (!gPlaybackActive || gPlaybackDurationMs == 0) {
    return 0;
  }
  return static_cast<uint8_t>(clampValue<uint32_t>((playbackElapsedMs() * 100ULL) / gPlaybackDurationMs, 0, 100));
}

String formatPlaybackMs(uint32_t ms) {
  char buf[8];
  uint32_t sec = ms / 1000U;
  snprintf(buf, sizeof(buf), "%02u:%02u", static_cast<unsigned>(sec / 60U),
           static_cast<unsigned>(sec % 60U));
  return String(buf);
}

void resetPlaybackAnalyzer() {
  gVoiceLevel8 = 28;
  for (uint8_t i = 0; i < kPlaybackSpectrumBands; ++i) {
    gVoiceEqBars[i] = 1;
    gVoiceEqPeaks[i] = 1;
  }
}

void updatePlaybackAnalyzerFromSamples(const int16_t* samples, size_t sampleCount, uint32_t sampleRate) {
  if (!samples || sampleCount < 32 || sampleRate < 4000) {
    return;
  }
  updateVoiceLevelFromSamples(samples, sampleCount);

  // Lightweight real spectrum: Goertzel bands over the PCM chunk. It avoids a
  // full FFT dependency while still reflecting the actual audio being played.
  const size_t n = min<size_t>(sampleCount, 384);
  int64_t dcAcc = 0;
  for (size_t i = 0; i < n; ++i) {
    dcAcc += samples[i];
  }
  const float dc = static_cast<float>(dcAcc) / static_cast<float>(n);
  const float maxFreq = max<float>(420.0f, min<float>(static_cast<float>(sampleRate) * 0.46f, 7600.0f));
  const float minFreq = 120.0f;
  const float ratio = maxFreq / minFreq;

  for (uint8_t band = 0; band < kPlaybackSpectrumBands; ++band) {
    float norm = (static_cast<float>(band) + 0.5f) / static_cast<float>(kPlaybackSpectrumBands);
    float freq = minFreq * powf(ratio, norm);
    float omega = 2.0f * kPi * freq / static_cast<float>(sampleRate);
    float coeff = 2.0f * cosf(omega);
    float q0 = 0.0f;
    float q1 = 0.0f;
    float q2 = 0.0f;
    for (size_t i = 0; i < n; i += 2) {
      float sample = static_cast<float>(samples[i]) - dc;
      q0 = coeff * q1 - q2 + sample;
      q2 = q1;
      q1 = q0;
    }
    float power = max<float>(0.0f, q1 * q1 + q2 * q2 - coeff * q1 * q2);
    float magnitude = sqrtf(power) / static_cast<float>(n);
    uint8_t target = static_cast<uint8_t>(clampValue<int>(
        1 + static_cast<int>(sqrtf(magnitude) * 0.24f),
        1,
        kPlaybackSpectrumMaxBar));

    uint8_t current = gVoiceEqBars[band];
    if (target >= current) {
      current = static_cast<uint8_t>((current + target * 3U) / 4U);
    } else {
      current = static_cast<uint8_t>(max<int>(1, static_cast<int>(current) - 1));
    }
    gVoiceEqBars[band] = current;
    if (current >= gVoiceEqPeaks[band]) {
      gVoiceEqPeaks[band] = current;
    } else if ((millis() / 90U + band) % 2U == 0U) {
      gVoiceEqPeaks[band] = static_cast<uint8_t>(max<int>(1, static_cast<int>(gVoiceEqPeaks[band]) - 1));
    }
  }
}

bool adjustSpeakerVolume(int delta, uint32_t ttlMs = 700) {
  int next = clampValue<int>(static_cast<int>(gSpeakerVolume) + delta, 0, 255);
  if (next == static_cast<int>(gSpeakerVolume)) {
    setStatus(delta > 0 ? "Vol max" : "Vol min", ttlMs);
    return false;
  }
  gSpeakerVolume = static_cast<uint8_t>(next);
  M5Cardputer.Speaker.setVolume(gSpeakerVolume);
  setStatus("Vol " + String(map(gSpeakerVolume, 0, 255, 0, 100)) + "%", ttlMs);
  return true;
}

void setVoiceDiag(const String& text) {
  gLastVoiceDiag = text;
}

void setHttpDiag(const String& text) {
  gLastHttpDiag = text;
}

void setKeyDiag(const String& text) {
  gLastKeyDiag = text;
}

String makeClientTurnId(const char* prefix) {
  uint32_t seq = ++gClientTurnSeq;
  char buf[48];
  snprintf(buf, sizeof(buf), "%s-%08lx-%04lx",
           prefix && prefix[0] ? prefix : "turn",
           static_cast<unsigned long>(millis()),
           static_cast<unsigned long>(seq & 0xFFFF));
  return String(buf);
}

void pumpBlockingNetworkUi() {
  uint32_t now = millis();
  if (gBlockingRenderPumpEnabled && now - gLastBlockingRenderPumpMs >= kBusyRenderIntervalMs) {
    gLastBlockingRenderPumpMs = now;
    if (gTransferUiActive) {
      renderTransferUi();
    } else {
      render();
      gLastRenderMs = now;
    }
  }
  yield();
  delay(1);
}

bool shouldSpeakAssistantTextReply(const String& reply) {
  String text = cleanReplyForDevice(reply);
  if (text.length() > 90) {
    return true;
  }
  uint8_t words = 0;
  bool inWord = false;
  for (size_t i = 0; i < text.length(); ++i) {
    bool space = isspace(static_cast<unsigned char>(text[i]));
    if (!space && !inWord) {
      ++words;
      inWord = true;
      if (words > 5) {
        return true;
      }
    } else if (space) {
      inWord = false;
    }
  }
  return false;
}
