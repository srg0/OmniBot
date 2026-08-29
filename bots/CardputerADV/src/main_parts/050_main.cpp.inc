#include "uncanny_default_eye.h"

// Full-screen eye renderer adapted from Adafruit Uncanny Eyes (Phil Burgess)
// and Bodmer's ESP32/TFT_eSPI port. The asset tables live in PROGMEM. Rendering
// uses one fixed strip and alternates eyes so the main loop remains responsive.
constexpr int16_t kUncannyEyeSize = 128;
constexpr int16_t kUncannyVisibleEyeWidth = 120;
constexpr int16_t kUncannyEyeTop = 3;
constexpr int16_t kUncannyStripRows = 8;
constexpr uint16_t kUncannyBackground = 0x0000;
constexpr uint32_t kUncannyBlinkDurationMs = 120;
uint16_t gUncannyStrip[kUncannyVisibleEyeWidth * kUncannyStripRows] = {};
bool gUncannyInitialized = false;
uint8_t gUncannyNextEye = 0;
uint8_t gUncannyUpperThreshold = 128;
uint32_t gUncannyLastRenderMs = 0;

void resetUncannyEyesRenderer() {
  gUncannyInitialized = false;
  gUncannyNextEye = 0;
  gUncannyUpperThreshold = 128;
  gUncannyLastRenderMs = 0;
}

uint8_t uncannyBlinkAmount(uint32_t now) {
  if (gBlinkEndMs == 0 || static_cast<int32_t>(gBlinkEndMs - now) <= 0) {
    return 0;
  }
  uint32_t remaining = min<uint32_t>(kUncannyBlinkDurationMs, gBlinkEndMs - now);
  uint32_t elapsed = kUncannyBlinkDurationMs - remaining;
  uint32_t half = kUncannyBlinkDurationMs / 2;
  uint32_t ramp = elapsed <= half ? elapsed : kUncannyBlinkDurationMs - elapsed;
  return static_cast<uint8_t>(min<uint32_t>(255, (ramp * 255U) / max<uint32_t>(1, half)));
}

uint16_t uncannyIrisScale() {
  switch (gFaceMode) {
    case FaceMode::Listening:
      return 96;
    case FaceMode::Speaking:
      return 104;
    case FaceMode::Thinking:
      return 122;
    case FaceMode::Error:
      return 128;
    case FaceMode::Idle:
    default:
      break;
  }
  switch (currentEyeEmotion()) {
    case EyeEmotion::Surprised:
    case EyeEmotion::Excited:
      return 94;
    case EyeEmotion::Sleepy:
    case EyeEmotion::Thinking:
      return 122;
    default:
      return 110;
  }
}

void uncannyEyePlacement(uint8_t eyeIndex, int16_t& scleraX, int16_t& scleraY) {
  constexpr int16_t maxOffsetX = UNCANNY_SCLERA_WIDTH - kUncannyEyeSize;
  constexpr int16_t maxOffsetY = UNCANNY_SCLERA_HEIGHT - kUncannyEyeSize;
  float normalizedX = clampValue<float>(gEyeLookX / 18.0f, -1.0f, 1.0f);
  float normalizedY = clampValue<float>(gEyeLookY / 14.0f, -1.0f, 1.0f);
  scleraX = static_cast<int16_t>(maxOffsetX / 2 + normalizedX * (maxOffsetX / 2));
  scleraY = static_cast<int16_t>(maxOffsetY / 2 + normalizedY * (maxOffsetY / 2));
  scleraX += eyeIndex == 0 ? -4 : 4;
  scleraX = clampValue<int16_t>(scleraX, 0, maxOffsetX);
  scleraY = clampValue<int16_t>(scleraY, 0, maxOffsetY);
}

