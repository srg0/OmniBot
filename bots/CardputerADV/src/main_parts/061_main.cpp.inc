  if (gBleActive) {
    renderBleUi();
    renderStatusBar();
  } else {
    switch (gUiMode) {
      case UiMode::Home:
        renderAssistantPulseUi();
        break;
      case UiMode::Face:
        renderFaceOnlyUi();
        break;
      case UiMode::Hero:
        renderHeroUi();
        break;
      case UiMode::Clock:
        renderClockUi();
        break;
      case UiMode::Battery:
        renderBatteryUi();
        break;
      case UiMode::Voice:
        renderVoicePlayerUi();
        break;
      case UiMode::Focus:
        renderFocusUi();
        break;
      case UiMode::Library:
        renderLibraryUi();
        break;
      case UiMode::AudioFolders:
        renderAudioFoldersUi();
        break;
      case UiMode::Notes:
        renderNotesUi();
        break;
      case UiMode::Assets:
        renderAssetsUi();
        break;
      case UiMode::Ota:
        renderOtaUi();
        break;
      case UiMode::Contexts:
        renderContextsUi();
        break;
      case UiMode::Settings:
        renderSettingsUi();
        break;
      case UiMode::Calculator:
        renderCalculatorUi();
        break;
      case UiMode::Logs:
        renderLogsUi();
        break;
      case UiMode::ChatFull:
      case UiMode::ChatFace:
      default:
        renderChatUi();
        break;
    }
  }
  gCanvas.pushSprite(0, 0);
  uint32_t renderCostMs = millis() - renderStartMs;
  if (renderCostMs >= 45U && millis() - gLastUiSlowLogMs >= 2000U) {
    gLastUiSlowLogMs = millis();
    logf("[UI] render slow mode=%d thinking=%d recording=%d playback=%d ms=%u",
         static_cast<int>(gUiMode), gThinking ? 1 : 0, gRecording ? 1 : 0,
         gPlaybackActive ? 1 : 0, static_cast<unsigned>(renderCostMs));
  }
}

