bool ensureWifi() {
  if (gWifiSsid.isEmpty()) {
    pushSystem("No Wi-Fi credentials. Entering BLE setup.");
    startBleProvisioning();
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
    delay(120);
  }

  if (WiFi.status() != WL_CONNECTED) {
    pushError("Wi-Fi connection failed. Entering BLE setup.");
    startBleProvisioning();
    logf("[WIFI] connect failed ssid=%s", gWifiSsid.c_str());
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
  caps["ble_provisioning"] = true;
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
    pushError("Hub endpoint is empty. Use BLE provisioning.");
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
  if (gBleActive || gHubHost.isEmpty()) {
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
  if (!gBleActive && gWifiReady && !gHubHost.isEmpty()) {
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
