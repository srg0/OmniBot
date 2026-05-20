String setupPortalHtmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  return value;
}

void parseSetupPortalBridgeUrl(String raw, String& host, uint16_t& port) {
  raw.trim();
  bool https = raw.startsWith("https://");
  bool http = raw.startsWith("http://");
  if (https) {
    raw.remove(0, 8);
  } else if (http) {
    raw.remove(0, 7);
  }
  int slash = raw.indexOf('/');
  if (slash >= 0) {
    raw = raw.substring(0, slash);
  }
  raw.trim();
  int colon = raw.lastIndexOf(':');
  if (colon > 0 && colon < raw.length() - 1) {
    int parsedPort = raw.substring(colon + 1).toInt();
    if (parsedPort > 0 && parsedPort <= 65535) {
      port = static_cast<uint16_t>(parsedPort);
      raw = raw.substring(0, colon);
    }
  } else if (https) {
    port = 443;
  } else if (http) {
    port = 80;
  }
  raw.trim();
  if (!raw.isEmpty()) {
    host = raw;
  }
}

String setupPortalNetworkOptions() {
  String html;
  int count = WiFi.scanNetworks(false, true);
  if (count <= 0) {
    html += "<option value=\"\">No networks found</option>";
    WiFi.scanDelete();
    return html;
  }
  uint8_t shown = 0;
  std::vector<String> seen;
  for (int i = 0; i < count && shown < 24; ++i) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) {
      continue;
    }
    bool duplicate = false;
    for (const auto& item : seen) {
      if (item == ssid) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }
    seen.push_back(ssid);
    String escaped = setupPortalHtmlEscape(ssid);
    html += "<option value=\"";
    html += escaped;
    html += "\">";
    html += escaped;
    html += " (";
    html += String(WiFi.RSSI(i));
    html += " dBm";
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
      html += ", open";
    }
    html += ")</option>";
    ++shown;
  }
  WiFi.scanDelete();
  if (html.isEmpty()) {
    html += "<option value=\"\">No visible SSID</option>";
  }
  return html;
}

String setupPortalPage(const String& message = String()) {
  String bridgeUrl = gHubHost.isEmpty() ? String("https://bridge.ai.k-digital.pro") : hubBaseUrl();
  String page;
  page.reserve(7200);
  page += F("<!doctype html><html><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>OpenClaw Setup</title><style>"
            ":root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;background:#071018;color:#e9f7ff;"
            "font:15px/1.45 -apple-system,BlinkMacSystemFont,Segoe UI,sans-serif}.wrap{max-width:560px;margin:0 auto;padding:22px}"
            "h1{font-size:24px;margin:0 0 6px}p{color:#9fb7c8;margin:0 0 18px}.panel{border:1px solid #1d4155;background:#0c1a24;"
            "border-radius:8px;padding:16px;margin:14px 0}.row{display:grid;gap:10px;margin:10px 0}label{font-weight:650;color:#c9eefb}"
            "input,select{width:100%;border:1px solid #31576b;background:#071018;color:#fff;border-radius:6px;padding:11px;font:inherit}"
            "button{width:100%;border:0;background:#21c7df;color:#021116;border-radius:6px;padding:12px 14px;font-weight:800;font:inherit}"
            ".hint{font-size:12px;color:#7fa1b4}.msg{padding:10px;border-radius:6px;background:#153626;color:#9ef7bc;margin:10px 0}"
            ".grid{display:grid;grid-template-columns:1fr 100px;gap:10px}@media(max-width:480px){.grid{grid-template-columns:1fr}}"
            "</style></head><body><main class=\"wrap\"><h1>OpenClaw Cardputer Setup</h1>"
            "<p>Connect Wi-Fi and bridge credentials. Saved values are stored only on this device.</p>");
  if (!message.isEmpty()) {
    page += "<div class=\"msg\">";
    page += setupPortalHtmlEscape(message);
    page += "</div>";
  }
  page += F("<form method=\"post\" action=\"/save\"><section class=\"panel\"><h2>Wi-Fi</h2>"
            "<div class=\"row\"><label for=\"scan\">Available networks</label><select id=\"scan\" onchange=\"ssid.value=this.value\">");
  page += setupPortalNetworkOptions();
  page += F("</select><div class=\"hint\">Refresh this page to scan again.</div></div>"
            "<div class=\"row\"><label for=\"ssid\">Network name</label><input id=\"ssid\" name=\"ssid\" required value=\"");
  page += setupPortalHtmlEscape(gWifiSsid);
  page += F("\"></div><div class=\"row\"><label for=\"password\">Password</label>"
            "<input id=\"password\" name=\"password\" type=\"password\" autocomplete=\"current-password\"></div></section>"
            "<section class=\"panel\"><h2>Bridge</h2><div class=\"row\"><label for=\"bridge_url\">Bridge URL</label>"
            "<input id=\"bridge_url\" name=\"bridge_url\" required value=\"");
  page += setupPortalHtmlEscape(bridgeUrl);
  page += F("\"><div class=\"hint\">Example: https://bridge.ai.k-digital.pro</div></div>"
            "<div class=\"row\"><label for=\"device_token\">Device token</label>"
            "<input id=\"device_token\" name=\"device_token\" type=\"password\" autocomplete=\"off\" placeholder=\"");
  page += gDeviceToken.isEmpty() ? "required" : "keep existing token if blank";
  page += F("\"></div><div class=\"hint\">Device ID: ");
  page += setupPortalHtmlEscape(gDeviceId);
  page += F("</div></section><button type=\"submit\">Save and restart</button></form></main></body></html>");
  return page;
}

