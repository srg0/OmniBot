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
  gAssetManifest = AssetManifestSummary{};
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, kAssetManifestPath)) {
    gAssetManifest.error = "missing";
    return;
  }
  gAssetManifest.present = true;
  File file = fs->open(kAssetManifestPath, "r");
  if (!file) {
    gAssetManifest.error = "open failed";
    return;
  }
  DynamicJsonDocument doc(8192);
  auto err = deserializeJson(doc, file);
  file.close();
  if (err) {
    gAssetManifest.error = err.c_str();
    return;
  }
  gAssetManifest.parsed = true;
  gAssetManifest.version = doc["asset_version"] | doc["version"] | "";
  gAssetManifest.generatedAt = doc["generated_at"] | doc["generated"] | "";
  JsonArray files = doc["files"].as<JsonArray>();
  gAssetManifest.fileCount = files.size();
  uint32_t total = 0;
  for (JsonObject item : files) {
    total += item["size"] | 0;
  }
  gAssetManifest.totalBytes = total;
  appendRuntimeLog("ASSET", "manifest files=" + String(gAssetManifest.fileCount), true);
}

void loadOtaManifestSummary() {
  gOtaManifest = OtaManifestSummary{};
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, kOtaManifestPath)) {
    gOtaManifest.error = "missing";
    return;
  }
  gOtaManifest.present = true;
  File file = fs->open(kOtaManifestPath, "r");
  if (!file) {
    gOtaManifest.error = "open failed";
    return;
  }
  DynamicJsonDocument doc(3072);
  auto err = deserializeJson(doc, file);
  file.close();
  if (err) {
    gOtaManifest.error = err.c_str();
    return;
  }
  gOtaManifest.parsed = true;
  gOtaManifest.version = doc["version"] | "";
  gOtaManifest.channel = doc["channel"] | "";
  gOtaManifest.url = doc["firmware_url"] | doc["url"] | "";
  gOtaManifest.sha256 = doc["sha256"] | "";
  gOtaManifest.size = doc["size"] | 0;
  gOtaManifest.minBattery = doc["min_battery"] | 50;
  gOtaManifest.confirmRequired = doc["confirm_required"] | true;
  appendRuntimeLog("OTA", "manifest version=" + gOtaManifest.version, true);
}

bool fetchRemoteJsonToFile(const String& path, const String& localPath, const String& label) {
  String url = hubBaseUrl() + path;
  String title = label == "OTA" ? "OTA Manifest" : (label == "ASSET" ? "Asset Manifest" : "Download");
  uint16_t accent = label == "OTA" ? rgb565_local(255, 93, 88) : rgb565_local(88, 240, 141);
  bool ok = downloadUrlToFile(url, localPath, "", 0, title, accent);
  appendRuntimeLog(label, ok ? "fetch ok" : "fetch failed", true);
  return ok;
}

bool fetchRemoteAssetManifest() {
  if (fetchRemoteJsonToFile(kAssetManifestFetchPath, kAssetManifestPath, "ASSET")) {
    loadAssetManifestSummary();
    return true;
  }
  return false;
}

bool fetchRemoteOtaManifest() {
  if (fetchRemoteJsonToFile(kFirmwareManifestFetchPath, kOtaManifestPath, "OTA")) {
    loadOtaManifestSummary();
    if (gDeviceSettings.beepOnUpdate && gOtaManifest.parsed && !gOtaManifest.version.isEmpty() &&
        gOtaManifest.version != kAppVersion) {
      focusTone(1320, 120);
    }
    return true;
  }
  return false;
}

