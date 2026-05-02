      d.fillCircle(fx, fy, 2, rgb565_local(255, 194, 74));
    }
  }
  if (gPet.activity == PetActivity::Playing || gPet.mood == PetMood::Excited) {
    drawPetSpark(cx - r - 11, cy - r + 10, 4, accent);
    drawPetSpark(cx + r + 12, cy - r / 3, 3, rgb565_local(255, 194, 74));
  }
  if (gPet.activity == PetActivity::Cleaning || gPet.mood == PetMood::Dirty) {
    for (int i = 0; i < 4; ++i) {
      int16_t bx = cx + r + 9 + (i % 2) * 8;
      int16_t by = cy - r / 2 + i * 10 - static_cast<int16_t>((now / 150) % 5);
      d.drawCircle(bx, by, 3 + (i % 2), rgb565_local(47, 227, 255));
    }
  }
  if (gPet.activity == PetActivity::Sleeping) {
    d.setFont(&fonts::Font2);
    d.setTextColor(accent, rgb565_local(8, 18, 26));
    d.setCursor(cx + r + 9, cy - r - 8);
    d.print("Z");
    d.setCursor(cx + r + 17, cy - r - 16);
    d.print("z");
  }
  if (gPet.activity == PetActivity::Exploring) {
    int sweep = static_cast<int>((now / 10) % 360);
    d.drawArc(cx, cy, r + 13, r + 10, sweep, sweep + 70, accent);
    d.drawArc(cx, cy, r + 23, r + 20, 220 - sweep, 290 - sweep, rgb565_local(255, 194, 74));
  }
  if (gPet.activity == PetActivity::Medicine || gPet.mood == PetMood::Sick) {
    d.drawFastHLine(cx + r + 8, cy + r / 2, 13, rgb565_local(128, 230, 104));
    d.drawFastVLine(cx + r + 14, cy + r / 2 - 6, 13, rgb565_local(128, 230, 104));
  }
  if (gPet.activity == PetActivity::Discipline) {
    d.setFont(&fonts::Font4);
    d.setTextColor(rgb565_local(255, 194, 74), rgb565_local(8, 18, 26));
    d.setCursor(cx + r + 8, cy - r / 2);
    d.print("!");
  }
}

const uint16_t* currentHeroFrame() {
  uint32_t tick = millis();
  switch (gFaceMode) {
    case FaceMode::Speaking:
      return ((tick / 170) % 2) == 0 ? kHeroTalk1 : kHeroTalk2;
    case FaceMode::Listening:
      return ((tick / 240) % 2) == 0 ? kHeroHappy1 : kHeroIdle1;
    case FaceMode::Thinking:
      return ((tick / 280) % 2) == 0 ? kHeroIdle3 : kHeroIdle1;
    case FaceMode::Error:
      return ((tick / 150) % 2) == 0 ? kHeroTalk1 : kHeroIdle2;
    case FaceMode::Idle:
    default: {
      uint32_t phase = (tick / 260) % 8;
      if (phase == 2) {
        return kHeroIdle2;
      }
      if (phase == 5 || phase == 6) {
        return kHeroIdle3;
      }
      if ((tick / 7000) % 5 == 0) {
        return kHeroHappy1;
      }
      return kHeroIdle1;
    }
  }
}

void drawHeroSprite(int16_t x, int16_t y, int scale, const uint16_t* sprite) {
  auto& d = gCanvas;
  for (int py = 0; py < kHeroSpriteH; ++py) {
    for (int px = 0; px < kHeroSpriteW; ++px) {
      uint16_t color = pgm_read_word(&sprite[py * kHeroSpriteW + px]);
      if (color == HCLEAR) {
        continue;
      }
      d.fillRect(x + px * scale, y + py * scale, scale, scale, color);
    }
  }
}

void renderLauncherLivingPreview(int16_t x, int16_t y, int16_t w, int16_t h) {
  auto& d = gCanvas;
  d.fillRoundRect(x, y, w, h, 10, 0x0841);
  d.drawRoundRect(x, y, w, h, 10, petMoodAccent());
  d.setFont(&fonts::Font2);
  d.setTextColor(petMoodAccent(), 0x0841);
  d.setCursor(x + 8, y + 8);
  d.print("Pet");

  int16_t cellY = y + 18;
  int16_t cellW = 38;
  int16_t cellH = 38;
  int16_t eyeX = x + 8;
  int16_t heroX = x + w - cellW - 8;

  d.fillRoundRect(eyeX, cellY, cellW, cellH, 8, 0x18C3);
  d.drawRoundRect(eyeX, cellY, cellW, cellH, 8, 0x4208);
  d.fillRoundRect(heroX, cellY, cellW, cellH, 8, 0x18C3);
  d.drawRoundRect(heroX, cellY, cellW, cellH, 8, 0x4208);

  renderOmniEyePair(eyeX + 12, eyeX + 26, cellY + 19, 7, 8, 0x18C3);

  drawProceduralOpenClawPet(heroX + cellW / 2, cellY + cellH / 2, 25, true);

  d.setTextColor(TFT_LIGHTGREY, 0x0841);
  d.setCursor(x + 8, y + h - 18);
  d.print(String(petStageName(gPet.stage)) + " " + petAgeText());
  d.setTextColor(petMoodAccent(), 0x0841);
  d.setCursor(x + 8, y + h - 8);
  d.print(String(petMoodName(gPet.mood)) + " " + petActivityName(gPet.activity));
}

