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

  if (gPet.stage == PetStage::Egg && gPet.alive) {
    uint16_t shell = rgb565_local(238, 224, 184);
    uint16_t shell2 = rgb565_local(255, 242, 202);
    d.fillCircle(cx, cy + r / 8, r, shell);
    d.fillRoundRect(cx - r, cy - r / 2, r * 2, r + r / 2, r / 2, shell);
    d.fillCircle(cx - r / 4, cy - r / 5, r / 3, shell2);
    d.drawCircle(cx, cy + r / 8, r, accent);
    d.drawLine(cx - r / 4, cy - r / 5, cx, cy, line);
    d.drawLine(cx, cy, cx - r / 7, cy + r / 4, line);
    d.drawLine(cx - r / 7, cy + r / 4, cx + r / 5, cy + r / 2, line);
    if (!mini) {
      d.setFont(&fonts::Font0);
      d.setTextColor(accent, rgb565_local(8, 18, 26));
      d.setCursor(cx - 18, cy + r + 4);
      d.print("hatching");
    }
    return;
  }

  int16_t bodyW = r * 2;
  int16_t bodyH = r * 2 + r / 4;
  d.fillTriangle(cx - r + 3, cy - r + 4, cx - r / 2, cy - r - r / 2, cx - r / 6, cy - r + 8, line);
  d.fillTriangle(cx + r - 3, cy - r + 4, cx + r / 2, cy - r - r / 2, cx + r / 6, cy - r + 8, line);
  d.fillRoundRect(cx - r, cy - r, bodyW, bodyH, r / 2, line);
  d.fillRoundRect(cx - r + 3, cy - r + 3, bodyW - 6, bodyH - 6, r / 2, body);
  d.fillCircle(cx - r / 3, cy - r / 3, max<int16_t>(3, r / 5), highlight);
  d.fillCircle(cx + r / 3, cy - r / 3, max<int16_t>(3, r / 5), highlight);

  int16_t clawY = cy + r / 2;
  d.fillCircle(cx - r - 2, clawY, max<int16_t>(3, r / 5), line);
  d.fillCircle(cx + r + 2, clawY, max<int16_t>(3, r / 5), line);
  d.fillCircle(cx - r - 2, clawY, max<int16_t>(2, r / 5 - 2), rgb565_local(196, 45, 48));
  d.fillCircle(cx + r + 2, clawY, max<int16_t>(2, r / 5 - 2), rgb565_local(196, 45, 48));

  int16_t eyeY = cy - r / 5;
  int16_t leftEye = cx - r / 3;
  int16_t rightEye = cx + r / 3;
  if (!gPet.alive) {
    int16_t e = max<int16_t>(3, r / 7);
    d.drawLine(leftEye - e, eyeY - e, leftEye + e, eyeY + e, line);
    d.drawLine(leftEye - e, eyeY + e, leftEye + e, eyeY - e, line);
    d.drawLine(rightEye - e, eyeY - e, rightEye + e, eyeY + e, line);
    d.drawLine(rightEye - e, eyeY + e, rightEye + e, eyeY - e, line);
  } else if (gPet.mood == PetMood::Sleepy || gPet.activity == PetActivity::Sleeping) {
    d.drawFastHLine(leftEye - r / 7, eyeY, max<int16_t>(4, r / 4), line);
    d.drawFastHLine(rightEye - r / 7, eyeY, max<int16_t>(4, r / 4), line);
  } else if (gPet.mood == PetMood::Happy || gPet.mood == PetMood::Excited) {
    d.drawArc(leftEye, eyeY + 2, max<int16_t>(5, r / 5), max<int16_t>(3, r / 5 - 2), 205, 335, line);
    d.drawArc(rightEye, eyeY + 2, max<int16_t>(5, r / 5), max<int16_t>(3, r / 5 - 2), 205, 335, line);
  } else {
    d.fillCircle(leftEye, eyeY, max<int16_t>(2, r / 8), line);
    d.fillCircle(rightEye, eyeY, max<int16_t>(2, r / 8), line);
  }

  int16_t mouthY = cy + r / 5;
  if (gPet.mood == PetMood::Hungry || gPet.mood == PetMood::Bored || gPet.mood == PetMood::Sick) {
    d.drawArc(cx, mouthY + r / 8, max<int16_t>(6, r / 4), max<int16_t>(4, r / 4 - 2), 205, 335, line);
  } else if (gPet.activity == PetActivity::Eating) {
    d.fillRoundRect(cx - r / 5, mouthY - 1, max<int16_t>(4, r / 3), max<int16_t>(3, r / 7), 2, line);
  } else {
    d.drawArc(cx, mouthY - r / 8, max<int16_t>(7, r / 4), max<int16_t>(5, r / 4 - 2), 25, 155, line);
  }

  if (mini) {
    return;
  }

  if (gPet.activity == PetActivity::Eating || gPet.activity == PetActivity::Hunting) {
    for (int i = 0; i < 5; ++i) {
      int16_t fx = cx - r - 16 + i * 7;
      int16_t fy = cy - 18 + static_cast<int16_t>((now / 120 + i * 3) % 18);
