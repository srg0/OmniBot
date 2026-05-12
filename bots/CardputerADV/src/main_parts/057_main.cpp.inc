String settingValue(uint8_t index) {
  switch (index) {
    case 0:
      return String(gFocusSettings.focusSec / 60) + "m";
    case 1:
      return String(gFocusSettings.shortBreakSec / 60) + "m";
    case 2:
      return String(gFocusSettings.longBreakSec / 60) + "m";
    case 3:
      return String(gFocusSettings.cyclesPerRound);
    case 4:
      return gFocusSettings.autoStart ? "on" : "off";
    case 5:
      return gDeviceSettings.autoUpdateFirmware ? "on" : "off";
    case 6:
      return String(gFocusSettings.bpm);
    case 7:
      return String(gDeviceSettings.minBatteryForUpdate) + "%";
    case 8:
      return gDeviceSettings.audioLanguage;
    case 9:
      return clockFaceName(gDeviceSettings.clockFace);
    default:
      return "-";
  }
}

void renderSettingsUi() {
  drawCardHeader("Settings", 0x7BEF);
  auto& d = gCanvas;
  gSelectedSetting = clampValue<int>(gSelectedSetting, 0, kSettingsItemCount - 1);
  constexpr int visible = 7;
  int start = max<int>(0, min<int>(gSelectedSetting - visible / 2, kSettingsItemCount - visible));
  d.setFont(&fonts::Font0);
  for (int i = 0; i < visible; ++i) {
    int idx = start + i;
    if (idx >= kSettingsItemCount) {
      break;
    }
    int y = 34 + i * 11;
    bool selected = idx == gSelectedSetting;
    uint16_t bg = selected ? 0x4208 : 0x0841;
    d.fillRoundRect(14, y - 1, 212, 10, 3, bg);
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, bg);
    d.setCursor(18, y + 1);
    d.print(fitCurrentFontToWidth(settingLabel(idx), 126));
    String value = settingValue(idx);
    String fittedValue = fitCurrentFontToWidth(value, 72);
    int w = d.textWidth(fittedValue);
    d.setCursor(max<int>(144, 222 - w), y + 1);
    d.print(fittedValue);
  }
  d.setFont(&fonts::Font2);
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "< > pick  +/- edit  Space toggle", 16, 116, 208);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderCalculatorUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(3, 46, 21);
  const uint16_t panel = rgb565_local(5, 69, 31);
  const uint16_t row = rgb565_local(11, 92, 42);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(176, 222, 190);
  uint16_t accent = gCalcStatus == "Error" ? amber : green;

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 12, panel);
  d.drawRoundRect(6, 5, 228, 124, 12, accent);
  d.drawRoundRect(10, 9, 220, 116, 9, rgb565_local(24, 126, 62));

  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(16, 14);
  d.print("CALCULATOR");
  d.setFont(&fonts::Font0);
  d.setTextColor(accent, panel);
  d.setCursor(224 - d.textWidth(gCalcStatus), 17);
  d.print(gCalcStatus);

  d.fillRoundRect(16, 34, 208, 28, 8, row);
  d.setFont(&fonts::efontCN_12);
  d.setTextColor(TFT_WHITE, row);
  String expr = gCalcExpression.isEmpty() ? "type expression" : tailUtf8ToFit(gCalcExpression, 188);
  d.setCursor(24, 41);
  d.print(expr);

  d.fillRoundRect(16, 68, 208, 25, 8, rgb565_local(2, 37, 18));
  d.setFont(&fonts::Font4);
  d.setTextColor(accent, rgb565_local(2, 37, 18));
  String result = fitCurrentFontToWidth(gCalcResult, 194);
  d.setCursor(218 - d.textWidth(result), 72);
  d.print(result);

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(19, 101);
  d.print("0-9 + - * / . ( )");
  d.setTextColor(cyan, panel);
  d.setCursor(142, 101);
  d.print("Enter =");
  d.setTextColor(amber, panel);
  d.setCursor(19, 114);
  d.print("C clear");
  d.setTextColor(muted, panel);
  d.setCursor(82, 114);
  d.print("Del back  Esc home");
  d.setFont(&fonts::Font2);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void resetCMatrixColumns() {
  for (uint8_t col = 0; col < kCMatrixCols; ++col) {
    gCMatrixHead[col] = -static_cast<int8_t>(random(0, 18));
    gCMatrixSpeed[col] = static_cast<uint8_t>(1 + random(0, 3));
  }
  gCMatrixTick = 0;
  gCMatrixLastStepMs = millis();
  gCMatrixInitialized = true;
}

char cMatrixGlyphAt(uint8_t col, int16_t row, uint8_t trail) {
  static const char glyphs[] = {'C', 'M', 'A', 'T', 'R', 'I', 'X', '0', '1', '+', '*', '#'};
  constexpr uint8_t glyphCount = sizeof(glyphs) / sizeof(glyphs[0]);
  uint16_t mix = static_cast<uint16_t>(col) * 13U + static_cast<uint16_t>(row + 32) * 7U +
                 static_cast<uint16_t>(trail) * 5U + gCMatrixTick;
  return glyphs[mix % glyphCount];
}

