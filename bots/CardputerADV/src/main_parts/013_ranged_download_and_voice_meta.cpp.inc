bool downloadFirmwareRangeToFileOnFs(fs::FS& fs, const String& url, const String& path,
                                     const String& expectedSha, uint32_t expectedSize,
                                     const String& progressTitle, uint16_t progressAccent) {
  if ((!gWifiReady && !ensureWifiForHttp("ota-range", 5000)) || url.isEmpty() || expectedSize == 0) {
    setStatus("OTA no Wi-Fi", 1200);
    return false;
  }
  if (!ensurePomodoroStorageOn(fs) || !ensureDirRecursive(fs, parentDirOf(path))) {
    setStatus("OTA no storage", 1200);
    return false;
  }

  const uint32_t kChunkBytes = 64UL * 1024UL;
  const uint8_t kMaxChunkAttempts = 5;
  String tempPath = path + ".tmp";
  fsRemove(fs, tempPath);
  beginTransferUi(progressTitle, "range download", expectedSize, progressAccent);

  uint32_t downloaded = 0;
  uint32_t lastLogged = 0;
  while (downloaded < expectedSize) {
    uint32_t chunkStart = downloaded;
    uint32_t chunkEnd = min<uint32_t>(expectedSize - 1, chunkStart + kChunkBytes - 1);
    uint32_t targetBytes = chunkEnd - chunkStart + 1;
    bool chunkDone = false;

    for (uint8_t attempt = 1; attempt <= kMaxChunkAttempts && !chunkDone; ++attempt) {
      HTTPClient http;
      std::unique_ptr<WiFiClientSecure> secureClient;
      if (!configureHttpClient(http, secureClient, url, 90000)) {
        appendRuntimeLog("OTA", "range begin failed at=" + String(static_cast<unsigned>(downloaded)), true);
        delay(250);
        continue;
      }
      if (!gDeviceToken.isEmpty()) {
        http.addHeader("Authorization", "Bearer " + gDeviceToken);
      }
      String rangeHeader = "bytes=" + String(static_cast<unsigned>(chunkStart)) + "-" +
                           String(static_cast<unsigned>(chunkEnd));
      http.addHeader("Range", rangeHeader);
      int code = http.GET();
      if (code != HTTP_CODE_PARTIAL_CONTENT) {
        String detail = code <= 0 ? HTTPClient::errorToString(code) : String(code);
        http.end();
        appendRuntimeLog("OTA", "range http " + detail + " at=" +
                         String(static_cast<unsigned>(downloaded)) +
                         " attempt=" + String(attempt), true);
        delay(350 * attempt);
        continue;
      }

      File out = fs.open(tempPath, downloaded == 0 ? "w" : FILE_APPEND);
      if (!out) {
        http.end();
        finishTransferUi("open failed", false);
        appendRuntimeLog("OTA", "range open failed", true);
        return false;
      }

      WiFiClient* stream = http.getStreamPtr();
      uint32_t partWritten = 0;
      uint32_t lastByteMs = millis();
      while (partWritten < targetBytes) {
        size_t available = stream ? stream->available() : 0;
        if (!available) {
          if (millis() - lastByteMs > 30000) {
            break;
          }
          updateTransferUi(downloaded + partWritten, expectedSize, "range wait");
          delay(2);
          continue;
        }
        size_t toRead = min<uint32_t>(min(available, kOtaIoBufferBytes), targetBytes - partWritten);
        int readBytes = stream->readBytes(gOtaIoBuffer, toRead);
        if (readBytes <= 0) {
          delay(2);
          continue;
        }
        if (out.write(gOtaIoBuffer, readBytes) != static_cast<size_t>(readBytes)) {
          out.close();
          http.end();
          finishTransferUi("write failed", false);
          appendRuntimeLog("OTA", "range write failed at=" +
                           String(static_cast<unsigned>(downloaded + partWritten)), true);
          return false;
        }
        partWritten += static_cast<uint32_t>(readBytes);
        lastByteMs = millis();
        updateTransferUi(downloaded + partWritten, expectedSize, "range download");
      }
      out.close();
      http.end();

      downloaded += partWritten;
      chunkDone = partWritten == targetBytes;
      if (!chunkDone) {
        appendRuntimeLog("OTA", "range short at=" + String(static_cast<unsigned>(downloaded)) +
                         " target=" + String(static_cast<unsigned>(targetBytes)) +
                         " got=" + String(static_cast<unsigned>(partWritten)) +
                         " attempt=" + String(attempt), true);
        delay(350 * attempt);
      }
      if (downloaded == expectedSize || downloaded - lastLogged >= 256UL * 1024UL) {
        lastLogged = downloaded;
        otaLog("range download " + String(static_cast<unsigned>(downloaded / 1024)) + "k/" +
               String(static_cast<unsigned>(expectedSize / 1024)) + "k", false);
      }
    }

    if (!chunkDone) {
      fsRemove(fs, tempPath);
      finishTransferUi("range failed", false);
      appendRuntimeLog("OTA", "range failed final at=" + String(static_cast<unsigned>(downloaded)), true);
      return false;
    }
  }

  updateTransferUi(downloaded, expectedSize, "verifying SHA");
  String actualSha = sha256File(fs, tempPath);
  actualSha.toLowerCase();
  String expected = expectedSha;
  expected.toLowerCase();
  if (actualSha != expected) {
    fsRemove(fs, tempPath);
    finishTransferUi("SHA mismatch", false);
    appendRuntimeLog("OTA", "range sha mismatch expected=" + expected.substring(0, 12) +
                     " actual=" + actualSha.substring(0, 12), true);
    return false;
  }

  fsRemove(fs, path);
  if (!fsRename(fs, tempPath, path)) {
    fsRemove(fs, tempPath);
    finishTransferUi("rename failed", false);
    appendRuntimeLog("OTA", "range rename failed", true);
    return false;
  }
  finishTransferUi("saved", true);
  appendRuntimeLog("OTA", "range download ok bytes=" + String(static_cast<unsigned>(downloaded)), true);
  return true;
}

String voiceAudioPath(const String& id) {
  return String(kVoiceDir) + "/" + id + kVoiceAudioExt;
}

String voiceMetaPath(const String& id) {
  return String(kVoiceDir) + "/" + id + kVoiceMetaExt;
}

String makeVoiceNoteId(bool assistant) {
  return String(assistant ? "a_" : "u_") + String(millis()) + "_" + chipIdSuffix();
}

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
