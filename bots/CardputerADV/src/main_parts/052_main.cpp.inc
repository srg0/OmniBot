void renderDebugOverlay() {
  if (!gDebugOverlayVisible) {
    return;
  }
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(0, 3, 8);
  const uint16_t line = rgb565_local(53, 68, 82);
  d.fillRoundRect(8, 8, 224, 104, 10, bg);
  d.drawRoundRect(8, 8, 224, 104, 10, line);
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_WHITE, bg);
  d.setCursor(14, 15);
  d.print("DBG");
  d.setCursor(48, 15);
  d.print(gWifiReady ? "wifi=ok" : "wifi=off");
  d.setCursor(118, 15);
  d.print(gWsReady ? "ws=ok" : "ws=off");
  d.setCursor(14, 28);
  d.print("face=");
  d.print(static_cast<int>(gFaceMode));
  d.setCursor(86, 28);
  d.print("scroll=");
  d.print(gChatScrollOffset);
  d.setCursor(150, 28);
  d.print(layoutName(gKeyboardLayout));
  d.setCursor(14, 41);
  d.print("rec=");
  d.print(static_cast<unsigned>(gRecordCapacityBytes / 1024));
  d.print("k");
  d.setCursor(86, 41);
  d.print("tts=");
  d.print(static_cast<unsigned>(gTtsCapacityBytes / 1024));
  d.print("k");
  d.setCursor(150, 41);
  d.print(gStorageReady ? gStorageLabel : "fs=off");
  d.setCursor(14, 55);
  String diag = gLastVoiceDiag;
  if (diag.length() > 28) {
    diag = diag.substring(diag.length() - 28);
  }
  d.print(diag);
  d.setCursor(14, 69);
  String httpDiag = gLastHttpDiag;
  if (httpDiag.length() > 28) {
    httpDiag = httpDiag.substring(httpDiag.length() - 28);
  }
  d.print(httpDiag);
  d.setCursor(14, 83);
  String keyDiag = gLastKeyDiag;
  if (keyDiag.length() > 28) {
    keyDiag = keyDiag.substring(keyDiag.length() - 28);
  }
  d.print(keyDiag);
  d.setCursor(150, 83);
  d.print(String(uiModeName(gUiMode)).substring(0, 6));
  d.setFont(&fonts::Font2);
}

void drawLauncherIcon(uint8_t group, int16_t x, int16_t y, int16_t s, uint16_t accent, uint16_t bg) {
  auto& d = gCanvas;
  int16_t cx = x + s / 2;
  int16_t cy = y + s / 2;
  int16_t r = max<int16_t>(5, s / 4);
  d.fillRoundRect(x, y, s, s, max<int16_t>(5, s / 5), bg);
  d.drawRoundRect(x, y, s, s, max<int16_t>(5, s / 5), accent);
  d.fillCircle(cx, cy, max<int16_t>(6, s / 3), rgb565_local(10, 14, 26));
  d.drawCircle(cx, cy, max<int16_t>(8, s / 3 + 3), accent);
  switch (group) {
    case 0:
      d.drawRoundRect(cx - r, cy - r / 2, r * 2, r, 4, accent);
      d.drawFastHLine(cx - r / 2, cy - 1, r, accent);
      d.fillCircle(cx - r / 2, cy - r / 2, 2, accent);
      d.fillCircle(cx + r / 2, cy - r / 2, 2, accent);
      break;
    case 1:
      for (int i = 0; i < 5; ++i) {
        int h = 4 + ((i * 5 + millis() / 100) % max<int>(5, s / 3));
        d.fillRoundRect(cx - 10 + i * 5, cy + 8 - h, 3, h, 1, accent);
      }
      break;
    case 2:
      d.setFont(&fonts::Font2);
      d.setTextColor(accent, bg);
      d.setCursor(cx - 10, cy - 6);
      d.print("25");
      d.drawArc(cx, cy, max<int16_t>(10, s / 2 - 4), max<int16_t>(7, s / 2 - 8), 35, 310, accent);
      break;
    case 3:
      d.drawLine(cx - 8, cy + 8, cx + 7, cy - 7, accent);
      d.drawCircle(cx + 7, cy - 7, 4, accent);
      d.fillCircle(cx - 8, cy + 8, 4, accent);
      d.fillCircle(cx - 8, cy + 8, 2, bg);
      break;
    case 4:
      d.drawRoundRect(cx - 11, cy - 7, 22, 14, 5, accent);
      d.drawFastHLine(cx - 8, cy, 7, accent);
      d.drawFastVLine(cx - 5, cy - 3, 7, accent);
      d.fillCircle(cx + 5, cy - 2, 2, accent);
      d.fillCircle(cx + 8, cy + 3, 2, accent);
      break;
    case 5:
      d.drawCircle(cx, cy, r + 2, accent);
      d.drawFastHLine(cx - r - 2, cy, (r + 2) * 2, accent);
      d.drawFastVLine(cx, cy - r - 2, (r + 2) * 2, accent);
      d.fillCircle(cx, cy, 5, accent);
      break;
    case 6:
    default:
      d.drawRoundRect(cx - r, cy - r / 2, r * 2, r, 4, accent);
      d.drawFastHLine(cx - r / 2, cy - 2, r, accent);
      d.drawFastHLine(cx - r / 2, cy + 3, r - 3, accent);
      d.fillCircle(cx + r / 2, cy, 2, accent);
      break;
  }
}

