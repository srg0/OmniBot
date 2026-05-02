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
      d.drawRoundRect(cx - r - 3, cy - r / 2, r, r, 4, accent);
      d.drawRoundRect(cx + 3, cy - r / 2, r, r, 4, accent);
      d.fillCircle(cx - r / 2 - 3, cy, 2, accent);
      d.fillCircle(cx + r / 2 + 3, cy, 2, accent);
      break;
    case 2:
      for (int i = 0; i < 6; ++i) {
        int h = max<int>(5, (s / 5) + ((i * 7 + millis() / 90) % max<int>(6, s / 3)));
        d.fillRoundRect(x + 7 + i * max<int>(3, s / 8), y + s - 7 - h, max<int>(2, s / 10), h, 2, accent);
      }
      break;
    case 3:
      d.setFont(&fonts::Font2);
      d.setTextColor(accent, bg);
      d.setCursor(cx - 10, cy - 6);
      d.print("25");
      d.drawArc(cx, cy, max<int16_t>(10, s / 2 - 4), max<int16_t>(7, s / 2 - 8), 35, 310, accent);
      break;
    case 4:
      d.drawRoundRect(cx - r, cy - r, r * 2, r * 2, 4, accent);
      d.drawLine(cx - r + 3, cy, cx - 1, cy + r - 3, accent);
      d.drawLine(cx - 1, cy + r - 3, cx + r - 3, cy - r + 3, accent);
      d.fillCircle(cx + r - 3, cy - r + 3, 2, accent);
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

  for (int idx = 0; idx < kLauncherGroupCount; ++idx) {
    const LauncherGroup& item = kLauncherGroups[idx];
    const bool isSelected = idx == gLauncherSelection;
    const int16_t y = railY + idx * (railH + railGap);
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
  d.setFont(&fonts::Font2);

  d.fillRoundRect(94, 33, 136, 78, 7, panel);
  d.drawRoundRect(94, 33, 136, 78, 7, group.accent);

  drawLauncherIcon(gLauncherSelection, 102, 41, 32, group.accent, rgb565_local(4, 9, 14));
  d.setFont(&fonts::Font2);
  d.setTextColor(group.accent, panel);
  d.setCursor(142, 41);
  d.print(fitCurrentFontToWidth(group.glyph, 76));
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(142, 56);
  d.print(fitCurrentFontToWidth(group.hint, 78));

  for (int i = 0; i < group.count; ++i) {
    const int16_t y = 76 + i * 8;
    const bool selected = i == gLauncherSubSelection;
    const uint16_t fill = selected ? rgb565_local(18, 39, 48) : panel;
    if (selected) {
      d.fillRoundRect(100, y - 1, 124, 8, 3, fill);
    }
    d.setFont(&fonts::Font0);
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, fill);
    d.setCursor(104, y);
    d.print(selected ? "> " : "  ");
    d.print(fitCurrentFontToWidth(group.items[i].label, 106));
  }
  d.setFont(&fonts::Font2);

  d.fillRoundRect(10, 116, 220, 10, 4, panel);
  drawTinyFooter(panel, muted, "Up/Down app  Left/Right screen  Ent", 15, 118, 210);
}

void renderBatteryUi() {
  updateBatterySnapshot();
  auto& d = gCanvas;
  d.fillScreen(0x0210);

  d.fillRoundRect(8, 8, 224, 118, 16, 0x0841);
  d.drawRoundRect(8, 8, 224, 118, 16, 0x31C7);

  int level = gBatterySnapshot.level < 0 ? 0 : gBatterySnapshot.level;
  level = clampValue(level, 0, 100);
  int barX = 22;
  int barY = 28;
  int barW = 140;
  int barH = 28;
  int fillW = (barW - 8) * level / 100;
  uint16_t barColor = level >= 60 ? rgb565_local(90, 255, 150)
                     : level >= 25 ? TFT_GOLD
                                   : rgb565_local(255, 110, 80);

  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_YELLOW, 0x0841);
  d.setCursor(22, 16);
  d.print("Battery");

  d.drawRoundRect(barX, barY, barW, barH, 8, TFT_LIGHTGREY);
  d.fillRect(barX + barW, barY + 8, 6, 12, TFT_LIGHTGREY);
  d.fillRoundRect(barX + 4, barY + 4, max(0, fillW), barH - 8, 5, barColor);

