bool handleSettingsKey(char key) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (key == ',' || key == ';') {
    gSelectedSetting = (gSelectedSetting + kSettingsItemCount - 1) % kSettingsItemCount;
    return true;
  }
  if (key == '.' || key == '/') {
    gSelectedSetting = (gSelectedSetting + 1) % kSettingsItemCount;
    return true;
  }
  if (lowerKey == 'r') {
    loadDeviceSettings();
    loadFocusSettings();
    setStatus("Settings reloaded", 900);
    return true;
  }
  if (lowerKey == 's') {
    saveDeviceSettings();
    saveFocusSettings();
    setStatus("Settings saved", 900);
    return true;
  }
  if (key == '-' || lowerKey == 'z') {
    switch (gSelectedSetting) {
      case 0:
        gFocusSettings.focusSec = max<uint32_t>(kFocusMinDurationSec, gFocusSettings.focusSec - 5UL * 60UL);
        break;
      case 1:
        gFocusSettings.shortBreakSec = max<uint32_t>(kFocusMinDurationSec, gFocusSettings.shortBreakSec - 60UL);
        break;
      case 2:
        gFocusSettings.longBreakSec = max<uint32_t>(kFocusMinDurationSec, gFocusSettings.longBreakSec - 5UL * 60UL);
        break;
      case 3:
        gFocusSettings.cyclesPerRound = max<uint8_t>(1, gFocusSettings.cyclesPerRound - 1);
        break;
      case 6:
        gFocusSettings.bpm = max<uint16_t>(kFocusMinBpm, gFocusSettings.bpm - 5);
        break;
      case 7:
        gDeviceSettings.minBatteryForUpdate = max<uint8_t>(5, gDeviceSettings.minBatteryForUpdate - 5);
        break;
      case 9:
        gDeviceSettings.clockFace = (gDeviceSettings.clockFace + kClockFaceCount - 1) % kClockFaceCount;
        break;
      default:
        break;
    }
    saveDeviceSettings();
    saveFocusSettings();
    setStatus("Setting -", 500);
    return true;
  }
  if (key == '+' || key == '=' || lowerKey == 'x') {
    switch (gSelectedSetting) {
      case 0:
        gFocusSettings.focusSec = min<uint32_t>(kFocusMaxDurationSec, gFocusSettings.focusSec + 5UL * 60UL);
        break;
      case 1:
        gFocusSettings.shortBreakSec = min<uint32_t>(kFocusMaxDurationSec, gFocusSettings.shortBreakSec + 60UL);
        break;
      case 2:
        gFocusSettings.longBreakSec = min<uint32_t>(kFocusMaxDurationSec, gFocusSettings.longBreakSec + 5UL * 60UL);
        break;
      case 3:
        gFocusSettings.cyclesPerRound = min<uint8_t>(9, gFocusSettings.cyclesPerRound + 1);
        break;
      case 6:
        gFocusSettings.bpm = min<uint16_t>(kFocusMaxBpm, gFocusSettings.bpm + 5);
        break;
      case 7:
        gDeviceSettings.minBatteryForUpdate = min<uint8_t>(95, gDeviceSettings.minBatteryForUpdate + 5);
        break;
      case 9:
        gDeviceSettings.clockFace = (gDeviceSettings.clockFace + 1) % kClockFaceCount;
        break;
      default:
        break;
    }
    saveDeviceSettings();
    saveFocusSettings();
    setStatus("Setting +", 500);
    return true;
  }
  if (key == ' ' || key == '\n' || key == '\r' || lowerKey == 't') {
    switch (gSelectedSetting) {
      case 4:
        gFocusSettings.autoStart = !gFocusSettings.autoStart;
        break;
      case 5:
        gDeviceSettings.autoUpdateFirmware = !gDeviceSettings.autoUpdateFirmware;
        break;
      case 8:
        if (gDeviceSettings.audioLanguage == "ru") {
          gDeviceSettings.audioLanguage = "en";
        } else if (gDeviceSettings.audioLanguage == "en") {
          gDeviceSettings.audioLanguage = "es";
        } else {
          gDeviceSettings.audioLanguage = "ru";
        }
        break;
      case 9:
        gDeviceSettings.clockFace = (gDeviceSettings.clockFace + 1) % kClockFaceCount;
        break;
      default:
        break;
    }
    saveDeviceSettings();
    saveFocusSettings();
    setStatus("Setting toggled", 700);
    return true;
  }
  return false;
}

const char* clockFaceName(uint8_t face) {
  (void)face;
  return "Pulse";
}

void cycleClockFace(int delta = 1) {
  (void)delta;
  gDeviceSettings.clockFace = 0;
  saveDeviceSettings();
  setStatus("Home face: Pulse", 900);
  setKeyDiag("homeface:Pulse");
}

void cancelRecordingDiscard() {
  if (!gRecording) {
    return;
  }
  gRecording = false;
  stopMic();
  if (gRecordingToFile) {
    gRecordInflightChunks.clear();
    if (gActiveRecordFile) {
      gActiveRecordFile.close();
    }
    fs::FS* fs = activeVoiceFs();
    if (fs && !gActiveRecordPath.isEmpty()) {
      fsRemove(*fs, gActiveRecordPath);
    }
    gRecordingToFile = false;
    gActiveRecordPath = "";
    gActiveRecordBytes = 0;
    gActiveRecordCommittedBytes = 0;
    gActiveRecordExpectedBytes = 0;
  } else {
    releaseRecordBuffer();
  }
  gRecordedSamples = 0;
}

void calcSkipSpaces(const char*& p) {
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
}

bool calcParseExpression(const char*& p, double& out);

bool calcParseFactor(const char*& p, double& out) {
  calcSkipSpaces(p);
  if (*p == '+') {
    ++p;
    return calcParseFactor(p, out);
  }
  if (*p == '-') {
    ++p;
    if (!calcParseFactor(p, out)) {
      return false;
    }
    out = -out;
    return true;
  }
  if (*p == '(') {
    ++p;
    if (!calcParseExpression(p, out)) {
      return false;
    }
    calcSkipSpaces(p);
    if (*p != ')') {
      return false;
    }
    ++p;
    return true;
  }
  char* end = nullptr;
  out = strtod(p, &end);
  if (end == p) {
    return false;
  }
  p = end;
  return true;
}

bool calcParseTerm(const char*& p, double& out) {
  if (!calcParseFactor(p, out)) {
    return false;
  }
  while (true) {
    calcSkipSpaces(p);
    char op = *p;
    if (op != '*' && op != '/') {
      return true;
    }
    ++p;
    double rhs = 0.0;
    if (!calcParseFactor(p, rhs)) {
      return false;
    }
    if (op == '*') {
      out *= rhs;
    } else {
      if (fabs(rhs) < 0.0000001) {
        return false;
      }
      out /= rhs;
    }
  }
}