void setupPortalSendPage(const String& message = String()) {
  gSetupServer.sendHeader("Cache-Control", "no-store");
  gSetupServer.send(200, "text/html; charset=utf-8", setupPortalPage(message));
}

void setupPortalRedirect() {
  gSetupServer.sendHeader("Location", "http://192.168.4.1/", true);
  gSetupServer.send(302, "text/plain", "");
}

void handleSetupPortalSave() {
  String ssid = gSetupServer.arg("ssid");
  String password = gSetupServer.arg("password");
  String bridgeUrl = gSetupServer.arg("bridge_url");
  String token = gSetupServer.arg("device_token");
  ssid.trim();
  bridgeUrl.trim();
  token.trim();
  String host = gHubHost.isEmpty() ? String(kProdBridgeHost) : gHubHost;
  uint16_t port = gHubPort == 0 ? kProdBridgePort : gHubPort;
  parseSetupPortalBridgeUrl(bridgeUrl, host, port);
  if (ssid.isEmpty()) {
    setupPortalSendPage("Wi-Fi network name is required.");
    return;
  }
  if (host.isEmpty()) {
    setupPortalSendPage("Bridge URL is required.");
    return;
  }
  if (token.isEmpty() && gDeviceToken.isEmpty()) {
    setupPortalSendPage("Device token is required.");
    return;
  }
  saveWifiCreds(ssid, password);
  saveHubEndpoint(host, port);
  if (!token.isEmpty()) {
    saveDeviceToken(token);
  }
  gSetupPortalSaved = true;
  gSetupPortalMessage = "Saved. Restarting Cardputer...";
  setupPortalSendPage(gSetupPortalMessage);
  logf("[SETUP] saved ssid=%s bridge=%s:%u token=%s",
       gWifiSsid.c_str(), gHubHost.c_str(), gHubPort, gDeviceToken.isEmpty() ? "empty" : "set");
  setStatus("Setup saved. Restarting", 1500);
  gPendingRestart = true;
}

