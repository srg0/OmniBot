
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

bool syncAssetsFromManifest() {
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, kAssetManifestPath)) {
    setStatus("No asset manifest", 1200);
    return false;
  }
  File file = fs->open(kAssetManifestPath, "r");
  if (!file) {
    return false;
  }
  DynamicJsonDocument doc(12288);
  auto err = deserializeJson(doc, file);
  file.close();
  if (err || !doc["files"].is<JsonArray>()) {
    setStatus("Bad manifest", 1200);
    return false;
  }
  uint16_t okCount = 0;
  uint16_t failCount = 0;
  for (JsonObject item : doc["files"].as<JsonArray>()) {
    String path = item["path"] | "";
    String url = item["url"] | "";
    String sha = item["sha256"] | "";
    uint32_t size = item["size"] | 0;
    if (path.isEmpty() || url.isEmpty()) {
      continue;
    }
    bool alreadyOk = false;
    if (!sha.isEmpty() && fsExists(*fs, path)) {
      String actual = sha256File(*fs, path);
      actual.toLowerCase();
      sha.toLowerCase();
      alreadyOk = actual == sha;
    }
    if (alreadyOk || downloadUrlToFile(url, path, sha, size,
                                       "Asset " + String(okCount + failCount + 1) + "/" +
                                           String(doc["files"].as<JsonArray>().size()),
                                       rgb565_local(88, 240, 141))) {
      ++okCount;
    } else {
      ++failCount;
    }
    setStatus("Assets " + String(okCount) + "/" + String(okCount + failCount), 500);
    render();
  }
  scanAudioAssets();
  loadAssetManifestSummary();
  appendRuntimeLog("ASSET", "sync ok=" + String(okCount) + " fail=" + String(failCount), true);
  setStatus(failCount ? "Asset sync partial" : "Asset sync done", 1600);
  return failCount == 0;
}

bool uploadRuntimeLogs() {
  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (!ensureWifiForHttp("logs", 7000) || gHubHost.isEmpty()) {
    setStatus("Logs no Wi-Fi", 1200);
    setHttpDiag("logs: wifi/hub unavailable");
    appendRuntimeLog("LOG", "upload skipped wifi/hub", true);
    return false;
  }

  String body;
  body.reserve(1024 + gRuntimeLogs.size() * 180);
  body += "{\"device_id\":\"";
  body += jsonEscapeLine(gDeviceId);
  body += "\",\"firmware_version\":\"";
  body += jsonEscapeLine(kAppVersion);
  body += "\",\"git\":\"";
  body += jsonEscapeLine(kBuildGitSha);
  body += "\",\"events\":[";
  bool first = true;
  for (const auto& line : gRuntimeLogs) {
    if (!first) {
      body += ",";
    }
    first = false;
    body += "{\"ts\":";
    body += String(millis());
    body += ",\"level\":\"info\",\"message\":\"";
    body += jsonEscapeLine(line);
    body += "\",\"free_heap\":";
    body += String(static_cast<unsigned>(ESP.getFreeHeap()));
    body += ",\"rssi\":";
    body += String(WiFi.isConnected() ? WiFi.RSSI() : 0);
    body += ",\"storage\":\"";
    body += jsonEscapeLine(gStorageLabel);
    body += "\"}";
  }
  body += "]}";

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + kCardputerLogsPath;
  if (!configureHttpClient(http, secureClient, url, 30000)) {
    setStatus("Logs connect failed", 1200);
    setHttpDiag("logs: begin failed");
    appendRuntimeLog("LOG", "upload begin failed", true);
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Id", gDeviceId);
  http.addHeader("X-Conversation-Key", currentConversationKey());
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  int code = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body.c_str())), body.length());
  http.end();
  bool ok = code >= 200 && code < 300;
  String detail = code <= 0 ? HTTPClient::errorToString(code) : String(code);
  setStatus(ok ? "Logs uploaded" : (code <= 0 ? "Logs transport fail" : ("Logs HTTP " + String(code))), 1400);
  setHttpDiag(ok ? "logs: ok" : ("logs: " + detail));
  appendRuntimeLog("LOG", ok ? "upload ok" : ("upload failed " + detail), true);
  return ok;
}

bool probeOtaBridgeHealth(uint32_t timeoutMs) {
  if ((!gWifiReady && !ensureWifiForHttp("ota-health", 3000)) || gHubHost.isEmpty()) {
    appendRuntimeLog("OTA", "health skipped wifi/hub", false);
    return false;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + "/healthz";
  if (!configureHttpClient(http, secureClient, url, timeoutMs)) {
    appendRuntimeLog("OTA", "health connect failed", false);
    return false;
  }
  int code = http.GET();
  String body = code > 0 ? http.getString() : "";
  http.end();
  if (code < 200 || code >= 300) {
    appendRuntimeLog("OTA", "health http " + String(code), false);
    return false;
  }

  DynamicJsonDocument doc(768);
  auto err = deserializeJson(doc, body);
  if (!err && doc["upstream_connected"].is<bool>() && !doc["upstream_connected"].as<bool>()) {
    appendRuntimeLog("OTA", "health upstream false", false);
    return false;
  }
  appendRuntimeLog("OTA", "health ok", false);
  return true;
}

bool confirmOtaBootIfPending(const char* reason) {
  if (!gOtaPendingVerify) {
    return true;
  }
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err != ESP_OK) {
    appendRuntimeLog("OTA", "confirm failed err=" + String(static_cast<int>(err)), true);
    setStatus("OTA confirm failed", 1600);
    return false;
  }
  gOtaPendingVerify = false;
  gOtaConfirmedThisBoot = true;
  gOtaBootState = "valid";
  clearOtaGuardPrefs();
  String why = reason && reason[0] ? String(reason) : String("boot ok");
  appendRuntimeLog("OTA", "confirmed " + why + " slot=" + gOtaRunningSlot, true);
  pushSystem("OTA confirmed: " + gOtaRunningSlot);
  setStatus("OTA confirmed", 1200);
  return true;
}

void rollbackPendingOta(const char* reason) {
  if (!gOtaPendingVerify) {
    setStatus("No pending OTA", 1000);
    return;
  }
  String why = reason && reason[0] ? String(reason) : String("manual");
  appendRuntimeLog("OTA", "rollback requested: " + why, true);
  setStatus("Rolling back OTA...", 0);
  render();
  delay(400);
  esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
  appendRuntimeLog("OTA", "rollback failed err=" + String(static_cast<int>(err)), true);
  setStatus("Rollback failed", 1800);
}

void serviceOtaBootGuard() {
  if (!gOtaPendingVerify) {
    return;
  }
  uint32_t now = millis();
  if (now - gOtaLastHealthProbeMs >= kOtaHealthProbeIntervalMs) {
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