void uncannyLidThresholds(int16_t scleraX, int16_t scleraY, uint32_t now,
                          uint8_t& upperThreshold, uint8_t& lowerThreshold) {
  int16_t sampleX = UNCANNY_SCLERA_WIDTH / 2 - scleraX / 2;
  int16_t sampleY = UNCANNY_SCLERA_HEIGHT / 2 - (scleraY + UNCANNY_IRIS_HEIGHT / 4);
  sampleX = clampValue<int16_t>(sampleX, 0, UNCANNY_SCREEN_WIDTH - 1);
  sampleY = clampValue<int16_t>(sampleY, 0, UNCANNY_SCREEN_HEIGHT - 1);
  uint8_t sampleA = pgm_read_byte(kUncannyUpper + sampleY * UNCANNY_SCREEN_WIDTH + sampleX);
  uint8_t sampleB = pgm_read_byte(kUncannyUpper + sampleY * UNCANNY_SCREEN_WIDTH +
                                  (UNCANNY_SCREEN_WIDTH - 1 - sampleX));
  uint8_t tracked = static_cast<uint8_t>((static_cast<uint16_t>(sampleA) + sampleB) / 2U);
  gUncannyUpperThreshold = static_cast<uint8_t>((gUncannyUpperThreshold * 3U + tracked) / 4U);
  uint8_t openUpper = gUncannyUpperThreshold;
  uint8_t openLower = static_cast<uint8_t>(254U - openUpper);
  if (gFaceMode == FaceMode::Thinking || currentEyeEmotion() == EyeEmotion::Sleepy) {
    openUpper = max<uint8_t>(openUpper, 150);
    openLower = max<uint8_t>(openLower, 104);
  }
  uint8_t blink = uncannyBlinkAmount(now);
  upperThreshold = static_cast<uint8_t>(openUpper + ((254U - openUpper) * blink) / 255U);
  lowerThreshold = static_cast<uint8_t>(openLower + ((254U - openLower) * blink) / 255U);
}

uint16_t uncannyEyePixel(uint8_t eyeIndex, int16_t screenX, int16_t screenY,
                         int16_t scleraX, int16_t scleraY, uint16_t irisScale,
                         uint8_t upperThreshold, uint8_t lowerThreshold) {
  int16_t lidX = eyeIndex == 0 ? (UNCANNY_SCREEN_WIDTH - 1 - screenX) : screenX;
  size_t lidOffset = static_cast<size_t>(screenY) * UNCANNY_SCREEN_WIDTH + lidX;
  if (pgm_read_byte(kUncannyLower + lidOffset) <= lowerThreshold ||
      pgm_read_byte(kUncannyUpper + lidOffset) <= upperThreshold) {
    return TFT_BLACK;
  }

  int16_t sourceX = scleraX + screenX;
  int16_t sourceY = scleraY + screenY;
  int16_t irisX = sourceX - (UNCANNY_SCLERA_WIDTH - UNCANNY_IRIS_WIDTH) / 2;
  int16_t irisY = sourceY - (UNCANNY_SCLERA_HEIGHT - UNCANNY_IRIS_HEIGHT) / 2;
  if (irisX >= 0 && irisX < UNCANNY_IRIS_WIDTH && irisY >= 0 && irisY < UNCANNY_IRIS_HEIGHT) {
    uint16_t polar = pgm_read_word(kUncannyPolar + irisY * UNCANNY_IRIS_WIDTH + irisX);
    uint16_t distance = static_cast<uint16_t>((irisScale * (polar & 0x7FU)) / 128U);
    if (distance < UNCANNY_IRIS_MAP_HEIGHT) {
      uint16_t angle = static_cast<uint16_t>((UNCANNY_IRIS_MAP_WIDTH * (polar >> 7)) / 512U);
      angle = min<uint16_t>(angle, UNCANNY_IRIS_MAP_WIDTH - 1);
      return pgm_read_word(kUncannyIris + distance * UNCANNY_IRIS_MAP_WIDTH + angle);
    }
  }
  return pgm_read_word(kUncannySclera + sourceY * UNCANNY_SCLERA_WIDTH + sourceX);
}

void renderOneUncannyEye(uint8_t eyeIndex, uint32_t now) {
  auto& d = gCanvas;
  const int16_t destX = eyeIndex == 0 ? 0 : kUncannyVisibleEyeWidth;
  const int16_t sourceStartX = eyeIndex == 0 ? kUncannyEyeSize - kUncannyVisibleEyeWidth : 0;
  d.fillRect(destX, 0, kUncannyVisibleEyeWidth, 135, kUncannyBackground);

  int16_t scleraX = 0;
  int16_t scleraY = 0;
  uncannyEyePlacement(eyeIndex, scleraX, scleraY);
  uint8_t upperThreshold = 0;
  uint8_t lowerThreshold = 0;
  uncannyLidThresholds(scleraX, scleraY, now, upperThreshold, lowerThreshold);
  uint16_t irisScale = uncannyIrisScale();

  for (int16_t stripY = 0; stripY < kUncannyEyeSize; stripY += kUncannyStripRows) {
    int16_t rows = min<int16_t>(kUncannyStripRows, kUncannyEyeSize - stripY);
    for (int16_t row = 0; row < rows; ++row) {
      for (int16_t x = 0; x < kUncannyVisibleEyeWidth; ++x) {
        gUncannyStrip[row * kUncannyVisibleEyeWidth + x] =
            uncannyEyePixel(eyeIndex, sourceStartX + x, stripY + row, scleraX, scleraY,
                            irisScale, upperThreshold, lowerThreshold);
      }
    }
    d.pushImage(destX, kUncannyEyeTop + stripY, kUncannyVisibleEyeWidth, rows, gUncannyStrip);
  }
}