bool calcParseExpression(const char*& p, double& out) {
  if (!calcParseTerm(p, out)) {
    return false;
  }
  while (true) {
    calcSkipSpaces(p);
    char op = *p;
    if (op != '+' && op != '-') {
      return true;
    }
    ++p;
    double rhs = 0.0;
    if (!calcParseTerm(p, rhs)) {
      return false;
    }
    out = op == '+' ? out + rhs : out - rhs;
  }
}

bool evaluateCalculatorExpression(const String& expression, String& result) {
  String trimmed = expression;
  trimmed.trim();
  if (trimmed.isEmpty()) {
    return false;
  }
  const char* p = trimmed.c_str();
  double value = 0.0;
  if (!calcParseExpression(p, value)) {
    return false;
  }
  calcSkipSpaces(p);
  if (*p != '\0' || !isfinite(value)) {
    return false;
  }
  result = String(value, 6);
  while (result.indexOf('.') >= 0 && result.endsWith("0")) {
    result.remove(result.length() - 1);
  }
  if (result.endsWith(".")) {
    result.remove(result.length() - 1);
  }
  if (result == "-0") {
    result = "0";
  }
  return true;
}

bool handleCalculatorKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (keys.ctrl && lowerKey == 'c') {
    gCalcExpression = "";
    gCalcResult = "0";
    gCalcStatus = "Cleared";
    setStatus("Calculator cleared", 700);
    return true;
  }
  if (keys.del || key == '\b') {
    if (!gCalcExpression.isEmpty()) {
      gCalcExpression.remove(gCalcExpression.length() - 1);
      gCalcStatus = "Editing";
    }
    return true;
  }
  if (keys.enter || key == '\r' || key == '\n' || key == '=') {
    String result;
    if (evaluateCalculatorExpression(gCalcExpression, result)) {
      gCalcResult = result;
      gCalcExpression = result;
      gCalcStatus = "OK";
      setStatus("Calculated", 700);
    } else {
      gCalcStatus = "Error";
      setStatus("Calculator error", 900);
    }
    return true;
  }
  if (key == 'c' || key == 'C') {
    gCalcExpression = "";
    gCalcResult = "0";
    gCalcStatus = "Cleared";
    return true;
  }
  if (key == 'x' || key == 'X') {
    key = '*';
  }
  if ((key >= '0' && key <= '9') || key == '+' || key == '-' || key == '*' || key == '/' ||
      key == '.' || key == '(' || key == ')' || key == ' ') {
    if (gCalcExpression.length() < 48) {
      gCalcExpression += key;
      gCalcStatus = "Editing";
    }
    return true;
  }
  return true;
}

constexpr uint8_t kTetrisCols = 10;
constexpr uint8_t kTetrisRows = 16;
constexpr uint8_t kTetrisPieceCount = 7;
constexpr uint16_t kTetrisShapes[kTetrisPieceCount][4] = {
    {0x0F00, 0x2222, 0x0F00, 0x2222},  // I
    {0x0660, 0x0660, 0x0660, 0x0660},  // O
    {0x4E00, 0x4640, 0x0E40, 0x4C40},  // T
    {0x6C00, 0x4620, 0x6C00, 0x4620},  // S
    {0xC600, 0x2640, 0xC600, 0x2640},  // Z
    {0x8E00, 0x6440, 0xE200, 0x44C0},  // J
    {0x2E00, 0x4460, 0xE800, 0xC440},  // L
};

const char* tetrisPieceName(uint8_t piece) {
  static constexpr const char* kNames[kTetrisPieceCount] = {"I", "O", "T", "S", "Z", "J", "L"};
  return kNames[piece % kTetrisPieceCount];
}

uint16_t tetrisPieceColor(uint8_t cell) {
  switch (cell) {
    case 1:
      return rgb565_local(47, 227, 255);
    case 2:
      return rgb565_local(255, 214, 88);
    case 3:
      return rgb565_local(177, 113, 255);
    case 4:
      return rgb565_local(88, 240, 141);
    case 5:
      return rgb565_local(255, 82, 82);
    case 6:
      return rgb565_local(88, 151, 255);
    case 7:
      return rgb565_local(255, 150, 80);
    default:
      return rgb565_local(15, 25, 34);
  }
}

bool tetrisShapeCell(uint8_t piece, uint8_t rotation, uint8_t x, uint8_t y) {
  if (x >= 4 || y >= 4) {
    return false;
  }
  uint16_t mask = kTetrisShapes[piece % kTetrisPieceCount][rotation % 4];
  uint8_t bit = static_cast<uint8_t>(15 - (y * 4 + x));
  return (mask & (1U << bit)) != 0;
}

bool tetrisCanPlace(uint8_t piece, uint8_t rotation, int8_t px, int8_t py) {
  for (uint8_t y = 0; y < 4; ++y) {
    for (uint8_t x = 0; x < 4; ++x) {
      if (!tetrisShapeCell(piece, rotation, x, y)) {
        continue;
      }
      int8_t bx = px + static_cast<int8_t>(x);
      int8_t by = py + static_cast<int8_t>(y);
      if (bx < 0 || bx >= static_cast<int8_t>(kTetrisCols) || by >= static_cast<int8_t>(kTetrisRows)) {
        return false;
      }
      if (by >= 0 && gTetrisBoard[by][bx] != 0) {
        return false;
      }
    }
  }
  return true;
}

uint8_t tetrisRandomPiece() {
  return static_cast<uint8_t>(random(kTetrisPieceCount));
}

uint16_t tetrisDropIntervalMs() {
  int interval = 660 - static_cast<int>(gTetrisLevel - 1) * 45;
  return static_cast<uint16_t>(max<int>(170, interval));
}

void tetrisSpawnPiece() {
  gTetrisPiece = gTetrisNextPiece % kTetrisPieceCount;
  gTetrisNextPiece = tetrisRandomPiece();
  gTetrisRotation = 0;
  gTetrisX = 3;
  gTetrisY = 0;
  gTetrisLastDropMs = millis();
  if (!tetrisCanPlace(gTetrisPiece, gTetrisRotation, gTetrisX, gTetrisY)) {
    gTetrisGameOver = true;
    gTetrisPaused = false;
    gTetrisStatus = "Game over";
    setStatus("Tetris game over", 1000);
  } else {
    gTetrisStatus = String("Piece ") + tetrisPieceName(gTetrisPiece);
  }
}