void startSetupPortal(const char* reason) {
  if (gSetupPortalStarted) {
    gSetupPortalActive = true;
    return;
  }
  IPAddress apIp(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  gSetupPortalSsid = "OpenClaw-" + chipIdSuffix();
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAPConfig(apIp, gateway, subnet);
  bool apOk = WiFi.softAP(gSetupPortalSsid.c_str(), kSetupPortalPassword);
  gSetupDns.start(kSetupPortalDnsPort, "*", apIp);
  gSetupServer.on("/", HTTP_GET, []() { setupPortalSendPage(gSetupPortalMessage); });
  gSetupServer.on("/save", HTTP_POST, handleSetupPortalSave);
  gSetupServer.on("/generate_204", HTTP_GET, setupPortalRedirect);
  gSetupServer.on("/fwlink", HTTP_GET, setupPortalRedirect);
  gSetupServer.on("/hotspot-detect.html", HTTP_GET, setupPortalRedirect);
  gSetupServer.onNotFound(setupPortalRedirect);
  gSetupServer.begin();
  gSetupPortalStarted = true;
  gSetupPortalActive = true;
  gSetupPortalMessage = String("Setup AP started");
  setStatus("Setup AP: " + gSetupPortalSsid, 0);
  pushSystem("Setup Wi-Fi at http://192.168.4.1");
  pushSystem(String("AP: ") + gSetupPortalSsid + " / " + kSetupPortalPassword);
  logf("[SETUP] portal start ok=%d ssid=%s reason=%s",
       static_cast<int>(apOk), gSetupPortalSsid.c_str(), reason ? reason : "-");
}

void serviceSetupPortal() {
  if (!gSetupPortalActive) {
    return;
  }
  gSetupDns.processNextRequest();
  gSetupServer.handleClient();
}

bool ensureWifi() {
  if (gWifiSsid.isEmpty()) {
    pushError("Wi-Fi credentials are missing. Setup AP started.");
    startSetupPortal("wifi-missing");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.begin(gWifiSsid.c_str(), gWifiPassword.c_str());
  setStatus("Connecting to Wi-Fi...");

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiConnectTimeoutMs) {
    handleHostSerialInput();
    delay(120);
  }

  if (WiFi.status() != WL_CONNECTED) {
    pushError("Wi-Fi connection failed. Setup AP started.");
    logf("[WIFI] connect failed ssid=%s", gWifiSsid.c_str());
    startSetupPortal("wifi-connect-failed");
    return false;
  }

  gWifiReady = true;
  setStatus("Wi-Fi connected", 1000);
  pushSystem("Wi-Fi connected: " + WiFi.localIP().toString());
  logf("[WIFI] connected ssid=%s ip=%s", gWifiSsid.c_str(), WiFi.localIP().toString().c_str());
  syncClockFromNtp(true);
  return true;
}

bool ensureWifiForHttp(const char* reason, uint32_t timeoutMs) {
  if (gWifiSsid.isEmpty() || gHubHost.isEmpty()) {
    gWifiReady = false;
    return false;
  }

  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (gWifiReady) {
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    esp_wifi_set_ps(WIFI_PS_NONE);
    return true;
  }

  logf("[WIFI] reconnect for %s", reason ? reason : "http");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.disconnect(false, false);
  delay(80);
  WiFi.begin(gWifiSsid.c_str(), gWifiPassword.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    handleHostSerialInput();
    delay(120);
  }

  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (gWifiReady) {
    logf("[WIFI] reconnect ok reason=%s ip=%s", reason ? reason : "http",
         WiFi.localIP().toString().c_str());
  } else {
    logf("[WIFI] reconnect failed reason=%s", reason ? reason : "http");
  }
  return gWifiReady;
}

void startDeferredBoot() {
  gBootBackgroundActive = true;
  gBootBackgroundStep = 1;
  gBootBackgroundLabel = "storage";
  gBootWifiStarted = false;
  gBootWifiStartMs = 0;
  gBootStepStartedMs = millis();
  setStatus("Starting background services", 1200);
  logf("[BOOT] background start");
}

bool bootBackgroundAllowsConnectivity() {
  return !gBootBackgroundActive || gBootBackgroundStep >= 8;
}

void schedulePostInstallStoryIfNeeded() {
  if (strcmp(kAppVersion, kPostInstallStoryVersion) != 0 || !gOtaPendingVerify) {
    return;
  }
  gPrefs.begin(kPrefsDeviceNs, true);
  String spokenVersion = gPrefs.getString(kPostInstallStoryPrefsKey, "");
  gPrefs.end();
  if (spokenVersion == kAppVersion) {
    return;
  }
  gPostInstallStoryPending = true;
  gPostInstallStoryAttempts = 0;
  gPostInstallStoryNextMs = millis() + 15000U;
  appendRuntimeLog("BOOT", "post-install story queued", true);
}

void servicePostInstallStory() {
  if (!gPostInstallStoryPending || millis() < gPostInstallStoryNextMs) {
    return;
  }
  if (strcmp(kAppVersion, kPostInstallStoryVersion) != 0) {
    gPostInstallStoryPending = false;
    return;
  }
  if (gOtaPendingVerify || !gOtaConfirmedThisBoot) {
    gPostInstallStoryNextMs = millis() + 3000U;
    return;
  }
  if (gBootBackgroundActive || !bootBackgroundAllowsConnectivity() || !gWifiReady || gHubHost.isEmpty() ||
      gOtaApplyInProgress || gTransferUiActive || gRecording || gSubmitting || gThinking || gPlaybackActive ||
      gRealtimeActive || gRealtimeConnected || gRealtimeRecording || gRealtimeAwaiting) {
    gPostInstallStoryNextMs = millis() + 5000U;
    return;
  }

  ++gPostInstallStoryAttempts;
  appendRuntimeLog("BOOT", "post-install story attempt " + String(gPostInstallStoryAttempts), true);
  bool played = requestAndPlayTts(kPostInstallStoryText);
  if (played) {
    gPrefs.begin(kPrefsDeviceNs, false);
    gPrefs.putString(kPostInstallStoryPrefsKey, kAppVersion);
    gPrefs.end();
    gPostInstallStoryPending = false;
    appendRuntimeLog("BOOT", "post-install story played", true);
    return;
  }
  appendRuntimeLog("BOOT", "post-install story tts failed", true);
  if (gPostInstallStoryAttempts >= 3) {
    gPostInstallStoryPending = false;
    return;
  }
  gPostInstallStoryNextMs = millis() + 60000U;
}

void markPomodoroMeditationAssetsSynced() {
  gPrefs.begin(kPrefsDeviceNs, false);
  gPrefs.putString(kPomodoroAssetSyncPrefsKey, kAppVersion);
  gPrefs.end();
}

void maybeSyncPomodoroMeditationAssetsOnBoot() {
  if (strcmp(kAppVersion, kPomodoroAssetSyncVersion) != 0 || !gWifiReady || gHubHost.isEmpty()) {
    return;
  }
  gPrefs.begin(kPrefsDeviceNs, true);
  String syncedVersion = gPrefs.getString(kPomodoroAssetSyncPrefsKey, "");
  gPrefs.end();
  bool present = pomodoroMeditationAssetsPresent();
  if (syncedVersion == kAppVersion && present) {
    return;
  }
  if (present) {
    markPomodoroMeditationAssetsSynced();
    appendRuntimeLog("ASSET", "pomodoro meditations already present", true);
    return;
  }
  appendRuntimeLog("ASSET", "pomodoro meditations sync start", true);
  setStatus("Syncing Pomodoro audio...", 0);
  render();
  bool ok = fetchRemoteAssetManifest() && syncAssetsFromManifest(true) && pomodoroMeditationAssetsPresent();
  if (ok) {
    markPomodoroMeditationAssetsSynced();
    pushSystem("Pomodoro audio synced: RU/EN/ES");
    setStatus("Pomodoro audio synced", 1600);
  } else {
    appendRuntimeLog("ASSET", "pomodoro meditations sync incomplete", true);
    setStatus("Pomodoro audio sync failed", 1800);
  }
}

void beginDeferredWifi() {
  gBootBackgroundLabel = "wifi";
  gBootWifiStarted = true;
  gBootWifiStartMs = millis();
  if (gWifiSsid.isEmpty()) {
    pushError("Wi-Fi credentials are missing. Setup AP started.");
    startSetupPortal("deferred-wifi-missing");
    gBootBackgroundStep = 10;
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.begin(gWifiSsid.c_str(), gWifiPassword.c_str());
  setStatus("Wi-Fi starting", 900);
  logf("[WIFI] deferred connect start ssid=%s", gWifiSsid.c_str());
}

void serviceDeferredBoot() {
  if (!gBootBackgroundActive || gOtaBootApplyMode) {
    return;
  }
  switch (gBootBackgroundStep) {
    case 1:
      gBootBackgroundLabel = "emoji";
      scanEmojiAssets();
      if (gEmojiAssetCount > 0) {
        pushSystem("Emoji SD: " + String(gEmojiAssetCount));
      }
      gBootBackgroundStep = 4;
      gBootStepStartedMs = millis();
      break;
    case 4:
      gBootBackgroundLabel = "manifest";
      appendRuntimeLog("LIB", "audio scan deferred", false);
      loadAssetManifestSummary();
      loadOtaManifestSummary();
      gBootBackgroundStep = 5;
      gBootStepStartedMs = millis();
      break;
    case 5:
      gBootBackgroundLabel = "topics";
      loadConversationContexts();
      gBootBackgroundStep = 6;
      gBootStepStartedMs = millis();
      break;
    case 6:
      beginDeferredWifi();
      gBootBackgroundStep = 7;
      gBootStepStartedMs = millis();
      break;
    case 7:
      gBootBackgroundLabel = "wifi";
      gWifiReady = WiFi.status() == WL_CONNECTED;
      if (gWifiReady) {
        setStatus("Wi-Fi connected", 1000);
        pushSystem("Wi-Fi connected: " + WiFi.localIP().toString());
        logf("[WIFI] connected ssid=%s ip=%s", gWifiSsid.c_str(), WiFi.localIP().toString().c_str());
        syncClockFromNtp(true);
        gBootBackgroundStep = 8;
        gBootStepStartedMs = millis();
      } else if (gBootWifiStarted && millis() - gBootWifiStartMs >= kWifiConnectTimeoutMs) {
        pushError("Wi-Fi connection failed. Setup AP started.");
        logf("[WIFI] deferred connect failed ssid=%s", gWifiSsid.c_str());
        startSetupPortal("deferred-wifi-failed");
        gBootBackgroundStep = 10;
      }
      break;
    case 8:
      gBootBackgroundLabel = "hub";
      startHubWebSocket();
      if (gOtaPendingVerify && probeOtaBridgeHealth(8000)) {
        confirmOtaBootIfPending("bridge health");
      }
      if (gDeviceSettings.checkUpdatesOnBoot) {
        appendRuntimeLog("OTA", "boot remote check deferred", true);
        setStatus("Update check deferred", 900);
      }
      if (!gOtaPendingVerify) {
        maybeSyncPomodoroMeditationAssetsOnBoot();
      }
      if (gDeviceSettings.autoUpdateFirmware && gOtaManifest.parsed &&
          !gOtaManifest.url.isEmpty() && gOtaManifest.version != kAppVersion) {
        applyOtaFromManifest();
      }
      appendRuntimeLog("CTX", "boot topic preload deferred", false);
      gBootBackgroundStep = 10;
      break;
    default:
      gBootBackgroundActive = false;
      gBootBackgroundLabel = "";
      setStatus("Ready", 900);
      logf("[BOOT] background done ms=%u", static_cast<unsigned>(millis()));
      break;
  }
}

void sendDeviceHello() {
  if (!kUseHubWebSocket) {
    return;
  }
  DynamicJsonDocument doc(512);
  doc["type"] = "device_hello";
  doc["device_id"] = gDeviceId;
  doc["display_name"] = gDisplayName;
  doc["device_type"] = kDeviceType;

  JsonObject caps = doc.createNestedObject("capabilities");
  caps["text_input"] = true;
  caps["voice_input"] = true;
  caps["vision"] = false;
  caps["face_animation"] = true;
  caps["presence_scan"] = false;
  caps["face_enrollment"] = false;
  caps["ble_provisioning"] = false;
  caps["device_actions"] = true;
  caps["device_actions_version"] = kDeviceActionCatalogVersion;
  caps["device_actions_count"] = static_cast<uint32_t>(kDeviceActionCatalogCount);

  String payload;
  serializeJson(doc, payload);
  gWs.sendTXT(payload);
}

void handleHubJson(const JsonDocument& doc) {
  const String msgType = doc["type"] | "";
  const String status = doc["status"] | "";

  if (msgType == "assistant_speech_face") {
    const String event = doc["event"] | "";
    if (event == "start") {
      setFaceMode(FaceMode::Speaking);
    } else if (event == "stop" && !gPlaybackActive) {
      setFaceMode(FaceMode::Idle);
    }
    return;
  }

  if (msgType == "gemini_first_token") {
    gThinking = true;
    setFaceMode(FaceMode::Thinking);
    setStatus("Hub is thinking...");
    return;
  }

  if (msgType == "runtime_timezone" || msgType == "runtime_vision" ||
      msgType == "runtime_presence_scan" || msgType == "runtime_wake_word" ||
      msgType == "runtime_live_voice" || msgType == "runtime_sleep_timeout") {
    setStatus("Runtime settings updated", 1200);
    return;
  }

  if (status == "success" && !doc["reply"].isNull()) {
    gSubmitting = false;
    gThinking = false;
    pushAssistant(doc["reply"].as<String>());
    if (!doc["device_actions"].isNull()) {
      executeDeviceActions(doc["device_actions"]);
    }
    setFaceMode(FaceMode::Idle);
    setStatus("Reply received", 1200);
    return;
  }

  if (status == "error" && !doc["message"].isNull()) {
    gSubmitting = false;
    gThinking = false;
    pushError(doc["message"].as<String>());
    setFaceMode(FaceMode::Error);
    setStatus("Hub error", 1800);
    logf("[HUB] error=%s", doc["message"].as<String>().c_str());
  }
}

void wsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      gWsReady = false;
      setStatus("Hub disconnected");
      logf("[WS] disconnected");
      break;
    case WStype_CONNECTED:
      gWsReady = true;
      setStatus("Hub connected", 1200);
      pushSystem("Hub WebSocket connected.");
      sendDeviceHello();
      logf("[WS] connected host=%s port=%u", gHubHost.c_str(), gHubPort);
      break;
    case WStype_TEXT: {
      DynamicJsonDocument doc(1024);
      auto err = deserializeJson(doc, payload, length);
      if (!err) {
        handleHubJson(doc);
      }
      break;
    }
    default:
      break;
  }
}

