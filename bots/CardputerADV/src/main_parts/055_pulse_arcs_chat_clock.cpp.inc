
void drawAssistantPulseArc(int16_t cx, int16_t cy, int16_t outerR, int16_t innerR,
                           float startRad, float endRad, uint16_t color) {
  int32_t startDeg = static_cast<int32_t>(startRad * 180.0f / kPi) % 360;
  int32_t endDeg = static_cast<int32_t>(endRad * 180.0f / kPi) % 360;
  if (startDeg < 0) startDeg += 360;
  if (endDeg < 0) endDeg += 360;
  gCanvas.drawArc(cx, cy, outerR, innerR, startDeg, endDeg, color);
  int16_t capR = max<int16_t>(2, (outerR - innerR) / 2);
  float midR = (outerR + innerR) * 0.5f;
  gCanvas.fillCircle(cx + static_cast<int16_t>(cosf(startRad) * midR),
                     cy + static_cast<int16_t>(sinf(startRad) * midR), capR, color);
  gCanvas.fillCircle(cx + static_cast<int16_t>(cosf(endRad) * midR),
                     cy + static_cast<int16_t>(sinf(endRad) * midR), capR, color);
}

void drawAssistantPulseButton(int16_t x, int16_t y, int16_t w, int16_t h,
                              const char* label, uint16_t accent, bool pressed) {
  auto& d = gCanvas;
  int16_t py = y + (pressed ? 1 : 0);
  uint16_t fill = pressed ? rgb565_local(16, 34, 42) : rgb565_local(7, 20, 26);
  d.fillRoundRect(x, py, w, h, 5, fill);
  d.drawRoundRect(x, py, w, h, 5, accent);
  if (pressed) {
    d.drawFastHLine(x + 8, py + 4, w - 16, rgb565_local(170, 200, 210));
  }
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, fill);
  int tw = d.textWidth(label);
  d.setCursor(x + (w - tw) / 2, py + 6);
  d.print(label);
}

void renderAssistantPulseChatHint() {
  const bool hasInput = !gInputBuffer.isEmpty();
  if (!hasInput && assistantPulseState() != AssistantPulseState::Ready) {
    return;
  }
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(6, 16, 22);
  const uint16_t edge = hasInput ? rgb565_local(47, 227, 255) : rgb565_local(42, 78, 88);
  const uint16_t text = hasInput ? TFT_YELLOW : rgb565_local(128, 145, 155);
  d.fillRoundRect(10, 107, 132, 19, 7, bg);
  d.drawRoundRect(10, 107, 132, 19, 7, edge);
  d.setFont(&fonts::efontCN_12);
  d.setTextColor(text, bg);
  d.setCursor(17, 112);
  d.print(hasInput ? tailUtf8ToFit("> " + gInputBuffer, 116) : "> type to chat");
}

