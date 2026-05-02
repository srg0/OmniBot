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

  uint32_t now = millis();
  if (gTopicCreateArmed && now > gTopicCreateArmedUntilMs) {
    gTopicCreateArmed = false;
  }
  if (gTopicCreateArmed) {
    float hold = 1.0f - min<float>(1.0f, static_cast<float>(gTopicCreateArmedUntilMs - now) / 3200.0f);
    d.fillRoundRect(6, 5, 228, 39, 10, rgb565_local(5, 12, 18));
    d.drawRoundRect(6, 5, 228, 39, 10, amber);
    d.fillRoundRect(10, 10, 42, 24, 8, rgb565_local(34, 22, 8));
    d.setFont(&fonts::Font2);
    d.setTextColor(amber, rgb565_local(34, 22, 8));
    d.setCursor(18, 17);
    d.print("NEW");
    d.setFont(&fonts::efontCN_12);
    d.setTextColor(TFT_WHITE, rgb565_local(5, 12, 18));
    d.setCursor(60, 9);
    d.print(fitCurrentFontToWidth(defaultNewTopicTitle(), 144));
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, rgb565_local(5, 12, 18));
    d.setCursor(61, 27);
    d.print("Alt+Left again creates");
    d.fillRoundRect(10, 39, 216, 2, 1, rgb565_local(52, 38, 16));
    d.fillRoundRect(10, 39, max<int16_t>(4, static_cast<int16_t>(216 * hold)), 2, 1, amber);
    d.setFont(&fonts::Font2);
    return;
  }

  if (gContexts.empty()) {
    return;
  }
  gSelectedContext = clampValue<int>(gSelectedContext, 0, static_cast<int>(gContexts.size()) - 1);
  const ConversationContext& ctx = gContexts[gSelectedContext];

  float animT = min<float>(1.0f, (now - gContextAnimStartMs) / 220.0f);
  float ease = 1.0f - (1.0f - animT) * (1.0f - animT);
  int16_t dx = (gContextAnimDir == 0 || animT >= 1.0f)
                   ? 0
                   : static_cast<int16_t>(gContextAnimDir * (1.0f - ease) * 28.0f);

  d.fillRoundRect(6, 5, 228, 35, 8, bg);
  d.drawRoundRect(6, 5, 228, 35, 8, cyan);
  d.fillRoundRect(10, 9, 42, 23, 7, panel);
  d.drawRoundRect(10, 9, 42, 23, 7, violet);

  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(16, 16);
  d.print(fitCurrentFontToWidth(topicDisplayIcon(ctx), 30));

  d.setFont(&fonts::efontCN_12);
  d.setTextColor(cyan, bg);
  d.setCursor(60 + dx, 9);
  d.print(fitCurrentFontToWidth(topicDisplayTitle(ctx), 132));

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, bg);
  d.setCursor(61 + dx, 25);
  String sub;
  if (ctx.unread > 0) {
    sub += "  +" + String(ctx.unread);
  } else if (gTopicTaskHandle != nullptr) {
    sub = "syncing...";
  } else {
    sub = "active chat topic";
  }
  d.print(fitCurrentFontToWidth(sub, 132));

  d.setTextColor(gTopicTaskHandle != nullptr ? amber : cyan, bg);
  d.setCursor(204, 14);
  d.print("<>");
  d.fillRect(12, 38, 216, 2, gTopicTaskHandle != nullptr ? amber : cyan);
  d.setFont(&fonts::Font2);
}

void renderModeFooter(const String& left, const String& right = "") {
  auto& d = gCanvas;
  d.fillRect(0, 123, 240, 12, 0x18C3);
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_LIGHTGREY, 0x18C3);
  d.setCursor(4, 124);
  d.print(fitCurrentFontToWidth(left, right.isEmpty() ? 230 : 132));
  if (!right.isEmpty()) {
    int rightWidth = d.textWidth(right);
    d.setCursor(max<int>(4, 236 - rightWidth), 124);
    d.print(fitCurrentFontToWidth(right, 96));
  }
  d.setFont(&fonts::Font2);
}

void renderFaceOnlyUi() {
  auto& d = gCanvas;
  d.fillScreen(0x0841);
