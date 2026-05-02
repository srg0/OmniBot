template <typename T>
T clampValue(T value, T minValue, T maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

String jsonEscapeLine(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\r", " ");
  value.replace("\n", " ");
  return value;
}

void appendRuntimeLog(const String& area, const String& message, bool persist = false) {
  String compact = String(millis() / 1000) + " " + area + " " + message;
  while (compact.length() > 96) {
    compact.remove(compact.length() - 1);
  }
  gRuntimeLogs.push_back(compact);
  while (gRuntimeLogs.size() > kMaxRuntimeLogLines) {
    gRuntimeLogs.pop_front();
  }
  if (!persist || !gStorageReady) {
    return;
  }
  fs::FS* fs = activeVoiceFs();
  if (!fs || !ensurePomodoroStorageOn(*fs)) {
    return;
  }
  File out = fs->open(kRuntimeLogPath, FILE_APPEND);
  if (!out) {
    return;
  }
  out.print("{\"t\":");
  out.print(millis());
  out.print(",\"area\":\"");
  out.print(jsonEscapeLine(area));
  out.print("\",\"msg\":\"");
  out.print(jsonEscapeLine(message));
  out.println("\"}");
  out.close();
}

void logf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
  appendRuntimeLog("SER", String(buf), false);
}

String chipIdSuffix() {
  uint64_t chip = ESP.getEfuseMac();
  char buf[13];
  snprintf(buf, sizeof(buf), "%06llx", (unsigned long long)(chip & 0xFFFFFFULL));
  return String(buf);
}

uint16_t compileTimeHubPort() {
  const char* raw = DEFAULT_HUB_PORT_STR;
  if (!raw || !raw[0]) {
    return kDefaultHubPort;
  }
  long parsed = strtol(raw, nullptr, 10);
  if (parsed <= 0 || parsed > 65535) {
    return kDefaultHubPort;
  }
  return static_cast<uint16_t>(parsed);
}

bool isLocalOrPrivateBridgeHost(const String& host) {
  if (host == "localhost" || host == "127.0.0.1" || host.startsWith("192.168.") || host.startsWith("10.")) {
    return true;
  }
  if (!host.startsWith("172.")) {
    return false;
  }
  int firstDot = host.indexOf('.');
  int secondDot = firstDot >= 0 ? host.indexOf('.', firstDot + 1) : -1;
  if (firstDot < 0 || secondDot < 0) {
    return false;
  }
  int secondOctet = host.substring(firstDot + 1, secondDot).toInt();
  return secondOctet >= 16 && secondOctet <= 31;
}

void migrateBridgeEndpoint() {
  bool legacyBridge = gHubHost == kLegacyBridgeHost && gHubPort == kLegacyBridgePort;
  bool staleDomainPort = gHubHost == kProdBridgeHost && (gHubPort == 80 || gHubPort == kLegacyBridgePort);
  bool localBridge = isLocalOrPrivateBridgeHost(gHubHost);
  bool nonProdSavedEndpoint = !gHubHost.isEmpty() && gHubHost != kProdBridgeHost;
  if (legacyBridge || staleDomainPort || localBridge || nonProdSavedEndpoint) {
    String previous = gHubHost + ":" + String(gHubPort);
    saveHubEndpoint(kProdBridgeHost, kProdBridgePort);
    logf("[CFG] migrated bridge endpoint from=%s to=%s:%u",
         previous.c_str(), gHubHost.c_str(), gHubPort);
  }
}

bool hubUsesTls() {
  return gHubHost == kProdBridgeHost || gHubPort == 443;
}

String hubBaseUrl() {
  return String(hubUsesTls() ? "https://" : "http://") + gHubHost + ":" + String(gHubPort);
}

void configureSecureClient(WiFiClientSecure& client) {
#if OPENCLAW_TLS_VERIFY
  client.setCACert(kTlsRootCaPem);
#else
  client.setInsecure();
#endif
  client.setHandshakeTimeout(12);
}

const char* tlsVerifyMode() {
#if OPENCLAW_TLS_VERIFY
  return "ca";
#else
  return "insecure";
#endif
}

