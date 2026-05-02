String makeVoiceNoteTitle(bool assistant, size_t audioLen, uint32_t sampleRate) {
  String who = assistant ? "Claw" : "Me";
  String stamp = String(millis() / 1000);
  if (hasValidSystemTime()) {
    time_t now = time(nullptr);
    struct tm localNow;
    localtime_r(&now, &localNow);
    char buf[5];
    strftime(buf, sizeof(buf), "%H%M", &localNow);
    stamp = buf;
  }
  uint32_t durationSec = sampleRate ? static_cast<uint32_t>((audioLen / 2U) / sampleRate) : 0;
  durationSec = max<uint32_t>(1, durationSec);
  return who + "-" + stamp + "-" + String(durationSec) + "s";
}

void sortVoiceNotesNewestFirst() {
  std::sort(gVoiceNotes.begin(), gVoiceNotes.end(), [](const VoiceNote& a, const VoiceNote& b) {
    return a.id > b.id;
  });
}

void clampVoiceSelection() {
  if (gVoiceNotes.empty()) {
    gSelectedVoiceNote = 0;
    return;
  }
  if (gSelectedVoiceNote < 0) {
    gSelectedVoiceNote = 0;
  }
  if (gSelectedVoiceNote >= static_cast<int>(gVoiceNotes.size())) {
    gSelectedVoiceNote = static_cast<int>(gVoiceNotes.size()) - 1;
  }
}

void pruneVoiceNotes() {
  sortVoiceNotesNewestFirst();
  fs::FS* fs = activeVoiceFs();
  size_t limit = voiceNoteLimit();
  while (gVoiceNotes.size() > limit) {
    const auto& note = gVoiceNotes.back();
    if (fs && !gSdReady) {
      fsRemove(*fs, note.audioPath);
      fsRemove(*fs, note.metaPath);
    }
    gVoiceNotes.pop_back();
  }
  clampVoiceSelection();
}

void loadVoiceNotes() {
  gVoiceNotes.clear();
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return;
  }
  File dir = fs->open(kVoiceDir);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  while (true) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }
    String path = file.path();
    file.close();
    if (!path.endsWith(kVoiceMetaExt)) {
      continue;
    }

    File meta = fs->open(path, "r");
    if (!meta) {
      continue;
    }
    DynamicJsonDocument doc(512);
    auto err = deserializeJson(doc, meta);
    meta.close();
    if (err) {
      continue;
    }

    VoiceNote note;
    note.id = doc["id"] | "";
    note.audioPath = doc["audio_path"] | "";
    note.metaPath = path;
    note.title = doc["title"] | "Voice note";
    note.preview = doc["preview"] | "";
    note.sampleRate = doc["sample_rate"] | 16000;
    note.durationMs = doc["duration_ms"] | 0;
    note.assistant = doc["assistant"] | false;
    if (!note.id.isEmpty() && !note.audioPath.isEmpty() && fsExists(*fs, note.audioPath)) {
      gVoiceNotes.push_back(note);
    }
  }
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
}

String filenameFromPath(const String& path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

String audioCategoryFromPath(const String& path) {
  String tail;
  String pomodoroPrefix = String(kPomodoroAudioDir) + "/";
  String userPrefix = String(kSdAudioDir) + "/";
  if (path.startsWith(userPrefix)) {
    tail = path.substring(userPrefix.length());
  } else if (path.startsWith(pomodoroPrefix)) {
    tail = path.substring(pomodoroPrefix.length());
  } else {
    return "voice";
  }
  int slash = tail.indexOf('/');
  if (slash > 0) {
    return tail.substring(0, slash);
  }
  return "root";
}

String audioFolderLabel(const String& category) {
  if (category.isEmpty()) {
    return "All audio";
  }
  if (category == "root") {
    return "Root";
  }
  return category;
}

bool audioAssetMatchesFolder(const AudioAsset& asset) {
  return gAudioFolderFilter.isEmpty() || asset.category == gAudioFolderFilter;
}

int audioAssetCountForCurrentFolder() {
  if (gAudioFolderFilter.isEmpty()) {
    return static_cast<int>(gAudioAssets.size());
  }
  int count = 0;
  for (const auto& asset : gAudioAssets) {
    if (audioAssetMatchesFolder(asset)) {
      count++;
    }
  }
  return count;
}

int firstAudioAssetInCurrentFolder() {
  for (int i = 0; i < static_cast<int>(gAudioAssets.size()); ++i) {
    if (audioAssetMatchesFolder(gAudioAssets[i])) {
      return i;
    }
  }
  return -1;
}

int audioAssetRankInCurrentFolder(int assetIndex) {
  int rank = 0;
  for (int i = 0; i < static_cast<int>(gAudioAssets.size()); ++i) {
    if (!audioAssetMatchesFolder(gAudioAssets[i])) {
      continue;
    }
    if (i == assetIndex) {
      return rank;
    }
    rank++;
  }
  return -1;
}

void clampSelectedAudioAssetToFolder() {
  if (gAudioAssets.empty()) {
    gSelectedAudioAsset = 0;
    return;
  }
  gSelectedAudioAsset = clampValue<int>(gSelectedAudioAsset, 0, static_cast<int>(gAudioAssets.size()) - 1);
  if (!audioAssetMatchesFolder(gAudioAssets[gSelectedAudioAsset])) {
    int first = firstAudioAssetInCurrentFolder();
    if (first >= 0) {
      gSelectedAudioAsset = first;
    }
  }
}

void moveSelectedAudioAssetInFolder(int delta) {
  if (gAudioAssets.empty() || audioAssetCountForCurrentFolder() == 0) {
    return;
  }
  clampSelectedAudioAssetToFolder();
  int index = gSelectedAudioAsset;
  int total = static_cast<int>(gAudioAssets.size());
  for (int step = 0; step < total; ++step) {
    index = (index + delta + total) % total;
    if (audioAssetMatchesFolder(gAudioAssets[index])) {
      gSelectedAudioAsset = index;
      return;
    }
  }
}