void renderFacePanel(bool fullscreen = false) {
  auto& d = gCanvas;
  int16_t panelX = (fullscreen ? 2 : 4) + static_cast<int16_t>(gEyePanelShiftX);
  int16_t panelY = (fullscreen ? 2 : 4) + static_cast<int16_t>(gEyePanelShiftY);
  int16_t panelW = fullscreen ? 236 : 232;
  int16_t panelH = fullscreen ? 126 : 72;
  d.fillRoundRect(panelX, panelY, panelW, panelH, 18, 0x10A2);
  d.drawRoundRect(panelX, panelY, panelW, panelH, 18, 0x4208);
  d.fillRoundRect(panelX + 3, panelY + 3, panelW - 6, panelH - 6, 16, 0x0861);
  int16_t eyeCenterY = fullscreen ? panelY + 63 : panelY + 35;
  int16_t leftEyeX = fullscreen ? panelX + 64 : panelX + 60;
  int16_t rightEyeX = fullscreen ? panelX + 172 : panelX + 172;
  int16_t eyeHalfW = fullscreen ? 52 : 41;
  int16_t eyeHalfH = fullscreen ? 36 : 25;
  renderOmniEyePair(leftEyeX, rightEyeX, eyeCenterY, eyeHalfW, eyeHalfH, 0x0861);

  if (gSubmitting || gThinking) {
    int16_t arrowY = fullscreen ? panelY + 12 : panelY + 10;
    int16_t centerX = panelX + panelW / 2;
    uint16_t thinkingAccent = currentEyeEmotion() == EyeEmotion::Thinking ? TFT_GOLD : rgb565_local(120, 255, 170);
    d.fillTriangle(centerX - 16, arrowY + 10, centerX - 10, arrowY, centerX - 4, arrowY + 10, thinkingAccent);
    d.fillTriangle(centerX + 4, arrowY + 10, centerX + 10, arrowY, centerX + 16, arrowY + 10, thinkingAccent);
  }
}

void drawScrollIndicators(bool canScrollUp, bool canScrollDown) {
  auto& d = gCanvas;
  uint16_t active = 0x9CD3;
  uint16_t muted = 0x4208;
  d.fillTriangle(228, 84, 234, 74, 239, 84, canScrollUp ? active : muted);
  d.fillTriangle(228, 110, 234, 120, 239, 110, canScrollDown ? active : muted);
}

void renderDebugOverlay() {
  if (!gDebugOverlayVisible) {
    return;
  }
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(0, 3, 8);
  const uint16_t line = rgb565_local(53, 68, 82);
  d.fillRoundRect(8, 8, 224, 104, 10, bg);
  d.drawRoundRect(8, 8, 224, 104, 10, line);
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_WHITE, bg);
  d.setCursor(14, 15);
  d.print("DBG");
  d.setCursor(48, 15);
  d.print(gWifiReady ? "wifi=ok" : "wifi=off");
  d.setCursor(118, 15);
  d.print(gWsReady ? "ws=ok" : "ws=off");
  d.setCursor(14, 28);
  d.print("face=");
  d.print(static_cast<int>(gFaceMode));
  d.setCursor(86, 28);
  d.print("scroll=");
  d.print(gChatScrollOffset);
  d.setCursor(150, 28);
  d.print(layoutName(gKeyboardLayout));
  d.setCursor(14, 41);
  d.print("rec=");
  d.print(static_cast<unsigned>(gRecordCapacityBytes / 1024));
  d.print("k");
  d.setCursor(86, 41);
  d.print("tts=");
  d.print(static_cast<unsigned>(gTtsCapacityBytes / 1024));
  d.print("k");
  d.setCursor(150, 41);
  d.print(gStorageReady ? gStorageLabel : "fs=off");
  d.setCursor(14, 55);
  String diag = gLastVoiceDiag;
  if (diag.length() > 28) {
    diag = diag.substring(diag.length() - 28);
  }
  d.print(diag);
  d.setCursor(14, 69);
  String httpDiag = gLastHttpDiag;
  if (httpDiag.length() > 28) {
    httpDiag = httpDiag.substring(httpDiag.length() - 28);
  }
  d.print(httpDiag);
  d.setCursor(14, 83);
  String keyDiag = gLastKeyDiag;
  if (keyDiag.length() > 28) {
    keyDiag = keyDiag.substring(keyDiag.length() - 28);
  }
  d.print(keyDiag);
  d.setCursor(150, 83);
  d.print(String(uiModeName(gUiMode)).substring(0, 6));
  d.setFont(&fonts::Font2);
}

void drawLauncherIcon(uint8_t group, int16_t x, int16_t y, int16_t s, uint16_t accent, uint16_t bg) {
  auto& d = gCanvas;
