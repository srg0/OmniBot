void stopRealtimeRecordingAndCommit() {
  if (!gRealtimeRecording) {
    return;
  }
  gRealtimeRecording = false;
  pulseButtonPress();
  drainRealtimeRecording();
  stopMic();
  gRealtimeInflightChunks.clear();
  DynamicJsonDocument doc(128);
  doc["type"] = "audio.commit";
  String payload;
  serializeJson(doc, payload);
  bool sent = gRealtimeConnected && gRealtimeWs.sendTXT(payload);
  if (!sent) {
    appendRuntimeLog("RT", "commit send failed", true);
    setStatus("Realtime commit failed", 1500);
    setFaceMode(FaceMode::Error);
    gRealtimeAwaiting = false;
    return;
  }
  gRealtimeAwaiting = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Realtime thinking...");
  setVoiceDiag("realtime: committed");
  appendRuntimeLog("RT", "record commit", false);
}

void toggleRealtimeVoice() {
  if (gRealtimeRecording) {
    stopRealtimeRecordingAndCommit();
    return;
  }
  if (!gRealtimeActive || !gRealtimeConnected) {
    startRealtimeSession(true);
    return;
  }
  if (gRealtimeAwaiting) {
    setStatus("Realtime waiting...", 700);
    return;
  }
  startRealtimeRecording();
}

void serviceRealtimeVoice() {
  if (!gRealtimeActive) {
    return;
  }
  gRealtimeWs.loop();
  serviceRealtimeRecording(true);
}

void maintainConnectivity() {
  bool wifiNow = WiFi.status() == WL_CONNECTED;

  if (!wifiNow) {
    if (gWifiReady) {
      gWifiReady = false;
      gWsReady = false;
      setStatus("Wi-Fi lost", 1500);
      pushError("Wi-Fi disconnected.");
      logf("[WIFI] disconnected");
    }
    uint32_t now = millis();
    if (now - gLastWifiReconnectMs >= kWifiReconnectIntervalMs && !gSubmitting && !gRecording) {
      gLastWifiReconnectMs = now;
      logf("[WIFI] background reconnect attempt");
      ensureWifiForHttp("background-reconnect", 3000);
    }
    return;
  }

  if (!gWifiReady) {
    gWifiReady = true;
    setStatus("Wi-Fi restored", 1200);
    pushSystem("Wi-Fi restored: " + WiFi.localIP().toString());
    logf("[WIFI] restored ip=%s", WiFi.localIP().toString().c_str());
    syncClockFromNtp(true);
    if (kUseHubWebSocket && !gBleActive && !gHubHost.isEmpty() && !gWsReady && !gResumeWsAfterVoice) {
      logf("[WS] restart after wifi restore");
      startHubWebSocket();
    }
  }
}

bool handleLocalCommand(const String& input) {
  String cmd = input;
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "/help" || cmd == "help" || cmd == "commands" || cmd == "actions" ||
      cmd == "команды" || cmd == "помощь") {
    pushSystem("Ctrl+Space = EN/RU");
    pushSystem("Voice: Ctrl x2, Ctrl+V, or hold G0");
    pushSystem("Tab = next screen, Tab+Down/Right = next app");
    pushSystem("Ctrl+L = launcher map, arrows = group/item");
    pushSystem("Ctrl+D = debug; type: voice focus topics pet notes");
    pushSystem("Pet: F food P play C clean S sleep H hunt");
    pushSystem("Agent actions: focus, ui.open, audio/voice, topics, pet, settings");
    pushSystem("Examples: start pomodoro 25; open library; next topic");
    return true;
  }

  if (cmd == "/home" || cmd == "/pulse") {
    setUiMode(UiMode::Home);
    return true;
  }

  if (cmd == "/status") {
    pushSystem("device_id=" + gDeviceId);
    pushSystem("wifi=" + String(gWifiReady ? WiFi.localIP().toString() : "offline"));
    pushSystem("hub=" + (gHubHost.isEmpty() ? String("unset") : gHubHost + ":" + String(gHubPort)));
    pushSystem("ctx=" + currentConversationKey());
    return true;
  }

  if (cmd == "/btsetup") {
    pushSystem("Switching to BLE provisioning mode.");
    startBleProvisioning();
    return true;
  }

  if (cmd == "/clearwifi") {
    pushSystem("Clearing Wi-Fi credentials and rebooting.");
    clearWifiCreds();
    delay(400);
    ESP.restart();
    return true;
  }

  if (cmd == "/dbg") {
    gDebugOverlayVisible = !gDebugOverlayVisible;
    setStatus(gDebugOverlayVisible ? "Debug overlay on" : "Debug overlay off", 1000);
    return true;
  }

  if (cmd == "/face") {
    setUiMode(UiMode::Face);
    return true;
  }

  if (cmd == "/chat") {
    setUiMode(UiMode::ChatFull);
    return true;
  }

  if (cmd == "/hero" || cmd == "/pet" || cmd == "/tama") {
    setUiMode(UiMode::Hero);
    return true;
  }

  if (cmd == "/petreset" || cmd == "/tamareset") {
    resetPet(true);
    setUiMode(UiMode::Hero);
    return true;
  }

  if (cmd == "/clock") {
    setUiMode(UiMode::Home);
    return true;
  }

  if (cmd == "/voice") {
    setUiMode(UiMode::Voice);
    return true;
  }

  if (cmd == "/battery") {
    setUiMode(UiMode::Battery);
    return true;
  }

  if (cmd == "/focus") {
    setUiMode(UiMode::Focus);
    return true;
  }

  if (cmd == "/focusreset" || cmd == "/freset") {
    focusReset();
    setUiMode(UiMode::Focus);
    return true;
  }

  if (cmd == "/library") {
    scanAudioAssets();
    setUiMode(UiMode::Library);
    return true;
  }

  if (cmd == "/assets") {
    loadAssetManifestSummary();
    scanAudioAssets();
    setUiMode(UiMode::Assets);
    return true;
  }

  if (cmd == "/ota") {
    loadOtaManifestSummary();
    setUiMode(UiMode::Ota);
    return true;
  }

  if (cmd == "/contexts" || cmd == "/topics") {
    loadConversationContexts();
    setUiMode(UiMode::Contexts);
    startTopicTask(true, false, true, "Syncing topics...");
    return true;
  }

  if (cmd == "/settings") {
    setUiMode(UiMode::Settings);
    return true;
  }

  if (cmd == "/syncassets") {
    fetchRemoteAssetManifest();
    syncAssetsFromManifest();
    setUiMode(UiMode::Assets);
    return true;
  }

  if (cmd == "/fetchota") {
    fetchRemoteOtaManifest();
    setUiMode(UiMode::Ota);
    return true;
  }

  if (cmd == "/sendlogs") {
    uploadRuntimeLogs();
    setUiMode(UiMode::Logs);
    return true;
  }

  if (cmd == "/logs") {
    setUiMode(UiMode::Logs);
    return true;
  }