void renderLauncherOverlay() {
  if (!gLauncherVisible) {
    return;
  }
  auto& d = gCanvas;
  gLauncherSelection = clampValue<int>(gLauncherSelection, 0, kLauncherGroupCount - 1);
  const LauncherGroup& group = kLauncherGroups[gLauncherSelection];
  gLauncherSubSelection = clampValue<int>(gLauncherSubSelection, 0, group.count - 1);

  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(5, 13, 18);
  const uint16_t rowBg = rgb565_local(8, 17, 23);
  const uint16_t selectedBg = rgb565_local(14, 31, 39);
  const uint16_t line = rgb565_local(42, 55, 63);
  const uint16_t muted = rgb565_local(128, 145, 155);

  d.fillRoundRect(5, 5, 230, 125, 8, bg);
  d.drawRoundRect(5, 5, 230, 125, 8, rgb565_local(48, 54, 61));
  d.fillRoundRect(10, 10, 220, 18, 5, panel);

  d.setFont(&fonts::Font2);
  d.setTextColor(group.accent, panel);
  d.setCursor(17, 14);
  d.print(group.title);
  String page = String(gLauncherSelection + 1) + "/" + String(kLauncherGroupCount);
  d.setTextColor(muted, panel);
  d.setCursor(220 - d.textWidth(page), 14);
  d.print(page);

  const int16_t railX = 10;
  const int16_t railY = 33;
  const int16_t railW = 82;
  const int16_t railH = 10;
  const int16_t railGap = 2;
  constexpr int kVisibleGroupRows = 6;
  int groupWindowStart = clampValue<int>(
      gLauncherSelection - kVisibleGroupRows + 1, 0,
      max<int>(0, kLauncherGroupCount - kVisibleGroupRows));

  for (int row = 0; row < min<int>(kVisibleGroupRows, kLauncherGroupCount); ++row) {
    int idx = groupWindowStart + row;
    const LauncherGroup& item = kLauncherGroups[idx];
    const bool isSelected = idx == gLauncherSelection;
    const int16_t y = railY + row * (railH + railGap);
    const uint16_t fill = isSelected ? selectedBg : rowBg;
    d.fillRoundRect(railX, y, railW, railH, 4, fill);
    d.drawRoundRect(railX, y, railW, railH, 4, isSelected ? item.accent : line);

    d.setFont(&fonts::Font0);
    d.setTextColor(isSelected ? TFT_WHITE : muted, fill);
    d.setCursor(railX + 5, y + 1);
    d.print(String(idx + 1));

    d.setTextColor(isSelected ? item.accent : TFT_LIGHTGREY, fill);
    d.setCursor(railX + 17, y + 1);
    d.print(fitCurrentFontToWidth(item.title, railW - 28));

    if (isSelected) {
      d.fillCircle(railX + railW - 7, y + railH / 2, 3, item.accent);
    }
  }
  if (groupWindowStart > 0) {
    d.fillTriangle(87, 31, 90, 31, 89, 29, muted);
  }
  if (groupWindowStart + kVisibleGroupRows < kLauncherGroupCount) {
    d.fillTriangle(87, 106, 90, 106, 89, 108, muted);
  }
  d.setFont(&fonts::Font2);

  d.fillRoundRect(94, 33, 136, 78, 7, panel);
  d.drawRoundRect(94, 33, 136, 78, 7, group.accent);

  drawLauncherIcon(gLauncherSelection, 101, 38, 28, group.accent, rgb565_local(4, 9, 14));
  d.setFont(&fonts::Font2);
  d.setTextColor(group.accent, panel);
  d.setCursor(136, 38);
  d.print(fitCurrentFontToWidth(group.glyph, 86));
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(136, 53);
  d.print(fitCurrentFontToWidth(group.hint, 86));

  constexpr int kVisibleSubRows = 5;
  int subWindowStart = clampValue<int>(
      gLauncherSubSelection - kVisibleSubRows + 1, 0,
      max<int>(0, group.count - kVisibleSubRows));
  for (int row = 0; row < min<int>(kVisibleSubRows, group.count); ++row) {
    int i = subWindowStart + row;
    const int16_t y = 69 + row * 8;
    const bool selected = i == gLauncherSubSelection;
    const uint16_t fill = selected ? rgb565_local(18, 39, 48) : panel;
    if (selected) {
      d.fillRoundRect(100, y - 1, 121, 8, 3, fill);
    }
    d.setFont(&fonts::Font0);
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, fill);
    d.setCursor(104, y);
    d.print(selected ? "> " : "  ");
    d.print(fitCurrentFontToWidth(group.items[i].label, 101));
  }
  if (group.count > kVisibleSubRows) {
    const int16_t scrollX = 225;
    const int16_t scrollY = 69;
    const int16_t scrollH = 38;
    const int16_t knobH = max<int16_t>(7, (scrollH * kVisibleSubRows) / group.count);
    const int16_t knobY = scrollY +
        ((scrollH - knobH) * subWindowStart) / max<int>(1, group.count - kVisibleSubRows);
    d.drawFastVLine(scrollX, scrollY, scrollH, line);
    d.drawFastVLine(scrollX, knobY, knobH, group.accent);
  }
  d.setFont(&fonts::Font2);

  d.fillRoundRect(10, 116, 220, 10, 4, panel);
  drawTinyFooter(panel, muted, "1-7 group  C+1-6 open  arrows", 15, 118, 210);
}