void renderBootSplash(const char* stage) {
  uint32_t renderStartMs = millis();
  auto& d = M5Cardputer.Display;
  const uint16_t bg = rgb565_local(3, 8, 14);
  const uint16_t panel = rgb565_local(4, 10, 14);
  const uint16_t panelLine = rgb565_local(36, 58, 66);
  const uint16_t mint = rgb565_local(111, 255, 230);
  const uint16_t mintDim = rgb565_local(32, 94, 89);
  const uint16_t gold = rgb565_local(255, 211, 106);
  const uint16_t muted = rgb565_local(135, 154, 149);
  uint8_t stageIndex = 0;
  String stageName = stage && stage[0] ? String(stage) : String("");
  if (stageName == "Display") {
    stageIndex = 1;
  } else if (stageName == "Audio") {
    stageIndex = 2;
  } else if (stageName == "Storage") {
    stageIndex = 3;
  } else if (stageName == "Input") {
    stageIndex = 4;
  } else if (stageName == "Config") {
    stageIndex = 5;
  } else if (stageName == "Wi-Fi") {
    stageIndex = 6;
  }

  d.fillScreen(bg);
  d.fillTriangle(0, 104, 240, 40, 240, 135, rgb565_local(5, 18, 23));
  d.fillRoundRect(7, 7, 226, 121, 9, panel);
  d.drawRoundRect(7, 7, 226, 121, 9, panelLine);

  d.setFont(&fonts::Font2);
  d.setTextSize(1);
  d.fillRoundRect(15, 14, 86, 17, 7, rgb565_local(6, 16, 13));
  d.drawRoundRect(15, 14, 86, 17, 7, mint);
  d.setTextColor(TFT_WHITE, rgb565_local(6, 16, 13));
  d.setCursor(23, 18);
  d.print("ADV ");
  d.setTextColor(mint, rgb565_local(6, 16, 13));
  d.print(kAppVersion);

  uint8_t pct = static_cast<uint8_t>(clampValue<int>(12 + stageIndex * 14, 12, 96));
  String pctText = String(pct) + "%";
  d.setTextColor(gold, panel);
  d.setCursor(220 - d.textWidth(pctText), 18);
  d.print(pctText);
  d.drawRoundRect(109, 18, 60, 4, 2, panelLine);
  d.fillRoundRect(111, 19, max<int>(2, pct * 56 / 100), 2, 1, mint);

  const char* names[] = {"DSP", "AUD", "SD", "KEY", "CFG", "NET"};
  for (int i = 0; i < 6; ++i) {
    int16_t x = 22 + i * 34;
    uint16_t fill = (i + 1 <= stageIndex) ? rgb565_local(13, 46, 43) : rgb565_local(7, 17, 21);
    uint16_t line = (i + 1 <= stageIndex) ? mint : rgb565_local(34, 47, 52);
    d.fillRoundRect(x, 48, 27, 25, 6, fill);
    d.drawRoundRect(x, 48, 27, 25, 6, line);
    d.setFont(&fonts::Font0);
    d.setTextColor((i + 1 <= stageIndex) ? TFT_WHITE : muted, fill);
    d.setCursor(x + (27 - d.textWidth(names[i])) / 2, 57);
    d.print(names[i]);
  }

  const char* states[] = {"BOOT", "CORE", "I/O", "NET"};
  const char* label = states[clampValue<int>((stageIndex + 1) / 2, 0, 3)];
  d.setFont(&fonts::Font4);
  d.setTextColor(TFT_WHITE, panel);
  int16_t tw = d.textWidth(label);
  d.setCursor((240 - tw) / 2, 91);
  d.print(label);
  d.setFont(&fonts::Font2);
  d.setTextColor(muted, panel);
  String subtitle = stage && stage[0] ? String("boot / ") + stage : "boot";
  d.setCursor((240 - d.textWidth(subtitle)) / 2, 115);
  d.print(subtitle);
  Serial.printf("[UI] boot stage=%s ms=%u\n", stage && stage[0] ? stage : "-",
                static_cast<unsigned>(millis() - renderStartMs));
}

}  // namespace

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
  gCanvas.setColorDepth(8);
  gCanvas.createSprite(240, 135);
  gCanvasSpriteActive = true;
  gCanvas.setTextWrap(false);
  gCanvas.setFont(&fonts::Font2);
  gCanvas.setTextSize(1);
  configureEmojiRendering();
  gOtaBootApplyMode = readOtaApplyBootRequest();

  if (!gOtaBootApplyMode) {
    renderBootSplash("Audio");
    initSpeaker();
  }
  renderBootSplash("Storage");
  initStorage();
  if (!gOtaBootApplyMode) {
    renderBootSplash("Input");
    randomSeed(micros());
    scheduleBlink();
    gImuEnabled = M5.Imu.isEnabled();
    logf("[IMU] enabled=%d type=%d", gImuEnabled, static_cast<int>(M5.Imu.getType()));
    setupKeyboardInput();
    gTimeValid = hasValidSystemTime();
  }

  renderBootSplash("Config");
  loadConfig();
  loadFocusSettings();
  loadDeviceSettings();
  beginOtaBootGuard();
  if (gOtaBootApplyMode) {
    Serial.printf("[OTA] clean boot updater heap=%u largest=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    renderBootSplash("Wi-Fi");
    if (ensureWifi()) {
      setStatus("OTA clean updater", 0);
      render();
      fetchRemoteOtaManifest();
      applyOtaFromManifest();
    } else {
      clearOtaApplyBootRequest();
      setStatus("OTA Wi-Fi failed", 2200);
    }
    render();
    return;
  }

  loadPetState();
  pushSystem("Device ready: " + gDeviceId);
  pushSystem("Ctrl+Space = EN/RU");
  pushSystem("Voice: Ctrl x2 / Ctrl+V / hold G0");
  pushSystem("Tab screen, Tab+Down app, Ctrl+L menu");
  pushSystem("/pet = OpenClaw pet, /petreset resets");
  pushSystem(String("FW ") + kAppVersion + " " + String(kBuildGitSha).substring(0, 10));
  logf("[BOOT] device_id=%s", gDeviceId.c_str());
  logf("[BOOT] version=%s git=%s build=%s", kAppVersion, kBuildGitSha, kBuildTime);
  logHeapStats("boot-before-wifi");

  renderBootSplash("Wi-Fi");
  if (ensureWifi()) {
    render();
    startHubWebSocket();
    if (gOtaPendingVerify && probeOtaBridgeHealth(8000)) {
      confirmOtaBootIfPending("bridge health");
    }
    if (gDeviceSettings.checkUpdatesOnBoot) {
      appendRuntimeLog("OTA", "boot remote check deferred", true);
      setStatus("Update check deferred", 900);
    }
    if (gDeviceSettings.autoUpdateFirmware && gOtaManifest.parsed &&
        !gOtaManifest.url.isEmpty() && gOtaManifest.version != kAppVersion) {
      applyOtaFromManifest();
    }
    appendRuntimeLog("CTX", "boot topic preload deferred", false);
  }

  render();
}

void loop() {
  handleHostSerialInput();
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
    if (gRealtimeRestartAfterPlayback && gRealtimeHandsFreeActive) {
      gRealtimeNextListenMs = millis() + 1800;
    }
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
  uint32_t renderInterval = kRenderIntervalMs;
  if (gRecording || gSubmitting || gThinking) {
    renderInterval = (gThinking && !gRecording && gUiMode == UiMode::Home)
                         ? kThinkRenderIntervalMs
                         : kBusyRenderIntervalMs;
  }
  if (now - gLastRenderMs >= renderInterval) {
    render();
    gLastRenderMs = now;
  }
}
