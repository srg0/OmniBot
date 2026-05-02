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
}

void renderBootSplash(const char* stage) {
  auto& d = M5Cardputer.Display;
  const uint16_t bg = rgb565_local(3, 8, 14);
  const uint16_t panel = rgb565_local(2, 6, 7);
  const uint16_t panelLine = rgb565_local(38, 53, 50);
  const uint16_t mint = rgb565_local(111, 255, 230);
  const uint16_t mintDim = rgb565_local(32, 94, 89);
  const uint16_t gold = rgb565_local(255, 211, 106);
  const uint16_t goldDim = rgb565_local(117, 82, 32);
  const uint16_t rungFront = rgb565_local(154, 169, 164);
  const uint16_t rungBack = rgb565_local(54, 70, 65);
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

  auto line = [&](int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, bool thick) {
    d.drawLine(x1, y1, x2, y2, color);
    if (thick) {
      d.drawLine(x1, y1 + 1, x2, y2 + 1, color);
    }
  };

  auto drawHelix = [&]() {
    constexpr int16_t axisY = 66;
    constexpr int16_t startX = 41;
    constexpr int16_t step = 14;
    constexpr int16_t count = 12;
    constexpr float amp = 24.0f;
    float phase = stageIndex * 0.36f;

    for (int i = 0; i < count; ++i) {
      float theta = phase + i * 0.72f;
      float s = sinf(theta);
      float c = cosf(theta);
      int16_t x = startX + i * step;
      int16_t yA = axisY + static_cast<int16_t>(s * amp);
      int16_t yB = axisY - static_cast<int16_t>(s * amp);
      bool frontA = c > 0.0f;
      line(x, yA, x, yB, frontA ? rungFront : rungBack, frontA);
    }

    for (int pass = 0; pass < 2; ++pass) {
      for (int i = 0; i < count; ++i) {
        float theta = phase + i * 0.72f;
        float s = sinf(theta);
        float c = cosf(theta);
        int16_t x = startX + i * step;
        int16_t y = axisY + static_cast<int16_t>((pass == 0 ? s : -s) * amp);
        bool front = pass == 0 ? c > 0.0f : c < 0.0f;
        uint16_t color = pass == 0 ? (front ? mint : mintDim) : (front ? gold : goldDim);
        int16_t r = front ? 5 : 3;
        d.fillCircle(x, y, r, color);
        if (front) {
          d.fillCircle(x - 1, y - 1, 1, TFT_WHITE);
        }
        if (i < count - 1) {
          float nextTheta = phase + (i + 1) * 0.72f;
          float nextS = sinf(nextTheta);
          float nextC = cosf(nextTheta);
          int16_t nx = startX + (i + 1) * step;
          int16_t ny = axisY + static_cast<int16_t>((pass == 0 ? nextS : -nextS) * amp);
          bool nextMostlyFront = pass == 0 ? nextC > -0.25f : nextC < 0.25f;
          line(x, y, nx, ny, color, front && nextMostlyFront);
        }
      }
    }
  };

  d.fillScreen(bg);
  d.fillTriangle(0, 92, 240, 28, 240, 135, rgb565_local(6, 21, 20));
  d.fillRoundRect(7, 7, 226, 121, 9, panel);
  d.drawRoundRect(7, 7, 226, 121, 9, panelLine);

  drawHelix();
  d.drawEllipse(120, 66, 82, 42 + static_cast<int16_t>(stageIndex % 2), mintDim);

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
  d.drawRoundRect(108, 17, 59, 4, 2, panelLine);
  d.fillRoundRect(111 + pct / 4, 18, 18, 2, 1, TFT_WHITE);

  const char* states[] = {"DNA", "LINK", "SYNC", "READY"};
  const char* label = states[clampValue<int>((stageIndex + 1) / 2, 0, 3)];
  d.setFont(&fonts::Font4);
  d.setTextColor(TFT_WHITE, panel);
  int16_t tw = d.textWidth(label);
  d.setCursor((240 - tw) / 2, 101);
  d.print(label);
  d.setFont(&fonts::Font2);
  d.setTextColor(muted, panel);
  String subtitle = stage && stage[0] ? String("molecular context / ") + stage : "molecular context";
  d.setCursor((240 - d.textWidth(subtitle)) / 2, 121);
  d.print(subtitle);
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
  gCanvas.createSprite(240, 135);
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