void startHubWebSocket() {
  if (!kUseHubWebSocket) {
    gWsReady = false;
    setStatus("Hub HTTP mode", 1200);
    return;
  }
  if (gHubHost.isEmpty()) {
    pushError("Hub endpoint is empty. Rebuild with defaults or restore saved config.");
    return;
  }

  gWs.begin(gHubHost.c_str(), gHubPort, kWsPath);
  gWs.onEvent(wsEvent);
  gWs.setReconnectInterval(5000);
  gWs.enableHeartbeat(15000, 3000, 2);
  gLastWsReconnectMs = millis();
  setStatus("Connecting to hub...");
}

void pauseHubWebSocketForVoice() {
  if (!kUseHubWebSocket) {
    gResumeWsAfterVoice = false;
    return;
  }
  if (gHubHost.isEmpty()) {
    return;
  }
  gResumeWsAfterVoice = true;
  if (gWsReady) {
    logf("[WS] pause for voice");
    gWs.disconnect();
    gWsReady = false;
  }
}

void resumeHubWebSocketAfterVoice() {
  if (!kUseHubWebSocket) {
    gResumeWsAfterVoice = false;
    return;
  }
  if (!gResumeWsAfterVoice) {
    return;
  }
  gResumeWsAfterVoice = false;
  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (gWifiReady && !gHubHost.isEmpty()) {
    logf("[WS] resume after voice");
    startHubWebSocket();
  }
}

