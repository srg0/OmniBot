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

void renderLibraryUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Audio Library");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(6, 16, 22);
  const uint16_t line = rgb565_local(34, 78, 88);
  const uint16_t accent = rgb565_local(88, 240, 141);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t amber = rgb565_local(246, 194, 74);
  const uint16_t muted = rgb565_local(130, 145, 155);
  d.fillScreen(bg);
  d.fillRoundRect(4, 4, 232, 127, 8, panel);
  d.drawRoundRect(4, 4, 232, 127, 8, rgb565_local(48, 54, 61));

  int folderCount = audioAssetCountForCurrentFolder();
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(13, 12);
  d.print("AUDIO");
  String folder = audioFolderLabel(gAudioFolderFilter);
  String folderLabel = trimPreview(folder, 14);
  d.setTextColor(muted, panel);
  d.setCursor(226 - d.textWidth(folderLabel), 12);
  d.print(folderLabel);

  if (gAudioAssets.empty() || folderCount == 0) {
    d.drawRoundRect(20, 38, 54, 54, 27, line);
    d.drawArc(47, 65, 21, 15, 220, 35, accent);
    d.setTextColor(accent, panel);
    d.setCursor(88, 44);
    d.print("No WAV assets");
    d.setTextColor(muted, panel);
    d.setCursor(88, 60);
    d.print("Put files in /audio");
    d.setCursor(88, 76);
    d.print("or /pomodoro/audio");
    d.setCursor(88, 92);
    d.print("Press R to rescan");
  } else {
    clampSelectedAudioAssetToFolder();
    const AudioAsset& asset = gAudioAssets[gSelectedAudioAsset];
    int rank = max<int>(0, audioAssetRankInCurrentFolder(gSelectedAudioAsset));
    int cx = 48;
    int cy = 66;
    d.fillCircle(cx, cy, 35, bg);
    d.drawCircle(cx, cy, 35, line);
    d.drawCircle(cx, cy, 26, rgb565_local(14, 41, 48));
    d.drawArc(cx, cy, 29, 23, 210, 55, gPlaybackActive ? accent : cyan);
    d.drawArc(cx, cy, 18, 13, 25, 295, amber);
    d.fillCircle(cx, cy, 9 + ((gPlaybackActive && ((millis() / 260) % 2)) ? 1 : 0), TFT_WHITE);
    if (gPlaybackActive) {
      for (int i = 0; i < 9; ++i) {
        int h = 4 + ((i * 9 + millis() / 70) % 18);
        d.fillRoundRect(17 + i * 7, 112 - h, 4, h, 2, i % 2 ? accent : cyan);
      }
    }

    d.setFont(&fonts::Font2);
    d.setTextColor(TFT_WHITE, panel);
    d.setCursor(92, 34);
    d.print(marqueeText(asset.title, 126, 260));
    d.setTextColor(muted, panel);
    d.setCursor(92, 49);
    d.print(trimPreview(asset.category, 13));
    d.setTextColor(accent, panel);
    String countText = String(rank + 1) + "/" + String(folderCount);
    d.setCursor(224 - d.textWidth(countText), 49);
    d.print(countText);

    d.drawRoundRect(92, 64, 132, 8, 4, line);
    int progress = 18;
    if (gPlaybackActive && gPlaybackStreamPath == asset.path && gPlaybackStreamTotalBytes > 0) {
      size_t played = gPlaybackStreamTotalBytes > gPlaybackStreamRemainingBytes
                          ? gPlaybackStreamTotalBytes - gPlaybackStreamRemainingBytes
                          : 0;
      progress = static_cast<int>((played * 126ULL) / gPlaybackStreamTotalBytes);
      progress = max<int>(4, min<int>(126, progress));
    } else if (gPlaybackActive) {
      progress = 10 + ((millis() / 180) % 28);
    }
    d.fillRoundRect(95, 67, min<int>(126, progress), 3, 2, gPlaybackActive ? accent : muted);

    d.fillRoundRect(92, 82, 33, 22, 6, bg);
    d.drawRoundRect(92, 82, 33, 22, 6, line);
    d.fillRoundRect(132, 79, 48, 27, 8, bg);
    d.drawRoundRect(132, 79, 48, 27, 8, gPlaybackActive ? accent : cyan);
    d.fillRoundRect(187, 82, 33, 22, 6, bg);
    d.drawRoundRect(187, 82, 33, 22, 6, line);
    d.setTextColor(muted, bg);
    d.setCursor(102, 89);
    d.print("<<");
    d.setTextColor(gPlaybackActive ? accent : cyan, bg);
    d.setCursor(148, 88);
    d.print(gPlaybackActive ? "PAU" : "PLAY");
    d.setTextColor(muted, bg);
    d.setCursor(197, 89);
    d.print(">>");
  }
  drawTinyFooter(panel, muted, "< > track  Ent play  F folder  R scan", 14, 118, 210);
  renderLauncherOverlay();
  renderDebugOverlay();
}