String batteryEtaText() {
  if (!gBatteryDrainKnown || gBatteryEtaMinutes == 0 || batteryExternalPowerPresent()) {
    return "--";
  }
  uint16_t h = gBatteryEtaMinutes / 60;
  uint16_t m = gBatteryEtaMinutes % 60;
  if (h > 0) {
    return String(h) + "h" + String(m) + "m";
  }
  return String(m) + "m";
}

void drawBatteryHistoryGraph(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t panel) {
  auto& d = gCanvas;
  uint16_t grid = rgb565_local(39, 54, 64);
  uint16_t line = rgb565_local(47, 227, 255);
  uint16_t muted = rgb565_local(126, 143, 157);
  d.fillRoundRect(x, y, w, h, 5, rgb565_local(5, 14, 19));
  d.drawRoundRect(x, y, w, h, 5, grid);
  d.drawFastHLine(x + 4, y + h / 2, w - 8, rgb565_local(21, 31, 38));
  if (gBatteryHistoryCount < 2) {
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, rgb565_local(5, 14, 19));
    d.setCursor(x + 8, y + 9);
    d.print("collecting drain history");
    return;
  }

  int16_t prevX = -1;
  int16_t prevY = -1;
  for (uint8_t i = 0; i < gBatteryHistoryCount; ++i) {
    BatteryHistorySample sample = batteryHistorySampleAt(i);
    int value = sample.level >= 0 ? sample.level : estimateBatteryLevelFromVoltage(sample.voltageMv);
    if (value < 0) {
      continue;
    }
    int16_t px = x + 4 + (gBatteryHistoryCount <= 1 ? 0 : (static_cast<int32_t>(i) * (w - 8)) / (gBatteryHistoryCount - 1));
    int16_t py = y + h - 4 - (clampValue<int>(value, 0, 100) * (h - 8)) / 100;
    if (prevX >= 0) {
      d.drawLine(prevX, prevY, px, py, line);
    }
    d.fillCircle(px, py, sample.externalPower ? 2 : 1, sample.externalPower ? rgb565_local(88, 240, 141) : line);
    prevX = px;
    prevY = py;
  }
}

void renderBatteryUi() {
  updateBatterySnapshot();
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(8, 17, 23);
  const uint16_t border = rgb565_local(48, 54, 61);
  const uint16_t accent = rgb565_local(47, 227, 255);
  const uint16_t muted = rgb565_local(126, 143, 157);
  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 12, panel);
  d.drawRoundRect(6, 5, 228, 124, 12, border);

  bool levelKnown = gBatterySnapshot.level >= 0;
  int level = levelKnown ? clampValue<int>(gBatterySnapshot.level, 0, 100) : 0;
  uint16_t barColor = !levelKnown ? rgb565_local(80, 130, 180)
                     : level >= 60 ? rgb565_local(88, 240, 141)
                     : level >= 25 ? rgb565_local(246, 194, 74)
                                   : rgb565_local(255, 93, 88);

  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(14, 12);
  d.print("BATTERY");
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(68, 15);
  d.print(gBatteryHistoryCount);
  d.print(" samples");

  d.setFont(&fonts::Font7);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(16, 28);
  d.print(levelKnown ? String(level) : "--");