bool queueRealtimeChunk(uint8_t chunkIndex) {
  return M5Cardputer.Mic.record(gRealtimeInputBuffers[chunkIndex],
                                kRealtimeInputChunkSamples,
                                kRealtimeInputSampleRate);
}

void upsampleRealtimeChunk(const int16_t* input) {
  for (size_t i = 0; i < kRealtimeInputChunkSamples; ++i) {
    const int16_t sample = input[i];
    const size_t out = i * kRealtimeUpsampleRatio;
    for (size_t r = 0; r < kRealtimeUpsampleRatio; ++r) {
      gRealtimeUpsampleBuffer[out + r] = sample;
    }
  }
}

bool sendRealtimeAudioChunk(const int16_t* samples, size_t sampleCount) {
  if (!gRealtimeActive || !gRealtimeConnected) {
    return false;
  }
  String encoded = base64::encode(reinterpret_cast<const uint8_t*>(samples),
                                  sampleCount * sizeof(int16_t));
  encoded.replace("\n", "");
  encoded.replace("\r", "");
  if (encoded.isEmpty() || encoded == "-FAIL-") {
    logf("[RT] audio encode failed samples=%u bytes=%u free=%u largest=%u",
         static_cast<unsigned>(sampleCount),
         static_cast<unsigned>(sampleCount * sizeof(int16_t)),
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    return false;
  }

  static constexpr const char* kAudioPrefix = "{\"type\":\"audio.append\",\"audio\":\"";
  static constexpr const char* kAudioSuffix = "\"}";
  static constexpr size_t kWsHeaderReserve = 14;
  const size_t prefixLen = strlen(kAudioPrefix);
  const size_t suffixLen = strlen(kAudioSuffix);
  const size_t payloadLen = prefixLen + encoded.length() + suffixLen;
  uint8_t* frame = static_cast<uint8_t*>(malloc(kWsHeaderReserve + payloadLen + 1));
  if (!frame) {
    logf("[RT] audio frame oom payload=%u b64=%u free=%u largest=%u",
         static_cast<unsigned>(payloadLen),
         static_cast<unsigned>(encoded.length()),
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    return false;
  }

  char* payload = reinterpret_cast<char*>(frame + kWsHeaderReserve);
  memcpy(payload, kAudioPrefix, prefixLen);
  memcpy(payload + prefixLen, encoded.c_str(), encoded.length());
  memcpy(payload + prefixLen + encoded.length(), kAudioSuffix, suffixLen);
  payload[payloadLen] = '\0';

  bool sent = gRealtimeWs.sendTXT(frame, payloadLen, true);
  static uint32_t audioChunkLogCounter = 0;
  ++audioChunkLogCounter;
  if (!sent || audioChunkLogCounter <= 2 || audioChunkLogCounter % 25 == 0) {
    logf("[RT] audio chunk sent=%d payload=%u b64=%u free=%u largest=%u",
         static_cast<int>(sent),
         static_cast<unsigned>(payloadLen),
         static_cast<unsigned>(encoded.length()),
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
  }
  free(frame);
  return sent;
}

bool commitRealtimeAudioBuffer(const char* logPrefix) {
  DynamicJsonDocument doc(128);
  doc["type"] = "audio.commit";
  String payload;
  serializeJson(doc, payload);
  bool sent = gRealtimeConnected && gRealtimeWs.sendTXT(payload);
  if (!sent) {
    logf("[%s] audio commit send failed connected=%d",
         logPrefix ? logPrefix : "RT",
         static_cast<int>(gRealtimeConnected));
    return false;
  }
  gRealtimeAwaiting = true;
  gRealtimeAwaitingStartedMs = millis();
  setFaceMode(FaceMode::Thinking);
  setStatus("Realtime thinking...");
  setVoiceDiag("realtime: committed");
  return true;
}

void beginRealtimeSyntheticAudioSmoke() {
  gRealtimeSmokeAudioStreaming = true;
  gRealtimeSmokeAudioChunkIndex = 0;
  gRealtimeSmokeAudioNextMs = millis();
  setFaceMode(FaceMode::Listening);
  setStatus("Realtime audio smoke...");
  logf("[SMOKE] realtime audio streaming start");
}

void stopRealtimeAudioSmoke(bool ok) {
  gRealtimeSmokeAudioPending = false;
  gRealtimeSmokeAudioStreaming = false;
  gRealtimeSmokeAudioChunkIndex = 0;
  gRealtimeSmokeAudioNextMs = 0;
  if (!ok) {
    gRealtimeSmokeActive = false;
  }
}

void serviceRealtimeAudioSmoke() {
  constexpr size_t kSmokeChunks = 24;
  constexpr uint32_t kSmokeChunkGapMs = 45;
  if (!gRealtimeSmokeAudioStreaming || !gRealtimeConnected || gRealtimeAwaiting) {
    return;
  }
  uint32_t now = millis();
  if (gRealtimeSmokeAudioNextMs != 0 && now < gRealtimeSmokeAudioNextMs) {
    return;
  }
  if (gRealtimeSmokeAudioChunkIndex >= kSmokeChunks) {
    logf("[SMOKE] realtime audio chunks=%u samples=%u",
         static_cast<unsigned>(gRealtimeSmokeAudioChunkIndex),
         static_cast<unsigned>(gRealtimeSmokeAudioChunkIndex * kRealtimeInputChunkSamples));
    stopRealtimeAudioSmoke(true);
    if (!commitRealtimeAudioBuffer("SMOKE")) {
      gRealtimeSmokeActive = false;
      setStatus("Realtime audio smoke failed", 1600);
      setFaceMode(FaceMode::Error);
      return;
    }
    logf("[SMOKE] realtime audio commit sent");
    return;
  }

  const size_t chunk = gRealtimeSmokeAudioChunkIndex;
  const size_t totalSamples = kSmokeChunks * kRealtimeInputChunkSamples;
  for (size_t i = 0; i < kRealtimeInputChunkSamples; ++i) {
    const size_t sampleIndex = chunk * kRealtimeInputChunkSamples + i;
    int period = (sampleIndex / 1200) % 2 == 0 ? 17 : 29;
    int16_t amp = static_cast<int16_t>((sampleIndex % period) < (period / 2) ? 1800 : -1800);
    if (sampleIndex < 600) {
      amp = static_cast<int16_t>((static_cast<int32_t>(amp) *
                                  static_cast<int32_t>(sampleIndex)) / 600);
    }
    if (totalSamples - sampleIndex < 600) {
      amp = static_cast<int16_t>((static_cast<int32_t>(amp) *
                                  static_cast<int32_t>(totalSamples - sampleIndex)) / 600);
    }
    gRealtimeInputBuffers[0][i] = amp;
  }
  updateVoiceLevelFromSamples(gRealtimeInputBuffers[0], kRealtimeInputChunkSamples);
  upsampleRealtimeChunk(gRealtimeInputBuffers[0]);
  if (!sendRealtimeAudioChunk(gRealtimeUpsampleBuffer, kRealtimeOutputChunkSamples)) {
    logf("[SMOKE] realtime audio send failed chunk=%u", static_cast<unsigned>(chunk));
    stopRealtimeAudioSmoke(false);
    setStatus("Realtime audio smoke failed", 1600);
    setFaceMode(FaceMode::Error);
    return;
  }
  ++gRealtimeSmokeAudioChunkIndex;
  gRealtimeSmokeAudioNextMs = now + kSmokeChunkGapMs;
}
