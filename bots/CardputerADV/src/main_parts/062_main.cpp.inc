namespace {

constexpr uint8_t kSonarTrigPin = 4;
constexpr uint8_t kSonarEchoPin = 6;
constexpr uint32_t kSonarSampleIntervalUs = 100000;
constexpr uint32_t kSonarEchoTimeoutUs = 30000;
constexpr uint32_t kSonarFreshEchoMs = 450;
constexpr uint32_t kSonarTooCloseBeepIntervalMs = 1200;
constexpr uint8_t kSonarFilterCapacity = 5;
constexpr float kSonarMinimumCm = 2.0f;
constexpr float kSonarMaximumCm = 400.0f;
constexpr float kSonarTooCloseCm = 15.0f;
constexpr float kSonarObjectCm = 80.0f;
constexpr float kSonarDisplayFarCm = 200.0f;
constexpr float kSonarDegreesToRadians = 0.017453292519943295f;

enum class SonarViewMode : uint8_t {
  Proximity,
  Radar,
};

enum class SonarProximityState : uint8_t {
  NoEcho,
  Clear,
  Object,
  TooClose,
};

struct SonarRuntime {
  bool active = false;
  bool waitingEcho = false;
  bool hasFilteredDistance = false;
  bool beepEnabled = false;
  SonarViewMode viewMode = SonarViewMode::Proximity;
  float filteredCm = 0.0f;
  float samples[kSonarFilterCapacity] = {};
  uint8_t sampleCount = 0;
  uint8_t sampleHead = 0;
  uint8_t consecutiveTimeouts = 0;
  uint32_t lastTriggerUs = 0;
  uint32_t lastPulseUs = 0;
  uint32_t lastValidMs = 0;
  uint32_t lastBeepMs = 0;
  SonarProximityState state = SonarProximityState::NoEcho;
  SonarProximityState lastBeepState = SonarProximityState::NoEcho;
};

SonarRuntime gSonar;
portMUX_TYPE gSonarEchoMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t gSonarEchoRiseUs = 0;
volatile uint32_t gSonarEchoPulseUs = 0;
volatile bool gSonarEchoHigh = false;
volatile bool gSonarEchoReady = false;

void IRAM_ATTR sonarEchoEdgeIsr() {
  uint32_t nowUs = micros();
  bool high = digitalRead(kSonarEchoPin) != LOW;
  portENTER_CRITICAL_ISR(&gSonarEchoMux);
  if (high) {
    gSonarEchoRiseUs = nowUs;
    gSonarEchoHigh = true;
  } else if (gSonarEchoHigh) {
    gSonarEchoPulseUs = nowUs - gSonarEchoRiseUs;
    gSonarEchoReady = true;
    gSonarEchoHigh = false;
  }
  portEXIT_CRITICAL_ISR(&gSonarEchoMux);
}

void clearSonarEchoCapture() {
  portENTER_CRITICAL(&gSonarEchoMux);
  gSonarEchoRiseUs = 0;
  gSonarEchoPulseUs = 0;
  gSonarEchoHigh = false;
  gSonarEchoReady = false;
  portEXIT_CRITICAL(&gSonarEchoMux);
}

void stopSonar() {
  if (!gSonar.active) {
    return;
  }
  detachInterrupt(digitalPinToInterrupt(kSonarEchoPin));
  digitalWrite(kSonarTrigPin, LOW);
  pinMode(kSonarTrigPin, INPUT);
  pinMode(kSonarEchoPin, INPUT);
  gSonar.active = false;
  gSonar.waitingEcho = false;
  clearSonarEchoCapture();
  appendRuntimeLog("SONAR", "stopped; released G4/G6", false);
}

void initSonar() {
  if (gSonar.active) {
    return;
  }
  pinMode(kSonarTrigPin, OUTPUT);
  digitalWrite(kSonarTrigPin, LOW);
  pinMode(kSonarEchoPin, INPUT);
  clearSonarEchoCapture();
  attachInterrupt(digitalPinToInterrupt(kSonarEchoPin), sonarEchoEdgeIsr, CHANGE);
  gSonar.active = true;
  gSonar.waitingEcho = false;
  gSonar.lastTriggerUs = micros() - kSonarSampleIntervalUs;
  appendRuntimeLog("SONAR", "HC-SR04 ready TRIG=G4 ECHO=G6", true);
}

void resetSonar(bool preserveSettings) {
  bool beepEnabled = preserveSettings && gSonar.beepEnabled;
  SonarViewMode viewMode = preserveSettings ? gSonar.viewMode : SonarViewMode::Proximity;
  stopSonar();
  gSonar = SonarRuntime{};
  gSonar.beepEnabled = beepEnabled;
  gSonar.viewMode = viewMode;
  initSonar();
}

float sonarMedianSample() {
  float sorted[kSonarFilterCapacity] = {};
  for (uint8_t i = 0; i < gSonar.sampleCount; ++i) {
    sorted[i] = gSonar.samples[i];
  }
  for (uint8_t i = 1; i < gSonar.sampleCount; ++i) {
    float value = sorted[i];
    int8_t j = static_cast<int8_t>(i) - 1;
    while (j >= 0 && sorted[j] > value) {
      sorted[j + 1] = sorted[j];
      --j;
    }
    sorted[j + 1] = value;
  }
  uint8_t middle = gSonar.sampleCount / 2;
  if ((gSonar.sampleCount & 1U) != 0U) {
    return sorted[middle];
  }
  return (sorted[middle - 1] + sorted[middle]) * 0.5f;
}

void acceptSonarPulse(uint32_t pulseUs) {
  float distanceCm = static_cast<float>(pulseUs) / 58.0f;
  if (!isfinite(distanceCm) || distanceCm < kSonarMinimumCm || distanceCm > kSonarMaximumCm) {
    return;
  }
  gSonar.samples[gSonar.sampleHead] = distanceCm;
  gSonar.sampleHead = (gSonar.sampleHead + 1) % kSonarFilterCapacity;
  if (gSonar.sampleCount < kSonarFilterCapacity) {
    ++gSonar.sampleCount;
  }
  float medianCm = sonarMedianSample();
  if (!gSonar.hasFilteredDistance) {
    gSonar.filteredCm = medianCm;
    gSonar.hasFilteredDistance = true;
  } else {
    gSonar.filteredCm += (medianCm - gSonar.filteredCm) * 0.42f;
  }
  gSonar.lastPulseUs = pulseUs;
  gSonar.lastValidMs = millis();
  gSonar.consecutiveTimeouts = 0;
}

bool sonarEchoFresh(uint32_t nowMs) {
  return gSonar.hasFilteredDistance && nowMs - gSonar.lastValidMs <= kSonarFreshEchoMs;
}

SonarProximityState classifySonarState(uint32_t nowMs) {
  if (!sonarEchoFresh(nowMs)) {
    return SonarProximityState::NoEcho;
  }
  if (gSonar.filteredCm <= kSonarTooCloseCm) {
    return SonarProximityState::TooClose;
  }
  if (gSonar.filteredCm <= kSonarObjectCm) {
    return SonarProximityState::Object;
  }
  return SonarProximityState::Clear;
}

const char* sonarStateLabel(SonarProximityState state) {
  switch (state) {
    case SonarProximityState::Clear:
      return "CLEAR";
    case SonarProximityState::Object:
      return "OBJECT";
    case SonarProximityState::TooClose:
      return "TOO CLOSE";
    case SonarProximityState::NoEcho:
    default:
      return "NO ECHO";
  }
}

void serviceSonarBeep(uint32_t nowMs, SonarProximityState previousState) {
  if (!gSonar.beepEnabled || gSonar.state == SonarProximityState::NoEcho ||
      gSonar.state == SonarProximityState::Clear) {
    gSonar.lastBeepState = gSonar.state;
    return;
  }
  bool stateChanged = gSonar.state != previousState || gSonar.state != gSonar.lastBeepState;
  bool periodicTooClose = gSonar.state == SonarProximityState::TooClose &&
                          nowMs - gSonar.lastBeepMs >= kSonarTooCloseBeepIntervalMs;
  if (!stateChanged && !periodicTooClose) {
    return;
  }
  focusTone(gSonar.state == SonarProximityState::TooClose ? 1480 : 1040,
            gSonar.state == SonarProximityState::TooClose ? 55 : 35);
  gSonar.lastBeepMs = nowMs;
  gSonar.lastBeepState = gSonar.state;
}

void serviceSonar() {
  if (gUiMode != UiMode::Sonar) {
    stopSonar();
    return;
  }
  initSonar();

  uint32_t pulseUs = 0;
  bool pulseReady = false;
  portENTER_CRITICAL(&gSonarEchoMux);
  if (gSonarEchoReady) {
    pulseUs = gSonarEchoPulseUs;
    gSonarEchoReady = false;
    gSonarEchoHigh = false;
    pulseReady = true;
  }
  portEXIT_CRITICAL(&gSonarEchoMux);

  if (pulseReady && gSonar.waitingEcho) {
    gSonar.waitingEcho = false;
    acceptSonarPulse(pulseUs);
  }

  uint32_t nowUs = micros();
  if (gSonar.waitingEcho && nowUs - gSonar.lastTriggerUs > kSonarEchoTimeoutUs) {
    gSonar.waitingEcho = false;
    if (gSonar.consecutiveTimeouts < UINT8_MAX) {
      ++gSonar.consecutiveTimeouts;
    }
    clearSonarEchoCapture();
  }

  if (!gSonar.waitingEcho && nowUs - gSonar.lastTriggerUs >= kSonarSampleIntervalUs) {
    clearSonarEchoCapture();
    gSonar.lastTriggerUs = nowUs;
    gSonar.waitingEcho = true;
    digitalWrite(kSonarTrigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(kSonarTrigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(kSonarTrigPin, LOW);
  }

  uint32_t nowMs = millis();
  SonarProximityState previousState = gSonar.state;
  gSonar.state = classifySonarState(nowMs);
  serviceSonarBeep(nowMs, previousState);
}

bool handleSonarKey(char key, const Keyboard_Class::KeysState& keys) {
  (void)keys;
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'm') {
    gSonar.viewMode = gSonar.viewMode == SonarViewMode::Proximity
                          ? SonarViewMode::Radar
                          : SonarViewMode::Proximity;
    setStatus(gSonar.viewMode == SonarViewMode::Radar ? "Sonar radar view"
                                                       : "Sonar proximity view",
              900);
    return true;
  }
  if (lowerKey == 'b') {
    gSonar.beepEnabled = !gSonar.beepEnabled;
    gSonar.lastBeepState = SonarProximityState::NoEcho;
    gSonar.lastBeepMs = 0;
    setStatus(gSonar.beepEnabled ? "Sonar beep on" : "Sonar beep off", 900);
    return true;
  }
  if (lowerKey == 'r') {
    resetSonar(true);
    setStatus("Sonar reset", 900);
    return true;
  }
  return false;
}

uint16_t sonarStateColor(SonarProximityState state) {
  switch (state) {
    case SonarProximityState::Clear:
      return rgb565_local(88, 240, 141);
    case SonarProximityState::Object:
      return rgb565_local(246, 194, 74);
    case SonarProximityState::TooClose:
      return rgb565_local(255, 82, 82);
    case SonarProximityState::NoEcho:
    default:
      return rgb565_local(126, 143, 157);
  }
}

void renderSonarRadarView(uint32_t nowMs, bool fresh, uint16_t accent) {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(7, 18, 25);
  const uint16_t panel2 = rgb565_local(6, 16, 23);
  const uint16_t line = rgb565_local(48, 60, 69);
  const uint16_t grid = rgb565_local(24, 55, 66);
  const uint16_t gridBright = rgb565_local(36, 88, 102);
  const uint16_t text = rgb565_local(238, 244, 248);
  const uint16_t muted = rgb565_local(130, 145, 155);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t teal = rgb565_local(34, 209, 189);

  d.fillScreen(bg);
  d.fillRoundRect(5, 5, 230, 125, 8, panel);
  d.drawRoundRect(5, 5, 230, 125, 8, line);
  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(14, 11);
  d.print("SONAR / RADAR");

  String stateText = sonarStateLabel(gSonar.state);
  d.setFont(&fonts::Font0);
  int stateWidth = max<int>(52, d.textWidth(stateText) + 16);
  int stateX = 226 - stateWidth;
  uint16_t stateBg = gSonar.state == SonarProximityState::NoEcho
                         ? rgb565_local(20, 27, 32)
                         : rgb565_local(16, 36, 31);
  d.fillRoundRect(stateX, 10, stateWidth, 15, 6, stateBg);
  d.drawRoundRect(stateX, 10, stateWidth, 15, 6, accent);
  d.setTextColor(accent, stateBg);
  d.setTextDatum(middle_center);
  d.drawString(stateText, stateX + stateWidth / 2, 17);
  d.setTextDatum(top_left);
  d.drawFastHLine(12, 30, 216, rgb565_local(30, 48, 57));

  constexpr int radarCenterX = 68;
  constexpr int radarCenterY = 76;
  constexpr int radarRadius = 39;
  const int radarRings[] = {13, 26, radarRadius};
  for (uint8_t i = 0; i < 3; ++i) {
    d.drawCircle(radarCenterX, radarCenterY, radarRings[i], i == 2 ? gridBright : grid);
  }
  d.drawFastHLine(radarCenterX - radarRadius, radarCenterY, radarRadius * 2 + 1, grid);
  d.drawFastVLine(radarCenterX, radarCenterY - radarRadius, radarRadius * 2 + 1, grid);
  d.drawLine(radarCenterX, radarCenterY, radarCenterX - 10, radarCenterY - radarRadius,
             rgb565_local(32, 81, 94));
  d.drawLine(radarCenterX, radarCenterY, radarCenterX + 10, radarCenterY - radarRadius,
             rgb565_local(32, 81, 94));

  uint16_t sweepDegrees = static_cast<uint16_t>((nowMs / 8U) % 360U);
  const uint16_t trailColors[] = {
      teal,
      rgb565_local(21, 136, 120),
      rgb565_local(18, 89, 79),
      rgb565_local(16, 59, 55),
  };
  for (uint8_t trail = 0; trail < 4; ++trail) {
    int degrees = static_cast<int>(sweepDegrees) - static_cast<int>(trail) * 5;
    if (degrees < 0) {
      degrees += 360;
    }
    float angle = static_cast<float>(degrees) * kSonarDegreesToRadians;
    int endX = radarCenterX + static_cast<int>(lroundf(cosf(angle) * radarRadius));
    int endY = radarCenterY + static_cast<int>(lroundf(sinf(angle) * radarRadius));
    d.drawLine(radarCenterX, radarCenterY, endX, endY, trailColors[trail]);
  }

  if (fresh) {
    float clamped = max<float>(kSonarMinimumCm, min<float>(gSonar.filteredCm, kSonarDisplayFarCm));
    int targetRadius = clampValue<int>(
        static_cast<int>(lroundf(clamped * radarRadius / kSonarDisplayFarCm)), 5,
        radarRadius - 2);
    int targetY = radarCenterY - targetRadius;
    bool sweepNearForward = sweepDegrees >= 258U && sweepDegrees <= 282U;
    d.drawCircle(radarCenterX, targetY, sweepNearForward ? 7 : 5,
                 sweepNearForward ? accent : rgb565_local(92, 72, 32));
    d.fillCircle(radarCenterX, targetY, 3, accent);
  }
  d.fillCircle(radarCenterX, radarCenterY, 2, fresh ? cyan : muted);
  d.setTextColor(muted, panel);
  d.setTextDatum(middle_center);
  d.drawString("FWD", radarCenterX, 34);
  d.drawString("200cm", radarCenterX, 117);
  d.setTextDatum(top_left);

  d.fillRoundRect(118, 38, 108, 68, 9, panel2);
  d.drawRoundRect(118, 38, 108, 68, 9, rgb565_local(30, 48, 57));
  String distanceText = fresh ? String(static_cast<int>(lroundf(gSonar.filteredCm))) : "--";
  d.setFont(&fonts::Font6);
  d.setTextColor(text, panel2);
  d.setTextDatum(middle_center);
  d.drawString(distanceText, 164, 57);
  d.setFont(&fonts::Font2);
  d.setTextColor(muted, panel2);
  d.drawString("cm", 205, 64);
  d.setTextDatum(top_left);

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel2);
  d.setCursor(129, 76);
  d.print("RANGE ONLY");
  d.setTextColor(cyan, panel2);
  String coneLabel = "FWD +/-15";
  d.setCursor(215 - d.textWidth(coneLabel), 76);
  d.print(coneLabel);
  d.setTextColor(accent, panel2);
  d.setCursor(129, 90);
  if (fresh) {
    d.print("echo " + String(static_cast<float>(gSonar.lastPulseUs) / 1000.0f, 2) + "ms");
  } else {
    d.print("waiting");
  }
  d.setTextColor(teal, panel2);
  d.setCursor(215 - d.textWidth("10 Hz"), 90);
  d.print("10 Hz");

  drawTinyFooter(panel, muted, "M proximity   B beep   R reset   Esc", 15, 121, 210);
  d.setFont(&fonts::Font2);
}

void renderSonarUi() {
  initSonar();
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(7, 18, 25);
  const uint16_t panel2 = rgb565_local(6, 16, 23);
  const uint16_t line = rgb565_local(48, 60, 69);
  const uint16_t text = rgb565_local(238, 244, 248);
  const uint16_t muted = rgb565_local(130, 145, 155);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  uint32_t nowMs = millis();
  gSonar.state = classifySonarState(nowMs);
  bool fresh = sonarEchoFresh(nowMs);
  uint16_t accent = sonarStateColor(gSonar.state);

  if (gSonar.viewMode == SonarViewMode::Radar) {
    renderSonarRadarView(nowMs, fresh, accent);
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }

  d.fillScreen(bg);
  d.fillRoundRect(5, 5, 230, 125, 8, panel);
  d.drawRoundRect(5, 5, 230, 125, 8, line);
  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(14, 11);
  d.print("SONAR");

  String stateText = sonarStateLabel(gSonar.state);
  d.setFont(&fonts::Font0);
  int stateWidth = max<int>(52, d.textWidth(stateText) + 16);
  int stateX = 226 - stateWidth;
  uint16_t stateBg = gSonar.state == SonarProximityState::NoEcho
                         ? rgb565_local(20, 27, 32)
                         : rgb565_local(16, 36, 31);
  d.fillRoundRect(stateX, 10, stateWidth, 15, 6, stateBg);
  d.drawRoundRect(stateX, 10, stateWidth, 15, 6, accent);
  d.setTextColor(accent, stateBg);
  d.setTextDatum(middle_center);
  d.drawString(stateText, stateX + stateWidth / 2, 17);
  d.setTextDatum(top_left);
  d.drawFastHLine(12, 30, 216, rgb565_local(30, 48, 57));

  uint8_t activeArc = static_cast<uint8_t>((nowMs / 180U) % 3U);
  const int radii[] = {14, 23, 32};
  for (uint8_t i = 0; i < 3; ++i) {
    uint16_t arcColor = fresh && i == activeArc
                            ? accent
                            : (fresh ? rgb565_local(24, 86, 99) : rgb565_local(36, 50, 58));
    d.drawArc(25, 67, radii[i], radii[i] - 3, 300, 60, arcColor);
  }
  d.fillCircle(25, 67, 3, fresh ? cyan : muted);

  d.fillRoundRect(62, 38, 164, 44, 9, panel2);
  d.drawRoundRect(62, 38, 164, 44, 9, rgb565_local(30, 48, 57));
  String distanceText = fresh ? String(static_cast<int>(lroundf(gSonar.filteredCm))) : "--";
  d.setFont(&fonts::Font6);
  d.setTextColor(text, panel2);
  d.setTextDatum(middle_center);
  d.drawString(distanceText, 139, 59);
  d.setFont(&fonts::Font2);
  d.setTextColor(muted, panel2);
  d.drawString("cm", 202, 67);
  d.setTextDatum(top_left);

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(15, 87);
  d.print("PROXIMITY");
  d.setTextColor(accent, panel);
  String nearLabel = fresh ? "near" : "waiting";
  d.setCursor(225 - d.textWidth(nearLabel), 87);
  d.print(nearLabel);

  constexpr int barX = 15;
  constexpr int barY = 97;
  constexpr int barWidth = 210;
  d.fillRoundRect(barX, barY, barWidth, 8, 4, rgb565_local(23, 36, 43));
  int fillWidth = 0;
  if (fresh) {
    float clamped = max<float>(kSonarMinimumCm, min<float>(gSonar.filteredCm, kSonarDisplayFarCm));
    float proximity = (kSonarDisplayFarCm - clamped) /
                      (kSonarDisplayFarCm - kSonarMinimumCm);
    fillWidth = clampValue<int>(static_cast<int>(lroundf(proximity * barWidth)), 0, barWidth);
  }
  if (fillWidth > 0) {
    d.fillRoundRect(barX, barY, fillWidth, 8, 4, accent);
    int markerX = min<int>(barX + barWidth - 2, barX + fillWidth);
    d.fillRect(markerX, barY - 1, 2, 10, text);
  }

  String echoInfo;
  if (fresh) {
    echoInfo = "echo " + String(static_cast<float>(gSonar.lastPulseUs) / 1000.0f, 2) + "ms  |  10 Hz";
  } else if (gSonar.consecutiveTimeouts > 0) {
    echoInfo = "no echo x" + String(gSonar.consecutiveTimeouts) + "  |  G4 -> G6";
  } else {
    echoInfo = "waiting for echo  |  G4 -> G6";
  }
  d.setTextColor(muted, panel);
  d.setCursor(15, 109);
  d.print(fitCurrentFontToWidth(echoInfo, 210));
  String footer = String("M radar   B beep ") + (gSonar.beepEnabled ? "on" : "off") +
                  "   R reset   Esc";
  drawTinyFooter(panel, muted, footer, 15, 121, 210);
  d.setFont(&fonts::Font2);

  renderLauncherOverlay();
  renderDebugOverlay();
}

}  // namespace
