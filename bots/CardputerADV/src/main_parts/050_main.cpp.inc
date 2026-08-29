// Smooth procedural eye renderer. The previous photographic hazel lookup tables
// looked mottled at this display size and consumed 158 KB of flash plus a fixed
// strip buffer. These primitives preserve blink, emotion and IMU gaze without
// assets, heap allocation or per-pixel sampling.
constexpr uint32_t kUncannyBlinkDurationMs = 120;

void resetUncannyEyesRenderer() {
  // Procedural rendering redraws the complete 240x135 canvas every frame.
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

struct EyeSkinPalette {
  uint16_t background;
  uint16_t face;
  uint16_t accent;
  uint16_t marking;
};

EyeSkinPalette eyeSkinPalette(uint8_t skin) {
  switch (skin % kEyeSkinCount) {
    case 1:
      return {rgb565_local(21, 25, 27), rgb565_local(53, 59, 61),
              rgb565_local(185, 197, 194), rgb565_local(117, 103, 104)};
    case 2:
      return {rgb565_local(53, 27, 8), rgb565_local(198, 109, 32),
              rgb565_local(255, 182, 79), rgb565_local(24, 16, 10)};
    case 3:
      return {rgb565_local(54, 39, 17), rgb565_local(184, 139, 63),
              rgb565_local(230, 200, 117), rgb565_local(41, 27, 16)};
    case 4:
      return {rgb565_local(40, 22, 8), rgb565_local(167, 94, 39),
              rgb565_local(224, 162, 84), rgb565_local(90, 45, 19)};
    case 5:
      return {rgb565_local(41, 40, 45), rgb565_local(170, 167, 173),
              rgb565_local(224, 220, 227), rgb565_local(199, 134, 152)};
    case 0:
    default:
      return {rgb565_local(2, 4, 5), rgb565_local(3, 6, 7),
              rgb565_local(12, 32, 36), rgb565_local(8, 13, 15)};
  }
}

void drawEyeSkinLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                     uint16_t color, uint8_t width) {
  for (int8_t offset = -static_cast<int8_t>(width / 2); offset <= static_cast<int8_t>(width / 2); ++offset) {
    gCanvas.drawLine(x1 + offset, y1, x2 + offset, y2, color);
  }
}

void drawEyeSkinBackdrop(uint8_t skin, const EyeSkinPalette& palette) {
  auto& d = gCanvas;
  skin %= kEyeSkinCount;
  d.fillScreen(palette.background);
  if (skin == 0) {
    d.fillEllipse(120, 71, 119, 72, palette.face);
    d.drawLine(16, 120, 224, 120, palette.accent);
    return;
  }

  if (skin == 4) {
    static const int16_t kManeCenters[][2] = {
        {10, 23}, {35, 8}, {68, 4}, {103, 2}, {137, 2}, {172, 4}, {205, 8}, {230, 23},
        {235, 55}, {236, 91}, {222, 126}, {190, 132}, {153, 134}, {87, 134}, {50, 132},
        {18, 126}, {4, 91}, {5, 55}};
    for (const auto& center : kManeCenters) {
      d.fillCircle(center[0], center[1], 22, palette.marking);
    }
  }

  d.fillEllipse(120, 73, skin == 4 ? 102 : 118, skin == 4 ? 72 : 78, palette.face);
  if (skin == 1 || skin == 2 || skin == 3) {
    d.fillTriangle(9, 43, 35, 1, 60, 43, palette.face);
    d.fillTriangle(180, 43, 205, 1, 231, 43, palette.face);
    d.fillTriangle(20, 34, 35, 10, 49, 35, palette.marking);
    d.fillTriangle(191, 35, 205, 10, 220, 34, palette.marking);
  } else if (skin == 5) {
    d.fillEllipse(54, 17, 20, 49, palette.face);
    d.fillEllipse(186, 17, 20, 49, palette.face);
    d.fillEllipse(54, 16, 9, 37, palette.marking);
    d.fillEllipse(186, 16, 9, 37, palette.marking);
    d.fillEllipse(120, 74, 118, 75, palette.face);
  }

  if (skin == 2) {
    drawEyeSkinLine(120, 0, 120, 28, palette.marking, 5);
    drawEyeSkinLine(94, 0, 105, 27, palette.marking, 5);
    drawEyeSkinLine(146, 0, 135, 27, palette.marking, 5);
    drawEyeSkinLine(18, 45, 49, 55, palette.marking, 5);
    drawEyeSkinLine(222, 45, 191, 55, palette.marking, 5);
    drawEyeSkinLine(6, 76, 43, 79, palette.marking, 5);
    drawEyeSkinLine(234, 76, 197, 79, palette.marking, 5);
  } else if (skin == 3) {
    static const int16_t kSpots[][3] = {
        {19, 56, 7}, {39, 25, 6}, {74, 17, 5}, {166, 17, 5}, {201, 25, 6},
        {221, 56, 7}, {27, 100, 6}, {213, 100, 6}, {89, 118, 5}, {151, 118, 5}};
    for (const auto& spot : kSpots) {
      d.fillCircle(spot[0], spot[1], spot[2], palette.marking);
      d.fillCircle(spot[0], spot[1], max<int16_t>(2, spot[2] - 3), palette.face);
    }
  }

  if (skin >= 1 && skin <= 4) {
    uint16_t muzzle = skin == 4 ? rgb565_local(213, 154, 88) : palette.accent;
    d.fillEllipse(120, 111, 42, 24, muzzle);
    d.fillTriangle(113, 105, 127, 105, 120, 113, palette.marking);
    d.drawLine(110, 112, 65, 106, palette.accent);
    d.drawLine(110, 116, 62, 118, palette.accent);
    d.drawLine(130, 112, 175, 106, palette.accent);
    d.drawLine(130, 116, 178, 118, palette.accent);
  } else if (skin == 5) {
    d.fillEllipse(120, 111, 35, 23, rgb565_local(214, 209, 214));
    d.fillTriangle(113, 105, 127, 105, 120, 113, palette.marking);
  }
}