void renderUncannyEyesUi() {
  uint32_t now = millis();
  bool fullRefresh = !gUncannyInitialized || now - gUncannyLastRenderMs > 300U;
  if (fullRefresh) {
    gCanvas.fillScreen(kUncannyBackground);
    renderOneUncannyEye(0, now);
    renderOneUncannyEye(1, now);
    gUncannyInitialized = true;
    gUncannyNextEye = 0;
  } else {
    renderOneUncannyEye(gUncannyNextEye, now);
    gUncannyNextEye ^= 1U;
  }
  gUncannyLastRenderMs = now;

  if (gFaceMode == FaceMode::Error) {
    gCanvas.drawRect(0, 0, 240, 135, rgb565_local(255, 72, 52));
  }
  if (gScreensaverActive && gDeviceSettings.screensaverClockOnWake) {
    String hhmm = "--:--";
    time_t ts = time(nullptr);
    if (ts > 100000) {
      struct tm timeinfo;
      localtime_r(&ts, &timeinfo);
      char buf[6];
      strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
      hhmm = buf;
    }
    const uint16_t clockBg = rgb565_local(3, 7, 8);
    const uint16_t clockLine = rgb565_local(80, 104, 92);
    gCanvas.fillRoundRect(176, 6, 56, 18, 6, clockBg);
    gCanvas.drawRoundRect(176, 6, 56, 18, 6, clockLine);
    gCanvas.setFont(&fonts::Font0);
    gCanvas.setTextColor(TFT_WHITE, clockBg);
    gCanvas.setCursor(189, 11);
    gCanvas.print(hhmm);
    gCanvas.setFont(&fonts::Font2);
  }
}

