
void clampNoteSelection() {
  if (gNotes.empty()) {
    gSelectedNote = 0;
    gNoteListScrollOffset = 0;
    return;
  }
  gSelectedNote = clampValue<int>(gSelectedNote, 0, static_cast<int>(gNotes.size()) - 1);
}

void scanNotes() {
  gNotes.clear();
  fs::FS* fs = activeNotesFs();
  if (!fs || !ensureNotesStorage()) {
    gNotesStatus = "Storage unavailable";
    gNotesScanned = true;
    return;
  }
  File dir = fs->open(kNotesDir);
  if (!dir || !dir.isDirectory()) {
    gNotesStatus = "Notes dir unavailable";
    gNotesScanned = true;
    return;
  }
  while (gNotes.size() < kMaxNotes) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }
    if (!file.isDirectory()) {
      String path = file.name();
      if (!path.startsWith("/")) {
        path = String(kNotesDir) + "/" + path;
      }
      String name = basenameOfPath(path);
      if (isNoteFileName(name)) {
        NoteFile note;
        note.name = name;
        note.path = path;
        note.size = static_cast<uint32_t>(file.size());
        note.lineCount = countNoteLines(*fs, path);
        gNotes.push_back(note);
      }
    }
    file.close();
  }
  dir.close();
  std::sort(gNotes.begin(), gNotes.end(), [](const NoteFile& a, const NoteFile& b) {
    return a.name > b.name;
  });
  clampNoteSelection();
  gNotesStatus = String(gNotes.size()) + " notes on " + activeNotesStorageLabel();
  gNotesScanned = true;
}

bool createNewNote(const String& titleHint = "") {
  fs::FS* fs = activeNotesFs();
  if (!fs || !ensureNotesStorage()) {
    gNotesStatus = "Storage unavailable";
    setStatus("Notes storage unavailable", 1200);
    return false;
  }
  String name = makeNoteFileName(titleHint);
  String path = String(kNotesDir) + "/" + name;
  int suffix = 2;
  while (fsExists(*fs, path) && suffix < 50) {
    name = makeNoteFileName(titleHint);
    name.replace(".md", "-" + String(suffix++) + ".md");
    path = String(kNotesDir) + "/" + name;
  }
  File file = fs->open(path, "w");
  if (!file) {
    gNotesStatus = "Create failed";
    setStatus("Note create failed", 1200);
    return false;
  }
  file.println(String("# ") + name);
  file.println();
  file.close();
  gActiveNotePath = path;
  gActiveNoteName = name;
  gNoteInputBuffer = "";
  gNoteScrollOffset = 0;
  gNotesMode = NotesMode::Edit;
  scanNotes();
  for (int i = 0; i < static_cast<int>(gNotes.size()); ++i) {
    if (gNotes[i].path == path) {
      gSelectedNote = i;
      break;
    }
  }
  gNotesStatus = "Editing new note";
  setStatus("New note", 900);
  return true;
}

bool openSelectedNote(NotesMode mode) {
  if (gNotes.empty()) {
    return createNewNote();
  }
  clampNoteSelection();
  gActiveNotePath = gNotes[gSelectedNote].path;
  gActiveNoteName = gNotes[gSelectedNote].name;
  gNoteScrollOffset = 0;
  gNoteInputBuffer = "";
  gNotesMode = mode;
  gNotesStatus = mode == NotesMode::Edit ? "Append mode" : "Viewing";
  return true;
}

bool appendLineToActiveNote(const String& line) {
  fs::FS* fs = activeNotesFs();
  if (!fs || gActiveNotePath.isEmpty()) {
    gNotesStatus = "No note open";
    return false;
  }
  File file = fs->open(gActiveNotePath, "a");
  if (!file) {
    gNotesStatus = "Save failed";
    setStatus("Note save failed", 1200);
    return false;
  }
  file.println(line);
  file.close();
  gNotesStatus = "Saved line";
  setStatus("Note saved", 700);
  scanNotes();
  for (int i = 0; i < static_cast<int>(gNotes.size()); ++i) {
    if (gNotes[i].path == gActiveNotePath) {
      gSelectedNote = i;
      break;
    }
  }
  return true;
}