void drawSmoothUncannyEye(int16_t cx, int16_t cy, uint32_t now, uint8_t blink) {
  auto& d = gCanvas;
  EyeEmotion emotion = currentEyeEmotion();
  int16_t baseHalfH = 37;
  if (gFaceMode == FaceMode::Thinking || emotion == EyeEmotion::Thinking) {
    baseHalfH = 31;
  } else if (emotion == EyeEmotion::Sleepy || emotion == EyeEmotion::Happy) {
    baseHalfH = 23;
  } else if (emotion == EyeEmotion::Surprised || emotion == EyeEmotion::Excited) {
    baseHalfH = 40;
  }
  int16_t halfH = max<int16_t>(2, (baseHalfH * (255 - blink)) / 255);
  uint16_t outline = rgb565_local(17, 25, 27);
  uint16_t iris = rgb565_local(11, 37, 43);
  uint16_t glint = rgb565_local(74, 182, 196);
  if (gFaceMode == FaceMode::Listening) {
    iris = rgb565_local(10, 49, 56);
    glint = rgb565_local(82, 220, 236);
  } else if (gFaceMode == FaceMode::Speaking) {
    iris = rgb565_local(10, 47, 36);
    glint = rgb565_local(90, 255, 185);
  } else if (gFaceMode == FaceMode::Thinking) {
    iris = rgb565_local(48, 37, 14);
    glint = rgb565_local(246, 194, 74);
  } else if (gFaceMode == FaceMode::Error) {
    outline = rgb565_local(255, 72, 52);
    iris = rgb565_local(62, 16, 12);
    glint = rgb565_local(255, 110, 80);
  }

  if (halfH <= 3) {
    d.drawLine(cx - 42, cy, cx + 42, cy, outline);
    return;
  }
  int16_t lookX = clampValue<int16_t>(static_cast<int16_t>(gEyeLookX * 0.45f), -8, 8);
  int16_t lookY = clampValue<int16_t>(static_cast<int16_t>(gEyeLookY * 0.36f), -5, 5);
  if (gFaceMode == FaceMode::Speaking) {
    lookX += static_cast<int16_t>((now / 120U) % 5U) - 2;
  }
  int16_t irisHalfH = min<int16_t>(29, halfH - 2);
  int16_t pupilHalfH = min<int16_t>(22, max<int16_t>(3, irisHalfH - 6));
  d.fillEllipse(cx, cy, 50, halfH + 3, outline);
  d.fillEllipse(cx, cy, 47, halfH, rgb565_local(1, 3, 3));
  d.fillEllipse(cx + lookX, cy + lookY, 24, irisHalfH, iris);
  d.fillEllipse(cx + lookX, cy + lookY, 15, pupilHalfH, TFT_BLACK);
  if (halfH > 10) {
    d.fillCircle(cx + lookX - 8, cy + lookY - min<int16_t>(10, halfH / 3), 7, rgb565_local(238, 252, 255));
    d.fillCircle(cx + lookX - 5, cy + lookY - min<int16_t>(7, halfH / 4), 3, TFT_WHITE);
    d.fillCircle(cx + lookX + 10, cy + lookY + min<int16_t>(13, halfH / 3), 3, glint);
  }
}

void renderUncannyEyesUi() {
  uint32_t now = millis();
  EyeSkinPalette palette = eyeSkinPalette(gDeviceSettings.eyeSkin);
  drawEyeSkinBackdrop(gDeviceSettings.eyeSkin, palette);
  uint8_t blink = uncannyBlinkAmount(now);
  drawSmoothUncannyEye(62, 67, now, blink);
  drawSmoothUncannyEye(178, 67, now, blink);
  gCanvas.drawLine(18, 24, 106, 24, palette.accent);
  gCanvas.drawLine(134, 24, 222, 24, palette.accent);
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
