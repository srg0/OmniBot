      size_t written = gRealtimeReplyFile.write(decoded, decodedLen);
      gRealtimeReplyBytes += written;
      gRealtimeGotAudio = true;
      setFaceMode(FaceMode::Speaking);
      setStatus("Realtime audio...", 700);
    }
    free(decoded);
    return;
  }
  if (type == "audio.done") {
    closeRealtimeReplyFile();
    if (gRealtimeReplyBytes > 0) {
      appendRuntimeLog("RT", "audio bytes=" + String(static_cast<unsigned>(gRealtimeReplyBytes)), false);
      startPlaybackStreamFromRawPcmFile(kRealtimeReplyPcmPath,
                                        kRealtimeModelSampleRate,
                                        gRealtimeReplyBytes,
                                        "realtime reply");
    } else {
      appendRuntimeLog("RT", "audio done empty", true);
    }
    return;
  }
  if (type == "realtime.error" || type == "protocol_error") {
    const String message = doc["message"] | "realtime error";
    appendRuntimeLog("RT", "error " + message, true);
    pushError("Realtime: " + message);
    setFaceMode(FaceMode::Error);
    setStatus("Realtime error", 1800);
    gRealtimeAwaiting = false;
    return;
  }
  if (type == "realtime.closed") {
    const String reason = doc["reason"] | "closed";
    gRealtimeActive = false;
    gRealtimeConnected = false;
    gRealtimeAwaiting = false;
    closeRealtimeReplyFile();
    setStatus("Realtime closed", 1000);
    appendRuntimeLog("RT", "closed " + reason, false);
  }
}

void realtimeWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      gRealtimeConnected = false;
      if (gRealtimeActive) {
        appendRuntimeLog("RT", "disconnected", true);
        setStatus("Realtime disconnected", 1200);
      }
      break;
    case WStype_CONNECTED: {
      DynamicJsonDocument doc(768);
      doc["type"] = "connect";
      doc["device_id"] = gDeviceId;
      doc["device_token"] = gDeviceToken;
      doc["topic_key"] = currentConversationKey();
      doc["sample_rate"] = kRealtimeModelSampleRate;
      String out;
      serializeJson(doc, out);
      gRealtimeWs.sendTXT(out);
      setStatus("Realtime auth...", 700);
      break;
    }
    case WStype_TEXT: {
      DynamicJsonDocument doc(8192);
      auto err = deserializeJson(doc, payload, length);
      if (err) {
        appendRuntimeLog("RT", "json parse failed", true);
        setStatus("Realtime JSON failed", 1200);
        break;
      }
      handleRealtimeJson(doc);
      break;
    }
    case WStype_ERROR:
      appendRuntimeLog("RT", "ws error", true);
      setStatus("Realtime WS error", 1200);
      break;
    default:
      break;
  }
}

void startRealtimeSession(bool startAfterReady) {
  if (gRealtimeActive) {
    if (gRealtimeConnected && startAfterReady) {
      startRealtimeRecording();
    } else {
      gRealtimeStartAfterReady = startAfterReady;
    }
    return;
  }
  if (!gWifiReady || gHubHost.isEmpty()) {
    pushError("Realtime needs Wi-Fi and bridge.");
    setStatus("Realtime unavailable", 1400);
    setFaceMode(FaceMode::Error);
    return;
  }
  resetRealtimeTurnState();
  gRealtimeActive = true;
  gRealtimeConnected = false;
  gRealtimeStartAfterReady = startAfterReady;
  gRealtimeAwaiting = false;
  if (hubUsesTls()) {
    gRealtimeWs.beginSSL(gHubHost.c_str(), gHubPort, kRealtimeWsPath);
  } else {
    gRealtimeWs.begin(gHubHost.c_str(), gHubPort, kRealtimeWsPath);
  }
  gRealtimeWs.onEvent(realtimeWsEvent);
  gRealtimeWs.setReconnectInterval(0);
  gRealtimeWs.enableHeartbeat(15000, 3000, 2);
  setStatus("Realtime connecting...");
  appendRuntimeLog("RT", "connect host=" + gHubHost + ":" + String(gHubPort), false);
}

void startRealtimeRecording() {
  if (!gRealtimeActive || !gRealtimeConnected) {
    startRealtimeSession(true);
    return;
  }
  if (gRealtimeRecording || gRecording || gSubmitting || gThinking) {
    setStatus("Realtime busy", 800);
    return;
  }
  if (gPlaybackActive) {
    stopPlaybackStream();
    gPlaybackActive = false;
  }
  resetRealtimeTurnState();
  gRealtimeInflightChunks.clear();
  gRealtimeNextChunkIndex = 0;
  gRealtimeRecording = true;
  gRealtimeAwaiting = false;
  gRealtimeStartedMs = millis();
  gVoiceLevel8 = 36;
  pulseButtonPress();
  startMic();
  serviceRealtimeRecording(true);
  setFaceMode(FaceMode::Listening);
  setStatus("Realtime listening...");
  setVoiceDiag("realtime: recording");
  appendRuntimeLog("RT", "record start", false);
}

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
