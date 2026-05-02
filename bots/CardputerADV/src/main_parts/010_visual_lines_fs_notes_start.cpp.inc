
std::vector<VisualLine> buildVisualLines(int32_t pixelLimit) {
  std::vector<VisualLine> lines;
  for (const auto& entry : gChatLines) {
    String full = entry.prefix + ": " + normalizeEmojiDisplayText(entry.text);
    auto wrapped = wrapText(full, pixelLimit);
    for (const auto& part : wrapped) {
      lines.push_back({part, entry.kind});
    }
  }
  if (gAssistantPendingVisible) {
    String dots = ".";
    uint8_t dotCount = 1 + static_cast<uint8_t>((millis() / 350) % 3);
    while (dots.length() < dotCount) {
      dots += '.';
    }
    auto pendingWrapped = wrapText("AI: " + dots, pixelLimit);
    for (const auto& part : pendingWrapped) {
      lines.push_back({part, LineKind::Assistant});
    }
  }
  return lines;
}

void scrollChatBy(int delta) {
  bool showFace = gUiMode == UiMode::ChatFace;
  bool showStatus = !gStatusText.isEmpty() && gStatusText != "Ready";
  int visibleRows = 0;
  int pixelLimit = 0;
  if (showFace) {
    visibleRows = showStatus ? 3 : 4;
    pixelLimit = 216;
  } else {
    visibleRows = showStatus ? 7 : 8;
    pixelLimit = 228;
  }
  auto lines = buildVisualLines(pixelLimit);
  int maxOffset = static_cast<int>(lines.size()) - visibleRows;
  if (maxOffset < 0) {
    maxOffset = 0;
  }
  gChatScrollOffset = clampValue(gChatScrollOffset + delta, 0, maxOffset);
  setStatus("Scroll " + String(gChatScrollOffset), 600);
}

fs::FS* activeVoiceFs() {
  if (gSdReady) {
    return &SD;
  }
  if (gLittleFsReady) {
    return &LittleFS;
  }
  return nullptr;
}

fs::FS* fallbackVoiceFs() {
  if (gSdReady && gLittleFsReady) {
    return &LittleFS;
  }
  return nullptr;
}

String activeVoiceStorageLabel() {
  if (gSdReady) {
    return "sd";
  }
  if (gLittleFsReady) {
    return "flash";
  }
  return "off";
}

bool fsExists(fs::FS& fs, const String& path) {
  return fs.exists(path.c_str());
}

bool fsRemove(fs::FS& fs, const String& path) {
  return fs.remove(path.c_str());
}

bool fsRename(fs::FS& fs, const String& from, const String& to) {
  return fs.rename(from.c_str(), to.c_str());
}

String parentDirOf(const String& path) {
  int slash = path.lastIndexOf('/');
  if (slash <= 0) {
    return "/";
  }
  return path.substring(0, slash);
}

bool ensureDirRecursive(fs::FS& fs, const String& dirPath) {
  if (dirPath.isEmpty() || dirPath == "/" || fsExists(fs, dirPath)) {
    return true;
  }
  int start = 1;
  while (start < dirPath.length()) {
    int slash = dirPath.indexOf('/', start);
    String part = slash < 0 ? dirPath : dirPath.substring(0, slash);
    if (!part.isEmpty() && !fsExists(fs, part) && !fs.mkdir(part)) {
      return false;
    }
    if (slash < 0) {
      break;
    }
    start = slash + 1;
  }
  return fsExists(fs, dirPath) || fs.mkdir(dirPath);
}

bool ensureVoiceStorageOn(fs::FS& fs) {
  if (!fsExists(fs, kVoiceDir)) {
    return fs.mkdir(kVoiceDir);
  }
  return true;
}

bool ensureVoiceStorage() {
  fs::FS* fs = activeVoiceFs();
  if (!fs) {
    return false;
  }
  return ensureVoiceStorageOn(*fs);
}

fs::FS* activeNotesFs() {
  if (gSdReady) {
    return &SD;
  }
  if (gLittleFsReady) {
    return &LittleFS;
  }
  return nullptr;
}

String activeNotesStorageLabel() {
  if (gSdReady) {
    return "SD";
  }
  if (gLittleFsReady) {
    return "FLASH";
  }
  return "OFF";
}

bool ensureNotesStorageOn(fs::FS& fs) {
  return ensureDirRecursive(fs, kNotesDir);
}

bool ensureNotesStorage() {
  fs::FS* fs = activeNotesFs();
  if (!fs) {
    return false;
  }
  return ensureNotesStorageOn(*fs);
}

bool ensurePomodoroStorageOn(fs::FS& fs) {
  if (!fsExists(fs, kPomodoroDir) && !fs.mkdir(kPomodoroDir)) {
    return false;
  }
  if (!fsExists(fs, kPomodoroAudioDir) && !fs.mkdir(kPomodoroAudioDir)) {
    return false;
  }
  if (!fsExists(fs, kPomodoroLogsDir) && !fs.mkdir(kPomodoroLogsDir)) {
    return false;
  }
  return true;
}

uint64_t activeStorageTotalBytes() {
  if (gSdReady) {
    return SD.totalBytes();
  }
  if (gLittleFsReady) {
    return LittleFS.totalBytes();
  }
  return 0;
}

uint64_t activeStorageUsedBytes() {
  if (gSdReady) {
    return SD.usedBytes();
  }
  if (gLittleFsReady) {
    return LittleFS.usedBytes();
  }
  return 0;
}

bool isNoteFileName(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".txt") || lower.endsWith(".md") || lower.endsWith(".note");
}

String basenameOfPath(const String& path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

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
