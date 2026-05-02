
void renderSdRequiredBanner(const String& title) {
  drawCardHeader(title, rgb565_local(255, 180, 60));
  auto& d = gCanvas;
  d.setFont(&fonts::Font2);
  d.fillRoundRect(18, 34, 204, 72, 10, rgb565_local(26, 18, 6));
  d.drawRoundRect(18, 34, 204, 72, 10, rgb565_local(255, 180, 60));
  d.setTextColor(TFT_ORANGE, rgb565_local(26, 18, 6));
  d.setCursor(32, 45);
  d.print("SD card required");
  d.setTextColor(TFT_WHITE, rgb565_local(26, 18, 6));
  d.setCursor(32, 62);
  d.print("Insert FAT32 card");
  d.setCursor(32, 76);
  d.print("then reboot device");
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "Pulse/chat still work without SD", 18, 116, 204);
}

String compactBytes(uint32_t bytes) {
  if (bytes >= 1024UL * 1024UL) {
    return String(bytes / (1024UL * 1024UL)) + "MB";
  }
  if (bytes >= 1024UL) {
    return String(bytes / 1024UL) + "KB";
  }
  return String(bytes) + "B";
}

String marqueeText(const String& text, int32_t pixelLimit, uint16_t stepMs = 240) {
  if (gCanvas.textWidth(text) <= pixelLimit) {
    return text;
  }
  String padded = text + "    " + text;
  int len = text.length() + 4;
  int offset = len > 0 ? static_cast<int>((millis() / stepMs) % len) : 0;
  String out = padded.substring(offset);
  while (out.length() > 1 && gCanvas.textWidth(out) > pixelLimit) {
    out.remove(out.length() - 1);
  }
  return out;
}

String fitCurrentFontToWidth(String text, int32_t pixelLimit) {
  if (gCanvas.textWidth(text) <= pixelLimit) {
    return text;
  }
  while (text.length() > 1 && gCanvas.textWidth(text + "..") > pixelLimit) {
    text.remove(text.length() - 1);
  }
  return text + "..";
}

void drawTinyFooter(uint16_t bg, uint16_t fg, const String& text, int16_t x, int16_t y, int16_t w) {
  auto& d = gCanvas;
  const int16_t textY = min<int16_t>(y, 118);
  const int16_t clearX = max<int16_t>(0, x - 2);
  const int16_t clearY = max<int16_t>(0, textY - 2);
  const int16_t clearW = min<int16_t>(240 - clearX, w + 4);
  d.fillRect(clearX, clearY, clearW, 14, bg);
  d.setFont(&fonts::Font0);
  d.setTextColor(fg, bg);
  d.setCursor(x, textY);
  d.print(fitCurrentFontToWidth(text, w));
  d.setFont(&fonts::Font2);
}

