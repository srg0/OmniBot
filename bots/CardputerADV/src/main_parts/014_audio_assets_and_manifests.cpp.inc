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

void rebuildAudioFolders() {
  gAudioFolders.clear();
  AudioFolder all;
  all.label = "All audio";
  all.category = "";
  for (const auto& asset : gAudioAssets) {
    all.count++;
    all.totalBytes += asset.size;
    int folderIndex = -1;
    for (int i = 0; i < static_cast<int>(gAudioFolders.size()); ++i) {
      if (gAudioFolders[i].category == asset.category) {
        folderIndex = i;
        break;
      }
    }
    if (folderIndex < 0) {
      AudioFolder folder;
      folder.category = asset.category;
      folder.label = audioFolderLabel(asset.category);
      gAudioFolders.push_back(folder);
      folderIndex = static_cast<int>(gAudioFolders.size()) - 1;
    }
    gAudioFolders[folderIndex].count++;
    gAudioFolders[folderIndex].totalBytes += asset.size;
  }
  gAudioFolders.insert(gAudioFolders.begin(), all);
  gSelectedAudioFolder = clampValue<int>(gSelectedAudioFolder, 0, max<int>(0, static_cast<int>(gAudioFolders.size()) - 1));
  if (!gAudioFolderFilter.isEmpty()) {
    bool filterStillExists = false;
    for (const auto& folder : gAudioFolders) {
      if (folder.category == gAudioFolderFilter) {
        filterStillExists = true;
        break;
      }
    }
    if (!filterStillExists) {
      gAudioFolderFilter = "";
      gSelectedAudioFolder = 0;
    }
  }
  clampSelectedAudioAssetToFolder();
}

void scanAudioAssetsRecursive(fs::FS& fs, const String& dirPath, uint8_t depth) {
  if (depth > 4 || gAudioAssets.size() >= kMaxAudioAssets) {
    return;
  }
  File dir = fs.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  while (gAudioAssets.size() < kMaxAudioAssets) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }
    String path = file.path();
    bool directory = file.isDirectory();
    uint32_t size = static_cast<uint32_t>(file.size());
    file.close();
    if (directory) {
      scanAudioAssetsRecursive(fs, path, depth + 1);
      continue;
    }
    String lower = path;
    lower.toLowerCase();
    if (!lower.endsWith(".wav")) {
      continue;
    }
    AudioAsset asset;
    asset.path = path;
    asset.title = filenameFromPath(path);
    asset.category = audioCategoryFromPath(path);
    asset.language = path.indexOf("/ru/") >= 0 || path.endsWith("_ru.wav") ? "ru" : "";
    asset.size = size;
    gAudioAssets.push_back(asset);
  }
}

void scanAudioAssets() {
  gAudioAssets.clear();
  fs::FS* fs = activeVoiceFs();
  if (!fs || !ensurePomodoroStorageOn(*fs)) {
    rebuildAudioFolders();
    appendRuntimeLog("ASSET", "storage unavailable", true);
    return;
  }
  if (fsExists(*fs, kSdAudioDir)) {
    scanAudioAssetsRecursive(*fs, kSdAudioDir, 0);
  }
  scanAudioAssetsRecursive(*fs, kPomodoroAudioDir, 0);
  rebuildAudioFolders();
  if (gSelectedAudioAsset >= static_cast<int>(gAudioAssets.size())) {
    gSelectedAudioAsset = max<int>(0, static_cast<int>(gAudioAssets.size()) - 1);
  }
  clampSelectedAudioAssetToFolder();
  appendRuntimeLog("LIB", "audio scan count=" + String(gAudioAssets.size()), true);
}

void loadAssetManifestSummary() {