void renderCMatrixUi() {
  auto& d = gCanvas;
  constexpr int16_t cellW = 8;
  constexpr int16_t cellH = 8;
  constexpr int16_t rows = 17;
  constexpr uint8_t trailLen = 9;
  const uint16_t bg = rgb565_local(0, 5, 2);
  const uint16_t deep = rgb565_local(0, 18, 6);
  const uint16_t dim = rgb565_local(0, 72, 22);
  const uint16_t mid = rgb565_local(0, 150, 48);
  const uint16_t bright = rgb565_local(118, 255, 157);

  if (!gCMatrixInitialized) {
    resetCMatrixColumns();
  }

  uint32_t now = millis();
  if (now - gCMatrixLastStepMs >= 75U) {
    gCMatrixLastStepMs = now;
    ++gCMatrixTick;
    for (uint8_t col = 0; col < kCMatrixCols; ++col) {
      if ((gCMatrixTick % gCMatrixSpeed[col]) != 0) {
        continue;
      }
      ++gCMatrixHead[col];
      if (gCMatrixHead[col] > rows + static_cast<int16_t>(trailLen) + random(0, 8)) {
        gCMatrixHead[col] = -static_cast<int8_t>(random(1, 15));
        gCMatrixSpeed[col] = static_cast<uint8_t>(1 + random(0, 3));
      }
    }
  }

  d.fillScreen(bg);
  d.setFont(&fonts::Font0);
  for (uint8_t col = 0; col < kCMatrixCols; ++col) {
    int16_t x = static_cast<int16_t>(col) * cellW;
    for (uint8_t trail = 0; trail < trailLen; ++trail) {
      int16_t row = static_cast<int16_t>(gCMatrixHead[col]) - trail;
      if (row < 0 || row >= rows) {
        continue;
      }
      uint16_t color = trail == 0 ? bright : (trail < 3 ? mid : (trail < 6 ? dim : deep));
      d.setTextColor(color, bg);
      d.setCursor(x + 1, row * cellH);
      d.print(cMatrixGlyphAt(col, row, trail));
    }
  }

  d.fillRoundRect(6, 5, 83, 16, 5, rgb565_local(0, 12, 4));
  d.drawRoundRect(6, 5, 83, 16, 5, dim);
  d.setTextColor(bright, rgb565_local(0, 12, 4));
  d.setCursor(12, 10);
  d.print("C-MATRIX");
  d.setTextColor(dim, bg);
  d.setCursor(141, 122);
  d.print("Esc home  Tab menu");

  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderTetrisPreview(uint8_t piece, int16_t x0, int16_t y0, int16_t cell, uint16_t bg) {
  auto& d = gCanvas;
  for (uint8_t y = 0; y < 4; ++y) {
    for (uint8_t x = 0; x < 4; ++x) {
      if (!tetrisShapeCell(piece, 0, x, y)) {
        continue;
      }
      uint16_t color = tetrisPieceColor(static_cast<uint8_t>(piece + 1));
      int16_t px = x0 + x * cell;
      int16_t py = y0 + y * cell;
      d.fillRoundRect(px, py, cell - 1, cell - 1, 2, color);
      d.drawRoundRect(px, py, cell - 1, cell - 1, 2, bg);
    }
  }
}

void renderTetrisRightHandPortraitUi() {
  auto& d = gCanvas;
  gLauncherVisible = false;
  const uint16_t bg = rgb565_local(2, 6, 12);
  const uint16_t panel = rgb565_local(7, 14, 23);
  const uint16_t panel2 = rgb565_local(4, 10, 17);
  const uint16_t grid = rgb565_local(18, 30, 40);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(128, 145, 155);
  const uint16_t red = rgb565_local(255, 82, 82);
  const int16_t vw = 135;
  const int16_t vh = 240;
  const int16_t cell = 13;
  const int16_t boardX = (vw - kTetrisCols * cell) / 2;
  const int16_t boardY = 17;
  const int16_t boardW = kTetrisCols * cell;
  const int16_t boardH = kTetrisRows * cell;

  // Right-hand portrait: device is held with the keyboard side in the right hand,
  // so the game is rendered clockwise into a tall 135x240 viewport.
  d.setRotation(1);
  d.fillScreen(bg);

  d.setFont(&fonts::Font0);
  d.setTextColor(cyan, bg);
  d.setCursor(4, 4);
  d.print("TETRIS");
  String scoreText = String("S ") + String(gTetrisScore);
  d.setTextColor(green, bg);
  d.setCursor(vw - 4 - d.textWidth(scoreText), 4);
  d.print(scoreText);

  d.fillRoundRect(boardX - 2, boardY - 2, boardW + 4, boardH + 4, 4, panel2);
  d.drawRoundRect(boardX - 2, boardY - 2, boardW + 4, boardH + 4, 4, gTetrisGameOver ? red : cyan);
  for (uint8_t y = 0; y < kTetrisRows; ++y) {
    for (uint8_t x = 0; x < kTetrisCols; ++x) {
      uint8_t cellValue = gTetrisBoard[y][x];
      uint8_t activeValue = 0;
      if (tetrisActiveCellAt(x, y, activeValue)) {
        cellValue = activeValue;
      }
      uint16_t fill = cellValue ? tetrisPieceColor(cellValue) : rgb565_local(8, 17, 24);
      int16_t px = boardX + x * cell;
      int16_t py = boardY + y * cell;
      d.fillRoundRect(px, py, cell - 1, cell - 1, 2, fill);
      if (!cellValue && ((x + y) % 2 == 0)) {
        d.drawPixel(px + cell / 2, py + cell / 2, grid);
      }
    }
  }

  String bottomLeft = String("L") + String(gTetrisLines) + " Lv" + String(gTetrisLevel);
  d.setTextColor(amber, bg);
  d.setCursor(4, 229);
  d.print(bottomLeft);
  d.setTextColor(muted, bg);
  String helpText = gTetrisHelpVisible ? String("Tab close") : String("Tab help");
  d.setCursor(vw - 4 - d.textWidth(helpText), 229);
  d.print(helpText);

  if (!gTetrisStarted || gTetrisGameOver) {
    d.fillRoundRect(12, 94, vw - 24, 48, 9, rgb565_local(3, 8, 14));
    d.drawRoundRect(12, 94, vw - 24, 48, 9, gTetrisGameOver ? red : amber);
    d.setFont(&fonts::Font2);
    d.setTextColor(gTetrisGameOver ? red : amber, rgb565_local(3, 8, 14));
    String prompt = gTetrisGameOver ? "GAME OVER" : "ENTER START";
    d.setCursor((vw - d.textWidth(prompt)) / 2, 103);
    d.print(prompt);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, rgb565_local(3, 8, 14));
    d.setCursor(27, 123);
    d.print("Tab shows controls");
  }

  if (gTetrisHelpVisible) {
    d.fillRoundRect(7, 35, vw - 14, 151, 10, panel);
    d.drawRoundRect(7, 35, vw - 14, 151, 10, cyan);
    d.setFont(&fonts::Font2);
    d.setTextColor(cyan, panel);
    d.setCursor(21, 45);
    d.print("ONE HAND");
    d.setFont(&fonts::Font0);
    d.setTextColor(TFT_WHITE, panel);
    const char* lines[] = {
        "J  left", "L  right", "K  soft drop", "I  rotate", "Enter hard drop",
        "P  pause", "R  restart", "G  landscape", "Tab close"};
    int16_t y = 68;
    for (const char* line : lines) {
      d.setCursor(20, y);
      d.print(line);
      y += 12;
    }
    d.setTextColor(amber, panel);
    d.setCursor(20, 173);
    d.print(fitCurrentFontToWidth(gTetrisStatus, vw - 40));
  }

  d.setRotation(0);
}
void renderTetrisUi() {
  auto& d = gCanvas;
  if (gTetrisRightHandPortrait) {
    renderTetrisRightHandPortraitUi();
    return;
  }
  const uint16_t bg = rgb565_local(2, 6, 12);
  const uint16_t panel = rgb565_local(7, 14, 23);
  const uint16_t panel2 = rgb565_local(4, 10, 17);
  const uint16_t grid = rgb565_local(18, 30, 40);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(128, 145, 155);
  const uint16_t red = rgb565_local(255, 82, 82);
  const int16_t cell = 6;
  const int16_t boardX = 17;
  const int16_t boardY = 24;
  const int16_t boardW = kTetrisCols * cell;
  const int16_t boardH = kTetrisRows * cell;

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 12, panel);
  d.drawRoundRect(6, 5, 228, 124, 12, gTetrisGameOver ? red : cyan);
  d.drawRoundRect(10, 9, 220, 116, 9, rgb565_local(18, 38, 54));

  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(16, 13);
  d.print("TETRIS");
  String state = gTetrisGameOver ? "OVER" : (gTetrisPaused ? "PAUSE" : (gTetrisStarted ? "RUN" : "READY"));
  d.setTextColor(gTetrisGameOver ? red : (gTetrisStarted ? green : amber), panel);
  d.setCursor(224 - d.textWidth(state), 13);
  d.print(state);

  d.fillRoundRect(boardX - 3, boardY - 3, boardW + 6, boardH + 6, 6, panel2);
  d.drawRoundRect(boardX - 3, boardY - 3, boardW + 6, boardH + 6, 6, rgb565_local(34, 55, 69));
  for (uint8_t y = 0; y < kTetrisRows; ++y) {
    for (uint8_t x = 0; x < kTetrisCols; ++x) {
      uint8_t cellValue = gTetrisBoard[y][x];
      uint8_t activeValue = 0;
      if (tetrisActiveCellAt(x, y, activeValue)) {
        cellValue = activeValue;
      }
      uint16_t fill = cellValue ? tetrisPieceColor(cellValue) : rgb565_local(8, 17, 24);
      int16_t px = boardX + x * cell;
      int16_t py = boardY + y * cell;
      d.fillRect(px, py, cell - 1, cell - 1, fill);
      if (!cellValue && ((x + y) % 2 == 0)) {
        d.drawPixel(px + 2, py + 2, grid);
      }
    }
  }

  const int16_t sideX = 88;
  d.fillRoundRect(sideX, 25, 134, 87, 8, rgb565_local(5, 12, 19));
  d.drawRoundRect(sideX, 25, 134, 87, 8, rgb565_local(30, 52, 66));

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, rgb565_local(5, 12, 19));
  d.setCursor(sideX + 9, 33);
  d.print("Score");
  d.setTextColor(green, rgb565_local(5, 12, 19));
  d.setCursor(sideX + 60, 33);
  d.print(gTetrisScore);
  d.setTextColor(muted, rgb565_local(5, 12, 19));
  d.setCursor(sideX + 9, 46);
  d.print("Lines");
  d.setTextColor(amber, rgb565_local(5, 12, 19));
  d.setCursor(sideX + 60, 46);
  d.print(gTetrisLines);
  d.setTextColor(muted, rgb565_local(5, 12, 19));
  d.setCursor(sideX + 9, 59);
  d.print("Level");
  d.setTextColor(cyan, rgb565_local(5, 12, 19));
  d.setCursor(sideX + 60, 59);
  d.print(gTetrisLevel);

  d.setTextColor(muted, rgb565_local(5, 12, 19));
  d.setCursor(sideX + 9, 75);
  d.print("Next");
  d.fillRoundRect(sideX + 54, 71, 39, 31, 5, panel2);
  renderTetrisPreview(gTetrisNextPiece, sideX + 60, 76, 7, panel2);

  d.setTextColor(gTetrisGameOver ? red : amber, rgb565_local(5, 12, 19));
  d.setCursor(sideX + 9, 102);
  d.print(fitCurrentFontToWidth(gTetrisStatus, 113));

  if (!gTetrisStarted || gTetrisGameOver) {
    d.fillRoundRect(27, 54, 186, 32, 9, rgb565_local(3, 8, 14));
    d.drawRoundRect(27, 54, 186, 32, 9, gTetrisGameOver ? red : amber);
    d.setFont(&fonts::Font2);
    d.setTextColor(gTetrisGameOver ? red : amber, rgb565_local(3, 8, 14));
    String prompt = gTetrisGameOver ? "GAME OVER" : "PRESS ENTER";
    d.setCursor((240 - d.textWidth(prompt)) / 2, 60);
    d.print(prompt);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, rgb565_local(3, 8, 14));
    d.setCursor(65, 75);
    d.print("Enter/R starts a new game");
  }

  d.setFont(&fonts::Font0);
  drawTinyFooter(panel, muted, "A/D move  W rot  S drop  Enter hard  G grip", 13, 120, 214);
  d.setFont(&fonts::Font2);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderIrRemoteButton(int16_t x, int16_t y, const char* keyLabel, const char* actionLabel, uint16_t fill,
                          uint16_t line, uint16_t keyColor, uint16_t textColor) {
  auto& d = gCanvas;
  d.fillRoundRect(x, y, 48, 17, 5, fill);
  d.drawRoundRect(x, y, 48, 17, 5, line);
  d.setFont(&fonts::Font0);
  d.setTextColor(keyColor, fill);
  d.setCursor(x + 4, y + 4);
  d.print(keyLabel);
  d.setTextColor(textColor, fill);
  d.setCursor(x + 16, y + 4);
  d.print(fitCurrentFontToWidth(actionLabel, 28));
}

