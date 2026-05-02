
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

