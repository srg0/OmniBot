String settingValue(uint8_t index) {
  switch (index) {
    case 0:
      return String(gFocusSettings.focusSec / 60) + "m";
    case 1:
      return String(gFocusSettings.shortBreakSec / 60) + "m";
    case 2:
      return String(gFocusSettings.longBreakSec / 60) + "m";
    case 3:
      return String(gFocusSettings.cyclesPerRound);
    case 4:
      return gFocusSettings.autoStart ? "on" : "off";
    case 5:
      return gDeviceSettings.autoUpdateFirmware ? "on" : "off";
    case 6:
      return String(gFocusSettings.bpm);
    case 7:
      return String(gDeviceSettings.minBatteryForUpdate) + "%";
    case 8:
      return gDeviceSettings.audioLanguage;
    case 9:
      return clockFaceName(gDeviceSettings.clockFace);
    default:
      return "-";
  }
}

void renderSettingsUi() {
  drawCardHeader("Settings", 0x7BEF);
  auto& d = gCanvas;
  gSelectedSetting = clampValue<int>(gSelectedSetting, 0, kSettingsItemCount - 1);
  constexpr int visible = 7;
  int start = max<int>(0, min<int>(gSelectedSetting - visible / 2, kSettingsItemCount - visible));
  d.setFont(&fonts::Font0);
  for (int i = 0; i < visible; ++i) {
    int idx = start + i;
    if (idx >= kSettingsItemCount) {
      break;
    }
    int y = 34 + i * 11;
    bool selected = idx == gSelectedSetting;
    uint16_t bg = selected ? 0x4208 : 0x0841;
    d.fillRoundRect(14, y - 1, 212, 10, 3, bg);
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, bg);
    d.setCursor(18, y + 1);
    d.print(fitCurrentFontToWidth(settingLabel(idx), 126));
    String value = settingValue(idx);
    String fittedValue = fitCurrentFontToWidth(value, 72);
    int w = d.textWidth(fittedValue);
    d.setCursor(max<int>(144, 222 - w), y + 1);
    d.print(fittedValue);
  }
  d.setFont(&fonts::Font2);
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "< > pick  +/- edit  Space toggle", 16, 116, 208);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderCalculatorUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(3, 7, 11);
  const uint16_t panel = rgb565_local(9, 17, 24);
  const uint16_t row = rgb565_local(14, 28, 38);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(126, 143, 157);
  uint16_t accent = gCalcStatus == "Error" ? amber : green;

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 12, panel);
  d.drawRoundRect(6, 5, 228, 124, 12, accent);
  d.drawRoundRect(10, 9, 220, 116, 9, rgb565_local(22, 44, 57));

  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(16, 14);
  d.print("CALCULATOR");
  d.setFont(&fonts::Font0);
  d.setTextColor(accent, panel);
  d.setCursor(224 - d.textWidth(gCalcStatus), 17);
  d.print(gCalcStatus);

  d.fillRoundRect(16, 34, 208, 28, 8, row);
  d.setFont(&fonts::efontCN_12);
  d.setTextColor(TFT_WHITE, row);
  String expr = gCalcExpression.isEmpty() ? "type expression" : tailUtf8ToFit(gCalcExpression, 188);
  d.setCursor(24, 41);
  d.print(expr);

  d.fillRoundRect(16, 68, 208, 25, 8, rgb565_local(3, 12, 16));
  d.setFont(&fonts::Font4);
  d.setTextColor(accent, rgb565_local(3, 12, 16));
  String result = fitCurrentFontToWidth(gCalcResult, 194);
  d.setCursor(218 - d.textWidth(result), 72);
  d.print(result);

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(19, 101);
  d.print("0-9 + - * / . ( )");
  d.setTextColor(cyan, panel);
  d.setCursor(142, 101);
  d.print("Enter =");
  d.setTextColor(amber, panel);
  d.setCursor(19, 114);
  d.print("C clear");
  d.setTextColor(muted, panel);
  d.setCursor(82, 114);
  d.print("Del back  Esc home");
  d.setFont(&fonts::Font2);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderContextsUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(3, 6, 12);
  const uint16_t panel = rgb565_local(8, 14, 24);
  const uint16_t panel2 = rgb565_local(12, 22, 34);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t violet = rgb565_local(170, 130, 255);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t green = rgb565_local(100, 255, 170);
  const uint16_t muted = rgb565_local(135, 151, 166);

  d.fillScreen(bg);
  uint32_t now = millis();
  int meshPhase = static_cast<int>((now / 90) % 24);
  for (int x = -24; x < 260; x += 24) {
    d.drawFastVLine(x + meshPhase, 0, 135, rgb565_local(7, 18, 28));
  }
  for (int y = 10; y < 135; y += 18) {
    int offset = static_cast<int>(sin((now + y * 31) * 0.0015f) * 5.0f);
    d.drawFastHLine(0, y + offset, 240, rgb565_local(8, 22, 34));
  }
  d.drawRoundRect(4, 4, 232, 127, 14, rgb565_local(24, 46, 68));
  d.drawRoundRect(5, 5, 230, 125, 13, rgb565_local(8, 18, 30));

  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, bg);
  d.setCursor(12, 10);
  d.print("OPENCLAW TOPIC");
  String source = gContextsRemoteLoaded ? "LIVE" : "LOCAL";
  d.setTextColor(gContextsRemoteLoaded ? green : amber, bg);
  d.setCursor(232 - d.textWidth(source), 10);
  d.print(source);

  if (gContexts.empty()) {
    d.fillRoundRect(20, 42, 200, 54, 10, panel);
    d.drawRoundRect(20, 42, 200, 54, 10, amber);
    d.setTextColor(amber, panel);
    d.setCursor(42, 60);
    d.print("No contexts");
    drawTinyFooter(bg, muted, "R sync topics  Esc home", 30, 116, 180);
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }

  gSelectedContext = clampValue<int>(gSelectedContext, 0, static_cast<int>(gContexts.size()) - 1);
  const ConversationContext& ctx = gContexts[gSelectedContext];
  d.setTextColor(muted, bg);
  d.setCursor(12, 24);
  d.print("Alt+<> switch topic");
  if (ctx.unread > 0) {
    String unread = String(ctx.unread) + " new";
    d.setTextColor(amber, bg);
    d.setCursor(232 - d.textWidth(unread), 24);
    d.print(unread);
  }

  float animT = min<float>(1.0f, (now - gContextAnimStartMs) / 260.0f);
  float ease = 1.0f - (1.0f - animT) * (1.0f - animT);
  int cardDx = (gContextAnimDir == 0 || animT >= 1.0f) ? 0 : static_cast<int>(gContextAnimDir * (1.0f - ease) * 34.0f);
  int cardX = 12 + cardDx;
  d.fillRoundRect(cardX, 35, 216, 47, 13, panel);
  d.drawRoundRect(cardX, 35, 216, 47, 13, ctx.key == gSavedContextKey ? green : violet);
  d.fillRoundRect(cardX + 8, 42, 52, 33, 10, panel2);
  d.drawRoundRect(cardX + 8, 42, 52, 33, 10, cyan);
  d.setTextColor(TFT_WHITE, panel2);
  d.setTextDatum(middle_center);
  d.setFont(&fonts::Font4);
  d.drawString(topicDisplayIcon(ctx), cardX + 34, 59);
  d.setTextDatum(top_left);
  d.setFont(&fonts::efontCN_12);

  d.setTextColor(TFT_WHITE, panel);
  String title = marqueeText(topicDisplayTitle(ctx), 146, 260);
  d.setCursor(cardX + 70, 43);
  d.print(title);
  d.setTextColor(muted, panel);
  d.setCursor(cardX + 70, 58);
  d.print(gTopicTaskHandle != nullptr ? "syncing..." : "chat context");
  d.setFont(&fonts::Font2);
  d.setTextColor(ctx.key == gSavedContextKey ? green : muted, panel);
  d.setCursor(cardX + 70, 70);
  d.print(ctx.key == gSavedContextKey ? "selected" : "Enter select");

  d.setTextColor(cyan, bg);
  d.setCursor(8, 58);
  d.print("<");
  d.setCursor(226, 58);
  d.print(">");

  d.fillRoundRect(12, 86, 216, 37, 8, rgb565_local(4, 10, 16));
  d.drawRoundRect(12, 86, 216, 37, 8, rgb565_local(20, 42, 58));
  d.setFont(&fonts::Font0);
  if (gContextHistoryKey == currentConversationKey() && !gContextPreview.empty()) {
    int y = 90;
    int shown = 0;
    for (const auto& line : gContextPreview) {
      if (shown >= 4) {
        break;
      }
      uint16_t roleColor = line.role == "assistant" ? green : (line.voice ? amber : cyan);
      d.setTextColor(roleColor, rgb565_local(4, 10, 16));
      d.setCursor(18, y);
      String prefix = line.role == "assistant" ? "AI" : (line.voice ? "VO" : "ME");
      d.print(prefix);
      d.setTextColor(TFT_LIGHTGREY, rgb565_local(4, 10, 16));
      d.setCursor(34, y);
      d.print(fitCurrentFontToWidth(line.text, 184));
      y += 8;
      shown++;
    }
    if (gContextPreview.size() > 4) {
      d.setTextColor(muted, rgb565_local(4, 10, 16));
      d.setFont(&fonts::Font0);
      d.setCursor(205, 115);
      d.print("+");
      d.print(static_cast<int>(gContextPreview.size()) - 4);
      d.setFont(&fonts::Font2);
    }
  } else {
    d.setTextColor(muted, rgb565_local(4, 10, 16));
    d.setCursor(18, 99);
    if (!gContextError.isEmpty()) {
      d.setTextColor(amber, rgb565_local(4, 10, 16));
      d.print(trimPreview(gContextError, 38));
    } else {
      d.print("Enter loads last 5 messages");
    }
  }

  drawTinyFooter(bg, muted, "Alt+<> topic  Ent select  R sync", 12, 124, 216);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderLogsUi() {
  drawCardHeader("Logs / Status", 0xC618);
  auto& d = gCanvas;
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_WHITE, 0x0841);
  String statusLine = String(WiFi.isConnected() ? WiFi.localIP().toString() : "wifi off") + " " + gStorageLabel;
  d.setCursor(16, 32);
  d.print(fitCurrentFontToWidth(statusLine, 208));
  int visible = 9;
  int maxOffset = max<int>(0, static_cast<int>(gRuntimeLogs.size()) - visible);
  gLogScrollOffset = clampValue<int>(gLogScrollOffset, 0, maxOffset);
  int start = max<int>(0, static_cast<int>(gRuntimeLogs.size()) - visible - gLogScrollOffset);
  for (int i = 0; i < visible; ++i) {
    int idx = start + i;
    if (idx >= static_cast<int>(gRuntimeLogs.size())) {
      break;
    }
    d.setTextColor(TFT_LIGHTGREY, 0x0841);
    d.setCursor(16, 43 + i * 8);
    d.print(fitCurrentFontToWidth(gRuntimeLogs[idx], 208));
  }
  d.setFont(&fonts::Font2);
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "< > scroll  R reload  U upload", 16, 116, 208);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderTopicOverlayIfVisible() {
  if (millis() > gTopicOverlayUntilMs) {
    return;
  }
  auto& d = gCanvas;

  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(5, 13, 18);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t violet = rgb565_local(157, 102, 255);
  const uint16_t muted = rgb565_local(128, 145, 155);
  const uint16_t amber = rgb565_local(246, 194, 74);