void tetrisResetGame() {
  memset(gTetrisBoard, 0, sizeof(gTetrisBoard));
  gTetrisScore = 0;
  gTetrisLines = 0;
  gTetrisLevel = 1;
  gTetrisStarted = true;
  gTetrisPaused = false;
  gTetrisGameOver = false;
  gTetrisNextPiece = tetrisRandomPiece();
  tetrisSpawnPiece();
  gTetrisStatus = "New game";
  setStatus("Tetris started", 900);
}

bool tetrisMovePiece(int8_t dx, int8_t dy) {
  int8_t nx = static_cast<int8_t>(gTetrisX + dx);
  int8_t ny = static_cast<int8_t>(gTetrisY + dy);
  if (!tetrisCanPlace(gTetrisPiece, gTetrisRotation, nx, ny)) {
    return false;
  }
  gTetrisX = nx;
  gTetrisY = ny;
  return true;
}

bool tetrisRotatePiece() {
  uint8_t nr = static_cast<uint8_t>((gTetrisRotation + 1) % 4);
  if (tetrisCanPlace(gTetrisPiece, nr, gTetrisX, gTetrisY)) {
    gTetrisRotation = nr;
    return true;
  }
  if (tetrisCanPlace(gTetrisPiece, nr, static_cast<int8_t>(gTetrisX - 1), gTetrisY)) {
    gTetrisX--;
    gTetrisRotation = nr;
    return true;
  }
  if (tetrisCanPlace(gTetrisPiece, nr, static_cast<int8_t>(gTetrisX + 1), gTetrisY)) {
    gTetrisX++;
    gTetrisRotation = nr;
    return true;
  }
  return false;
}

void tetrisMergePiece() {
  for (uint8_t y = 0; y < 4; ++y) {
    for (uint8_t x = 0; x < 4; ++x) {
      if (!tetrisShapeCell(gTetrisPiece, gTetrisRotation, x, y)) {
        continue;
      }
      int8_t bx = gTetrisX + static_cast<int8_t>(x);
      int8_t by = gTetrisY + static_cast<int8_t>(y);
      if (bx >= 0 && bx < static_cast<int8_t>(kTetrisCols) && by >= 0 && by < static_cast<int8_t>(kTetrisRows)) {
        gTetrisBoard[by][bx] = static_cast<uint8_t>(gTetrisPiece + 1);
      }
    }
  }
}

uint8_t tetrisClearFullLines() {
  uint8_t cleared = 0;
  for (int8_t y = static_cast<int8_t>(kTetrisRows) - 1; y >= 0; --y) {
    bool full = true;
    for (uint8_t x = 0; x < kTetrisCols; ++x) {
      if (gTetrisBoard[y][x] == 0) {
        full = false;
        break;
      }
    }
    if (!full) {
      continue;
    }
    for (int8_t yy = y; yy > 0; --yy) {
      memcpy(gTetrisBoard[yy], gTetrisBoard[yy - 1], kTetrisCols);
    }
    memset(gTetrisBoard[0], 0, kTetrisCols);
    ++cleared;
    ++y;
  }
  if (cleared > 0) {
    static constexpr uint16_t kLineScore[5] = {0, 40, 100, 300, 1200};
    gTetrisLines += cleared;
    gTetrisScore += static_cast<uint32_t>(kLineScore[min<uint8_t>(cleared, 4)]) * gTetrisLevel;
    gTetrisLevel = static_cast<uint8_t>(1 + min<uint16_t>(8, gTetrisLines / 10));
    gTetrisStatus = String("Lines +") + String(cleared);
    setStatus(gTetrisStatus, 700);
  }
  return cleared;
}

void tetrisLockPiece() {
  tetrisMergePiece();
  tetrisClearFullLines();
  tetrisSpawnPiece();
}

void tetrisHardDrop() {
  if (!gTetrisStarted || gTetrisPaused || gTetrisGameOver) {
    return;
  }
  while (tetrisMovePiece(0, 1)) {
    gTetrisScore += 1;
  }
  tetrisLockPiece();
}

void serviceTetris() {
  if (gUiMode != UiMode::Tetris || !gTetrisStarted || gTetrisPaused || gTetrisGameOver) {
    return;
  }
  uint32_t now = millis();
  if (now - gTetrisLastDropMs < tetrisDropIntervalMs()) {
    return;
  }
  gTetrisLastDropMs = now;
  if (!tetrisMovePiece(0, 1)) {
    tetrisLockPiece();
  }
}

bool tetrisActiveCellAt(uint8_t boardX, uint8_t boardY, uint8_t& cell) {
  if (!gTetrisStarted || gTetrisGameOver) {
    return false;
  }
  for (uint8_t y = 0; y < 4; ++y) {
    for (uint8_t x = 0; x < 4; ++x) {
      if (!tetrisShapeCell(gTetrisPiece, gTetrisRotation, x, y)) {
        continue;
      }
      int8_t bx = gTetrisX + static_cast<int8_t>(x);
      int8_t by = gTetrisY + static_cast<int8_t>(y);
      if (bx == static_cast<int8_t>(boardX) && by == static_cast<int8_t>(boardY)) {
        cell = static_cast<uint8_t>(gTetrisPiece + 1);
        return true;
      }
    }
  }
  return false;
}

bool handleTetrisKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'g') {
    gTetrisRightHandPortrait = !gTetrisRightHandPortrait;
    gTetrisStatus = gTetrisRightHandPortrait ? "Right grip" : "Landscape";
    setStatus(gTetrisRightHandPortrait ? "Tetris right grip" : "Tetris landscape", 900);
    return true;
  }
  if (!gTetrisStarted || gTetrisGameOver) {
    if (keys.enter || keys.space || key == '\r' || key == '\n' || key == ' ' || lowerKey == 'r') {
      tetrisResetGame();
    } else {
      setStatus("Enter starts Tetris", 800);
    }
    return true;
  }
  if (lowerKey == 'p') {
    gTetrisPaused = !gTetrisPaused;
    gTetrisStatus = gTetrisPaused ? "Paused" : "Running";
    setStatus(gTetrisStatus, 700);
    return true;
  }
  if (lowerKey == 'r') {
    tetrisResetGame();
    return true;
  }
  if (gTetrisPaused) {
    return true;
  }
  if (key == ',' || key == ';' || lowerKey == 'a' || lowerKey == 'h') {
    tetrisMovePiece(-1, 0);
    return true;
  }
  if (key == '.' || key == '/' || lowerKey == 'd' || lowerKey == 'l') {
    tetrisMovePiece(1, 0);
    return true;
  }
  if (lowerKey == 'w' || lowerKey == 'k') {
    if (tetrisRotatePiece()) {
      gTetrisStatus = "Rotated";
    }
    return true;
  }
  if (lowerKey == 's' || lowerKey == 'j') {
    if (tetrisMovePiece(0, 1)) {
      gTetrisScore += 1;
      gTetrisLastDropMs = millis();
    } else {
      tetrisLockPiece();
    }
    return true;
  }
  if (keys.enter || keys.space || key == '\r' || key == '\n' || key == ' ') {
    tetrisHardDrop();
    return true;
  }
  return true;
}