void renderOmniEyePair(int16_t leftEyeX, int16_t rightEyeX, int16_t eyeCenterY,
                       int16_t eyeHalfW, int16_t eyeHalfH, uint16_t panelBg) {
  (void)panelBg;
  auto& d = gCanvas;
  EyeEmotion emotion = currentEyeEmotion();
  uint16_t accent = TFT_CYAN;
  float opennessL = 1.0f;
  float opennessR = 1.0f;
  int16_t pupilX = clampValue<int16_t>(static_cast<int16_t>(gEyeLookX), -max<int16_t>(2, eyeHalfW / 3), max<int16_t>(2, eyeHalfW / 3));
  int16_t pupilY = clampValue<int16_t>(static_cast<int16_t>(gEyeLookY), -max<int16_t>(2, eyeHalfH / 3), max<int16_t>(2, eyeHalfH / 3));
  bool happyArc = false;
  bool sleepyArc = false;
  bool angryBrows = false;
  bool sadBrows = false;
  bool lovePupils = false;
  bool surprisedRings = false;
  bool excitedSparks = false;

  if (gFaceMode == FaceMode::Error) {
    accent = TFT_ORANGE;
    d.drawLine(leftEyeX - eyeHalfW, eyeCenterY - eyeHalfH, leftEyeX + eyeHalfW, eyeCenterY + eyeHalfH, accent);
    d.drawLine(leftEyeX - eyeHalfW, eyeCenterY + eyeHalfH, leftEyeX + eyeHalfW, eyeCenterY - eyeHalfH, accent);
    d.drawLine(rightEyeX - eyeHalfW, eyeCenterY - eyeHalfH, rightEyeX + eyeHalfW, eyeCenterY + eyeHalfH, accent);
    d.drawLine(rightEyeX - eyeHalfW, eyeCenterY + eyeHalfH, rightEyeX + eyeHalfW, eyeCenterY - eyeHalfH, accent);
    return;
  }

  switch (emotion) {
    case EyeEmotion::Speaking:
      accent = rgb565_local(90, 255, 185);
      opennessL = 0.92f;
      opennessR = 0.92f;
      pupilX += static_cast<int16_t>(((millis() / 120) % 5) - 2);
      break;
    case EyeEmotion::Happy:
      accent = rgb565_local(130, 255, 220);
      happyArc = true;
      pupilY = 0;
      break;
    case EyeEmotion::Mad:
      accent = rgb565_local(255, 110, 64);
      opennessL = 0.74f;
      opennessR = 0.74f;
      angryBrows = true;
      pupilY -= max<int16_t>(1, eyeHalfH / 6);
      break;
    case EyeEmotion::Sad:
      accent = rgb565_local(110, 195, 255);
      opennessL = 0.76f;
      opennessR = 0.76f;
      sadBrows = true;
      pupilY += max<int16_t>(1, eyeHalfH / 7);
      break;
    case EyeEmotion::Surprised:
      accent = rgb565_local(165, 235, 255);
      opennessL = 1.18f;
      opennessR = 1.18f;
      surprisedRings = true;
      pupilY -= max<int16_t>(1, eyeHalfH / 8);
      break;
    case EyeEmotion::Sleepy:
      accent = rgb565_local(120, 180, 225);
      sleepyArc = true;
      pupilY = 0;
      break;
    case EyeEmotion::Thinking:
      accent = TFT_GOLD;
      opennessL = 0.88f;
      opennessR = 0.88f;
      pupilY -= max<int16_t>(2, eyeHalfH / 4);
      pupilX += static_cast<int16_t>(((millis() / 220) % 3) - 1);
      break;
    case EyeEmotion::Confused:
      accent = rgb565_local(170, 220, 255);
      opennessL = 0.5f;
      opennessR = 1.08f;
      pupilX -= 1;
      break;
    case EyeEmotion::Excited:
      accent = rgb565_local(120, 255, 170);
      opennessL = 1.12f;
      opennessR = 1.12f;
      excitedSparks = true;
      break;
    case EyeEmotion::Love:
      accent = rgb565_local(255, 90, 180);
      opennessL = 1.02f;
      opennessR = 1.02f;
      lovePupils = true;
      break;
    case EyeEmotion::Neutral:
    default:
      accent = TFT_CYAN;
      pupilX += static_cast<int16_t>(((millis() / 520) % 5) - 2);
      break;
  }

  if (gBlinkEndMs != 0) {
    happyArc = false;
    sleepyArc = true;
    lovePupils = false;
    surprisedRings = false;
    excitedSparks = false;
    opennessL = 0.08f;
    opennessR = 0.08f;
  }

  pupilX = clampValue<int16_t>(pupilX, -max<int16_t>(2, eyeHalfW / 3), max<int16_t>(2, eyeHalfW / 3));
  pupilY = clampValue<int16_t>(pupilY, -max<int16_t>(2, eyeHalfH / 3), max<int16_t>(2, eyeHalfH / 3));

  if (happyArc) {
    drawArcEye(leftEyeX, eyeCenterY + eyeHalfH / 5, eyeHalfW, max<int16_t>(2, eyeHalfH / 2), accent, true,
               eyeHalfW > 14 ? 3 : 2);
    drawArcEye(rightEyeX, eyeCenterY + eyeHalfH / 5, eyeHalfW, max<int16_t>(2, eyeHalfH / 2), accent, true,
               eyeHalfW > 14 ? 3 : 2);
  } else if (sleepyArc) {
    drawArcEye(leftEyeX, eyeCenterY - eyeHalfH / 8, eyeHalfW, max<int16_t>(1, eyeHalfH / 5), accent, false,
               eyeHalfW > 14 ? 3 : 2);
    drawArcEye(rightEyeX, eyeCenterY - eyeHalfH / 8, eyeHalfW, max<int16_t>(1, eyeHalfH / 5), accent, false,
               eyeHalfW > 14 ? 3 : 2);
  } else {
    drawEye(leftEyeX, eyeCenterY, eyeHalfW, eyeHalfH, opennessL, accent, pupilX, pupilY);
    drawEye(rightEyeX, eyeCenterY, eyeHalfW, eyeHalfH, opennessR, accent, pupilX, pupilY);
  }

  if (lovePupils) {
    drawHeartGlyph(leftEyeX + pupilX, eyeCenterY + pupilY + max<int16_t>(1, eyeHalfH / 10), max<int16_t>(2, eyeHalfW / 5), accent);
    drawHeartGlyph(rightEyeX + pupilX, eyeCenterY + pupilY + max<int16_t>(1, eyeHalfH / 10), max<int16_t>(2, eyeHalfW / 5), accent);
  }

  if (surprisedRings) {
    d.drawCircle(leftEyeX, eyeCenterY, max<int16_t>(3, eyeHalfH - 2), accent);
    d.drawCircle(rightEyeX, eyeCenterY, max<int16_t>(3, eyeHalfH - 2), accent);
  }

  if (excitedSparks) {
    drawSparkGlyph(leftEyeX - eyeHalfW + 1, eyeCenterY - eyeHalfH - 4, max<int16_t>(1, eyeHalfW / 5), accent);
    drawSparkGlyph(rightEyeX + eyeHalfW - 1, eyeCenterY - eyeHalfH - 4, max<int16_t>(1, eyeHalfW / 5), accent);
  }

  int16_t browLift = max<int16_t>(3, eyeHalfH / 2);
  if (angryBrows) {
    drawBrowStroke(leftEyeX - eyeHalfW - 2, eyeCenterY - browLift - 2, leftEyeX + eyeHalfW / 2, eyeCenterY - eyeHalfH / 2, accent,
                   eyeHalfW > 14 ? 3 : 2);
    drawBrowStroke(rightEyeX + eyeHalfW + 2, eyeCenterY - browLift - 2, rightEyeX - eyeHalfW / 2, eyeCenterY - eyeHalfH / 2, accent,
                   eyeHalfW > 14 ? 3 : 2);
  } else if (sadBrows) {
    drawBrowStroke(leftEyeX - eyeHalfW - 2, eyeCenterY - eyeHalfH / 2, leftEyeX + eyeHalfW / 2, eyeCenterY - browLift - 2, accent,
                   eyeHalfW > 14 ? 3 : 2);
    drawBrowStroke(rightEyeX + eyeHalfW + 2, eyeCenterY - eyeHalfH / 2, rightEyeX - eyeHalfW / 2, eyeCenterY - browLift - 2, accent,
                   eyeHalfW > 14 ? 3 : 2);
  }
}

