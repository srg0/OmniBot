bool applyOtaFromManifest() {
  if (gOtaApplyInProgress) {
    otaLog("apply ignored: already running", true);
    setStatus("OTA already running", 1400);
    return false;
  }
  gOtaApplyInProgress = true;
  auto fail = [&](const String& reason) -> bool {
    gOtaApplyInProgress = false;
    otaLog("apply failed: " + reason, true);
    finishTransferUi("failed: " + reason, false);
    return false;
  };

  if (!gOtaManifest.parsed || gOtaManifest.url.isEmpty() || gOtaManifest.sha256.isEmpty()) {
    return fail("manifest incomplete");
  }
  uint32_t stackWords = uxTaskGetStackHighWaterMark(nullptr);
  otaLog("apply start version=" + gOtaManifest.version +
         " size=" + String(gOtaManifest.size) +
         " heap=" + String(static_cast<unsigned>(ESP.getFreeHeap())) +
         " stackw=" + String(static_cast<unsigned>(stackWords)), true);
  setStatus("OTA preparing...", 0);
  render();
  updateBatterySnapshot(true);
  int level = gBatterySnapshot.level < 0 ? 100 : gBatterySnapshot.level;
  uint8_t minBattery = max<uint8_t>(gDeviceSettings.minBatteryForUpdate, gOtaManifest.minBattery);
  if (level < minBattery) {
    return fail("battery too low");
  }
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  otaLog("partition running=" + describeOtaPartition(running) +
         " boot=" + describeOtaPartition(boot) +
         " next=" + describeOtaPartition(next), true);
  if (!next) {
    return fail("slot missing");
  }

  uint32_t expectedSize = gOtaManifest.size;
  if (expectedSize == 0) {
    return fail("size missing");
  }
  if (expectedSize > next->size) {
    return fail("image too large " + String(static_cast<unsigned>(expectedSize)) +
                "/" + String(static_cast<unsigned>(next->size)));
  }

  if (!ensureWifiForHttp("ota", 7000)) {
    setHttpDiag("ota: wifi unavailable");
    return fail("no Wi-Fi");
  }

  if (gSdReady && ensurePomodoroStorageOn(SD) && ensureDirRecursive(SD, parentDirOf(kOtaTempPath))) {
    bool stagedOk = false;
    String expectedSha = gOtaManifest.sha256;
    expectedSha.toLowerCase();
    if (fsExists(SD, kOtaTempPath)) {
      String cachedSha = sha256File(SD, kOtaTempPath);
      cachedSha.toLowerCase();
      stagedOk = cachedSha == expectedSha;
      otaLog(stagedOk ? "sd stage cache hit" : "sd stage cache stale", true);
      if (!stagedOk) {
        fsRemove(SD, kOtaTempPath);
      }
    }
    if (!stagedOk) {
      otaLog("sd stage download start", true);
      stagedOk = downloadFirmwareRangeToFileOnFs(SD, gOtaManifest.url, kOtaTempPath,
                                                 gOtaManifest.sha256, expectedSize,
                                                 "Firmware SD stage", rgb565_local(255, 93, 88));
      otaLog(stagedOk ? "sd stage download ok" : "sd stage download failed", true);
    }
    if (stagedOk) {
      otaLog("sd stage verified; flashing", true);
      bool ok = writeStagedOtaFileToPartition(SD, kOtaTempPath, next, expectedSize, gOtaManifest.sha256);
      if (!ok) {
        otaLog("staged flash failed; direct fallback", true);
        fsRemove(SD, kOtaTempPath);
        setStatus("OTA direct retry", 900);
        render();
      } else {
        return true;
      }
    }
    otaLog("sd stage failed; direct fallback", true);
    setStatus("OTA direct fallback", 700);
    render();
  } else {
    otaLog("sd stage unavailable; direct stream", true);
  }

  beginTransferUi("Firmware OTA", "connecting", expectedSize, rgb565_local(255, 93, 88));
  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  if (!configureHttpClient(http, secureClient, gOtaManifest.url, 120000)) {
    finishTransferUi("connect failed", false);
    setHttpDiag("ota: begin failed");
    return fail("http begin failed");
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  int code = http.GET();
  if (code < 200 || code >= 300) {
    String detail = code <= 0 ? HTTPClient::errorToString(code) : String(code);
    http.end();
    finishTransferUi("HTTP " + detail, false);
    setHttpDiag("ota: " + detail);
    return fail("download http " + detail);
  }

  int contentLength = http.getSize();
  if (contentLength > 0 && static_cast<uint32_t>(contentLength) != expectedSize) {
    http.end();
    finishTransferUi("size mismatch", false);
    return fail("size mismatch manifest=" + String(static_cast<unsigned>(expectedSize)) +
                " http=" + String(contentLength));
  }

  esp_ota_handle_t otaHandle = 0;
  // Direct streaming is a fallback path. Use incremental erase so flash erase
  // cannot stall the already-open HTTP response long enough to break TLS/TCP.
  esp_err_t otaErr = esp_ota_begin(next, OTA_WITH_SEQUENTIAL_WRITES, &otaHandle);
  if (otaErr != ESP_OK) {
    http.end();
    return fail("begin err " + String(static_cast<int>(otaErr)));
  }
  otaLog("direct flash begin size=" + String(static_cast<unsigned>(expectedSize)), true);

  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, 0);
  WiFiClient* stream = http.getStreamPtr();
  uint32_t written = 0;
  uint32_t lastLogged = 0;
  uint32_t lastByteMs = millis();
  while (written < expectedSize) {
    size_t available = stream ? stream->available() : 0;
    if (!available) {
      if (!http.connected()) {
        break;
      }
      if (millis() - lastByteMs > 20000) {
        esp_ota_abort(otaHandle);
        mbedtls_sha256_free(&shaCtx);
        http.end();
        finishTransferUi("download stalled", false);
        setHttpDiag("ota: stalled");
        return fail("stream stalled " + String(static_cast<unsigned>(written)));
      }
      updateTransferUi(written, expectedSize, "receiving");
      delay(2);
      continue;
    }

    size_t toRead = min(available, kOtaIoBufferBytes);
    toRead = min<uint32_t>(static_cast<uint32_t>(toRead), expectedSize - written);
    int readBytes = stream->readBytes(gOtaIoBuffer, toRead);
    if (readBytes <= 0) {
      delay(1);
      continue;
    }

    otaErr = esp_ota_write(otaHandle, gOtaIoBuffer, static_cast<size_t>(readBytes));
    if (otaErr != ESP_OK) {
      esp_ota_abort(otaHandle);
      mbedtls_sha256_free(&shaCtx);
      http.end();
      finishTransferUi("flash write failed", false);
      return fail("write err " + String(static_cast<int>(otaErr)) +
                  " at " + String(static_cast<unsigned>(written)));
    }
    mbedtls_sha256_update(&shaCtx, gOtaIoBuffer, static_cast<size_t>(readBytes));
    written += static_cast<uint32_t>(readBytes);
    lastByteMs = millis();
    updateTransferUi(written, expectedSize, "receiving + flashing");
    if (written == expectedSize || written - lastLogged >= 256UL * 1024UL) {
      lastLogged = written;
      otaLog("direct flash " + String(static_cast<unsigned>(written / 1024)) + "k/" +
             String(static_cast<unsigned>(expectedSize / 1024)) + "k", false);
    }
    delay(1);
  }

  uint8_t hash[32];
  mbedtls_sha256_finish(&shaCtx, hash);
  mbedtls_sha256_free(&shaCtx);
  http.end();

  if (written != expectedSize) {
    esp_ota_abort(otaHandle);
    finishTransferUi("flash short", false);
    return fail("short write " + String(static_cast<unsigned>(written)) +
                "/" + String(static_cast<unsigned>(expectedSize)));
  }

  String actualSha = bytesToHex(hash, sizeof(hash));
  actualSha.toLowerCase();
  String expectedSha = gOtaManifest.sha256;
  expectedSha.toLowerCase();
  if (actualSha != expectedSha) {
    esp_ota_abort(otaHandle);
    finishTransferUi("SHA mismatch", false);
    return fail("sha mismatch expected=" + expectedSha.substring(0, 12) +
                " actual=" + actualSha.substring(0, 12));
  }

  bool ok = finishOtaApply(otaHandle, next, written, expectedSize);
  if (!ok) {
    return fail("direct flash failed");
  }
  return true;
}

String urlEncodePathSegment(const String& raw) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  out.reserve(raw.length() + 8);
  for (size_t i = 0; i < raw.length(); ++i) {
    uint8_t c = static_cast<uint8_t>(raw[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

String makeTopicShortName(const String& title, int32_t threadId) {
  (void)threadId;
  String displayTitle = normalizeTopicTitleForDisplay(title, false);
  String out;
  for (size_t i = 0; i < displayTitle.length() && out.length() < 4; ++i) {
    char c = displayTitle[i];
    if (isalnum(static_cast<unsigned char>(c))) {
      out += static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
  }
  if (!out.isEmpty()) {
    return out;
  }
  return "TG";
}