uint32_t stopwatchElapsedMs() {
  if (!gStopwatchRunning) {
    return gStopwatchElapsedMs;
  }
  return gStopwatchElapsedMs + (millis() - gStopwatchStartedMs);
}

String formatStopwatchElapsed(uint32_t elapsedMs) {
  uint32_t centis = (elapsedMs / 10UL) % 100UL;
  uint32_t totalSec = elapsedMs / 1000UL;
  uint32_t seconds = totalSec % 60UL;
  uint32_t minutes = (totalSec / 60UL) % 60UL;
  uint32_t hours = totalSec / 3600UL;
  char buf[18];
  if (hours > 0) {
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu.%02lu", static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(centis));
  } else {
    snprintf(buf, sizeof(buf), "%02lu:%02lu.%02lu", static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds), static_cast<unsigned long>(centis));
  }
  return String(buf);
}

void stopwatchToggle() {
  if (gStopwatchRunning) {
    gStopwatchElapsedMs = stopwatchElapsedMs();
    gStopwatchRunning = false;
    gStopwatchStatus = "Paused";
    setStatus("Stopwatch paused", 700);
  } else {
    gStopwatchStartedMs = millis();
    gStopwatchRunning = true;
    gStopwatchStatus = "Running";
    setStatus("Stopwatch running", 700);
  }
}

void stopwatchReset() {
  gStopwatchRunning = false;
  gStopwatchElapsedMs = 0;
  gStopwatchStartedMs = millis();
  gStopwatchLapMs = 0;
  gStopwatchStatus = "Reset";
  setStatus("Stopwatch reset", 700);
}

bool handleStopwatchKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (keys.enter || keys.space || key == '\r' || key == '\n' || key == ' ') {
    stopwatchToggle();
    return true;
  }
  if (lowerKey == 'r' || keys.del || key == '\b') {
    stopwatchReset();
    return true;
  }
  if (lowerKey == 'l') {
    gStopwatchLapMs = stopwatchElapsedMs();
    gStopwatchStatus = "Lap";
    setStatus("Stopwatch lap", 700);
    return true;
  }
  return true;
}

String alarmTimeString(uint8_t hour, uint8_t minute) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", hour, minute);
  return String(buf);
}

bool alarmLocalTime(tm& out) {
  if (!hasValidSystemTime()) {
    return false;
  }
  time_t now = time(nullptr);
  localtime_r(&now, &out);
  return true;
}

String alarmClockText() {
  tm localNow = {};
  if (!alarmLocalTime(localNow)) {
    return "--:--";
  }
  return alarmTimeString(static_cast<uint8_t>(localNow.tm_hour), static_cast<uint8_t>(localNow.tm_min));
}

void alarmMarkTriggeredToday(const tm& localNow) {
  gAlarmLastTriggerYear = static_cast<int16_t>(localNow.tm_year);
  gAlarmLastTriggerYday = static_cast<int16_t>(localNow.tm_yday);
}

void triggerAlarm(const String& reason) {
  if (gAlarmRinging) {
    return;
  }
  if (gPlaybackActive) {
    stopPlaybackStream();
    gPlaybackActive = false;
  }
  gAlarmRinging = true;
  gAlarmTriggeredAtMs = millis();
  gAlarmNextBeepMs = 0;
  gAlarmStatus = reason.isEmpty() ? "Ringing" : reason;
  gAlarmEditBuffer = "";
  setUiMode(UiMode::Alarm);
  setStatus("Alarm ringing", 0);
  appendRuntimeLog("ALARM", String("ring ") + alarmTimeString(gAlarmSettings.hour, gAlarmSettings.minute), true);
}

void dismissAlarm(const String& reason) {
  if (gAlarmRinging) {
    M5Cardputer.Speaker.stop();
  }
  gAlarmRinging = false;
  gAlarmNextBeepMs = 0;
  gAlarmStatus = reason.isEmpty() ? "Dismissed" : reason;
  setStatus(gAlarmStatus, 1200);
  appendRuntimeLog("ALARM", gAlarmStatus, true);
}

String alarmSnoozeUntilText() {
  if (gAlarmSnoozeEpoch <= 0) {
    return "";
  }
  tm snoozeTm = {};
  localtime_r(&gAlarmSnoozeEpoch, &snoozeTm);
  return alarmTimeString(static_cast<uint8_t>(snoozeTm.tm_hour), static_cast<uint8_t>(snoozeTm.tm_min));
}

void snoozeAlarm() {
  time_t now = time(nullptr);
  gAlarmSnoozeEpoch = now + static_cast<time_t>(kAlarmSnoozeSec);
  dismissAlarm(String("Snoozed ") + alarmSnoozeUntilText());
}

void adjustAlarmMinutes(int delta) {
  int total = static_cast<int>(gAlarmSettings.hour) * 60 + static_cast<int>(gAlarmSettings.minute) + delta;
  while (total < 0) {
    total += 24 * 60;
  }
  total %= 24 * 60;
  gAlarmSettings.hour = static_cast<uint8_t>(total / 60);
  gAlarmSettings.minute = static_cast<uint8_t>(total % 60);
  gAlarmSettings.enabled = true;
  gAlarmSnoozeEpoch = 0;
  saveAlarmSettings();
  gAlarmStatus = String("Set ") + alarmTimeString(gAlarmSettings.hour, gAlarmSettings.minute);
  setStatus(gAlarmStatus, 700);
}

void commitAlarmEditBuffer() {
  if (gAlarmEditBuffer.length() < 4) {
    gAlarmStatus = "Type HHMM";
    setStatus(gAlarmStatus, 700);
    return;
  }
  int hour = (gAlarmEditBuffer[0] - '0') * 10 + (gAlarmEditBuffer[1] - '0');
  int minute = (gAlarmEditBuffer[2] - '0') * 10 + (gAlarmEditBuffer[3] - '0');
  gAlarmEditBuffer = "";
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    gAlarmStatus = "Invalid time";
    setStatus("Alarm invalid HHMM", 1000);
    return;
  }
  gAlarmSettings.hour = static_cast<uint8_t>(hour);
  gAlarmSettings.minute = static_cast<uint8_t>(minute);
  gAlarmSettings.enabled = true;
  gAlarmSnoozeEpoch = 0;
  saveAlarmSettings();
  gAlarmStatus = String("Set ") + alarmTimeString(gAlarmSettings.hour, gAlarmSettings.minute);
  setStatus(gAlarmStatus + " on", 1000);
}