const char* otaImageStateName(esp_ota_img_states_t state) {
  switch (state) {
    case ESP_OTA_IMG_NEW:
      return "new";
    case ESP_OTA_IMG_PENDING_VERIFY:
      return "pending";
    case ESP_OTA_IMG_VALID:
      return "valid";
    case ESP_OTA_IMG_INVALID:
      return "invalid";
    case ESP_OTA_IMG_ABORTED:
      return "aborted";
    case ESP_OTA_IMG_UNDEFINED:
    default:
      return "undefined";
  }
}

String describeOtaPartition(const esp_partition_t* partition) {
  if (!partition) {
    return "none";
  }
  return String(partition->label) + "@0x" + String(static_cast<uint32_t>(partition->address), HEX) +
         "/" + String(static_cast<unsigned>(partition->size));
}

void otaLog(const String& message, bool persist = true) {
  appendRuntimeLog("OTA", message, persist);
  logf("[OTA] %s", message.c_str());
}

void clearOtaGuardPrefs() {
  gPrefs.begin(kPrefsOtaGuardNs, false);
  gPrefs.clear();
  gPrefs.end();
}

bool readOtaApplyBootRequest() {
  gPrefs.begin(kPrefsOtaGuardNs, true);
  bool requested = gPrefs.getBool("apply_boot", false);
  gPrefs.end();
  return requested;
}

void clearOtaApplyBootRequest() {
  gPrefs.begin(kPrefsOtaGuardNs, false);
  gPrefs.remove("apply_boot");
  gPrefs.remove("apply_target");
  gPrefs.end();
}

void requestOtaApplyBootMode(const String& targetVersion) {
  gPrefs.begin(kPrefsOtaGuardNs, false);
  gPrefs.putBool("apply_boot", true);
  gPrefs.putString("apply_target", targetVersion);
  gPrefs.putString("from", kAppVersion);
  gPrefs.end();
  otaLog("boot apply requested target=" + targetVersion, true);
}

void recordOtaApplyIntent() {
  gPrefs.begin(kPrefsOtaGuardNs, false);
  gPrefs.putString("target", gOtaManifest.version);
  gPrefs.putString("sha", gOtaManifest.sha256);
  gPrefs.putString("from", kAppVersion);
  gPrefs.putString("git", kBuildGitSha);
  gPrefs.putUInt("started", millis());
  gPrefs.end();
}

void beginOtaBootGuard() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  gOtaRunningSlot = running ? String(running->label) : String("-");
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  esp_err_t err = running ? esp_ota_get_state_partition(running, &state) : ESP_ERR_NOT_FOUND;
  if (err == ESP_OK) {
    gOtaBootState = otaImageStateName(state);
  } else {
    gOtaBootState = "no-state";
  }

  gOtaPendingVerify = err == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
  gOtaConfirmedThisBoot = false;
  gOtaVerifyStartMs = millis();
  gOtaLastHealthProbeMs = 0;

  gPrefs.begin(kPrefsOtaGuardNs, false);
  String intendedTarget = gPrefs.getString("target", "");
  String intendedFrom = gPrefs.getString("from", "");
  gPrefs.end();
  if (!intendedTarget.isEmpty() && intendedTarget != kAppVersion && !gOtaPendingVerify) {
    appendRuntimeLog("OTA", "boot returned to " + String(kAppVersion) +
                              " after target=" + intendedTarget +
                              " from=" + intendedFrom +
                              " slot=" + gOtaRunningSlot +
                              " state=" + gOtaBootState, true);
    pushError("OTA did not switch to " + intendedTarget);
    setStatus("OTA stayed on " + String(kAppVersion), 1800);
    clearOtaGuardPrefs();
  }

  if (gOtaPendingVerify) {
    gPrefs.begin(kPrefsOtaGuardNs, false);
    uint32_t boots = gPrefs.getUInt("boots", 0) + 1;
    gPrefs.putUInt("boots", boots);
    gPrefs.putString("slot", gOtaRunningSlot);
    gPrefs.putString("boot_version", kAppVersion);
    gPrefs.end();
    appendRuntimeLog("OTA", "pending verify slot=" + gOtaRunningSlot +
                              " boot=" + String(boots), true);
    pushSystem("OTA verify: " + gOtaRunningSlot);
    setStatus("OTA verifying...", 1200);
  } else if (err == ESP_OK && state == ESP_OTA_IMG_VALID) {
    clearOtaGuardPrefs();
  }
  logf("[OTA] running=%s state=%s pending=%d", gOtaRunningSlot.c_str(),
       gOtaBootState.c_str(), static_cast<int>(gOtaPendingVerify));
}
