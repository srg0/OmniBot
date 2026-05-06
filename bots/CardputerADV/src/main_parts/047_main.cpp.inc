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

uint8_t currentIrReceivePin() {
  return kIrReceivePins[clampValue<int>(gIrReceivePinIndex, 0, kIrReceivePinCount - 1)];
}

String irRemoteReceivePinsLabel() {
  String label;
  for (uint8_t i = 0; i < kIrReceivePinCount; ++i) {
    if (!label.isEmpty()) {
      label += "/";
    }
    label += "G";
    label += String(kIrReceivePins[i]);
  }
  return label;
}

String irRemoteCurrentReceivePinLabel() {
  return String("G") + String(currentIrReceivePin());
}

void configureIrReceivePinPullup(uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
}

void setIrReceivePinIndex(uint8_t pinIndex) {
  if (pinIndex >= kIrReceivePinCount || pinIndex == gIrReceivePinIndex) {
    return;
  }
  bool wasReady = gIrRemoteReady;
  if (wasReady) {
    IrReceiver.stop();
  }
  gIrReceivePinIndex = pinIndex;
  if (wasReady) {
    IrReceiver.setReceivePin(currentIrReceivePin());
    configureIrReceivePinPullup(currentIrReceivePin());
    IrReceiver.start();
  }
  gIrLastReceivePinProbeMs = millis();
}

void probeNextIrReceivePinIfLearning() {
  if (!gIrRemoteLearning || kIrReceivePinCount < 2 || !IrReceiver.isIdle()) {
    return;
  }
  uint32_t now = millis();
  if (now - gIrLastReceivePinProbeMs < kIrReceivePinProbeMs) {
    return;
  }
  uint8_t nextIndex = (gIrReceivePinIndex + 1) % kIrReceivePinCount;
  setIrReceivePinIndex(nextIndex);
  gIrRemoteStatus = String("Listening slot ") + String(gSelectedIrRemoteSlot + 1) + " on RX " +
                    irRemoteCurrentReceivePinLabel() + " (" + irRemoteReceivePinsLabel() + ")";
}

bool irRemoteSlotUsesRaw(const IrRemoteSlot& slot) {
  decode_type_t protocol = slot.receivedIRData.protocol;
  return protocol == UNKNOWN || protocol == PULSE_WIDTH || protocol == PULSE_DISTANCE;
}

String irRemoteProtocolName(decode_type_t protocol) {
  const char* name = getProtocolString(protocol);
  return name && name[0] ? String(name) : String("UNKNOWN");
}

String irRemoteSlotSummary(const IrRemoteSlot& slot) {
  if (!slot.learned) {
    return "empty";
  }
  String proto = irRemoteProtocolName(slot.receivedIRData.protocol);
  if (irRemoteSlotUsesRaw(slot)) {
    return proto + " raw " + String(slot.rawCodeLength);
  }
  String address = String(slot.receivedIRData.address, HEX);
  String command = String(slot.receivedIRData.command, HEX);
  address.toUpperCase();
  command.toUpperCase();
  return proto + " A" + address + " C" + command;
}

void saveIrRemoteSlot(uint8_t slotIndex) {
  if (slotIndex >= kIrRemoteSlotCount) {
    return;
  }
  if (!gPrefs.begin(kPrefsIrRemoteNs, false)) {
    return;
  }
  const IrRemoteSlot& slot = gIrRemoteSlots[slotIndex];
  IrRemotePersistSlot stored;
  stored.learned = slot.learned ? 1 : 0;
  stored.rawCodeLength = min<uint16_t>(slot.rawCodeLength, RAW_BUFFER_LENGTH);
  stored.receivedIRData = slot.receivedIRData;
  memcpy(stored.rawCode, slot.rawCode, stored.rawCodeLength);
  String key = irRemoteSlotKey(slotIndex);
  gPrefs.putBytes(key.c_str(), &stored, sizeof(stored));
  gPrefs.end();
}

