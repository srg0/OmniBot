  renderFacePanel(true);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void drawPetStatBar(int16_t x, int16_t y, int16_t w, const char* label, uint8_t value, uint16_t color, uint16_t bg) {
  auto& d = gCanvas;
  d.setFont(&fonts::Font0);
  d.setTextColor(rgb565_local(150, 164, 176), bg);
  d.setCursor(x, y);
  d.print(label);
  int16_t barX = x + 22;
  int16_t barW = max<int16_t>(20, w - 24);
  d.fillRoundRect(barX, y + 1, barW, 6, 3, rgb565_local(20, 32, 43));
  int16_t fillW = max<int16_t>(2, (barW * value) / 100);
  d.fillRoundRect(barX, y + 1, fillW, 6, 3, color);
}

void renderPetHelpOverlay() {
  if (!gPetHelpVisible) {
    return;
  }
  auto& d = gCanvas;
  const uint16_t shade = rgb565_local(2, 6, 12);
  const uint16_t panel = rgb565_local(8, 16, 28);
  const uint16_t accent = petMoodAccent();
  const uint16_t text = rgb565_local(223, 236, 244);
  const uint16_t muted = rgb565_local(139, 156, 170);
  d.fillRect(0, 0, 240, 135, shade);
  d.fillRoundRect(10, 9, 220, 117, 14, panel);
  d.drawRoundRect(10, 9, 220, 117, 14, accent);
  d.drawRoundRect(14, 13, 212, 109, 11, rgb565_local(28, 47, 63));

  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(20, 19);
  d.print("PET CONTROLS");
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(169, 23);
  d.print("Q close");

  const char* lines[] = {
      "Enter  auto-care / revive if gone",
      "F food   P play   C clean",
      "S sleep  M meds   D discipline",
      "H hunt Wi-Fi      E explore",
      "Ctrl+Shift+R      revive now",
      "/petrevive        chat command",
  };
  int16_t y = 42;
  for (const char* line : lines) {
    d.setTextColor(strstr(line, "Ctrl") || strstr(line, "/pet") ? accent : text, panel);
    d.setCursor(20, y);
    d.print(line);
    y += 13;
  }
}

void renderHeroUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 6, 12);
  const uint16_t panel = rgb565_local(7, 14, 24);
  const uint16_t panel2 = rgb565_local(9, 20, 31);
  const uint16_t grid = rgb565_local(13, 32, 45);
  const uint16_t muted = rgb565_local(132, 148, 160);
  const uint16_t accent = petMoodAccent();
  d.fillScreen(bg);

  d.fillRoundRect(6, 5, 228, 124, 14, panel);
  d.drawRoundRect(6, 5, 228, 124, 14, accent);
  d.drawRoundRect(10, 9, 220, 116, 11, rgb565_local(18, 34, 48));

  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(16, 14);
  d.print("OPENCLAW PET");
  String stage = petStageName(gPet.stage);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(222 - d.textWidth(stage), 14);
  d.print(stage);

  int16_t arenaX = 15;
  int16_t arenaY = 29;
  int16_t arenaW = 116;
  int16_t arenaH = 82;
  d.fillRoundRect(arenaX, arenaY, arenaW, arenaH, 10, panel2);
  d.drawRoundRect(arenaX, arenaY, arenaW, arenaH, 10, rgb565_local(30, 52, 68));
  for (int16_t gx = arenaX + 10; gx < arenaX + arenaW - 8; gx += 22) {
    d.drawFastVLine(gx, arenaY + 8, arenaH - 16, grid);
  }
  for (int16_t gy = arenaY + 12; gy < arenaY + arenaH - 8; gy += 16) {
    d.drawFastHLine(arenaX + 8, gy, arenaW - 16, grid);
  }

  int16_t petX = arenaX + arenaW / 2 + static_cast<int16_t>(gEyeLookX / 5.5f);
  int16_t petY = arenaY + 45 + static_cast<int16_t>(gEyeLookY / 7.0f);
  drawProceduralOpenClawPet(petX, petY, gPet.stage == PetStage::Egg ? 38 : 42, false);

  int16_t statX = 139;
  int16_t statY = 32;
  int16_t statW = 87;
  d.fillRoundRect(statX - 4, statY - 4, 91, 79, 9, rgb565_local(4, 11, 19));
  d.drawRoundRect(statX - 4, statY - 4, 91, 79, 9, rgb565_local(24, 42, 56));
  drawPetStatBar(statX, statY, statW, "HUN", gPet.hunger, rgb565_local(255, 194, 74), rgb565_local(4, 11, 19));
  drawPetStatBar(statX, statY + 11, statW, "FUN", gPet.happiness, rgb565_local(88, 240, 141), rgb565_local(4, 11, 19));
  drawPetStatBar(statX, statY + 22, statW, "HP", gPet.health, rgb565_local(128, 230, 104), rgb565_local(4, 11, 19));
  drawPetStatBar(statX, statY + 33, statW, "EN", gPet.energy, rgb565_local(47, 227, 255), rgb565_local(4, 11, 19));
  drawPetStatBar(statX, statY + 44, statW, "CLN", gPet.cleanliness, rgb565_local(34, 209, 189), rgb565_local(4, 11, 19));
  drawPetStatBar(statX, statY + 55, statW, "DSC", gPet.discipline, rgb565_local(170, 130, 255), rgb565_local(4, 11, 19));

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(18, 114);
  String bottom = String(petMoodName(gPet.mood)) + " " + petActivityName(gPet.activity) +
                  "  age " + petAgeText();
  d.print(fitCurrentFontToWidth(bottom, 124));
  d.setTextColor(accent, panel);
  d.setCursor(151, 114);
  d.print("Q controls");
  if (!gPet.alive) {
    d.setCursor(151, 123);
    d.print("Ctrl+Shift+R");
  }

  renderPetHelpOverlay();
  renderLauncherOverlay();
  renderDebugOverlay();
}