void toggleAlarmEnabled() {
  gAlarmSettings.enabled = !gAlarmSettings.enabled;
  if (!gAlarmSettings.enabled) {
    gAlarmSnoozeEpoch = 0;
  }
  saveAlarmSettings();
  gAlarmStatus = gAlarmSettings.enabled ? "Enabled" : "Disabled";
  setStatus(String("Alarm ") + (gAlarmSettings.enabled ? "on" : "off"), 900);
}

void serviceAlarm() {
  uint32_t nowMs = millis();
  if (gAlarmRinging) {
    if (gAlarmNextBeepMs == 0 || nowMs >= gAlarmNextBeepMs) {
      uint8_t phase = static_cast<uint8_t>((nowMs - gAlarmTriggeredAtMs) / kAlarmBeepIntervalMs);
      focusTone((phase % 2) == 0 ? 1320 : 880, 180);
      flashActivityLed(255, phase % 2 ? 80 : 20, 20, 160);
      gAlarmNextBeepMs = nowMs + kAlarmBeepIntervalMs;
    }
    return;
  }

  tm localNow = {};
  bool timeOk = alarmLocalTime(localNow);
  time_t nowEpoch = time(nullptr);
  if (gAlarmSnoozeEpoch > 0 && nowEpoch >= gAlarmSnoozeEpoch) {
    gAlarmSnoozeEpoch = 0;
    triggerAlarm("Snooze ringing");
    return;
  }
  if (!timeOk || !gAlarmSettings.enabled) {
    return;
  }
  if (localNow.tm_hour == gAlarmSettings.hour && localNow.tm_min == gAlarmSettings.minute &&
      (gAlarmLastTriggerYear != localNow.tm_year || gAlarmLastTriggerYday != localNow.tm_yday)) {
    alarmMarkTriggeredToday(localNow);
    triggerAlarm("Daily alarm");
  }
}

bool handleAlarmKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (gAlarmRinging) {
    if (lowerKey == 's') {
      snoozeAlarm();
      return true;
    }
    if (keys.enter || keys.space || key == '\r' || key == '\n' || key == ' ' || lowerKey == 'd') {
      dismissAlarm("Dismissed");
      return true;
    }
    return true;
  }
  if (key >= '0' && key <= '9') {
    if (gAlarmEditBuffer.length() >= 4) {
      gAlarmEditBuffer = "";
    }
    gAlarmEditBuffer += key;
    gAlarmStatus = String("Edit ") + gAlarmEditBuffer;
    if (gAlarmEditBuffer.length() == 4) {
      commitAlarmEditBuffer();
    } else {
      setStatus(String("Alarm HHMM ") + gAlarmEditBuffer, 700);
    }
    return true;
  }
  if (keys.del || key == '\b' || lowerKey == 'c') {
    if (!gAlarmEditBuffer.isEmpty()) {
      gAlarmEditBuffer.remove(gAlarmEditBuffer.length() - 1);
      gAlarmStatus = gAlarmEditBuffer.isEmpty() ? "Ready" : (String("Edit ") + gAlarmEditBuffer);
    } else {
      gAlarmStatus = "Ready";
    }
    return true;
  }
  if (keys.enter || keys.space || key == '\r' || key == '\n' || key == ' ' || lowerKey == 't') {
    toggleAlarmEnabled();
    return true;
  }
  if (key == '-' || key == '_' || lowerKey == 'z') {
    adjustAlarmMinutes(lowerKey == 'z' ? -5 : -1);
    return true;
  }
  if (key == '+' || key == '=' || lowerKey == 'x') {
    adjustAlarmMinutes(lowerKey == 'x' ? 5 : 1);
    return true;
  }
  if (lowerKey == 's') {
    saveAlarmSettings();
    setStatus("Alarm saved", 700);
    return true;
  }
  return true;
}

bool handleCalendarKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (key == ',' || key == '<' || key == ';' || key == '-' || key == '_' || lowerKey == 'z') {
    gCalendarMonthOffset = max<int>(-120, gCalendarMonthOffset - 1);
    setStatus("Calendar previous month", 700);
    return true;
  }
  if (key == '.' || key == '>' || key == '/' || key == '+' || key == '=' || lowerKey == 'x') {
    gCalendarMonthOffset = min<int>(120, gCalendarMonthOffset + 1);
    setStatus("Calendar next month", 700);
    return true;
  }
  if (keys.enter || keys.space || key == '\r' || key == '\n' || key == ' ' || lowerKey == 't' || lowerKey == 'r') {
    gCalendarMonthOffset = 0;
    setStatus("Calendar today", 700);
    return true;
  }
  return true;
}

String irRemoteSlotKey(uint8_t slot) {
  return String("slot") + String(slot);
}