void renderAssistantPulseUi() {
  auto& d = gCanvas;
  ensureBangkokTimezone();
  AssistantPulseState state = assistantPulseState();

  constexpr uint16_t bg = rgb565_local(2, 7, 10);
  constexpr uint16_t white = rgb565_local(244, 251, 255);
  constexpr uint16_t cyan = rgb565_local(47, 227, 255);
  constexpr uint16_t violet = rgb565_local(157, 102, 255);
  constexpr uint16_t red = rgb565_local(255, 63, 85);
  constexpr uint16_t amber = rgb565_local(246, 194, 74);
  constexpr uint16_t green = rgb565_local(88, 240, 141);
  uint16_t c1 = cyan;
  uint16_t c2 = violet;
  uint16_t c3 = rgb565_local(23, 77, 90);
  uint16_t fg = white;
  const char* label = "LISTEN";

  if (state == AssistantPulseState::Recording) {
    c1 = red;
    c2 = rgb565_local(255, 178, 59);
    c3 = rgb565_local(88, 32, 42);
    fg = rgb565_local(255, 241, 239);
    label = "REC";
  } else if (state == AssistantPulseState::Thinking) {
    c1 = amber;
    c2 = cyan;
    c3 = rgb565_local(64, 52, 26);
    fg = rgb565_local(255, 248, 232);
    label = "THINK";
  } else if (state == AssistantPulseState::Playback) {
    c1 = green;
    c2 = cyan;
    c3 = rgb565_local(22, 73, 46);
    fg = rgb565_local(241, 255, 246);
    label = "PLAY";
  }

  d.fillScreen(bg);
  d.fillRoundRect(0, 0, 240, 135, 6, bg);
  d.fillCircle(70, 58, 86, rgb565_local(4, 16, 20));
  d.drawRoundRect(0, 0, 240, 135, 6, rgb565_local(48, 54, 61));

  uint32_t nowMs = millis();
  float t = nowMs / 1000.0f;
  float breathe = sinf(t * 2.2f) * 0.9f;
  float spin = t * 0.55f;
  if (state == AssistantPulseState::Thinking) {
    spin = t * 1.9f;
  } else if (state == AssistantPulseState::Recording) {
    spin = t * 1.25f;
  }
  float level = max<uint8_t>(18, gVoiceLevel8) / 255.0f;
  if (!gRecording && gVoiceLevel8 > 28) {
    gVoiceLevel8 = static_cast<uint8_t>(max<int>(28, static_cast<int>(gVoiceLevel8) - 2));
  }

  const int16_t cx = 58;
  const int16_t cy = 68;
  d.fillCircle(cx, cy, 50 + static_cast<int16_t>(breathe), rgb565_local(4, 16, 21));
  d.drawCircle(cx, cy, 51 + static_cast<int16_t>(breathe), rgb565_local(8, 34, 43));
  d.drawCircle(cx, cy, 49 + static_cast<int16_t>(breathe), c3);
  drawAssistantPulseArc(cx, cy, 45, 36, -2.25f, 1.00f, rgb565_local(10, 38, 48));
  drawAssistantPulseArc(cx, cy, 31, 23, 0.58f, 4.54f, rgb565_local(11, 35, 48));
  drawAssistantPulseArc(cx, cy, 45, 36, -2.25f + spin, 1.00f + spin, c1);
  drawAssistantPulseArc(cx, cy, 31, 23, 0.58f - spin * 1.08f, 4.54f - spin * 1.08f, c2);

  if (state == AssistantPulseState::Recording) {
    int16_t pulseR = 18 + static_cast<int16_t>(level * 14.0f + sinf(t * 12.0f));
    d.fillCircle(cx, cy, pulseR, c1);
    d.fillCircle(cx, cy, 9, fg);
  } else if (state == AssistantPulseState::Thinking) {
    for (int i = 0; i < 4; ++i) {
      float a = spin * 1.9f + i * 1.55f;
      drawAssistantPulseArc(cx, cy, 22, 16, a, a + 0.72f, (i % 2) ? c2 : c1);
    }
    d.fillCircle(cx + static_cast<int16_t>(cosf(spin * 2.4f) * 27.0f),
                 cy + static_cast<int16_t>(sinf(spin * 2.4f) * 21.0f), 4, c1);
    d.fillCircle(cx + static_cast<int16_t>(cosf(spin * 2.0f + 2.1f) * 26.0f),
                 cy + static_cast<int16_t>(sinf(spin * 2.0f + 2.1f) * 23.0f), 3, c2);
  } else if (state == AssistantPulseState::Playback) {
    d.fillCircle(cx, cy, 16, fg);
    d.fillTriangle(cx - 4, cy - 8, cx - 4, cy + 8, cx + 9, cy, bg);
  } else {
    d.fillCircle(cx, cy, 14 + static_cast<int16_t>(sinf(t * 2.2f) * 1.4f), fg);
  }

  String hhmm = "--:--";
  time_t now = time(nullptr);
  if (now > 100000) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[6];
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    hhmm = buf;
  }
	  if (state == AssistantPulseState::Recording) {
	    uint32_t elapsed = (millis() - gRecordStartMs) / 1000;
	    char rec[6];
	    snprintf(rec, sizeof(rec), "%02u:%02u", static_cast<unsigned>(elapsed / 60),
	             static_cast<unsigned>(elapsed % 60));
	    hhmm = rec;
	  }

  const uint16_t clockPanel = state == AssistantPulseState::Recording
                                  ? rgb565_local(32, 13, 19)
                                  : state == AssistantPulseState::Thinking
                                        ? rgb565_local(28, 22, 11)
                                        : state == AssistantPulseState::Playback
                                              ? rgb565_local(5, 27, 24)
                                              : rgb565_local(3, 21, 38);
  d.fillRoundRect(119, 13, 103, 57, 14, clockPanel);
  d.drawRoundRect(119, 13, 103, 57, 14, rgb565_local(14, 58, 82));
  for (int i = 0; i < 5; ++i) {
    int16_t yy = 21 + i * 9 + static_cast<int16_t>(sinf(t * 0.9f + i * 0.8f) * 2.0f);
    d.drawFastHLine(126, yy, 89, rgb565_local(5, 35, 58));
  }
  d.fillRoundRect(128, 61, 82, 2, 1, rgb565_local(16, 83, 112));

	  d.setFont(&fonts::Font7);
  d.setTextColor(fg, clockPanel);
	  int tw = d.textWidth(hhmm);
	  d.setCursor(171 - tw / 2, state == AssistantPulseState::Recording ? 18 : 20);
	  d.print(hhmm);

  int16_t waveX = state == AssistantPulseState::Ready ? 121 : 117;
  int16_t waveY = state == AssistantPulseState::Ready ? 88 : 86;
  int bars = state == AssistantPulseState::Ready ? 15 : 16;
  for (int i = 0; i < bars; ++i) {
    int16_t bx = waveX + i * 7;
    if (state == AssistantPulseState::Thinking) {
      int16_t dotY = waveY + static_cast<int16_t>(sinf(t * 5.0f + i * 0.7f) * 6.0f);
      d.fillCircle(bx, dotY, (i % 3 == 0) ? 3 : 2, (i % 2) ? c1 : c2);
      continue;
    }
    if (state == AssistantPulseState::Playback) {
      uint8_t band = static_cast<uint8_t>(clampValue<int>(
          (i * kPlaybackSpectrumBands) / max<int>(1, bars), 0, kPlaybackSpectrumBands - 1));
      int16_t h = static_cast<int16_t>(4 + gVoiceEqBars[band] * 2);
      h = clampValue<int16_t>(h, 5, 42);
      uint16_t barColor = (i % 3 == 0) ? c2 : c1;
      d.fillRoundRect(bx - 2, waveY - h / 2, 4, h, 2, barColor);
      uint8_t peak = gVoiceEqPeaks[band];
      int16_t py = waveY - static_cast<int16_t>(min<int>(42, 4 + peak * 2)) / 2 - 2;
      d.drawFastHLine(bx - 2, py, 4, fg);
      continue;
    }
    float amp = state == AssistantPulseState::Recording ? 1.05f :
                0.68f;
    int16_t h = static_cast<int16_t>((8.0f + fabsf(sinf(t * 3.6f + i * 0.74f)) * 22.0f +
                                      fabsf(cosf(t * 1.7f + i * 0.31f)) * 7.0f) *
                                     amp * (0.65f + level * 0.75f));
    h = clampValue<int16_t>(h, 3, 42);
    d.fillRoundRect(bx - 1, waveY - h / 2, 3, h, 2, (i % 2) ? c1 : c2);
  }

  if (state == AssistantPulseState::Playback) {
    d.fillRoundRect(117, 110, 88, 2, 1, rgb565_local(23, 61, 69));
    int16_t progress = static_cast<int16_t>((playbackProgressPct() * 88U) / 100U);
    d.fillRoundRect(117, 110, max<int16_t>(1, progress), 3, 1, c1);
    d.setFont(&fonts::Font0);
    d.setTextColor(rgb565_local(150, 244, 211), bg);
    d.setCursor(206, 107);
    d.print(formatPlaybackMs(playbackRemainingMs()));
  }
  if (state == AssistantPulseState::Recording) {
    d.fillCircle(133, 112, 4 + static_cast<int16_t>(sinf(t * 8.0f)), c1);
  }
  drawAssistantPulseButton(154, 105, 62, 21, label, c1, millis() < gAssistantPulsePressUntilMs);
  if (gAssistantPendingVisible && state == AssistantPulseState::Thinking) {
    d.setFont(&fonts::Font2);
    d.setTextColor(amber, bg);
    d.setCursor(123, 13);
    d.print("agent thinking");
  }
  renderAssistantPulseChatHint();
  renderTopicOverlayIfVisible();
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderClockUi() {
  renderAssistantPulseUi();
}

