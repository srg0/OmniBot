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

void renderAudioFoldersUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Audio Folders");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(6, 16, 22);
  const uint16_t rowBg = rgb565_local(7, 17, 22);
  const uint16_t line = rgb565_local(39, 51, 58);
  const uint16_t accent = rgb565_local(34, 209, 189);
  const uint16_t selected = rgb565_local(12, 32, 34);
  const uint16_t muted = rgb565_local(130, 145, 155);
  d.fillScreen(bg);
  d.fillRoundRect(5, 5, 230, 125, 6, panel);
  d.drawRoundRect(5, 5, 230, 125, 6, rgb565_local(48, 54, 61));

  if (gAudioFolders.empty()) {
    rebuildAudioFolders();
  }
  gSelectedAudioFolder = clampValue<int>(gSelectedAudioFolder, 0, max<int>(0, static_cast<int>(gAudioFolders.size()) - 1));

  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(15, 14);
  d.print("Meditation folders");
  d.setTextColor(accent, panel);
  d.setCursor(196, 14);
  d.print(gSdReady ? "SD" : "FS");

  d.fillCircle(44, 68, 36, bg);
  d.drawCircle(44, 68, 36, rgb565_local(23, 77, 90));
  d.drawArc(44, 68, 28, 23, 220, 38, accent);
  d.drawArc(44, 68, 17, 13, 35, 300, rgb565_local(47, 227, 255));
  d.fillCircle(44, 68, 9, TFT_WHITE);
  d.setTextColor(bg, TFT_WHITE);
  d.setCursor(38, 64);
  d.print("DIR");

  int first = gSelectedAudioFolder - 1;
  first = clampValue<int>(first, 0, max<int>(0, static_cast<int>(gAudioFolders.size()) - 4));
  for (int r = 0; r < 4; ++r) {
    int idx = first + r;
    if (idx >= static_cast<int>(gAudioFolders.size())) {
      break;
    }
    const AudioFolder& folder = gAudioFolders[idx];
    bool isSelected = idx == gSelectedAudioFolder;
    int y = 35 + r * 20;
    uint16_t fill = isSelected ? selected : rowBg;
    d.fillRoundRect(84, y, 137, 17, 6, fill);
    d.drawRoundRect(84, y, 137, 17, 6, isSelected ? accent : line);
    d.setTextColor(isSelected ? TFT_WHITE : TFT_LIGHTGREY, fill);
    d.setCursor(92, y + 3);
    d.print(trimPreview(folder.label, 13));
    String count = String(folder.count);
    d.setTextColor(isSelected ? accent : muted, fill);
    d.setCursor(214 - d.textWidth(count), y + 3);
    d.print(count);
  }

  drawTinyFooter(panel, muted, "< > folder  Ent open  R scan", 15, 118, 210);
  renderLauncherOverlay();
  renderDebugOverlay();
}

String readNoteLineAt(fs::FS& fs, const String& path, uint32_t targetLine) {
  File file = fs.open(path, "r");
  if (!file) {
    return "";
  }
  uint32_t line = 0;
  String value;
  while (file.available()) {
    value = file.readStringUntil('\n');
    value.trim();
    if (line == targetLine) {
      file.close();
      return value;
    }
    ++line;
  }
  file.close();
  return "";
}

void drawNotesHeader(const String& title, uint16_t accent, uint16_t bg, uint16_t panel) {
  auto& d = gCanvas;
  d.fillScreen(bg);
  d.fillRoundRect(5, 5, 230, 125, 8, panel);
  d.drawRoundRect(5, 5, 230, 125, 8, rgb565_local(42, 55, 63));
  d.fillRoundRect(10, 10, 220, 17, 5, rgb565_local(3, 11, 17));
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, rgb565_local(3, 11, 17));
  d.setCursor(16, 14);
  d.print(fitCurrentFontToWidth(title, 148));
  String store = activeNotesStorageLabel();
  d.setTextColor(TFT_LIGHTGREY, rgb565_local(3, 11, 17));
  d.setCursor(224 - d.textWidth(store), 14);
  d.print(store);
}