namespace {
constexpr uint8_t kDaikinAcStateLength = 35;
constexpr uint8_t kDaikinAcSection1Length = 8;
constexpr uint8_t kDaikinAcSection2Length = 8;
constexpr uint8_t kDaikinAcSection3Length = kDaikinAcStateLength - kDaikinAcSection1Length - kDaikinAcSection2Length;
constexpr uint16_t kDaikinAcHdrMark = 3650;
constexpr uint16_t kDaikinAcHdrSpace = 1623;
constexpr uint16_t kDaikinAcBitMark = 428;
constexpr uint16_t kDaikinAcZeroSpace = 428;
constexpr uint16_t kDaikinAcOneSpace = 1280;
constexpr uint16_t kDaikinAcGap = 29000;
constexpr uint8_t kDaikinAcAuto = 0;
constexpr uint8_t kDaikinAcDry = 2;
constexpr uint8_t kDaikinAcCool = 3;
constexpr uint8_t kDaikinAcHeat = 4;
constexpr uint8_t kDaikinAcFanOnly = 6;
constexpr uint8_t kDaikinAcTempMin = 18;
constexpr uint8_t kDaikinAcTempMax = 30;
constexpr uint8_t kDaikinAcFanAuto = 0xA;
constexpr uint8_t kDaikinAcFanQuiet = 0xB;
constexpr uint8_t kDaikinAcSwingOff = 0x0;
constexpr uint8_t kDaikinAcSwingOn = 0xF;
constexpr uint8_t kIrRemoteSlotDaikin = 0;
constexpr uint8_t kIrRemoteSlotHisense = 1;
constexpr uint8_t kIrRemotePresetCount = 2;
constexpr uint16_t kHisenseTvNecAddress = 0x0004;  // ERF3W80H 4K/A6G/U6K family: NEC bytes 04 FB.
constexpr uint8_t kHisensePowerToggle = 0x08;
constexpr uint8_t kHisensePowerOn = 0x71;   // Hisense discrete power-on in the same 04 FB family.
constexpr uint8_t kHisensePowerOff = 0x72;  // Hisense discrete power-off in the same 04 FB family.
constexpr uint8_t kHisenseInput = 0x0B;
constexpr uint8_t kHisenseHome = 0x4A;
constexpr uint8_t kHisenseBack = 0x04;
constexpr uint8_t kHisenseSettings = 0x43;
constexpr uint8_t kHisenseExit = 0x4D;
constexpr uint8_t kHisenseUp = 0x56;
constexpr uint8_t kHisenseDown = 0x57;
constexpr uint8_t kHisenseLeft = 0x58;
constexpr uint8_t kHisenseRight = 0x59;
constexpr uint8_t kHisenseOk = 0x5A;
constexpr uint8_t kHisenseMute = 0x09;
constexpr uint8_t kHisenseVolUp = 0x02;
constexpr uint8_t kHisenseVolDown = 0x03;
constexpr uint8_t kHisenseChUp = 0x00;
constexpr uint8_t kHisenseChDown = 0x01;
constexpr uint8_t kHisenseYouTube = 0x49;
constexpr uint8_t kHisenseNetflix = 0x4C;
constexpr uint8_t kHisensePrimeVideo = 0x47;
constexpr uint8_t kHisenseAssistant = 0x1E;

bool gDaikinPower = false;
uint8_t gDaikinMode = kDaikinAcCool;
uint8_t gDaikinTempC = 24;
uint8_t gDaikinFan = kDaikinAcFanAuto;
bool gDaikinSwingV = false;
bool gDaikinQuiet = false;
bool gDaikinPowerful = false;
String gDaikinLastCommand = "Ready";
uint8_t gHisenseLastCommand = kHisensePowerToggle;
String gHisenseLastCommandLabel = "Ready";

uint8_t daikinSumBytes(const uint8_t* data, uint8_t len) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < len; ++i) {
    sum += data[i];
  }
  return static_cast<uint8_t>(sum & 0xFF);
}

void daikinResetState(uint8_t state[kDaikinAcStateLength]) {
  memset(state, 0, kDaikinAcStateLength);
  state[0] = 0x11;
  state[1] = 0xDA;
  state[2] = 0x27;
  state[4] = 0xC5;
  state[8] = 0x11;
  state[9] = 0xDA;
  state[10] = 0x27;
  state[12] = 0x42;
  state[16] = 0x11;
  state[17] = 0xDA;
  state[18] = 0x27;
  state[21] = 0x08;
  state[22] = 24 * 2;
  state[24] = 0xB0;
  state[27] = 0x06;
  state[28] = 0x60;
  state[31] = 0xC0;
}

void daikinApplyState(uint8_t state[kDaikinAcStateLength]) {
  state[21] = static_cast<uint8_t>((state[21] & 0x8E) | (gDaikinPower ? 0x01 : 0x00) |
                                   ((gDaikinMode & 0x07) << 4));
  state[22] = static_cast<uint8_t>(clampValue<int>(gDaikinTempC, kDaikinAcTempMin, kDaikinAcTempMax) * 2);
  state[24] = static_cast<uint8_t>(((gDaikinFan & 0x0F) << 4) | (gDaikinSwingV ? kDaikinAcSwingOn : kDaikinAcSwingOff));
  state[29] = static_cast<uint8_t>((state[29] & 0xDE) | (gDaikinPowerful ? 0x01 : 0x00) |
                                   (gDaikinQuiet ? 0x20 : 0x00));
  state[7] = daikinSumBytes(state, kDaikinAcSection1Length - 1);
  state[15] = daikinSumBytes(state + kDaikinAcSection1Length, kDaikinAcSection2Length - 1);
  state[34] = daikinSumBytes(state + kDaikinAcSection1Length + kDaikinAcSection2Length,
                             kDaikinAcSection3Length - 1);
}

void daikinSendBit(bool one) {
  IrSender.mark(kDaikinAcBitMark);
  IrSender.space(one ? kDaikinAcOneSpace : kDaikinAcZeroSpace);
}

void daikinSendBytes(const uint8_t* data, uint8_t len) {
  for (uint8_t i = 0; i < len; ++i) {
    for (uint8_t mask = 0x01; mask != 0; mask <<= 1) {
      daikinSendBit((data[i] & mask) != 0);
    }
  }
}

void daikinSendLeadIn() {
  for (uint8_t i = 0; i < 5; ++i) {
    daikinSendBit(false);
  }
  IrSender.mark(kDaikinAcBitMark);
  IrSender.space(kDaikinAcZeroSpace + kDaikinAcGap);
}

void daikinSendSection(const uint8_t* data, uint8_t len) {
  IrSender.mark(kDaikinAcHdrMark);
  IrSender.space(kDaikinAcHdrSpace);
  daikinSendBytes(data, len);
  IrSender.mark(kDaikinAcBitMark);
  IrSender.space(kDaikinAcZeroSpace + kDaikinAcGap);
}
}  // namespace

const char* daikinModeName(uint8_t mode) {
  switch (mode) {
    case kDaikinAcCool:
      return "Cool";
    case kDaikinAcFanOnly:
      return "Fan";
    case kDaikinAcDry:
      return "Dry";
    case kDaikinAcHeat:
      return "Heat";
    case kDaikinAcAuto:
      return "Auto";
    default:
      return "AC";
  }
}

String daikinFanName() {
  if (gDaikinFan == kDaikinAcFanAuto) {
    return "Auto";
  }
  if (gDaikinFan == kDaikinAcFanQuiet) {
    return "Quiet";
  }
  int speed = static_cast<int>(gDaikinFan) - 2;
  return String(clampValue<int>(speed, 1, 5));
}

String daikinStateSummary() {
  return String(gDaikinPower ? "ON" : "OFF") + " " + daikinModeName(gDaikinMode) + " " +
         String(gDaikinTempC) + "C F" + daikinFanName();
}

bool isHisenseRemoteSlot() {
  return gSelectedIrRemoteSlot == kIrRemoteSlotHisense;
}

const char* irRemotePresetName(uint8_t slotIndex) {
  return slotIndex == kIrRemoteSlotHisense ? "ERF3W80H 4K" : "Daikin AC";
}

