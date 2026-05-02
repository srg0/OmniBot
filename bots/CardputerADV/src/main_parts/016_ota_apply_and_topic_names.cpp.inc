    gOtaLastHealthProbeMs = now;
    if (probeOtaBridgeHealth(6000)) {
      confirmOtaBootIfPending("bridge health");
      return;
    }
  }
  if (now - gOtaVerifyStartMs >= kOtaCoreBootGraceMs) {
    // If the app survived the boot window, commit it to avoid false rollbacks
    // caused by external Wi-Fi/bridge outages. Crashes before this point roll
    // back automatically because the slot remains PENDING_VERIFY.
    confirmOtaBootIfPending("core boot grace");
  }
}

bool finishOtaApply(esp_ota_handle_t& otaHandle, const esp_partition_t* next, uint32_t written,
                    uint32_t expectedSize) {
  updateTransferUi(written, expectedSize, "finalizing");
  esp_err_t otaErr = esp_ota_end(otaHandle);
  otaHandle = 0;
  if (otaErr != ESP_OK) {
    finishTransferUi("finalize failed", false);
    setStatus("OTA end err " + String(static_cast<int>(otaErr)), 1800);
    appendRuntimeLog("OTA", "end failed err=" + String(static_cast<int>(otaErr)) +
                              " written=" + String(static_cast<unsigned>(written)) +
                              " size=" + String(static_cast<unsigned>(expectedSize)), true);
    return false;
  }
  recordOtaApplyIntent();
  otaErr = esp_ota_set_boot_partition(next);
  if (otaErr != ESP_OK) {
    finishTransferUi("boot select failed", false);
    setStatus("OTA boot err " + String(static_cast<int>(otaErr)), 2200);
    appendRuntimeLog("OTA", "set boot failed err=" + String(static_cast<int>(otaErr)) +
                              " next=" + describeOtaPartition(next), true);
    return false;
  }
  const esp_partition_t* newBoot = esp_ota_get_boot_partition();
  if (!newBoot || (next && strcmp(newBoot->label, next->label) != 0)) {
    finishTransferUi("boot slot not set", false);
    setStatus("OTA boot slot failed", 2200);
    appendRuntimeLog("OTA", "boot slot mismatch expected=" + describeOtaPartition(next) +
                              " actual=" + describeOtaPartition(newBoot), true);
    return false;
  }
  appendRuntimeLog("OTA", "apply ok boot=" + describeOtaPartition(newBoot) + " reboot pending verify", true);
  finishTransferUi("rebooting", true);
  setStatus("OTA staged; rebooting", 900);
  delay(500);
  ESP.restart();
  return true;
}

bool writeStagedOtaFileToPartition(fs::FS& fs, const String& path, const esp_partition_t* next,
                                   uint32_t expectedSize, const String& expectedSha) {
  File firmware = fs.open(path, "r");
  if (!firmware) {
    setStatus("OTA stage missing", 1200);
    appendRuntimeLog("OTA", "staged file missing", true);
    return false;
  }
  size_t stagedSize = firmware.size();
  if (stagedSize != expectedSize) {
    firmware.close();
    setStatus("OTA staged size bad", 1800);
    appendRuntimeLog("OTA", "staged size mismatch file=" + String(static_cast<unsigned>(stagedSize)) +
                              " manifest=" + String(static_cast<unsigned>(expectedSize)), true);
    return false;
  }

  esp_ota_handle_t otaHandle = 0;
  esp_err_t otaErr = esp_ota_begin(next, expectedSize, &otaHandle);
  if (otaErr != ESP_OK) {
    firmware.close();
    setStatus("OTA begin err " + String(static_cast<int>(otaErr)), 1800);
    appendRuntimeLog("OTA", "begin failed staged err=" + String(static_cast<int>(otaErr)) +
                              " size=" + String(static_cast<unsigned>(expectedSize)), true);
    return false;
  }

  beginTransferUi("Flashing OTA", "writing staged file", expectedSize, rgb565_local(255, 93, 88));
  otaLog("staged flash begin size=" + String(static_cast<unsigned>(expectedSize)), true);
  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, 0);
  uint32_t written = 0;
  uint32_t lastLogged = 0;
  while (firmware.available() && written < expectedSize) {
    size_t toRead = min<uint32_t>(kOtaIoBufferBytes, expectedSize - written);
    int readBytes = firmware.read(gOtaIoBuffer, toRead);
    if (readBytes <= 0) {
      break;
    }
    otaErr = esp_ota_write(otaHandle, gOtaIoBuffer, static_cast<size_t>(readBytes));
    if (otaErr != ESP_OK) {
      esp_ota_abort(otaHandle);
      mbedtls_sha256_free(&shaCtx);
      firmware.close();
      finishTransferUi("flash write failed", false);
      setStatus("OTA write err " + String(static_cast<int>(otaErr)), 1800);
      appendRuntimeLog("OTA", "staged flash write failed err=" + String(static_cast<int>(otaErr)) +
                                " written=" + String(static_cast<unsigned>(written)), true);
      return false;
    }
    mbedtls_sha256_update(&shaCtx, gOtaIoBuffer, static_cast<size_t>(readBytes));
    written += static_cast<uint32_t>(readBytes);
    updateTransferUi(written, expectedSize, "writing staged file");
    if (written == expectedSize || written - lastLogged >= 256UL * 1024UL) {
      lastLogged = written;
      otaLog("staged flash " + String(static_cast<unsigned>(written / 1024)) + "k/" +
             String(static_cast<unsigned>(expectedSize / 1024)) + "k", false);
    }
    delay(1);
  }
  firmware.close();

  uint8_t hash[32];
  mbedtls_sha256_finish(&shaCtx, hash);
  mbedtls_sha256_free(&shaCtx);
  if (written != expectedSize) {
    esp_ota_abort(otaHandle);
    finishTransferUi("flash short", false);
    setStatus("OTA short write", 1800);
    appendRuntimeLog("OTA", "staged short write written=" + String(static_cast<unsigned>(written)) +
                              " size=" + String(static_cast<unsigned>(expectedSize)), true);
    return false;
  }

  String actualSha = bytesToHex(hash, sizeof(hash));
  actualSha.toLowerCase();
  String expected = expectedSha;
  expected.toLowerCase();
  if (actualSha != expected) {
    esp_ota_abort(otaHandle);
    finishTransferUi("SHA mismatch", false);
    setStatus("OTA SHA mismatch", 2200);
    appendRuntimeLog("OTA", "staged sha mismatch expected=" + expected.substring(0, 12) +
                              " actual=" + actualSha.substring(0, 12), true);
    return false;
  }

  fsRemove(fs, path);
  return finishOtaApply(otaHandle, next, written, expectedSize);
}

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
