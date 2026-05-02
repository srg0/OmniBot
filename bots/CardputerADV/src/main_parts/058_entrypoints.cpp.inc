void setup() {
  Serial.begin(115200);
  Serial.println("[BOOT] serial ready");
  delay(800);
  ensureBangkokTimezone();

  auto cfg = M5.config();
  cfg.internal_imu = true;
  M5Cardputer.begin(cfg, false);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(180);
  M5Cardputer.Display.setFont(&fonts::Font2);
  M5Cardputer.Display.setTextSize(1);
  renderBootSplash("Display");
  gCanvas.createSprite(240, 135);
  gCanvas.setTextWrap(false);
  gCanvas.setFont(&fonts::Font2);
  gCanvas.setTextSize(1);
  configureEmojiRendering();

  renderBootSplash("Audio");
  initSpeaker();
  renderBootSplash("Storage");
  initStorage();
  renderBootSplash("Input");
  randomSeed(micros());
  scheduleBlink();
  gImuEnabled = M5.Imu.isEnabled();
  logf("[IMU] enabled=%d type=%d", gImuEnabled, static_cast<int>(M5.Imu.getType()));
  setupKeyboardInput();
  gTimeValid = hasValidSystemTime();

  renderBootSplash("Config");
  loadConfig();
  loadFocusSettings();
  loadDeviceSettings();
  loadPetState();
  beginOtaBootGuard();
  pushSystem("Device ready: " + gDeviceId);
  pushSystem("Ctrl+Space = EN/RU");
  pushSystem("Voice: Ctrl x2 / Ctrl+V / hold G0");
  pushSystem("Tab screen, Tab+Down app, Ctrl+L menu");
  pushSystem("/pet = OpenClaw pet, /petreset resets");
  pushSystem(String("FW ") + kAppVersion + " " + String(kBuildGitSha).substring(0, 10));
  logf("[BOOT] device_id=%s", gDeviceId.c_str());
  logf("[BOOT] version=%s git=%s build=%s", kAppVersion, kBuildGitSha, kBuildTime);

  renderBootSplash("Wi-Fi");
  if (ensureWifi()) {
    render();
    startHubWebSocket();
    if (gOtaPendingVerify && probeOtaBridgeHealth(8000)) {
      confirmOtaBootIfPending("bridge health");
    }
    if (gDeviceSettings.checkUpdatesOnBoot) {
      setStatus("Checking updates...", 900);
      render();
      fetchRemoteOtaManifest();
      fetchRemoteAssetManifest();
    }
    if (gDeviceSettings.autoUpdateFirmware && gOtaManifest.parsed &&
        !gOtaManifest.url.isEmpty() && gOtaManifest.version != kAppVersion) {
      applyOtaFromManifest();
    }
    startTopicTask(true, false, false, "Topics preload...");
  }

  render();
}

void loop() {
  M5Cardputer.update();
  refreshKeyboardState();
  updateImuMotion();
  updateStatusLed();
  gTimeValid = hasValidSystemTime();

  if (gPendingRestart) {
    delay(600);
    ESP.restart();
  }

  if (!gBleActive) {
    maintainConnectivity();
    if (kUseHubWebSocket) {
      gWs.loop();
    }
    serviceOtaBootGuard();
    syncClockFromNtp(false);
    updateBatterySnapshot();
    updateFocusTimer();
    serviceFocusMetronome();
    servicePet();
    processCompletedTopicTask();
    processCompletedTextTurn();
    updateG0VoiceTrigger();
    updateVoiceTrigger();
    pollTyping();
    serviceRealtimeVoice();
    serviceRecordingToFile();
    servicePlaybackStream();
  }

  if (gPlaybackActive && !gPlaybackStreaming && !M5Cardputer.Speaker.isPlaying()) {
    gPlaybackActive = false;
    if (gDynamicPlaybackBuffer) {
      free(gDynamicPlaybackBuffer);
      gDynamicPlaybackBuffer = nullptr;
    }
    if (gTtsBuffer) {
      releaseTtsBuffer();
    }
    if (gRecordBuffer && !gRecording && !gSubmitting && !gThinking) {
      releaseRecordBuffer();
    }
    if (!gRecording && !gThinking && !gSubmitting) {
      setFaceMode(FaceMode::Idle);
      setStatus("Reply finished", 1000);
    }
  }

  updateBlink();

  uint32_t now = millis();
  uint32_t renderInterval = (gRecording || gSubmitting || gThinking) ? kBusyRenderIntervalMs : kRenderIntervalMs;
  if (now - gLastRenderMs >= renderInterval) {
    render();
    gLastRenderMs = now;
  }
}