void selectIrRemoteSlot(uint8_t slotIndex) {
  gSelectedIrRemoteSlot = clampValue<int>(slotIndex, 0, kIrRemotePresetCount - 1);
  gIrRemoteStatus = String("Slot ") + String(gSelectedIrRemoteSlot + 1) + ": " + irRemotePresetName(gSelectedIrRemoteSlot);
  setStatus(gIrRemoteStatus, 850);
}

void cycleIrRemoteSlot(int delta) {
  int next = (gSelectedIrRemoteSlot + delta + kIrRemotePresetCount) % kIrRemotePresetCount;
  selectIrRemoteSlot(static_cast<uint8_t>(next));
}

void initIrRemote() {
  if (gIrRemoteReady) {
    return;
  }
  pinMode(kIrSendPin, OUTPUT);
  IrSender.begin(kIrSendPin);
  gIrRemoteReady = true;
  gIrRemoteLearning = false;
  gIrRemoteAutoLearning = false;
  gSelectedIrRemoteSlot = kIrRemoteSlotDaikin;
  gIrRemoteStatus = "Slot 1 Daikin, Slot 2 ERF3W80H 4K";
  appendRuntimeLog("IR", gIrRemoteStatus, true);
}

void sendDaikinAcState(const String& commandLabel) {
  initIrRemote();
  uint8_t state[kDaikinAcStateLength];
  daikinResetState(state);
  daikinApplyState(state);

  IrSender.enableIROut(38);
  daikinSendLeadIn();
  daikinSendSection(state, kDaikinAcSection1Length);
  daikinSendSection(state + kDaikinAcSection1Length, kDaikinAcSection2Length);
  daikinSendSection(state + kDaikinAcSection1Length + kDaikinAcSection2Length, kDaikinAcSection3Length);
  IrSender.IRLedOff();

  gDaikinLastCommand = commandLabel;
  gIrRemoteStatus = String("Daikin ") + commandLabel + ": " + daikinStateSummary();
  setStatus(gIrRemoteStatus, 1100);
  appendRuntimeLog("IR", gIrRemoteStatus, true);
  flashActivityLed(255, 80, 20, 120);
}

void setDaikinModeAndSend(uint8_t mode, const String& label) {
  gDaikinPower = true;
  gDaikinMode = mode;
  if (mode == kDaikinAcFanOnly) {
    gDaikinFan = kDaikinAcFanAuto;
  }
  sendDaikinAcState(label);
}

void cycleDaikinFanAndSend() {
  gDaikinPower = true;
  static constexpr uint8_t kFanCycle[] = {kDaikinAcFanAuto, 3, 4, 5, 6, 7, kDaikinAcFanQuiet};
  uint8_t next = 0;
  for (uint8_t i = 0; i < sizeof(kFanCycle); ++i) {
    if (gDaikinFan == kFanCycle[i]) {
      next = static_cast<uint8_t>((i + 1) % sizeof(kFanCycle));
      break;
    }
  }
  gDaikinFan = kFanCycle[next];
  if (gDaikinFan == kDaikinAcFanQuiet) {
    gDaikinQuiet = true;
    gDaikinPowerful = false;
  } else {
    gDaikinQuiet = false;
  }
  sendDaikinAcState(String("Fan ") + daikinFanName());
}

void sendHisenseTvCommand(uint8_t command, const String& label) {
  initIrRemote();
  IrSender.sendNEC(kHisenseTvNecAddress, command, NO_REPEATS);
  IrSender.IRLedOff();
  gHisenseLastCommand = command;
  gHisenseLastCommandLabel = label;
  gIrRemoteStatus = String("ERF3W80H ") + label;
  setStatus(gIrRemoteStatus, 950);
  appendRuntimeLog("IR", gIrRemoteStatus, true);
  flashActivityLed(80, 190, 255, 110);
}

bool handleHisenseTvRemoteKey(char key, char lowerKey, const Keyboard_Class::KeysState& keys) {
  if (lowerKey == 'q') {
    sendHisenseTvCommand(kHisensePowerToggle, "Power");
    return true;
  }
  if (lowerKey == 'w') {
    sendHisenseTvCommand(kHisenseUp, "Up");
    return true;
  }
  if (lowerKey == 'a') {
    sendHisenseTvCommand(kHisenseLeft, "Left");
    return true;
  }
  if (lowerKey == 's') {
    sendHisenseTvCommand(kHisenseDown, "Down");
    return true;
  }
  if (lowerKey == 'd') {
    sendHisenseTvCommand(kHisenseRight, "Right");
    return true;
  }
  if (keys.enter || keys.space || key == ' ' || key == '\r' || key == '\n') {
    sendHisenseTvCommand(kHisenseOk, "OK");
    return true;
  }
  if (lowerKey == 'e') {
    sendHisenseTvCommand(kHisenseInput, "Input");
    return true;
  }
  if (lowerKey == 'r') {
    sendHisenseTvCommand(kHisenseBack, "Back");
    return true;
  }
  if (lowerKey == 't') {
    sendHisenseTvCommand(kHisenseHome, "Home");
    return true;
  }
  if (lowerKey == 'y') {
    sendHisenseTvCommand(kHisenseSettings, "Menu");
    return true;
  }
  if (lowerKey == 'f') {
    sendHisenseTvCommand(kHisenseMute, "Mute");
    return true;
  }
  if (lowerKey == 'g') {
    sendHisenseTvCommand(kHisenseChDown, "Ch -");
    return true;
  }
  if (lowerKey == 'h') {
    sendHisenseTvCommand(kHisenseChUp, "Ch +");
    return true;
  }
  if (lowerKey == 'z' || key == '-' || key == '_') {
    sendHisenseTvCommand(kHisenseVolDown, "Vol -");
    return true;
  }
  if (lowerKey == 'x' || key == '+' || key == '=') {
    sendHisenseTvCommand(kHisenseVolUp, "Vol +");
    return true;
  }
  if (lowerKey == 'c') {
    sendHisenseTvCommand(kHisenseExit, "Exit");
    return true;
  }
  if (lowerKey == 'v') {
    sendHisenseTvCommand(kHisenseYouTube, "YouTube");
    return true;
  }
  if (lowerKey == 'b') {
    sendHisenseTvCommand(kHisenseNetflix, "Netflix");
    return true;
  }
  if (lowerKey == 'n') {
    sendHisenseTvCommand(kHisensePrimeVideo, "Prime Video");
    return true;
  }
  if (lowerKey == 'm') {
    sendHisenseTvCommand(kHisenseAssistant, "Assistant");
    return true;
  }
  if (keys.del || key == '\b') {
    sendHisenseTvCommand(kHisensePowerOff, "Power off");
    return true;
  }
  if (lowerKey == 'o') {
    sendHisenseTvCommand(kHisensePowerOn, "Power on");
    return true;
  }
  if (lowerKey == 'p') {
    sendHisenseTvCommand(kHisensePowerOff, "Power off");
    return true;
  }
  gIrRemoteStatus = "ERF3W80H keys: WASD nav, Enter OK, Z/X volume";
  setStatus("ERF3W80H: WASD Enter Z/X E/R/T/Y", 1200);
  return true;
}