void loadIrRemoteSlots() {
  if (!gPrefs.begin(kPrefsIrRemoteNs, true)) {
    return;
  }
  for (uint8_t i = 0; i < kIrRemoteSlotCount; ++i) {
    String key = irRemoteSlotKey(i);
    if (gPrefs.getBytesLength(key.c_str()) != sizeof(IrRemotePersistSlot)) {
      continue;
    }
    IrRemotePersistSlot stored;
    size_t bytes = gPrefs.getBytes(key.c_str(), &stored, sizeof(stored));
    if (bytes != sizeof(stored) || stored.magic != kIrPersistMagic) {
      continue;
    }
    IrRemoteSlot& slot = gIrRemoteSlots[i];
    slot.learned = stored.learned != 0;
    slot.rawCodeLength = min<uint16_t>(stored.rawCodeLength, RAW_BUFFER_LENGTH);
    slot.receivedIRData = stored.receivedIRData;
    memcpy(slot.rawCode, stored.rawCode, slot.rawCodeLength);
  }
  gPrefs.end();
}

void clearIrRemoteSlot(uint8_t slotIndex) {
  if (slotIndex >= kIrRemoteSlotCount) {
    return;
  }
  gIrRemoteSlots[slotIndex] = IrRemoteSlot{};
  saveIrRemoteSlot(slotIndex);
  gIrRemoteStatus = String("Cleared slot ") + String(slotIndex + 1);
  setStatus(gIrRemoteStatus, 900);
  appendRuntimeLog("IR", gIrRemoteStatus, true);
}

void initIrRemote() {
  if (gIrRemoteReady) {
    return;
  }
  loadIrRemoteSlots();
  for (uint8_t i = 0; i < kIrReceivePinCount; ++i) {
    configureIrReceivePinPullup(kIrReceivePins[i]);
  }
  pinMode(kIrSendPin, OUTPUT);
  IrSender.begin(kIrSendPin);
  IrReceiver.begin(currentIrReceivePin(), false);
  configureIrReceivePinPullup(currentIrReceivePin());
  IrReceiver.resume();
  gIrRemoteReady = true;
  gIrLastReceivePinProbeMs = millis();
  gIrRemoteStatus = String("IRG") + String(kIrSendPin) + " sends only; Learn uses Grove RX " + irRemoteReceivePinsLabel();
  appendRuntimeLog("IR", gIrRemoteStatus, true);
}

void startIrRemoteLearn(uint8_t slotIndex, bool automatic = false) {
  initIrRemote();
  if (slotIndex >= kIrRemoteSlotCount) {
    return;
  }
  gSelectedIrRemoteSlot = slotIndex;
  gIrRemoteLearning = true;
  gIrRemoteAutoLearning = automatic;
  gIrRemoteLearnStartedMs = millis();
  gIrLastReceivePinProbeMs = millis();
  IrReceiver.resume();
  gIrRemoteStatus = String(automatic ? "Auto-learning slot " : "Learning slot ") + String(slotIndex + 1) +
                    " on Grove RX " + irRemoteReceivePinsLabel() + " (IRG44 TX only)";
  setStatus(automatic ? "Empty slot: aim at Grove RX" : "Aim remote at Grove RX, not IRG44", 1800);
  appendRuntimeLog("IR", gIrRemoteStatus, true);
}

void autoStartIrRemoteLearnIfNeeded() {
  initIrRemote();
  uint8_t slotIndex = static_cast<uint8_t>(clampValue<int>(gSelectedIrRemoteSlot, 0, kIrRemoteSlotCount - 1));
  if (gIrRemoteLearning || gIrRemoteSlots[slotIndex].learned) {
    return;
  }
  startIrRemoteLearn(slotIndex, true);
}