void renderIrRemoteSlotTabs(uint16_t panel, uint16_t panel2, uint16_t cyan, uint16_t amber, uint16_t muted) {
  auto& d = gCanvas;
  const char* labels[] = {"1 Daikin", "2 ERF3"};
  for (uint8_t i = 0; i < kIrRemotePresetCount; ++i) {
    int16_t x = 18 + i * 77;
    bool selected = gSelectedIrRemoteSlot == i;
    uint16_t fill = selected ? rgb565_local(29, 43, 64) : panel2;
    uint16_t line = selected ? amber : rgb565_local(48, 55, 70);
    d.fillRoundRect(x, 31, 70, 12, 4, fill);
    d.drawRoundRect(x, 31, 70, 12, 4, line);
    d.setFont(&fonts::Font0);
    d.setTextColor(selected ? cyan : muted, fill);
    d.setCursor(x + 5, 34);
    d.print(labels[i]);
  }
  d.setTextColor(muted, panel);
  d.setCursor(174, 34);
  d.print(",/. swap");
}

void renderDaikinRemoteUi(uint16_t panel, uint16_t panel2, uint16_t cyan, uint16_t green, uint16_t amber,
                          uint16_t muted, uint16_t red) {
  auto& d = gCanvas;
  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(16, 13);
  d.print("IR REMOTE");
  String io = String("IRG") + String(kIrSendPin) + " TX";
  d.setTextColor(green, panel);
  d.setCursor(224 - d.textWidth(io), 16);
  d.print(io);

  renderIrRemoteSlotTabs(panel, panel2, cyan, amber, muted);

  String state = daikinStateSummary();
  d.fillRoundRect(17, 46, 206, 12, 5, rgb565_local(6, 10, 18));
  d.setFont(&fonts::Font0);
  d.setTextColor(gDaikinPower ? green : red, rgb565_local(6, 10, 18));
  d.setCursor(23, 49);
  d.print(fitCurrentFontToWidth(state, 194));

  const char* keys[] = {"Q", "W", "E", "R", "T", "Y", "A", "S", "D", "F", "G", "H"};
  const char* labels[] = {"Power", "Cool", "Fan", "Dry", "Heat", "Auto", "Temp-", "Temp+", "FanSp", "Swing", "Quiet", "Turbo"};
  for (uint8_t i = 0; i < 12; ++i) {
    const int col = i % 4;
    const int row = i / 4;
    uint16_t line = muted;
    if ((i == 0 && gDaikinPower) || (i == 1 && gDaikinMode == kDaikinAcCool) ||
        (i == 2 && gDaikinMode == kDaikinAcFanOnly) || (i == 3 && gDaikinMode == kDaikinAcDry) ||
        (i == 4 && gDaikinMode == kDaikinAcHeat) || (i == 5 && gDaikinMode == kDaikinAcAuto) ||
        (i == 9 && gDaikinSwingV) || (i == 10 && gDaikinQuiet) || (i == 11 && gDaikinPowerful)) {
      line = amber;
    }
    renderIrRemoteButton(18 + col * 53, 62 + row * 18, keys[i], labels[i], panel2, line, cyan, TFT_LIGHTGREY);
  }

  d.fillRoundRect(17, 116, 206, 9, 4, rgb565_local(6, 10, 18));
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_LIGHTGREY, rgb565_local(6, 10, 18));
  d.setCursor(22, 118);
  d.print(fitCurrentFontToWidth(String(gDaikinLastCommand) + " | 1/2 slots", 196));
}

