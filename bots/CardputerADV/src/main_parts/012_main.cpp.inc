String sanitizeNoteTitle(String value) {
  value.trim();
  value.toLowerCase();
  String out;
  out.reserve(32);
  for (int i = 0; i < value.length() && out.length() < 28; ++i) {
    char c = value[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      out += c;
    } else if ((c == ' ' || c == '-' || c == '_' || c == '.') && !out.endsWith("-")) {
      out += '-';
    }
  }
  while (out.endsWith("-")) {
    out.remove(out.length() - 1);
  }
  return out.isEmpty() ? "note" : out;
}

String makeNoteFileName(const String& titleHint = "") {
  String stem = sanitizeNoteTitle(titleHint);
  char stamp[24];
  if (hasValidSystemTime()) {
    time_t now = time(nullptr);
    struct tm localNow;
    localtime_r(&now, &localNow);
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &localNow);
  } else {
    snprintf(stamp, sizeof(stamp), "%lu", static_cast<unsigned long>(millis() / 1000UL));
  }
  return stem + "-" + String(stamp) + ".md";
}

uint32_t countNoteLines(fs::FS& fs, const String& path) {
  File file = fs.open(path, "r");
  if (!file) {
    return 0;
  }
  uint32_t lines = 0;
  bool hasChars = false;
  while (file.available()) {
    char c = static_cast<char>(file.read());
    hasChars = true;
    if (c == '\n') {
      ++lines;
    }
  }
  file.close();
  return lines + (hasChars ? 1 : 0);
}

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