bool storeIrRemoteCode(uint8_t slotIndex) {
  if (slotIndex >= kIrRemoteSlotCount) {
    return false;
  }
  if (IrReceiver.irparams.rawlen < 4) {
    gIrRemoteStatus = "Ignored short IR frame";
    return false;
  }
  uint8_t flags = IrReceiver.decodedIRData.flags;
  if (flags & IRDATA_FLAGS_WAS_OVERFLOW) {
    gIrRemoteStatus = "IR frame too long";
    setStatus(gIrRemoteStatus, 1200);
    return false;
  }
  if (flags & (IRDATA_FLAGS_IS_REPEAT | IRDATA_FLAGS_IS_AUTO_REPEAT | IRDATA_FLAGS_PARITY_FAILED)) {
    gIrRemoteStatus = "Ignored repeat/parity frame";
    return false;
  }

  IrRemoteSlot& slot = gIrRemoteSlots[slotIndex];
  slot = IrRemoteSlot{};
  slot.receivedIRData = IrReceiver.decodedIRData;
  slot.receivedIRData.flags = 0;
  slot.receivedIRData.rawlen = IrReceiver.irparams.rawlen;
  slot.learned = true;
  slot.learnedAtMs = millis();

  if (irRemoteSlotUsesRaw(slot)) {
    slot.rawCodeLength = min<uint16_t>(IrReceiver.irparams.rawlen - 1, RAW_BUFFER_LENGTH);
    IrReceiver.compensateAndStoreIRResultInArray(slot.rawCode);
  }

  saveIrRemoteSlot(slotIndex);
  gIrRemoteLearning = false;
  gIrRemoteAutoLearning = false;
  gIrRemoteStatus = String("Saved slot ") + String(slotIndex + 1) + " via Grove RX " + irRemoteCurrentReceivePinLabel() +
                    ": " + irRemoteSlotSummary(slot);
  setStatus(gIrRemoteStatus, 1500);
  appendRuntimeLog("IR", gIrRemoteStatus, true);
  return true;
}

void sendIrRemoteSlot(uint8_t slotIndex) {
  initIrRemote();
  if (slotIndex >= kIrRemoteSlotCount) {
    return;
  }
  IrRemoteSlot& slot = gIrRemoteSlots[slotIndex];
  if (!slot.learned) {
    gIrRemoteStatus = String("Slot ") + String(slotIndex + 1) + " is empty";
    setStatus(gIrRemoteStatus, 900);
    return;
  }
  gIrRemoteLearning = false;
  gIrRemoteAutoLearning = false;
  IrReceiver.stop();
  if (irRemoteSlotUsesRaw(slot)) {
    if (slot.rawCodeLength == 0) {
      gIrRemoteStatus = "No raw IR data";
      setStatus(gIrRemoteStatus, 900);
      IrReceiver.start();
      return;
    }
    IrSender.sendRaw(slot.rawCode, slot.rawCodeLength, 38);
  } else {
    IRData data = slot.receivedIRData;
    data.flags = 0;
    IrSender.write(&data, NO_REPEATS);
  }
  IrReceiver.restartAfterSend();
  IrReceiver.start();
  gIrRemoteStatus = String("Sent slot ") + String(slotIndex + 1) + " via IRG" + String(kIrSendPin) + ": " + irRemoteSlotSummary(slot);
  setStatus(gIrRemoteStatus, 1000);
  appendRuntimeLog("IR", gIrRemoteStatus, true);
  flashActivityLed(255, 80, 20, 120);
}