uint16_t petMoodAccent() {
  switch (gPet.mood) {
    case PetMood::Happy:
      return rgb565_local(88, 240, 141);
    case PetMood::Hungry:
      return rgb565_local(255, 194, 74);
    case PetMood::Bored:
      return rgb565_local(126, 143, 157);
    case PetMood::Curious:
      return rgb565_local(47, 227, 255);
    case PetMood::Sick:
      return rgb565_local(128, 230, 104);
    case PetMood::Excited:
      return rgb565_local(255, 105, 135);
    case PetMood::Sleepy:
      return rgb565_local(143, 126, 255);
    case PetMood::Dirty:
      return rgb565_local(181, 142, 86);
    case PetMood::Dead:
      return rgb565_local(120, 128, 136);
    case PetMood::Calm:
    default:
      return rgb565_local(34, 209, 189);
  }
}

uint16_t petBodyColor() {
  if (!gPet.alive) {
    return rgb565_local(84, 90, 96);
  }
  if (gPet.mood == PetMood::Sick) {
    return rgb565_local(119, 176, 95);
  }
  if (gPet.mood == PetMood::Sleepy) {
    return rgb565_local(130, 86, 198);
  }
  switch (gPet.stage) {
    case PetStage::Egg:
      return rgb565_local(236, 218, 172);
    case PetStage::Baby:
      return rgb565_local(228, 74, 62);
    case PetStage::Teen:
      return rgb565_local(222, 58, 64);
    case PetStage::Adult:
      return rgb565_local(203, 52, 58);
    case PetStage::Elder:
    default:
      return rgb565_local(186, 74, 88);
  }
}

void drawPetSpark(int16_t x, int16_t y, int16_t r, uint16_t color) {
  auto& d = gCanvas;
  d.drawFastHLine(x - r, y, r * 2 + 1, color);
  d.drawFastVLine(x, y - r, r * 2 + 1, color);
  d.drawLine(x - r + 1, y - r + 1, x + r - 1, y + r - 1, color);
  d.drawLine(x - r + 1, y + r - 1, x + r - 1, y - r + 1, color);
}

void drawProceduralOpenClawPet(int16_t cx, int16_t cy, int16_t size, bool mini) {
  auto& d = gCanvas;
  uint32_t now = millis();
  uint16_t body = petBodyColor();
  uint16_t accent = petMoodAccent();
  uint16_t line = rgb565_local(13, 18, 22);
  uint16_t highlight = rgb565_local(255, 134, 108);
  int16_t r = max<int16_t>(8, size / 2);
  int16_t bob = mini ? 0 : static_cast<int16_t>(abs(static_cast<int>((now / 170) % 10) - 5) / 2);
  cy += bob;

  d.fillRoundRect(cx - r + 4, cy + r - 3, (r - 4) * 2, max<int16_t>(4, r / 4), max<int16_t>(2, r / 8), rgb565_local(0, 0, 0));
