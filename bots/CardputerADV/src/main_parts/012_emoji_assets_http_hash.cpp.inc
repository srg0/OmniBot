
void scanEmojiAssets() {
  gEmojiAssetCount = 0;
  clearEmojiCache();
  if (!gSdReady) {
    logf("[EMOJI] SD not mounted; emoji assets disabled");
    return;
  }
  // Directory enumeration can block boot on large or fragmented SD cards.
  // Emoji PNG files are loaded by exact codepoint path on demand.
  logf("[EMOJI] lazy SD lookup enabled");
}

String bytesToHex(const uint8_t* bytes, size_t len) {
  static const char* hex = "0123456789abcdef";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out += hex[(bytes[i] >> 4) & 0x0F];
    out += hex[bytes[i] & 0x0F];
  }
  return out;
}

String sha256File(fs::FS& fs, const String& path) {
  File file = fs.open(path, "r");
  if (!file) {
    return "";
  }
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  uint8_t buffer[1024];
  while (file.available()) {
    size_t n = file.read(buffer, sizeof(buffer));
    if (n == 0) {
      break;
    }
    mbedtls_sha256_update(&ctx, buffer, n);
  }
  file.close();
  uint8_t hash[32];
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);
  return bytesToHex(hash, sizeof(hash));
}

bool configureHttpClient(HTTPClient& http, std::unique_ptr<WiFiClientSecure>& secureClient, const String& url,
                         uint32_t timeoutMs = 60000) {
  http.setTimeout(timeoutMs);
  http.setReuse(false);
  bool tls = url.startsWith("https://");
  if (tls) {
    secureClient.reset(new WiFiClientSecure());
    secureClient->setInsecure();
    secureClient->setHandshakeTimeout(12);
    return http.begin(*secureClient, url);
  }
  return http.begin(url);
}

bool downloadUrlToFile(const String& url, const String& path, const String& expectedSha = "",
                       uint32_t expectedSize = 0, const String& progressTitle = "Downloading",
                       uint16_t progressAccent = TFT_CYAN) {
  if ((!gWifiReady && !ensureWifiForHttp("download", 3000)) || url.isEmpty()) {
    setStatus("Download no Wi-Fi", 1200);
    return false;
  }
  fs::FS* fs = activeVoiceFs();
  if (!fs || !ensurePomodoroStorageOn(*fs) || !ensureDirRecursive(*fs, parentDirOf(path))) {
    setStatus("Download no storage", 1200);
    return false;
  }

  String tempPath = path + ".tmp";
  fsRemove(*fs, tempPath);
  File out = fs->open(tempPath, "w");
  if (!out) {
    setStatus("Download open failed", 1200);
    return false;
  }

  beginTransferUi(progressTitle, "connecting", expectedSize, progressAccent);
  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  if (!configureHttpClient(http, secureClient, url, 90000)) {
    out.close();
    fsRemove(*fs, tempPath);
    finishTransferUi("connect failed", false);
    return false;
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  int code = http.GET();
  if (code < 200 || code >= 300) {
    http.end();
    out.close();
    fsRemove(*fs, tempPath);
    finishTransferUi("HTTP " + String(code), false);
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint32_t written = 0;
  int contentLength = http.getSize();
  uint32_t progressTotal = contentLength > 0 ? static_cast<uint32_t>(contentLength) : expectedSize;
  uint32_t lastProgress = millis();
  while (http.connected() && (contentLength < 0 || written < static_cast<uint32_t>(contentLength))) {
    size_t available = stream->available();
    if (!available) {
      if (millis() - lastProgress > 15000) {
        break;
      }
      updateTransferUi(written, progressTotal, "waiting");
      delay(2);
      continue;
    }
    size_t chunk = min(available, kOtaIoBufferBytes);
    int readBytes = stream->readBytes(gOtaIoBuffer, chunk);
    if (readBytes <= 0) {
      break;
    }
    if (out.write(gOtaIoBuffer, readBytes) != static_cast<size_t>(readBytes)) {
      http.end();
      out.close();
      fsRemove(*fs, tempPath);
      finishTransferUi("write failed", false);
      return false;
    }
    written += readBytes;
    lastProgress = millis();
    updateTransferUi(written, progressTotal, "receiving");
  }
  http.end();
  out.close();

  if ((expectedSize > 0 && written != expectedSize) ||
      (contentLength > 0 && written != static_cast<uint32_t>(contentLength))) {
    appendRuntimeLog("DL", "short path=" + path + " got=" + String(static_cast<unsigned>(written)) +
                     " len=" + String(contentLength) +
                     " expected=" + String(static_cast<unsigned>(expectedSize)), true);
    fsRemove(*fs, tempPath);
    finishTransferUi("download short", false);
    return false;
  }
  if (!expectedSha.isEmpty()) {
    updateTransferUi(written, progressTotal, "verifying SHA");
    String actualSha = sha256File(*fs, tempPath);
    actualSha.toLowerCase();
    String expected = expectedSha;
    expected.toLowerCase();
    if (actualSha != expected) {
      fsRemove(*fs, tempPath);
      finishTransferUi("SHA mismatch", false);
      appendRuntimeLog("SHA", path + " mismatch", true);
      return false;
    }
  }
  fsRemove(*fs, path);
  if (!fsRename(*fs, tempPath, path)) {
    fsRemove(*fs, tempPath);
    finishTransferUi("rename failed", false);
    return false;
  }
  finishTransferUi("saved", true);
  return true;
}