void serviceIrRemote() {
  if (!gIrRemoteReady) {
    return;
  }
  if (IrReceiver.decode()) {
    uint8_t slotIndex = static_cast<uint8_t>(clampValue<int>(gSelectedIrRemoteSlot, 0, kIrRemoteSlotCount - 1));
    if (gIrRemoteLearning || (gUiMode == UiMode::IrRemote && !gIrRemoteSlots[slotIndex].learned)) {
      if (!gIrRemoteLearning) {
        gIrRemoteAutoLearning = true;
        gIrRemoteLearnStartedMs = millis();
      }
      storeIrRemoteCode(static_cast<uint8_t>(clampValue<int>(gSelectedIrRemoteSlot, 0, kIrRemoteSlotCount - 1)));
    }
    IrReceiver.resume();
  } else {
    probeNextIrReceivePinIfLearning();
  }
  if (gIrRemoteLearning && millis() - gIrRemoteLearnStartedMs > kIrLearnTimeoutMs) {
    gIrRemoteLearning = false;
    gIrRemoteAutoLearning = false;
    gIrRemoteStatus = String("No Grove RX signal on ") + irRemoteReceivePinsLabel() + "; IRG44 sends only";
    setStatus("Need Grove IR receiver", 1300);
    appendRuntimeLog("IR", gIrRemoteStatus, true);
  }
}

bool handleIrRemoteKey(char key, const Keyboard_Class::KeysState& keys) {
  initIrRemote();
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (key >= '1' && key <= '6') {
    gSelectedIrRemoteSlot = key - '1';
    if (gIrRemoteLearning && !gIrRemoteSlots[gSelectedIrRemoteSlot].learned) {
      startIrRemoteLearn(static_cast<uint8_t>(gSelectedIrRemoteSlot), true);
    } else if (gIrRemoteLearning && gIrRemoteAutoLearning) {
      gIrRemoteLearning = false;
      gIrRemoteAutoLearning = false;
    }
    setStatus(String("IR slot ") + String(gSelectedIrRemoteSlot + 1), 700);
    return true;
  }
  if (key == ',' || key == '<' || key == ';' || key == '-' || key == '_' || lowerKey == 'z') {
    gSelectedIrRemoteSlot = (gSelectedIrRemoteSlot + kIrRemoteSlotCount - 1) % kIrRemoteSlotCount;
    if (gIrRemoteLearning && !gIrRemoteSlots[gSelectedIrRemoteSlot].learned) {
      startIrRemoteLearn(static_cast<uint8_t>(gSelectedIrRemoteSlot), true);
    } else if (gIrRemoteLearning && gIrRemoteAutoLearning) {
      gIrRemoteLearning = false;
      gIrRemoteAutoLearning = false;
    }
    setStatus(String("IR slot ") + String(gSelectedIrRemoteSlot + 1), 600);
    return true;
  }
  if (key == '.' || key == '>' || key == '/' || key == '+' || key == '=' || lowerKey == 'x') {
    gSelectedIrRemoteSlot = (gSelectedIrRemoteSlot + 1) % kIrRemoteSlotCount;
    if (gIrRemoteLearning && !gIrRemoteSlots[gSelectedIrRemoteSlot].learned) {
      startIrRemoteLearn(static_cast<uint8_t>(gSelectedIrRemoteSlot), true);
    } else if (gIrRemoteLearning && gIrRemoteAutoLearning) {
      gIrRemoteLearning = false;
      gIrRemoteAutoLearning = false;
    }
    setStatus(String("IR slot ") + String(gSelectedIrRemoteSlot + 1), 600);
    return true;
  }
  if (lowerKey == 'l' || keys.space || key == ' ') {
    startIrRemoteLearn(static_cast<uint8_t>(gSelectedIrRemoteSlot));
    return true;
  }
  if (keys.enter || key == '\r' || key == '\n' || lowerKey == 's') {
    sendIrRemoteSlot(static_cast<uint8_t>(gSelectedIrRemoteSlot));
    return true;
  }
  if (keys.del || key == '\b' || lowerKey == 'c') {
    clearIrRemoteSlot(static_cast<uint8_t>(gSelectedIrRemoteSlot));
    return true;
  }
  if (lowerKey == 'r') {
    gIrRemoteStatus = String("IRG") + String(kIrSendPin) + " sends only; Learn needs Grove RX " + irRemoteReceivePinsLabel();
    setStatus("IR ready", 700);
    return true;
  }
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