void renderHisenseRemoteUi(uint16_t panel, uint16_t panel2, uint16_t cyan, uint16_t green, uint16_t amber,
                           uint16_t muted) {
  auto& d = gCanvas;
  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(16, 13);
  d.print("ERF3W80H 4K");
  String io = String("IRG") + String(kIrSendPin) + " TX";
  d.setTextColor(green, panel);
  d.setCursor(224 - d.textWidth(io), 16);
  d.print(io);

  renderIrRemoteSlotTabs(panel, panel2, cyan, amber, muted);

  d.fillRoundRect(17, 46, 206, 12, 5, rgb565_local(6, 10, 18));
  d.setFont(&fonts::Font0);
  d.setTextColor(green, rgb565_local(6, 10, 18));
  d.setCursor(23, 49);
  d.print(fitCurrentFontToWidth(String("Last ") + gHisenseLastCommandLabel + " | Enter OK", 194));

  const char* keys[] = {"Q", "W", "E", "R", "T", "Y", "A", "S", "D", "F", "Z", "X"};
  const char* labels[] = {"Power", "Up", "Input", "Back", "Home", "Menu", "Left", "Down", "Right", "Mute", "Vol-", "Vol+"};
  for (uint8_t i = 0; i < 12; ++i) {
    const int col = i % 4;
    const int row = i / 4;
    uint16_t line = (i == 0 || i == 1 || i == 6 || i == 7 || i == 8 || i == 10 || i == 11) ? amber : muted;
    renderIrRemoteButton(18 + col * 53, 62 + row * 18, keys[i], labels[i], panel2, line, cyan, TFT_LIGHTGREY);
  }

  d.fillRoundRect(17, 116, 206, 9, 4, rgb565_local(6, 10, 18));
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_LIGHTGREY, rgb565_local(6, 10, 18));
  d.setCursor(22, 118);
  d.print(fitCurrentFontToWidth("G/H Ch  M Assist  V/B/N Apps", 196));
}