void drawAssistantPulseArc(int16_t cx, int16_t cy, int16_t outerR, int16_t innerR,
                           float startRad, float endRad, uint16_t color) {
  int32_t startDeg = static_cast<int32_t>(startRad * 180.0f / kPi) % 360;
  int32_t endDeg = static_cast<int32_t>(endRad * 180.0f / kPi) % 360;
  if (startDeg < 0) startDeg += 360;
  if (endDeg < 0) endDeg += 360;
  gCanvas.drawArc(cx, cy, outerR, innerR, startDeg, endDeg, color);
  int16_t capR = max<int16_t>(2, (outerR - innerR) / 2);
  float midR = (outerR + innerR) * 0.5f;
  gCanvas.fillCircle(cx + static_cast<int16_t>(cosf(startRad) * midR),
                     cy + static_cast<int16_t>(sinf(startRad) * midR), capR, color);
  gCanvas.fillCircle(cx + static_cast<int16_t>(cosf(endRad) * midR),
                     cy + static_cast<int16_t>(sinf(endRad) * midR), capR, color);
}

void drawAssistantPulseButton(int16_t x, int16_t y, int16_t w, int16_t h,
                              const char* label, uint16_t accent, bool pressed) {
  auto& d = gCanvas;
  int16_t py = y + (pressed ? 1 : 0);
  uint16_t fill = pressed ? rgb565_local(16, 34, 42) : rgb565_local(7, 20, 26);
  d.fillRoundRect(x, py, w, h, 5, fill);
  d.drawRoundRect(x, py, w, h, 5, accent);
  if (pressed) {
    d.drawFastHLine(x + 8, py + 4, w - 16, rgb565_local(170, 200, 210));
  }
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, fill);
  int tw = d.textWidth(label);
  d.setCursor(x + (w - tw) / 2, py + 6);
  d.print(label);
}

void renderAssistantPulseChatHint() {
  const bool hasInput = !gInputBuffer.isEmpty();
  if (!hasInput && assistantPulseState() != AssistantPulseState::Ready) {
    return;
  }
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(6, 16, 22);
  const uint16_t edge = hasInput ? rgb565_local(47, 227, 255) : rgb565_local(42, 78, 88);
  const uint16_t text = hasInput ? TFT_YELLOW : rgb565_local(128, 145, 155);
  d.fillRoundRect(10, 107, 132, 19, 7, bg);
  d.drawRoundRect(10, 107, 132, 19, 7, edge);
  d.setFont(&fonts::efontCN_12);
  d.setTextColor(text, bg);
  d.setCursor(17, 112);
  d.print(hasInput ? tailUtf8ToFit("> " + gInputBuffer, 116) : "> type to chat");
}

void renderAssistantPulseUi() {
  auto& d = gCanvas;
  ensureBangkokTimezone();
  AssistantPulseState state = assistantPulseState();

  constexpr uint16_t bg = rgb565_local(2, 7, 10);
  constexpr uint16_t white = rgb565_local(244, 251, 255);
  constexpr uint16_t cyan = rgb565_local(47, 227, 255);
  constexpr uint16_t violet = rgb565_local(157, 102, 255);
  constexpr uint16_t red = rgb565_local(255, 63, 85);
  constexpr uint16_t amber = rgb565_local(246, 194, 74);
  constexpr uint16_t green = rgb565_local(88, 240, 141);
  uint16_t c1 = cyan;
  uint16_t c2 = violet;
  uint16_t c3 = rgb565_local(23, 77, 90);
  uint16_t fg = white;
  const char* label = "LISTEN";

  if (state == AssistantPulseState::Recording) {
    c1 = red;
    c2 = rgb565_local(255, 178, 59);
    c3 = rgb565_local(88, 32, 42);
    fg = rgb565_local(255, 241, 239);
    label = "REC";
  } else if (state == AssistantPulseState::Thinking) {
    c1 = amber;
    c2 = cyan;
    c3 = rgb565_local(64, 52, 26);
    fg = rgb565_local(255, 248, 232);
    label = "THINK";
  } else if (state == AssistantPulseState::Playback) {
    c1 = green;
    c2 = cyan;
    c3 = rgb565_local(22, 73, 46);
    fg = rgb565_local(241, 255, 246);
    label = "PLAY";
  }

  d.fillScreen(bg);
  d.fillRoundRect(0, 0, 240, 135, 6, bg);
