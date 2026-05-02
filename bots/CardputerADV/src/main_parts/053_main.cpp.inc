  d.setFont(&fonts::Font7);
  d.setTextColor(TFT_WHITE, 0x0841);
  d.setCursor(166, 22);
  d.print(String(level));
  d.setFont(&fonts::Font4);
  d.setCursor(208, 36);
  d.print("%");

  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_CYAN, 0x0841);
  d.setCursor(22, 70);
  d.print("State");
  d.setCursor(92, 70);
  d.setTextColor(TFT_WHITE, 0x0841);
  d.print(batteryChargeLabel(gBatterySnapshot.charging));

  d.setTextColor(TFT_CYAN, 0x0841);
  d.setCursor(22, 84);
  d.print("Voltage");
  d.setCursor(92, 84);
  d.setTextColor(TFT_WHITE, 0x0841);
  if (gBatterySnapshot.voltageMv > 0) {
    d.print(String(gBatterySnapshot.voltageMv / 1000.0f, 2) + "V");
  } else {
    d.print("--");
  }

  d.setTextColor(TFT_CYAN, 0x0841);
  d.setCursor(22, 98);
  d.print("Current");
  d.setCursor(92, 98);
  d.setTextColor(TFT_WHITE, 0x0841);
  d.print(String(gBatterySnapshot.currentMa) + "mA");

  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_CYAN, 0x0841);
  d.setCursor(22, 113);
  d.print("Storage");
  d.setCursor(92, 113);
  d.setTextColor(TFT_WHITE, 0x0841);
  if (gStorageReady) {
    uint64_t totalKb = activeStorageTotalBytes() / 1024ULL;
    uint64_t usedKb = activeStorageUsedBytes() / 1024ULL;
    String unit = "KB";
    uint64_t totalShown = totalKb;
    uint64_t usedShown = usedKb;
    if (totalKb > 1024ULL * 10ULL) {
      unit = "MB";
      totalShown = totalKb / 1024ULL;
      usedShown = usedKb / 1024ULL;
    }
    d.print(gStorageLabel + " " + String(static_cast<unsigned long>(usedShown)) + "/" +
            String(static_cast<unsigned long>(totalShown)) + unit);
  } else {
    d.print("off");
  }
  d.setFont(&fonts::Font2);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void drawCardHeader(const String& title, uint16_t accent) {
  auto& d = gCanvas;
  d.fillScreen(0x0208);
  d.fillRoundRect(6, 6, 228, 123, 14, 0x0841);
  d.drawRoundRect(6, 6, 228, 123, 14, accent);
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, 0x0841);
  d.setCursor(14, 14);
  d.print(title);
}

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

