  int cx = 52;
  int cy = 68;
  for (int i = 0; i < 18; ++i) {
    float a = i * 0.349f + now * 0.004f;
    int r = 18 + (i % 3) * 4;
    int x = cx + static_cast<int>(cosf(a) * r);
    int y = cy + static_cast<int>(sinf(a) * r);
    d.fillCircle(x, y, 1 + (i % 2), gPlaybackActive ? green : cyan);
  }
  d.fillCircle(cx, cy, 18, panel2);
  d.drawCircle(cx, cy, 19, gPlaybackActive ? green : cyan);
  d.setTextColor(gPlaybackActive ? green : cyan, panel2);
  d.setTextDatum(middle_center);
  d.setFont(&fonts::Font4);
  d.drawString(gPlaybackActive ? "PLAY" : "WAV", cx, cy - 4);
  d.setTextDatum(top_left);
  d.setFont(&fonts::Font2);

  if (gVoiceNotes.empty()) {
    d.setTextColor(amber, panel);
    d.setCursor(92, 54);
    d.print("No saved notes");
    d.setTextColor(muted, panel);
    d.setCursor(92, 70);
    d.print("Record voice first");
  } else {
    gSelectedVoiceNote = clampValue<int>(gSelectedVoiceNote, 0, static_cast<int>(gVoiceNotes.size()) - 1);
    const auto& note = gVoiceNotes[gSelectedVoiceNote];
    String who = note.assistant ? "AI" : "ME";
    d.fillRoundRect(88, 38, 134, 54, 10, panel2);
    d.drawRoundRect(88, 38, 134, 54, 10, note.assistant ? green : cyan);
    d.setTextColor(note.assistant ? green : cyan, panel2);
    d.setCursor(98, 46);
    d.print(who);
    d.setTextColor(TFT_WHITE, panel2);
    d.setCursor(120, 46);
    d.print(marqueeText(note.title, 90, 260));
    d.setTextColor(muted, panel2);
    d.setCursor(98, 61);
    d.print(trimPreview(note.preview, 22));
    d.setCursor(98, 76);
    d.print(String(gSelectedVoiceNote + 1) + "/" + String(gVoiceNotes.size()));

    String timeText = "--:--";
    if (gPlaybackActive) {
      timeText = formatPlaybackMs(playbackRemainingMs());
    }
    int progressW = gPlaybackActive ? static_cast<int>((playbackProgressPct() * 112U) / 100U) : 0;
    d.fillRoundRect(98, 87, 114, 3, 2, rgb565_local(26, 43, 55));
    if (progressW > 0) {
      d.fillRoundRect(98, 87, progressW, 3, 2, green);
    }
    d.setTextColor(gPlaybackActive ? green : amber, panel);
    d.setCursor(184, 96);
    d.print(timeText);
  }

  gVoiceGraphSpeed++;

  int volW = map(gSpeakerVolume, 0, 255, 0, 58);
  d.fillRoundRect(88, 99, 64, 7, 4, rgb565_local(21, 35, 46));
  d.fillRoundRect(91, 101, volW, 3, 2, amber);
  drawTinyFooter(panel, muted, "< > note  Ent play  +/- vol", 15, 120, 210);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderChatUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(3, 7, 13);
  const uint16_t panel = rgb565_local(6, 13, 23);
  const uint16_t userBg = rgb565_local(8, 43, 55);
  const uint16_t aiBg = rgb565_local(11, 42, 31);
  const uint16_t sysBg = rgb565_local(20, 25, 35);
  const uint16_t accent = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(100, 255, 170);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(126, 143, 157);
  d.fillScreen(bg);
  d.setTextSize(1);
  d.setFont(&fonts::Font2);

  bool showFace = gUiMode == UiMode::ChatFace;
  if (showFace) {
    renderFacePanel(false);
  }

  bool showStatus = !gStatusText.isEmpty() && gStatusText != "Ready";
  int inputLines = 1;
  if (gInputBuffer.length() > 36) {
    inputLines = 2;
  }
  if (gInputBuffer.length() > 78) {
    inputLines = 3;
  }
  int composerH = 17 + (inputLines - 1) * 11;
  int composerY = 135 - composerH - 3;
  int transcriptTop = showFace ? 78 : 8;
  int transcriptBottom = composerY - (showStatus ? 14 : 4);

  d.setFont(&fonts::efontCN_12);
  int lineHeight = d.fontHeight() + 1;
  int visibleRows = max<int>(1, (transcriptBottom - transcriptTop) / max<int>(1, lineHeight + 2));
  int y = transcriptTop;
  auto lines = buildVisualLines(showFace ? 216 : 228);
  int maxOffset = max<int>(0, static_cast<int>(lines.size()) - visibleRows);
  if (gChatScrollOffset > maxOffset) {
    gChatScrollOffset = maxOffset;
  }
  int startIndex = max<int>(0, static_cast<int>(lines.size()) - visibleRows - gChatScrollOffset);
  int endIndex = min<int>(static_cast<int>(lines.size()), startIndex + visibleRows);

  if (!showFace) {
    d.fillRoundRect(4, 4, 232, transcriptBottom - 2, 10, panel);
    d.drawRoundRect(4, 4, 232, transcriptBottom - 2, 10, rgb565_local(18, 35, 50));
  }

  for (int i = startIndex; i < endIndex; ++i) {
    LineKind kind = lines[i].kind;
    uint16_t bubble = kind == LineKind::User ? userBg : (kind == LineKind::Assistant ? aiBg : sysBg);
    uint16_t ink = kind == LineKind::User ? accent : (kind == LineKind::Assistant ? green : muted);
    int bx = kind == LineKind::User ? 22 : 8;
    int bw = showFace ? 212 : 224;
    if (kind == LineKind::User && !showFace) {
      bw = 210;
    }
    d.fillRoundRect(bx, y - 1, bw, lineHeight + 1, 4, bubble);
    d.setTextColor(ink, bubble);
    d.setCursor(bx + 5, y);
    d.print(fitCurrentFontToWidth(lines[i].text, bw - 10));
    y += lineHeight + 2;
  }
  if (maxOffset > 0 && !showFace) {
    int railX = 232;
    int railH = max<int>(12, transcriptBottom - transcriptTop - 8);
    int knobH = max<int>(8, railH / (maxOffset + 1));
    int knobY = transcriptTop + 4 + ((railH - knobH) * (maxOffset - gChatScrollOffset)) / max<int>(1, maxOffset);
    d.fillRoundRect(railX, transcriptTop + 4, 3, railH, 2, rgb565_local(24, 35, 48));
    d.fillRoundRect(railX, knobY, 3, knobH, 2, accent);
  } else if (showFace) {
    drawScrollIndicators(gChatScrollOffset < maxOffset, gChatScrollOffset > 0);
  }

  if (showStatus) {
    d.setFont(&fonts::Font0);
    d.setTextColor(amber, bg);
    d.setCursor(8, composerY - 11);
    d.print(fitCurrentFontToWidth(gStatusText, 220));
  }

  d.setFont(&fonts::efontCN_12);
  d.fillRoundRect(6, composerY, 228, composerH, 7, rgb565_local(2, 17, 29));
  d.drawRoundRect(6, composerY, 228, composerH, 7, gInputBuffer.isEmpty() ? rgb565_local(28, 51, 65) : accent);
  d.fillRoundRect(10, composerY + 4, 18, composerH - 8, 5, rgb565_local(4, 32, 45));
  d.setTextColor(gInputBuffer.isEmpty() ? muted : amber, rgb565_local(4, 32, 45));
  d.setCursor(15, composerY + 5);
  d.print(">");
  d.setTextColor(gInputBuffer.isEmpty() ? muted : TFT_WHITE, rgb565_local(2, 17, 29));
  String inputLine = gInputBuffer.isEmpty() ? "type to chat" : tailUtf8ToFit(gInputBuffer, 174);
  d.setCursor(34, composerY + 4);
  d.print(inputLine);
  d.setFont(&fonts::Font0);
  d.setTextColor(accent, rgb565_local(2, 17, 29));
  d.setCursor(204, composerY + composerH - 9);
  d.print("ENT");
  renderTopicOverlayIfVisible();
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderSmartLampUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(7, 18, 25);
  const uint16_t row = rgb565_local(9, 24, 32);
  const uint16_t selected = rgb565_local(12, 37, 45);
  const uint16_t line = rgb565_local(48, 60, 69);
  const uint16_t muted = rgb565_local(130, 145, 155);
  const uint16_t amber = rgb565_local(255, 150, 80);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t red = rgb565_local(255, 93, 88);
  const bool online = gSmartLamp.online;
  const uint16_t statusColor = online ? green : (WiFi.status() == WL_CONNECTED ? amber : red);

  d.fillScreen(bg);
  d.fillRoundRect(5, 5, 230, 125, 7, panel);
  d.drawRoundRect(5, 5, 230, 125, 7, line);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(12, 9);
  d.print("smartLamp");
  d.fillCircle(151, 17, 3, statusColor);
  d.setFont(&fonts::Font0);
  d.setTextColor(statusColor, panel);
  d.setCursor(158, 13);
  d.print(gSmartLamp.status);
  if (gSmartLamp.found && (gSmartLamp.online || gSmartLamp.status == "SENT")) {
    d.setTextColor(muted, panel);
    d.setTextDatum(middle_right);
    d.drawString("..." + String(gSmartLamp.ip[3]), 228, 17);
    d.setTextDatum(top_left);
  }
  d.drawFastHLine(11, 27, 218, rgb565_local(30, 48, 57));

  d.drawCircle(44, 65, 29, rgb565_local(86, 48, 31));
  d.drawArc(44, 65, 29, 25, 220, 140, gSmartLamp.power ? amber : muted);
  d.fillRoundRect(42, 36, 4, 18, 2, gSmartLamp.power ? TFT_WHITE : muted);
  d.setTextDatum(middle_center);
  d.setFont(&fonts::Font2);
  d.setTextColor(gSmartLamp.power ? TFT_WHITE : muted, panel);
  d.drawString(gSmartLamp.power ? "ON" : "OFF", 44, 66);
  d.setFont(&fonts::Font0);
  d.setTextColor(amber, panel);
  d.drawString("EFFECT " + String(gSmartLamp.effect), 44, 99);
  d.setTextDatum(top_left);

  const char* labels[kSmartLampControlCount] = {"EFFECT", "BRIGHT", "SPEED", "SCALE"};
  const int values[kSmartLampControlCount] = {
      gSmartLamp.effect, gSmartLamp.brightness, gSmartLamp.speed, gSmartLamp.scale};
  const int maximum[kSmartLampControlCount] = {kSmartLampEffectCount - 1, 255, 255, 100};
  for (uint8_t i = 0; i < kSmartLampControlCount; ++i) {
    int y = 31 + i * 20;
    bool active = i == gSmartLamp.selected;
    uint16_t fill = active ? selected : row;
    d.fillRoundRect(81, y, 146, 17, 5, fill);
    d.drawRoundRect(81, y, 146, 17, 5, active ? cyan : rgb565_local(36, 51, 59));
    d.setFont(&fonts::Font0);
    d.setTextColor(active ? TFT_WHITE : muted, fill);
    d.setCursor(88, y + 4);
    d.print(labels[i]);
    d.setTextDatum(middle_right);
    d.setTextColor(active ? cyan : TFT_WHITE, fill);
    d.drawString(String(values[i]), 218, y + 7);
    d.setTextDatum(top_left);
    if (i > 0) {
      int progress = clampValue<int>((values[i] * 130) / max<int>(1, maximum[i]), 0, 130);
      d.fillRoundRect(88, y + 12, 130, 3, 2, rgb565_local(23, 36, 43));
      if (progress > 0) {
        d.fillRoundRect(88, y + 12, progress, 3, 2, active ? cyan : amber);
      }
    }
  }

  drawTinyFooter(panel, muted, "^v select   <> change   Enter power", 19, 118, 202);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderStatusBar() {
  auto& d = gCanvas;
  d.fillRect(0, 126, 240, 9, 0x18C3);
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_LIGHTGREY, 0x18C3);
  d.setCursor(4, 127);
  d.print(fitCurrentFontToWidth(gStatusText, 230));
  d.setFont(&fonts::Font2);
}

void renderSetupPortalUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(4, 13, 18);
  const uint16_t panel = rgb565_local(8, 28, 36);
  const uint16_t accent = rgb565_local(33, 199, 223);
  const uint16_t ok = rgb565_local(132, 245, 174);
  d.fillSprite(bg);
  d.fillRoundRect(8, 8, 224, 118, 6, panel);
  d.drawRoundRect(8, 8, 224, 118, 6, accent);
  d.setTextDatum(TL_DATUM);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(18, 16);
  d.print("OpenClaw setup");
  d.setFont(&fonts::Font0);
  d.setTextColor(rgb565_local(180, 222, 236), panel);
  d.setCursor(18, 36);
  d.print("Connect phone/laptop to AP");
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(18, 52);
  d.print(fitCurrentFontToWidth(gSetupPortalSsid, 204));
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(18, 72);
  d.print("pass: ");
  d.setTextColor(ok, panel);
  d.print(kSetupPortalPassword);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(18, 92);
  d.print("open: ");
  d.setTextColor(ok, panel);
  d.print("192.168.4.1");
  d.setFont(&fonts::Font0);
  d.setTextColor(rgb565_local(132, 157, 171), panel);
  d.setCursor(18, 112);
  d.print(gSetupPortalSaved ? "saved, restarting" : "Wi-Fi + bridge credentials");
  d.pushSprite(0, 0);
}

void render() {
  if (!gCanvasSpriteActive) {
    return;
  }
  uint32_t renderStartMs = millis();
  if (gStatusUntilMs != 0 && millis() > gStatusUntilMs) {
    gStatusUntilMs = 0;
    if (gRecording) {
      gStatusText = "Listening...";
    } else if (gPlaybackActive) {
      gStatusText = "Speaking...";
    } else {
      gStatusText = "Ready";
    }
  }
  if (gSetupPortalActive) {
    renderSetupPortalUi();
    return;
  }
