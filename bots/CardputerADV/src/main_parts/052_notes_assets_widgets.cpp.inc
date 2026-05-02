void renderNotesUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(5, 13, 19);
  const uint16_t rowBg = rgb565_local(7, 18, 25);
  const uint16_t selected = rgb565_local(14, 40, 48);
  const uint16_t line = rgb565_local(35, 52, 62);
  const uint16_t accent = rgb565_local(255, 211, 106);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(100, 255, 170);
  const uint16_t muted = rgb565_local(132, 148, 160);

  if (!gNotesScanned) {
    scanNotes();
  }
  if (!activeNotesFs()) {
    renderSdRequiredBanner("Notes");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }

  if (gNotesMode == NotesMode::List) {
    drawNotesHeader("Notes", accent, bg, panel);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(16, 31);
    d.print(fitCurrentFontToWidth(gNotesStatus, 208));

    if (gNotes.empty()) {
      d.fillRoundRect(20, 48, 200, 42, 9, rowBg);
      d.drawRoundRect(20, 48, 200, 42, 9, accent);
      d.setFont(&fonts::Font2);
      d.setTextColor(accent, rowBg);
      d.setCursor(54, 61);
      d.print("No notes yet");
      d.setFont(&fonts::Font0);
      d.setTextColor(muted, rowBg);
      d.setCursor(44, 77);
      d.print("Press N to create first note");
      drawTinyFooter(panel, muted, "N new  R rescan  Tab menu", 16, 118, 208);
      renderLauncherOverlay();
      renderDebugOverlay();
      return;
    }

    clampNoteSelection();
    constexpr int visible = 6;
    int maxStart = max<int>(0, static_cast<int>(gNotes.size()) - visible);
    int start = clampValue<int>(min<int>(gNoteListScrollOffset, gSelectedNote), 0, maxStart);
    if (gSelectedNote < start) {
      start = gSelectedNote;
    } else if (gSelectedNote >= start + visible) {
      start = gSelectedNote - visible + 1;
    }
    gNoteListScrollOffset = start;
    for (int i = 0; i < visible; ++i) {
      int idx = start + i;
      if (idx >= static_cast<int>(gNotes.size())) {
        break;
      }
      const NoteFile& note = gNotes[idx];
      bool isSelected = idx == gSelectedNote;
      int y = 43 + i * 12;
      uint16_t fill = isSelected ? selected : rowBg;
      d.fillRoundRect(14, y, 212, 10, 4, fill);
      d.drawRoundRect(14, y, 212, 10, 4, isSelected ? accent : line);
      d.setTextColor(isSelected ? TFT_WHITE : TFT_LIGHTGREY, fill);
      d.setCursor(20, y + 2);
      d.print(fitCurrentFontToWidth(note.name, 145));
      String meta = String(note.lineCount) + "l " + compactBytes(note.size);
      d.setTextColor(isSelected ? accent : muted, fill);
      d.setCursor(222 - d.textWidth(meta), y + 2);
      d.print(meta);
    }
    drawTinyFooter(panel, muted, "< > pick  Ent open  N new  E edit", 13, 118, 214);
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }

  const bool edit = gNotesMode == NotesMode::Edit;
  drawNotesHeader(edit ? "Edit Note" : "Read Note", edit ? green : cyan, bg, panel);
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(16, 31);
  d.print(fitCurrentFontToWidth(gActiveNoteName.isEmpty() ? "untitled" : gActiveNoteName, 150));
  d.setTextColor(edit ? green : cyan, panel);
  String mode = edit ? "APPEND" : "VIEW";
  d.setCursor(224 - d.textWidth(mode), 31);
  d.print(mode);

  fs::FS* fs = activeNotesFs();
  uint32_t lines = 0;
  if (fs && !gActiveNotePath.isEmpty()) {
    lines = countNoteLines(*fs, gActiveNotePath);
  }
  int previewRows = edit ? 3 : 5;
  int maxScroll = max<int>(0, static_cast<int>(lines) - previewRows);
  gNoteScrollOffset = clampValue<int>(gNoteScrollOffset, 0, maxScroll);
  int startLine = edit ? max<int>(0, static_cast<int>(lines) - previewRows) : gNoteScrollOffset;

  d.fillRoundRect(12, 43, 216, edit ? 49 : 71, 7, rgb565_local(3, 10, 15));
  d.drawRoundRect(12, 43, 216, edit ? 49 : 71, 7, rgb565_local(22, 40, 52));
  d.setFont(&fonts::efontCN_12);
  int y = 47;
  for (int i = 0; i < previewRows; ++i) {
    uint32_t lineNo = static_cast<uint32_t>(startLine + i);
    if (lineNo >= lines) {
      break;
    }
    String lineText = fs ? readNoteLineAt(*fs, gActiveNotePath, lineNo) : "";
    d.setTextColor(i % 2 == 0 ? TFT_LIGHTGREY : muted, rgb565_local(3, 10, 15));
    d.setCursor(18, y);
    d.print(fitCurrentFontToWidth(lineText, 202));
    y += 13;
  }
  d.setFont(&fonts::Font0);
  if (!edit && maxScroll > 0) {
    int railH = 64;
    int knobH = max<int>(8, railH / (maxScroll + 1));
    int knobY = 47 + ((railH - knobH) * gNoteScrollOffset) / max<int>(1, maxScroll);
    d.fillRoundRect(224, 48, 2, railH, 1, rgb565_local(23, 32, 42));
    d.fillRoundRect(224, knobY, 2, knobH, 1, cyan);
  }

  if (edit) {
    d.fillRoundRect(12, 98, 216, 18, 7, rgb565_local(2, 17, 22));
    d.drawRoundRect(12, 98, 216, 18, 7, gNoteInputBuffer.isEmpty() ? line : green);
    d.setTextColor(gNoteInputBuffer.isEmpty() ? muted : TFT_WHITE, rgb565_local(2, 17, 22));
    d.setFont(&fonts::efontCN_12);
    String input = gNoteInputBuffer.isEmpty() ? "type note line..." : tailUtf8ToFit(gNoteInputBuffer, 190);
    d.setCursor(20, 101);
    d.print(input);
    d.setFont(&fonts::Font0);
    d.setTextColor(green, panel);
    d.setCursor(16, 119);
    d.print("Enter save line  Ctrl+S done");
  } else {
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(16, 119);
    d.print("< > scroll  E append  L list");
  }
  renderLauncherOverlay();
  renderDebugOverlay();
}

void drawServiceOrb(int16_t cx, int16_t cy, uint16_t accent, uint16_t secondary, const String& label, bool active) {
  auto& d = gCanvas;
  uint32_t now = millis();
  float t = now / 1000.0f;
  int pulse = active ? static_cast<int>((sinf(t * 4.0f) + 1.0f) * 2.0f) : 0;
  d.fillCircle(cx, cy, 37, rgb565_local(3, 12, 18));
  d.drawCircle(cx, cy, 34 + pulse, rgb565_local(18, 45, 58));
  d.drawArc(cx, cy, 31, 25, static_cast<int>(t * 42.0f) % 360,
            (static_cast<int>(t * 42.0f) + 250) % 360, accent);
  d.drawArc(cx, cy, 20, 15, (220 - static_cast<int>(t * 64.0f)) % 360,
            (40 - static_cast<int>(t * 64.0f)) % 360, secondary);
  d.fillCircle(cx, cy, 12, active ? accent : rgb565_local(13, 31, 40));
  d.setFont(&fonts::Font0);
  d.setTextColor(active ? rgb565_local(2, 6, 12) : accent, active ? accent : rgb565_local(13, 31, 40));
  d.setCursor(cx - d.textWidth(label) / 2, cy - 3);
  d.print(label);
  d.setFont(&fonts::Font2);
}

void drawInfoRow(int16_t x, int16_t y, int16_t w, const String& label, const String& value,
                 uint16_t bg, uint16_t labelColor, uint16_t valueColor) {
  auto& d = gCanvas;
  d.fillRoundRect(x, y, w, 12, 4, bg);
  d.setFont(&fonts::Font0);
  d.setTextColor(labelColor, bg);
  d.setCursor(x + 6, y + 3);
  d.print(fitCurrentFontToWidth(label, 52));
  String fitted = fitCurrentFontToWidth(value, w - 68);
  d.setTextColor(valueColor, bg);
  d.setCursor(x + w - 7 - d.textWidth(fitted), y + 3);
  d.print(fitted);
  d.setFont(&fonts::Font2);
}

void drawActionChip(int16_t x, int16_t y, int16_t w, const String& key, const String& label,
                    uint16_t accent, uint16_t bg) {
  auto& d = gCanvas;
  d.fillRoundRect(x, y, w, 14, 5, bg);
  d.drawRoundRect(x, y, w, 14, 5, accent);
  d.setFont(&fonts::Font0);
  d.setTextColor(accent, bg);
  d.setCursor(x + 5, y + 4);
  d.print(key);
  d.setTextColor(TFT_LIGHTGREY, bg);
  d.setCursor(x + 18, y + 4);
  d.print(fitCurrentFontToWidth(label, w - 23));
  d.setFont(&fonts::Font2);
}

void renderAssetsUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Asset Sync");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  auto& d = gCanvas;
  fs::FS* fs = activeVoiceFs();
  const bool indexReady = fs && fsExists(*fs, kAudioIndexPath);
  const bool ready = gAssetManifest.parsed && gAssetManifest.fileCount > 0;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(7, 14, 24);
  const uint16_t panel2 = rgb565_local(10, 24, 34);
  const uint16_t row = rgb565_local(13, 28, 40);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(126, 143, 157);

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 14, panel);
  d.drawRoundRect(6, 5, 228, 124, 14, ready ? green : amber);
  d.drawRoundRect(10, 9, 220, 116, 11, rgb565_local(18, 34, 48));

  d.setFont(&fonts::Font2);
  d.setTextColor(green, panel);
  d.setCursor(16, 14);
  d.print("ASSET SYNC");
  d.setFont(&fonts::Font0);
  String state = ready ? "READY" : (gAssetManifest.present ? "CHECK" : "FETCH");
  d.setTextColor(ready ? green : amber, panel);
  d.setCursor(224 - d.textWidth(state), 17);
  d.print(state);

  drawServiceOrb(48, 66, green, cyan, "AS", ready);

  int x = 91;
  drawInfoRow(x, 34, 128, "manifest", gAssetManifest.present ? (gAssetManifest.parsed ? "ok" : "bad") : "missing",
              row, muted, ready ? green : amber);
  drawInfoRow(x, 49, 128, "version", gAssetManifest.version.isEmpty() ? "-" : gAssetManifest.version,
              row, muted, TFT_WHITE);
  drawInfoRow(x, 64, 128, "files", String(gAssetManifest.fileCount) + " / " + compactBytes(gAssetManifest.totalBytes),
              row, muted, cyan);
  drawInfoRow(x, 79, 128, "storage", String(gStorageReady ? gStorageLabel : "off") + (indexReady ? " index" : ""),
              row, muted, gStorageReady ? green : amber);

  if (!gAssetManifest.error.isEmpty()) {
    d.setFont(&fonts::Font0);
    d.setTextColor(amber, panel);
    d.setCursor(18, 99);
    d.print(fitCurrentFontToWidth(gAssetManifest.error, 202));
  } else {
    int fillW = ready ? min<int>(128, max<int>(12, gAssetManifest.fileCount * 128 / max<int>(1, gAssetManifest.fileCount))) : 24 + ((millis() / 90) % 82);
    d.fillRoundRect(91, 100, 128, 7, 4, rgb565_local(19, 35, 46));
    d.fillRoundRect(92, 101, fillW, 5, 3, ready ? green : amber);
  }

  drawActionChip(17, 112, 58, "F", "fetch", green, panel2);
  drawActionChip(81, 112, 58, "S", "sync", cyan, panel2);
  drawActionChip(145, 112, 58, "R", "load", amber, panel2);
  renderLauncherOverlay();
  renderDebugOverlay();
}