void renderIrRemoteUi() {
  initIrRemote();
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(4, 5, 12);
  const uint16_t panel = rgb565_local(12, 13, 24);
  const uint16_t panel2 = rgb565_local(6, 8, 17);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(132, 148, 162);
  const uint16_t red = rgb565_local(255, 82, 82);

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 12, panel);
  d.drawRoundRect(6, 5, 228, 124, 12, isHisenseRemoteSlot() ? green : cyan);
  d.drawRoundRect(10, 9, 220, 116, 9, rgb565_local(28, 34, 54));

  if (isHisenseRemoteSlot()) {
    renderHisenseRemoteUi(panel, panel2, cyan, green, amber, muted);
  } else {
    renderDaikinRemoteUi(panel, panel2, cyan, green, amber, muted, red);
  }

  d.setFont(&fonts::Font2);
  renderLauncherOverlay();
  renderDebugOverlay();
}

bool calendarBaseLocalTime(tm& today, tm& monthStart) {
  if (!hasValidSystemTime()) {
    return false;
  }
  time_t now = time(nullptr);
  localtime_r(&now, &today);
  monthStart = today;
  monthStart.tm_sec = 0;
  monthStart.tm_min = 0;
  monthStart.tm_hour = 12;
  monthStart.tm_mday = 1;
  monthStart.tm_mon += gCalendarMonthOffset;
  time_t monthEpoch = mktime(&monthStart);
  localtime_r(&monthEpoch, &monthStart);
  return true;
}

