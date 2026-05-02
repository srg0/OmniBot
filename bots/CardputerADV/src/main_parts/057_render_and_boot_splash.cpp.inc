
void renderStatusBar() {
  auto& d = gCanvas;
  d.fillRect(0, 126, 240, 9, 0x18C3);
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_LIGHTGREY, 0x18C3);
  d.setCursor(4, 127);
  d.print(fitCurrentFontToWidth(gStatusText, 230));
  d.setFont(&fonts::Font2);
}

void render() {
  if (gStatusUntilMs != 0 && millis() > gStatusUntilMs) {
    gStatusUntilMs = 0;
    if (gBleActive) {
      gStatusText = "Waiting for BLE provisioning";
    } else if (gRecording) {
      gStatusText = "Listening...";
    } else if (gPlaybackActive) {
      gStatusText = "Speaking...";
    } else {
      gStatusText = "Ready";
    }
  }

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

