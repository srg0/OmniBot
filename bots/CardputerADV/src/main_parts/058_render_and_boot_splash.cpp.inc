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

void renderBleUi() {
  auto& d = gCanvas;
  d.fillScreen(TFT_BLACK);
  d.setFont(&fonts::Font2);
  d.setTextSize(2);
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  d.setCursor(18, 18);
  d.print("BT SETUP");

  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setCursor(10, 52);
  d.print("Open OmniBot hub");
  d.setCursor(10, 64);
  d.print("Add New Bot over BLE");
  d.setCursor(10, 88);
  d.print("Sends Wi-Fi + endpoint.");
  d.setCursor(10, 100);
  d.print("After provisioning, device reboots.");
  d.setFont(&fonts::Font0);
  d.setCursor(10, 124);
  d.print("Use /clearwifi later to reset");
  d.setFont(&fonts::Font2);
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

void render() {
  if (gStatusUntilMs != 0 && millis() > gStatusUntilMs) {
    gStatusUntilMs = 0;
    if (gBleActive) {
      gStatusText = "Waiting for BLE provisioning";
    } else if (gRecording) {
      gStatusText = "Listening...";
    } else if (gPlaybackActive) {
      gStatusText = "Speaking...";
    } else {
      gStatusText = "Ready";
    }
  }

  if (gBleActive) {
    renderBleUi();
    renderStatusBar();
  } else {
    switch (gUiMode) {
      case UiMode::Home:
        renderAssistantPulseUi();
        break;
      case UiMode::Face:
        renderFaceOnlyUi();
        break;
      case UiMode::Hero:
        renderHeroUi();
        break;
      case UiMode::Clock:
        renderClockUi();
        break;
      case UiMode::Battery:
        renderBatteryUi();
        break;
      case UiMode::Voice:
        renderVoicePlayerUi();
        break;
      case UiMode::Focus:
        renderFocusUi();
        break;
      case UiMode::Library:
        renderLibraryUi();
        break;
      case UiMode::AudioFolders:
        renderAudioFoldersUi();
        break;
      case UiMode::Notes:
        renderNotesUi();
        break;
      case UiMode::Assets:
