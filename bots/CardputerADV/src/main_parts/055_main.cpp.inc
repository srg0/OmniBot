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