void serviceIrRemote() {
  if (gUiMode == UiMode::IrRemote) {
    initIrRemote();
  }
}

bool handleIrRemoteKey(char key, const Keyboard_Class::KeysState& keys) {
  initIrRemote();
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));

  if (key == '1' || key == '2') {
    selectIrRemoteSlot(static_cast<uint8_t>(key - '1'));
    return true;
  }
  if (key == ',' || key == '<' || key == ';') {
    cycleIrRemoteSlot(-1);
    return true;
  }
  if (key == '.' || key == '>' || key == '/') {
    cycleIrRemoteSlot(1);
    return true;
  }

  if (isHisenseRemoteSlot()) {
    return handleHisenseTvRemoteKey(key, lowerKey, keys);
  }

  if (lowerKey == 'q') {
    gDaikinPower = !gDaikinPower;
    sendDaikinAcState(gDaikinPower ? "Power on" : "Power off");
    return true;
  }
  if (lowerKey == 'w') {
    setDaikinModeAndSend(kDaikinAcCool, "Cool");
    return true;
  }
  if (lowerKey == 'e') {
    setDaikinModeAndSend(kDaikinAcFanOnly, "Fan only");
    return true;
  }
  if (lowerKey == 'r') {
    setDaikinModeAndSend(kDaikinAcDry, "Dry");
    return true;
  }
  if (lowerKey == 't') {
    setDaikinModeAndSend(kDaikinAcHeat, "Heat");
    return true;
  }
  if (lowerKey == 'y') {
    setDaikinModeAndSend(kDaikinAcAuto, "Auto");
    return true;
  }
  if (lowerKey == 'a' || key == '-' || key == '_') {
    gDaikinPower = true;
    gDaikinTempC = static_cast<uint8_t>(clampValue<int>(gDaikinTempC - 1, kDaikinAcTempMin, kDaikinAcTempMax));
    sendDaikinAcState("Temp -");
    return true;
  }
  if (lowerKey == 's' || key == '+' || key == '=') {
    gDaikinPower = true;
    gDaikinTempC = static_cast<uint8_t>(clampValue<int>(gDaikinTempC + 1, kDaikinAcTempMin, kDaikinAcTempMax));
    sendDaikinAcState("Temp +");
    return true;
  }
  if (lowerKey == 'd') {
    cycleDaikinFanAndSend();
    return true;
  }
  if (lowerKey == 'f') {
    gDaikinPower = true;
    gDaikinSwingV = !gDaikinSwingV;
    sendDaikinAcState(gDaikinSwingV ? "Swing on" : "Swing off");
    return true;
  }
  if (lowerKey == 'g') {
    gDaikinPower = true;
    gDaikinQuiet = !gDaikinQuiet;
    if (gDaikinQuiet) {
      gDaikinPowerful = false;
      gDaikinFan = kDaikinAcFanQuiet;
    } else if (gDaikinFan == kDaikinAcFanQuiet) {
      gDaikinFan = kDaikinAcFanAuto;
    }
    sendDaikinAcState(gDaikinQuiet ? "Quiet on" : "Quiet off");
    return true;
  }
  if (lowerKey == 'h') {
    gDaikinPower = true;
    gDaikinPowerful = !gDaikinPowerful;
    if (gDaikinPowerful) {
      gDaikinQuiet = false;
      if (gDaikinFan == kDaikinAcFanQuiet) {
        gDaikinFan = kDaikinAcFanAuto;
      }
    }
    sendDaikinAcState(gDaikinPowerful ? "Powerful on" : "Powerful off");
    return true;
  }
  if (keys.enter || keys.space || key == ' ' || key == '\r' || key == '\n') {
    sendDaikinAcState("Resend");
    return true;
  }
  if (keys.del || key == '\b') {
    gDaikinPower = false;
    sendDaikinAcState("Power off");
    return true;
  }

  gIrRemoteStatus = "Daikin keys: Q power, W cool, E fan, A/S temp";
  setStatus("Daikin: 1/2 slots Q/W/E/R/T/Y A/S", 1200);
  return true;
}

bool handleGlobalEscape() {
  bool changed = false;

  if (gAlarmRinging) {
    dismissAlarm("Dismissed");
    changed = true;
  }

  if (gRecording) {
    cancelRecordingDiscard();
    changed = true;
  }
  if (gRealtimeActive || gRealtimeRecording) {
    closeRealtimeSession("escape");
    changed = true;
  }
  if (gPlaybackActive) {
    stopPlaybackStream();
    gPlaybackActive = false;
    changed = true;
  }
  if (gDebugOverlayVisible) {
    gDebugOverlayVisible = false;
    changed = true;
  }
  if (gLauncherVisible) {
    gLauncherVisible = false;
    changed = true;
  }
  if (gFocus.helpVisible) {
    gFocus.helpVisible = false;
    changed = true;
  }
  if (!gInputBuffer.isEmpty()) {
    gInputBuffer = "";
    changed = true;
  }
  if (!gNoteInputBuffer.isEmpty() || gNotesMode != NotesMode::List) {
    gNoteInputBuffer = "";
    gNotesMode = NotesMode::List;
    changed = true;
  }
  if (gChatScrollOffset != 0 || gLogScrollOffset != 0 || gNoteScrollOffset != 0 || gNoteListScrollOffset != 0) {
    gChatScrollOffset = 0;
    gLogScrollOffset = 0;
    gNoteScrollOffset = 0;
    gNoteListScrollOffset = 0;
    changed = true;
  }

  gLastCtrlTapMs = 0;
  gCtrlSoloHeld = false;
  gCtrlSoloCandidate = false;

  if (!gSubmitting && !gThinking) {
    gAssistantPendingVisible = false;
    setFaceMode(FaceMode::Idle);
  }

  setUiMode(UiMode::Home);
  setStatus("Ready", 1000);
  setKeyDiag("esc:reset");
  appendRuntimeLog("UI", "escape reset", false);
  return changed || gUiMode != UiMode::Home;
}