bool calendarLeapYear(int year) {
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

uint8_t calendarDaysInMonth(int year, int monthZero) {
  static constexpr uint8_t kDaysByMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (monthZero == 1 && calendarLeapYear(year)) {
    return 29;
  }
  return kDaysByMonth[clampValue<int>(monthZero, 0, 11)];
}

const char* calendarMonthName(uint8_t monthZero) {
  static constexpr const char* kNames[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                             "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  return kNames[clampValue<int>(monthZero, 0, 11)];
}

void renderCalendarUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(6, 15, 22);
  const uint16_t cell = rgb565_local(9, 22, 31);
  const uint16_t todayBg = rgb565_local(8, 58, 46);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(126, 143, 157);

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 12, panel);
  d.drawRoundRect(6, 5, 228, 124, 12, cyan);
  d.drawRoundRect(10, 9, 220, 116, 9, rgb565_local(18, 48, 62));

  tm today = {};
  tm monthStart = {};
  if (!calendarBaseLocalTime(today, monthStart)) {
    d.setFont(&fonts::Font2);
    d.setTextColor(amber, panel);
    d.setCursor(18, 18);
    d.print("CALENDAR");
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(22, 55);
    d.print("Time not synced yet");
    drawTinyFooter(panel, muted, "Wait for Wi-Fi/NTP  Esc home", 16, 116, 208);
    d.setFont(&fonts::Font2);
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }

  const int year = monthStart.tm_year + 1900;
  const uint8_t month = static_cast<uint8_t>(monthStart.tm_mon);
  const uint8_t days = calendarDaysInMonth(year, month);
  const uint8_t firstDowMonday = static_cast<uint8_t>((monthStart.tm_wday + 6) % 7);
  const bool thisMonth = today.tm_year == monthStart.tm_year && today.tm_mon == monthStart.tm_mon;

  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(16, 14);
  d.print("CALENDAR");
  String title = String(calendarMonthName(month)) + " " + String(year);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(224 - d.textWidth(title), 14);
  d.print(title);

  d.setFont(&fonts::Font0);
  const char* labels[7] = {"M", "T", "W", "T", "F", "S", "S"};
  for (int i = 0; i < 7; ++i) {
    int16_t x = 17 + i * 30;
    d.setTextColor(i >= 5 ? amber : muted, panel);
    d.setCursor(x + 8, 32);
    d.print(labels[i]);
  }

  uint8_t day = 1;
  for (int rowIdx = 0; rowIdx < 6; ++rowIdx) {
    for (int col = 0; col < 7; ++col) {
      const int cellIndex = rowIdx * 7 + col;
      if (cellIndex < firstDowMonday || day > days) {
        continue;
      }
      const int16_t x = 15 + col * 30;
      const int16_t y = 42 + rowIdx * 11;
      const bool isToday = thisMonth && day == today.tm_mday;
      const bool weekend = col >= 5;
      const uint16_t fill = isToday ? todayBg : cell;
      d.fillRoundRect(x, y, 24, 9, 3, fill);
      if (isToday) {
        d.drawRoundRect(x, y, 24, 9, 3, green);
      }
      char buf[3];
      snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(day));
      d.setTextColor(isToday ? TFT_WHITE : (weekend ? amber : TFT_LIGHTGREY), fill);
      d.setCursor(x + (day < 10 ? 9 : 6), y + 1);
      d.print(buf);
      ++day;
    }
  }

  String todayLine = String("Today ") + calendarMonthName(static_cast<uint8_t>(today.tm_mon)) + " " +
                     String(today.tm_mday);
  d.setTextColor(green, panel);
  d.setCursor(16, 109);
  d.print(fitCurrentFontToWidth(todayLine, 86));
  drawTinyFooter(panel, muted, "< > month   T today   Esc home", 96, 118, 126);
  d.setFont(&fonts::Font2);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderStopwatchUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(3, 7, 11);
  const uint16_t panel = rgb565_local(8, 16, 24);
  const uint16_t panel2 = rgb565_local(3, 12, 16);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(126, 143, 157);
  uint32_t elapsed = stopwatchElapsedMs();
  uint16_t accent = gStopwatchRunning ? green : amber;

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 12, panel);
  d.drawRoundRect(6, 5, 228, 124, 12, accent);
  d.drawRoundRect(10, 9, 220, 116, 9, rgb565_local(22, 44, 57));

  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(16, 14);
  d.print("STOPWATCH");
  String state = gStopwatchRunning ? "RUN" : (elapsed == 0 ? "READY" : "PAUSE");
  d.setTextColor(accent, panel);
  d.setCursor(224 - d.textWidth(state), 17);
  d.print(state);

  int16_t cx = 120;
  int16_t cy = 62;
  d.fillCircle(cx, cy, 36, panel2);
  d.drawCircle(cx, cy, 37, rgb565_local(24, 46, 58));
  for (int i = 0; i < 60; i += 5) {
    float a = (i - 15) * 0.10472f;
    int16_t x1 = cx + static_cast<int16_t>(cosf(a) * 30.0f);
    int16_t y1 = cy + static_cast<int16_t>(sinf(a) * 30.0f);
    int16_t x2 = cx + static_cast<int16_t>(cosf(a) * 34.0f);
    int16_t y2 = cy + static_cast<int16_t>(sinf(a) * 34.0f);
    d.drawLine(x1, y1, x2, y2, i == 0 ? accent : muted);
  }
  float hand = ((elapsed / 1000UL) % 60UL - 15) * 0.10472f;
  d.drawLine(cx, cy, cx + static_cast<int16_t>(cosf(hand) * 27.0f),
             cy + static_cast<int16_t>(sinf(hand) * 27.0f), accent);
  d.fillCircle(cx, cy, 3, accent);

  d.fillRoundRect(19, 82, 202, 26, 8, panel2);
  d.setFont(&fonts::Font4);
  d.setTextColor(accent, panel2);
  String timeText = formatStopwatchElapsed(elapsed);
  d.setCursor((240 - d.textWidth(timeText)) / 2, 86);
  d.print(timeText);

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(18, 111);
  String lap = gStopwatchLapMs > 0 ? String("Lap ") + formatStopwatchElapsed(gStopwatchLapMs)
                                  : String("No lap yet");
  d.print(fitCurrentFontToWidth(lap, 106));
  d.setTextColor(green, panel);
  d.setCursor(132, 111);
  d.print("Ent start/pause");
  d.setTextColor(amber, panel);
  d.setCursor(18, 122);
  d.print("R reset  L lap  Esc home");
  d.setFont(&fonts::Font2);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderAlarmUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(5, 5, 10);
  const uint16_t panel = rgb565_local(13, 15, 24);
  const uint16_t panel2 = rgb565_local(6, 10, 18);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t red = rgb565_local(255, 82, 82);
  const uint16_t muted = rgb565_local(126, 143, 157);
  uint32_t nowMs = millis();
  bool flash = gAlarmRinging && ((nowMs / 320U) % 2U == 0U);
  uint16_t accent = gAlarmRinging ? (flash ? red : amber) : (gAlarmSettings.enabled ? green : muted);

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 12, panel);
  d.drawRoundRect(6, 5, 228, 124, 12, accent);
  d.drawRoundRect(10, 9, 220, 116, 9, rgb565_local(28, 37, 52));

  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(16, 14);
  d.print(gAlarmRinging ? "ALARM RINGING" : "ALARM");
  String state = gAlarmRinging ? "RING" : (gAlarmSettings.enabled ? "ON" : "OFF");
  d.setCursor(224 - d.textWidth(state), 14);
  d.print(state);

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(18, 29);
  d.print("now ");
  d.setTextColor(cyan, panel);
  d.print(alarmClockText());
  String snooze = alarmSnoozeUntilText();
  if (!snooze.isEmpty()) {
    d.setTextColor(amber, panel);
    d.setCursor(146, 29);
    d.print("snooze ");
    d.print(snooze);
  }

  d.fillRoundRect(19, 43, 202, 45, 10, panel2);
  d.drawRoundRect(19, 43, 202, 45, 10, accent);
  d.setFont(&fonts::Font6);
  d.setTextColor(accent, panel2);
  String alarmText = alarmTimeString(gAlarmSettings.hour, gAlarmSettings.minute);
  d.setCursor((240 - d.textWidth(alarmText)) / 2, 43);
  d.print(alarmText);

  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel2);
  d.setCursor(33, 78);
  if (!gAlarmEditBuffer.isEmpty()) {
    d.print("entry HHMM: ");
    d.setTextColor(amber, panel2);
    d.print(gAlarmEditBuffer);
  } else if (gAlarmRinging) {
    d.setTextColor(red, panel2);
    d.print("Enter dismiss  S snooze  Esc dismiss");
  } else {
    d.print(gAlarmSettings.enabled ? "daily alarm armed" : "toggle on to arm daily alarm");
  }

  d.fillRoundRect(18, 94, 204, 15, 5, rgb565_local(7, 13, 20));
  d.setTextColor(gAlarmRinging ? red : amber, rgb565_local(7, 13, 20));
  d.setCursor(25, 99);
  d.print(fitCurrentFontToWidth(gAlarmStatus, 190));

  d.setFont(&fonts::Font0);
  if (gAlarmRinging) {
    drawTinyFooter(panel, muted, "Enter/Space dismiss   S snooze 5m", 16, 120, 208);
  } else {
    drawTinyFooter(panel, muted, "0-9 HHMM  +/- min  Z/X 5m  T toggle", 10, 120, 220);
  }
  d.setFont(&fonts::Font2);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderContextsUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(3, 6, 12);
  const uint16_t panel = rgb565_local(8, 14, 24);
  const uint16_t panel2 = rgb565_local(12, 22, 34);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t violet = rgb565_local(170, 130, 255);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t green = rgb565_local(100, 255, 170);
  const uint16_t muted = rgb565_local(135, 151, 166);

  d.fillScreen(bg);
  uint32_t now = millis();
  int meshPhase = static_cast<int>((now / 90) % 24);
  for (int x = -24; x < 260; x += 24) {
    d.drawFastVLine(x + meshPhase, 0, 135, rgb565_local(7, 18, 28));
  }
  for (int y = 10; y < 135; y += 18) {
    int offset = static_cast<int>(sin((now + y * 31) * 0.0015f) * 5.0f);
    d.drawFastHLine(0, y + offset, 240, rgb565_local(8, 22, 34));
  }
  d.drawRoundRect(4, 4, 232, 127, 14, rgb565_local(24, 46, 68));
  d.drawRoundRect(5, 5, 230, 125, 13, rgb565_local(8, 18, 30));

  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, bg);
  d.setCursor(12, 10);
  d.print("OPENCLAW TOPIC");
  String source = gContextsRemoteLoaded ? "LIVE" : "LOCAL";
  d.setTextColor(gContextsRemoteLoaded ? green : amber, bg);
  d.setCursor(232 - d.textWidth(source), 10);
  d.print(source);

  if (gContexts.empty()) {
    d.fillRoundRect(20, 42, 200, 54, 10, panel);
    d.drawRoundRect(20, 42, 200, 54, 10, amber);
    d.setTextColor(amber, panel);
    d.setCursor(42, 60);
    d.print("No contexts");
    drawTinyFooter(bg, muted, "R sync topics  Esc home", 30, 116, 180);
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }

  gSelectedContext = clampValue<int>(gSelectedContext, 0, static_cast<int>(gContexts.size()) - 1);
  const ConversationContext& ctx = gContexts[gSelectedContext];
  d.setTextColor(muted, bg);
  d.setCursor(12, 24);
  d.print("Alt+<> switch topic");
  if (ctx.unread > 0) {
    String unread = String(ctx.unread) + " new";
    d.setTextColor(amber, bg);
    d.setCursor(232 - d.textWidth(unread), 24);
    d.print(unread);
  }

  float animT = min<float>(1.0f, (now - gContextAnimStartMs) / 260.0f);
  float ease = 1.0f - (1.0f - animT) * (1.0f - animT);
  int cardDx = (gContextAnimDir == 0 || animT >= 1.0f) ? 0 : static_cast<int>(gContextAnimDir * (1.0f - ease) * 34.0f);
  int cardX = 12 + cardDx;
  d.fillRoundRect(cardX, 35, 216, 47, 13, panel);
  d.drawRoundRect(cardX, 35, 216, 47, 13, ctx.key == gSavedContextKey ? green : violet);
  d.fillRoundRect(cardX + 8, 42, 52, 33, 10, panel2);
  d.drawRoundRect(cardX + 8, 42, 52, 33, 10, cyan);
  d.setTextColor(TFT_WHITE, panel2);
  d.setTextDatum(middle_center);
  d.setFont(&fonts::Font4);
  d.drawString(topicDisplayIcon(ctx), cardX + 34, 59);
  d.setTextDatum(top_left);
  d.setFont(&fonts::efontCN_12);

  d.setTextColor(TFT_WHITE, panel);
  String title = marqueeText(topicDisplayTitle(ctx), 146, 260);
  d.setCursor(cardX + 70, 43);
  d.print(title);
  d.setTextColor(muted, panel);
  d.setCursor(cardX + 70, 58);
  d.print(gTopicTaskHandle != nullptr ? "syncing..." : "chat context");
  d.setFont(&fonts::Font2);
  d.setTextColor(ctx.key == gSavedContextKey ? green : muted, panel);
  d.setCursor(cardX + 70, 70);
  d.print(ctx.key == gSavedContextKey ? "selected" : "Enter select");

  d.setTextColor(cyan, bg);
  d.setCursor(8, 58);
  d.print("<");
  d.setCursor(226, 58);
  d.print(">");

  d.fillRoundRect(12, 86, 216, 37, 8, rgb565_local(4, 10, 16));
  d.drawRoundRect(12, 86, 216, 37, 8, rgb565_local(20, 42, 58));
  d.setFont(&fonts::Font0);
  if (gContextHistoryKey == currentConversationKey() && !gContextPreview.empty()) {
    int y = 90;
    int shown = 0;
    for (const auto& line : gContextPreview) {
      if (shown >= 4) {
        break;
      }
      uint16_t roleColor = line.role == "assistant" ? green : (line.voice ? amber : cyan);
      d.setTextColor(roleColor, rgb565_local(4, 10, 16));
      d.setCursor(18, y);
      String prefix = line.role == "assistant" ? "AI" : (line.voice ? "VO" : "ME");
      d.print(prefix);
      d.setTextColor(TFT_LIGHTGREY, rgb565_local(4, 10, 16));
      d.setCursor(34, y);
      d.print(fitCurrentFontToWidth(line.text, 184));
      y += 8;
      shown++;
    }
    if (gContextPreview.size() > 4) {
      d.setTextColor(muted, rgb565_local(4, 10, 16));
      d.setFont(&fonts::Font0);
      d.setCursor(205, 115);
      d.print("+");
      d.print(static_cast<int>(gContextPreview.size()) - 4);
      d.setFont(&fonts::Font2);
    }
  } else {
    d.setTextColor(muted, rgb565_local(4, 10, 16));
    d.setCursor(18, 99);
    if (!gContextError.isEmpty()) {
      d.setTextColor(amber, rgb565_local(4, 10, 16));
      d.print(trimPreview(gContextError, 38));
    } else {
      d.print("Enter loads last 5 messages");
    }
  }

  drawTinyFooter(bg, muted, "Alt+<> topic  Ent select  R sync", 12, 124, 216);
  renderLauncherOverlay();
  renderDebugOverlay();
}

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