void renderFocusUi() {
  auto& d = gCanvas;
  const bool isBreak = gFocus.state == FocusState::ShortBreak || gFocus.state == FocusState::LongBreak;
  const bool isReflect = gFocus.state == FocusState::Reflect;
  const bool isPaused = gFocus.state == FocusState::Paused || gFocus.state == FocusState::Stopped;
  uint16_t bg = isBreak ? rgb565_local(1, 10, 8) : isReflect ? rgb565_local(10, 4, 16) : rgb565_local(2, 6, 12);
  uint16_t panel = isBreak ? rgb565_local(5, 23, 19) : isReflect ? rgb565_local(20, 11, 31) : rgb565_local(7, 14, 24);
  uint16_t panel2 = isBreak ? rgb565_local(10, 40, 31) : isReflect ? rgb565_local(34, 18, 52) : rgb565_local(10, 23, 36);
  uint16_t accent = isBreak ? rgb565_local(105, 255, 156)
                    : isReflect ? rgb565_local(214, 133, 255)
                    : isPaused ? rgb565_local(154, 143, 255)
                               : rgb565_local(47, 227, 255);
  uint16_t secondary = isBreak ? rgb565_local(47, 227, 255)
                       : isReflect ? rgb565_local(255, 194, 74)
                                   : rgb565_local(246, 194, 74);
  uint16_t muted = rgb565_local(126, 143, 157);
  uint32_t now = millis();
  float t = now / 1000.0f;

  String timeText = focusTimeString(gFocus.remainingSec);
  uint32_t elapsed = gFocus.sessionTotalSec > gFocus.remainingSec ? gFocus.sessionTotalSec - gFocus.remainingSec : 0;
  int progressDeg = gFocus.sessionTotalSec ? static_cast<int>((elapsed * 300ULL) / gFocus.sessionTotalSec) : 0;
  progressDeg = clampValue<int>(progressDeg, 0, 300);

  d.fillScreen(bg);
  for (int x = -20; x < 260; x += 20) {
    int offset = static_cast<int>(sinf(t * 0.45f + x * 0.045f) * 5.0f);
    d.drawFastVLine(x + offset, 0, 135, rgb565_local(5, 18, 27));
  }
  d.fillRoundRect(6, 5, 228, 124, 14, panel);
  d.drawRoundRect(6, 5, 228, 124, 14, rgb565_local(28, 50, 70));

  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(14, 12);
  d.print(fitCurrentFontToWidth(focusStateName(gFocus.state), 74));
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(101, 15);
  d.print("cycle ");
  d.print(gFocus.cycle);
  d.print("/");
  d.print(gFocusSettings.cyclesPerRound);
  String done = String(gFocus.completedToday) + " done";
  d.setCursor(222 - d.textWidth(done), 15);
  d.print(done);

  if (isBreak) {
    float breath = (sinf(t * 1.3f) + 1.0f) * 0.5f;
    int r = 20 + static_cast<int>(breath * 15.0f);
    int cx = 70;
    int cy = 67;
    d.fillCircle(cx, cy, r + 13, rgb565_local(4, 18, 20));
    d.drawCircle(cx, cy, r + 7, rgb565_local(28, 84, 74));
    d.drawCircle(cx, cy, r, accent);
    d.fillCircle(cx, cy, max<int>(8, r - 13), panel2);
    d.setFont(&fonts::Font0);
    d.setTextColor(accent, panel2);
    d.setCursor(cx - 22, cy - 3);
    d.print(breath > 0.55f ? "inhale" : "exhale");
    d.setFont(&fonts::Font7);
    d.setTextColor(TFT_WHITE, panel);
    int timeWidth = d.textWidth(timeText);
    d.setCursor(clampValue<int>(226 - timeWidth, 98, 128), 45);
    d.print(timeText);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(130, 86);
    d.print("follow the ball");
  } else if (isReflect) {
    d.fillRoundRect(18, 34, 204, 62, 10, panel2);
    d.drawRoundRect(18, 34, 204, 62, 10, accent);
    d.setFont(&fonts::Font2);
    d.setTextColor(secondary, panel2);
    d.setCursor(31, 43);
    d.print("REFLECT");
    d.setTextColor(TFT_WHITE, panel2);
    d.setFont(&fonts::Font7);
    d.setCursor(31, 58);
    d.print(timeText);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel2);
    d.setCursor(132, 46);
    d.print("write 1 line");
    d.setCursor(132, 58);
    d.print("then return");
    for (int i = 0; i < 9; ++i) {
      int px = 156 + i * 6;
      int py = 83 + static_cast<int>(sinf(t * 1.2f + i) * 3.0f);
      d.fillCircle(px, py, 1, (i % 2) ? accent : secondary);
    }
  } else {
    int cx = 72;
    int cy = 70;
    d.fillCircle(cx, cy, 47, rgb565_local(4, 16, 24));
    d.drawCircle(cx, cy, 44, rgb565_local(22, 47, 64));
    d.drawArc(cx, cy, 40, 33, -150, -150 + progressDeg, accent);
    d.drawArc(cx, cy, 28, 23, 40 + static_cast<int>(t * 18) % 360,
              280 + static_cast<int>(t * 18) % 360, secondary);
    d.fillCircle(cx, cy, isPaused ? 16 : 19 + static_cast<int>(sinf(t * 2.0f) * 2.0f), panel2);
    d.setFont(&fonts::Font0);
    d.setTextColor(accent, panel2);
    d.setCursor(cx - 15, cy - 3);
    d.print(isPaused ? "idle" : "deep");
    d.setFont(&fonts::Font7);
    d.setTextColor(TFT_WHITE, panel);
    int timeWidth = d.textWidth(timeText);
    d.setCursor(clampValue<int>(226 - timeWidth, 92, 124), 44);
    d.print(timeText);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(132, 86);
    d.print(gFocusSettings.metronome ? "metro pulse" : "quiet focus");
  }

  if (gFocusSettings.metronome && !isBreak && !isReflect) {
    int pivotX = 204;
    int pivotY = 39;
    float swing = sinf(t * max<float>(0.6f, gFocusSettings.bpm / 60.0f) * kPi);
    int tipX = pivotX + static_cast<int>(swing * 18.0f);
    int tipY = pivotY + 42;
    d.drawLine(pivotX, pivotY, tipX, tipY, secondary);
    d.fillCircle(pivotX, pivotY, 3, secondary);
    d.fillCircle(tipX, tipY, 5, accent);
  }

  int progressW = gFocus.sessionTotalSec ? static_cast<int>((elapsed * 204ULL) / gFocus.sessionTotalSec) : 0;
  progressW = clampValue<int>(progressW, 0, 204);
  d.fillRoundRect(18, 102, 204, 8, 4, rgb565_local(18, 33, 45));
  d.fillRoundRect(19, 103, max<int>(1, progressW), 6, 3, accent);

  if (gFocus.helpVisible) {
    drawTinyFooter(panel, TFT_LIGHTGREY, "Ent pause  R reset  S +5  N next", 16, 114, 208);
  } else {
    String footer = String("Ent start  M ") + (gFocusSettings.metronome ? "metro" : "quiet") +
                    "  " + String(gFocusSettings.bpm) + "bpm  H setup";
    drawTinyFooter(panel, TFT_LIGHTGREY, footer, 16, 114, 208);
  }

  if (gFocus.helpVisible) {
    d.fillRoundRect(18, 28, 204, 86, 10, 0x0000);
    d.drawRoundRect(18, 28, 204, 86, 10, accent);
    d.setFont(&fonts::Font2);
    d.setTextColor(TFT_WHITE, 0x0000);
    d.setCursor(28, 38);
    d.print("Focus setup");
    d.setTextColor(TFT_LIGHTGREY, 0x0000);
    d.setCursor(28, 54);
    d.print("Q/W focus ");
    d.print(gFocusSettings.focusSec / 60);
    d.print("m");
    d.setCursor(28, 68);
    d.print("A auto ");
    d.print(gFocusSettings.autoStart ? "on" : "off");
    d.print("  M ");
    d.print(gFocusSettings.metronome ? "metro" : "quiet");
    d.setCursor(28, 82);
    d.print("Z/X bpm ");
    d.print(gFocusSettings.bpm);
    d.print("  S +5m");
    d.setCursor(28, 96);
    d.print("Enter pause  R reset  N next");
  }

  renderLauncherOverlay();
  renderDebugOverlay();
}