bool isEmojiAssetCodepoint(uint32_t codepoint) {
  return codepoint == 0x00A9 || codepoint == 0x00AE || codepoint == 0x203C ||
         codepoint == 0x2049 || codepoint == 0x2122 || codepoint == 0x2139 ||
         (codepoint >= 0x2194 && codepoint <= 0x21AA) ||
         (codepoint >= 0x2300 && codepoint <= 0x23FF) ||
         (codepoint >= 0x2460 && codepoint <= 0x27BF) ||
         (codepoint >= 0x2900 && codepoint <= 0x2BFF) ||
         (codepoint >= 0x3000 && codepoint <= 0x32FF) ||
         (codepoint >= 0x1F000 && codepoint <= 0x1FAFF) ||
         (codepoint >= 0xFE000 && codepoint <= 0xFFFFD);
}

void releaseEmojiCacheEntry(EmojiCacheEntry& entry) {
  if (entry.data) {
    free(entry.data);
  }
  entry = EmojiCacheEntry{};
}

void clearEmojiCache() {
  for (uint8_t i = 0; i < gEmojiCacheSize; ++i) {
    releaseEmojiCacheEntry(gEmojiCache[i]);
  }
  gEmojiCacheSize = 0;
}

const EmojiCacheEntry* rememberEmojiEntry(const EmojiCacheEntry& entry) {
  if (gEmojiCacheSize < kEmojiCacheCap) {
    gEmojiCache[gEmojiCacheSize] = entry;
    return &gEmojiCache[gEmojiCacheSize++];
  }
  releaseEmojiCacheEntry(gEmojiCache[0]);
  memmove(&gEmojiCache[0], &gEmojiCache[1], sizeof(gEmojiCache[0]) * (kEmojiCacheCap - 1));
  gEmojiCache[kEmojiCacheCap - 1] = entry;
  return &gEmojiCache[kEmojiCacheCap - 1];
}

const EmojiCacheEntry* emojiCacheLookup(uint32_t codepoint) {
  for (uint8_t i = 0; i < gEmojiCacheSize; ++i) {
    if (gEmojiCache[i].codepoint == codepoint) {
      return &gEmojiCache[i];
    }
  }

  EmojiCacheEntry entry;
  entry.codepoint = codepoint;
  if (!gSdReady || !isEmojiAssetCodepoint(codepoint)) {
    return rememberEmojiEntry(entry);
  }

  char path[40];
  snprintf(path, sizeof(path), "%s/u%lX.png", kEmojiDir, static_cast<unsigned long>(codepoint));
  File file = SD.open(path, "r");
  if (!file) {
    snprintf(path, sizeof(path), "%s/u%lx.png", kEmojiDir, static_cast<unsigned long>(codepoint));
    file = SD.open(path, "r");
  }
  if (!file) {
    return rememberEmojiEntry(entry);
  }

  size_t size = file.size();
  if (size > 24 && size <= kEmojiMaxPngBytes) {
    uint8_t* data = static_cast<uint8_t*>(malloc(size));
    if (data && file.read(data, size) == size &&
        memcmp(data, "\x89PNG\r\n\x1A\n", 8) == 0) {
      uint32_t width = (static_cast<uint32_t>(data[16]) << 24) |
                       (static_cast<uint32_t>(data[17]) << 16) |
                       (static_cast<uint32_t>(data[18]) << 8) | data[19];
      uint32_t height = (static_cast<uint32_t>(data[20]) << 24) |
                        (static_cast<uint32_t>(data[21]) << 16) |
                        (static_cast<uint32_t>(data[22]) << 8) | data[23];
      if (width > 0 && width <= 240 && height > 0 && height <= 240) {
        entry.data = data;
        entry.len = static_cast<uint32_t>(size);
        entry.width = static_cast<int16_t>(width);
        entry.height = static_cast<int16_t>(height);
        data = nullptr;
      }
    }
    if (data) {
      free(data);
    }
  }
  file.close();
  return rememberEmojiEntry(entry);
}

int32_t drawEmojiFromSd(lgfx::LGFXBase* gfx, int32_t x, int32_t y, uint32_t codepoint,
                        int32_t fontHeight) {
  const EmojiCacheEntry* entry = emojiCacheLookup(codepoint);
  if (!gfx || !entry || !entry->data || entry->height <= 0 || fontHeight <= 0) {
    return 0;
  }

  float scale = static_cast<float>(fontHeight) / static_cast<float>(entry->height);
  int32_t drawY = y - static_cast<int32_t>((fontHeight * 90) / 100);
  if (!gfx->drawPng(entry->data, entry->len, x, drawY, 0, 0, 0, 0, scale, 0)) {
    return 0;
  }
  return max<int32_t>(1, static_cast<int32_t>(entry->width * scale));
}

void configureEmojiRendering() {
  M5Cardputer.Display.setEmojiCallback(drawEmojiFromSd);
  gCanvas.setEmojiCallback(drawEmojiFromSd);
}
