
void renderLogsUi() {
  drawCardHeader("Logs / Status", 0xC618);
  auto& d = gCanvas;
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_WHITE, 0x0841);
  String statusLine = String(WiFi.isConnected() ? WiFi.localIP().toString() : "wifi off") + " " + gStorageLabel;
  d.setCursor(16, 32);
  d.print(fitCurrentFontToWidth(statusLine, 208));
  int visible = 9;
  int maxOffset = max<int>(0, static_cast<int>(gRuntimeLogs.size()) - visible);
  gLogScrollOffset = clampValue<int>(gLogScrollOffset, 0, maxOffset);
  int start = max<int>(0, static_cast<int>(gRuntimeLogs.size()) - visible - gLogScrollOffset);
  for (int i = 0; i < visible; ++i) {
    int idx = start + i;
    if (idx >= static_cast<int>(gRuntimeLogs.size())) {
      break;
    }
    d.setTextColor(TFT_LIGHTGREY, 0x0841);
    d.setCursor(16, 43 + i * 8);
    d.print(fitCurrentFontToWidth(gRuntimeLogs[idx], 208));
  }
  d.setFont(&fonts::Font2);
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "< > scroll  R reload  U upload", 16, 116, 208);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderTopicOverlayIfVisible() {
  if (millis() > gTopicOverlayUntilMs) {
    return;
  }
  auto& d = gCanvas;

  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(5, 13, 18);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t violet = rgb565_local(157, 102, 255);
  const uint16_t muted = rgb565_local(128, 145, 155);
  const uint16_t amber = rgb565_local(246, 194, 74);

  uint32_t now = millis();
  if (gTopicCreateArmed && now > gTopicCreateArmedUntilMs) {
    gTopicCreateArmed = false;
  }
  if (gTopicCreateArmed) {
    float hold = 1.0f - min<float>(1.0f, static_cast<float>(gTopicCreateArmedUntilMs - now) / 3200.0f);
    d.fillRoundRect(6, 5, 228, 39, 10, rgb565_local(5, 12, 18));
    d.drawRoundRect(6, 5, 228, 39, 10, amber);
    d.fillRoundRect(10, 10, 42, 24, 8, rgb565_local(34, 22, 8));
    d.setFont(&fonts::Font2);
    d.setTextColor(amber, rgb565_local(34, 22, 8));
    d.setCursor(18, 17);
    d.print("NEW");
    d.setFont(&fonts::efontCN_12);
    d.setTextColor(TFT_WHITE, rgb565_local(5, 12, 18));
    d.setCursor(60, 9);
    d.print(fitCurrentFontToWidth(defaultNewTopicTitle(), 144));
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, rgb565_local(5, 12, 18));
    d.setCursor(61, 27);
    d.print("Alt+Left again creates");
    d.fillRoundRect(10, 39, 216, 2, 1, rgb565_local(52, 38, 16));
    d.fillRoundRect(10, 39, max<int16_t>(4, static_cast<int16_t>(216 * hold)), 2, 1, amber);
    d.setFont(&fonts::Font2);
    return;
  }

  if (gContexts.empty()) {
    return;
  }
  gSelectedContext = clampValue<int>(gSelectedContext, 0, static_cast<int>(gContexts.size()) - 1);
  const ConversationContext& ctx = gContexts[gSelectedContext];

  float animT = min<float>(1.0f, (now - gContextAnimStartMs) / 220.0f);
  float ease = 1.0f - (1.0f - animT) * (1.0f - animT);
  int16_t dx = (gContextAnimDir == 0 || animT >= 1.0f)
                   ? 0
                   : static_cast<int16_t>(gContextAnimDir * (1.0f - ease) * 28.0f);

  d.fillRoundRect(6, 5, 228, 35, 8, bg);
  d.drawRoundRect(6, 5, 228, 35, 8, cyan);
  d.fillRoundRect(10, 9, 42, 23, 7, panel);
  d.drawRoundRect(10, 9, 42, 23, 7, violet);

  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(16, 16);
  d.print(fitCurrentFontToWidth(topicDisplayIcon(ctx), 30));

  d.setFont(&fonts::efontCN_12);
  d.setTextColor(cyan, bg);
  d.setCursor(60 + dx, 9);
  d.print(fitCurrentFontToWidth(topicDisplayTitle(ctx), 132));

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, bg);
  d.setCursor(61 + dx, 25);
  String sub;
  if (ctx.unread > 0) {
    sub += "  +" + String(ctx.unread);
  } else if (gTopicTaskHandle != nullptr) {
    sub = "syncing...";
  } else {
    sub = "active chat topic";
  }
  d.print(fitCurrentFontToWidth(sub, 132));

  d.setTextColor(gTopicTaskHandle != nullptr ? amber : cyan, bg);
  d.setCursor(204, 14);
  d.print("<>");
  d.fillRect(12, 38, 216, 2, gTopicTaskHandle != nullptr ? amber : cyan);
  d.setFont(&fonts::Font2);
}

void renderModeFooter(const String& left, const String& right = "") {
  auto& d = gCanvas;
  d.fillRect(0, 123, 240, 12, 0x18C3);
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_LIGHTGREY, 0x18C3);
  d.setCursor(4, 124);
  d.print(fitCurrentFontToWidth(left, right.isEmpty() ? 230 : 132));
  if (!right.isEmpty()) {
    int rightWidth = d.textWidth(right);
    d.setCursor(max<int>(4, 236 - rightWidth), 124);
    d.print(fitCurrentFontToWidth(right, 96));
  }
  d.setFont(&fonts::Font2);
}

void renderFaceOnlyUi() {
  auto& d = gCanvas;
  d.fillScreen(0x0841);
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
