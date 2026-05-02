    gThinking = false;
    resumeHubWebSocketAfterVoice();
  };

  if (gRecordingToFile) {
    rememberVoiceNoteFromPcmFile("My voice", "Outbound voice note", pcmUploadPath, dataSize, kMicSampleRate, false);
  } else {
    uint8_t wavHeader[44];
    writeWavHeader(wavHeader, dataSize, kMicSampleRate);
    rememberVoiceNote("My voice", "Outbound voice note", wavHeader, sizeof(wavHeader),
                      reinterpret_cast<const uint8_t*>(gRecordBuffer), dataSize, kMicSampleRate, false);
  }
  logHeapStats("voice-upload");

  String turnId = makeClientTurnId("voice");
  setStatus("Uploading voice...");
  render();
  gLastRenderMs = millis();
  appendRuntimeLog("VOICE", "upload start bytes=" + String(static_cast<unsigned>(dataSize)) +
                            " ctx=" + currentConversationKey() +
                            " turn=" + turnId, true);
  int statusCode = -1;
  String response;
  bool transportOk = false;
  bool prevRenderPump = gBlockingRenderPumpEnabled;
  gBlockingRenderPumpEnabled = true;
  gLastBlockingRenderPumpMs = 0;
  pumpBlockingNetworkUi();
  if (gRecordingToFile) {
    setHttpDiag("voice_pcm: raw file " + String(static_cast<unsigned>(dataSize / 1024)) + "k");
    transportOk = postRawVoiceTurn(nullptr, pcmUploadPath, dataSize, turnId, statusCode, response);
  } else {
    setHttpDiag("voice_pcm: raw ram " + String(static_cast<unsigned>(dataSize / 1024)) + "k");
    transportOk = postRawVoiceTurn(reinterpret_cast<const uint8_t*>(gRecordBuffer), "", dataSize,
                                   turnId, statusCode, response);
  }
  gBlockingRenderPumpEnabled = prevRenderPump;
  finishVoiceTurn();
  if (gRecordingToFile) {
    fs::FS* fs = activeVoiceFs();
    if (fs) {
      fsRemove(*fs, pcmUploadPath);
    }
    gRecordingToFile = false;
    gActiveRecordPath = "";
    gActiveRecordBytes = 0;
    gActiveRecordCommittedBytes = 0;
    gActiveRecordExpectedBytes = 0;
  } else {
    releaseRecordBuffer();
  }

  if (!transportOk || statusCode <= 0) {
    String transportError = gLastHttpDiag;
    if (transportError.isEmpty()) {
      transportError = statusCode <= 0 ? HTTPClient::errorToString(statusCode) : String("unknown");
    }
    pushError("Voice transport failed: " + transportError);
    setFaceMode(FaceMode::Error);
    setStatus("Voice transport failed", 2200);
    setVoiceDiag("voice: transport " + String(statusCode));
    logf("[VOICE] raw upload failed code=%d err=%s bytes=%u",
         statusCode,
         transportError.c_str(),
         static_cast<unsigned>(dataSize));
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    logf("[VOICE] HTTP error status=%d body=%s", statusCode, response.c_str());
    if (statusCode == 504) {
      pushError("Assistant reply timeout.");
      setStatus("Assistant timeout", 2200);
      setVoiceDiag("voice: assistant timeout");
    } else {
      pushError("Voice HTTP error: " + String(statusCode));
      setStatus("Voice turn failed", 2000);
      setVoiceDiag("voice: http " + String(statusCode));
    }
    if (!response.isEmpty()) {
      pushError(response);
    }
    setFaceMode(FaceMode::Error);
    setHttpDiag("voice: http " + String(statusCode));
    return false;
  }

  DynamicJsonDocument resp(4096);
  auto err = deserializeJson(resp, response);
  if (err) {
    pushError("Could not parse voice reply.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice parse failed", 2000);
    setVoiceDiag("voice: json parse failed");
    logf("[VOICE] JSON parse failed body=%s", response.c_str());
    return false;
  }

  String transcript = resp["transcript"] | resp["text"] | "";
  String reply = resp["reply"] | resp["full_text"] | "";
  String shortText = resp["short_text"] | "";
  String deviceActionsJson;
  if (!resp["device_actions"].isNull()) {
    serializeJson(resp["device_actions"], deviceActionsJson);
  }
  if (reply.isEmpty()) {
    reply = shortText;
  }
  String audioUrl;
  String audioPath;
  String audioTitle;
  String audioSha;
  uint32_t audioSize = 0;
  if (resp["audio"].is<JsonObject>()) {
    JsonObject audio = resp["audio"].as<JsonObject>();
    audioUrl = audio["url"] | "";
    audioPath = audio["path"] | "";
    audioTitle = audio["title"] | "";
    audioSha = audio["sha256"] | "";
    audioSize = audio["size"] | 0;
  }
  logf("[VOICE] transcript=%s", transcript.c_str());
  logf("[VOICE] reply=%s", reply.c_str());
  if (!transcript.isEmpty()) {
    pushUser(transcript);
    setVoiceDiag("voice: transcript ok");
  }
  if (!reply.isEmpty()) {
    appendAssistantReply(reply);
    if (!downloadAndPlayResponseAudio(audioUrl, audioPath, audioSha, audioSize, audioTitle, reply) &&
        !requestAndPlayTts(reply)) {
      logf("[VOICE] TTS request failed");
      setFaceMode(FaceMode::Idle);
      setStatus("Reply text only", 1200);
    }
  } else {
    setFaceMode(FaceMode::Idle);
    setStatus("No reply", 1200);
  }
  if (!deviceActionsJson.isEmpty()) {
    executeDeviceActionsJson(deviceActionsJson);
  }
  return true;
}

class ProvisionCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    std::string rx = characteristic->getValue();
    if (rx.empty() || rx.size() > 900) {
      return;
    }

    DynamicJsonDocument doc(768);
    DeserializationError err = deserializeJson(doc, rx);
    if (err) {
      return;
    }

    bool changed = false;

    if (!doc["ssid"].isNull()) {
      String ssid = doc["ssid"].as<String>();
      ssid.trim();
      if (!ssid.isEmpty()) {
        saveWifiCreds(ssid, doc["password"].as<String>());
        changed = true;
      }
    }

    if (!doc["hub_ip"].isNull()) {
      String host = doc["hub_ip"].as<String>();
      host.trim();
      uint16_t port = static_cast<uint16_t>(doc["hub_port"] | kDefaultHubPort);
      if (!host.isEmpty()) {
        saveHubEndpoint(host, port);
        changed = true;
      }
    }

    if (changed) {
      setStatus("Provisioning saved, rebooting...");
      gPendingRestart = true;
    }
  }
};

void startBleProvisioning() {
  if (gBleActive) {
    return;
  }

  if (gWsReady) {
    gWs.disconnect();
    gWsReady = false;
  }
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  delay(100);

  BLEDevice::init(kBleServiceName);
  BLEServer* server = BLEDevice::createServer();
  BLEService* service = server->createService(kBleServiceUuid);
  BLECharacteristic* characteristic = service->createCharacteristic(
      kBleWifiCharUuid, BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new ProvisionCallbacks());
  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kBleServiceUuid);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  gBleActive = true;
  gWifiReady = false;
  setStatus("Waiting for BLE provisioning");
  pushSystem("Bluetooth provisioning started.");
}

void syncClockFromNtp(bool force) {
  ensureBangkokTimezone();
  if (!gWifiReady) {
    return;
  }
  if (!force) {
    if (gTimeValid && gLastTimeSyncMs != 0 && millis() - gLastTimeSyncMs < kTimeResyncMs) {
      return;
    }
    if (gLastTimeSyncAttemptMs != 0 && millis() - gLastTimeSyncAttemptMs < kTimeSyncRetryMs) {
