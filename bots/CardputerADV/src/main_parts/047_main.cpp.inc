bool isSettingsMode(UiMode mode) {
  return mode == UiMode::Settings || mode == UiMode::FocusSettings || mode == UiMode::ScreenSettings;
}

uint8_t currentSettingsItemCount() {
  if (gUiMode == UiMode::FocusSettings) {
    return kFocusSettingsItemCount;
  }
  if (gUiMode == UiMode::ScreenSettings) {
    return kScreenSettingsItemCount;
  }
  return kDeviceSettingsItemCount;
}

const char* currentSettingsTitle() {
  if (gUiMode == UiMode::FocusSettings) {
    return "Pomodoro Settings";
  }
  if (gUiMode == UiMode::ScreenSettings) {
    return "Screensaver";
  }
  return "Device Settings";
}

uint32_t secondsWithDelta(uint32_t value, int32_t deltaSec, uint32_t minValue, uint32_t maxValue) {
  int64_t next = static_cast<int64_t>(value) + static_cast<int64_t>(deltaSec);
  if (next < static_cast<int64_t>(minValue)) {
    return minValue;
  }
  if (next > static_cast<int64_t>(maxValue)) {
    return maxValue;
  }
  return static_cast<uint32_t>(next);
}

void cycleAudioLanguage(int delta) {
  static const char* langs[] = {"ru", "en", "es"};
  constexpr int langCount = sizeof(langs) / sizeof(langs[0]);
  int current = 0;
  for (int i = 0; i < langCount; ++i) {
    if (gDeviceSettings.audioLanguage == langs[i]) {
      current = i;
      break;
    }
  }
  int next = (current + delta + langCount) % langCount;
  gDeviceSettings.audioLanguage = langs[next];
}

void adjustCurrentSetting(int delta) {
  if (gUiMode == UiMode::FocusSettings) {
    switch (gSelectedSetting) {
      case 0:
        gFocusSettings.focusSec = secondsWithDelta(gFocusSettings.focusSec, delta * 5L * 60L,
                                                   kFocusMinDurationSec, kFocusMaxDurationSec);
        break;
      case 1:
        gFocusSettings.shortBreakSec = secondsWithDelta(gFocusSettings.shortBreakSec, delta * 60L,
                                                        kFocusMinDurationSec, kFocusMaxDurationSec);
        break;
      case 2:
        gFocusSettings.longBreakSec = secondsWithDelta(gFocusSettings.longBreakSec, delta * 5L * 60L,
                                                       kFocusMinDurationSec, kFocusMaxDurationSec);
        break;
      case 3:
        gFocusSettings.cyclesPerRound = static_cast<uint8_t>(clampValue<int>(gFocusSettings.cyclesPerRound + delta, 1, 9));
        break;
      case 6:
        gFocusSettings.bpm = static_cast<uint16_t>(clampValue<int>(gFocusSettings.bpm + delta * 5, kFocusMinBpm, kFocusMaxBpm));
        break;
      default:
        break;
    }
    saveFocusSettings();
    return;
  }

  if (gUiMode == UiMode::ScreenSettings) {
    switch (gSelectedSetting) {
      case 0:
        gDeviceSettings.displayBrightnessPct = static_cast<uint8_t>(clampValue<int>(gDeviceSettings.displayBrightnessPct + delta * 5, 0, 100));
        applyDisplayBrightness();
        break;
      case 2:
        gDeviceSettings.screensaverTimeoutMin = static_cast<uint8_t>(clampValue<int>(gDeviceSettings.screensaverTimeoutMin + delta, 1, 60));
        break;
      case 3:
        gDeviceSettings.screensaverDimAfterMin = static_cast<uint8_t>(clampValue<int>(gDeviceSettings.screensaverDimAfterMin + delta * 5, 1, 120));
        break;
      case 4:
        gDeviceSettings.screensaverDimBrightnessPct = static_cast<uint8_t>(clampValue<int>(gDeviceSettings.screensaverDimBrightnessPct + delta * 5, 0, 100));
        applyDisplayBrightness();
        break;
      case 6:
        gDeviceSettings.screenOffAfterMin = static_cast<uint8_t>(clampValue<int>(gDeviceSettings.screenOffAfterMin + delta * 5, 5, 240));
        break;
      default:
        break;
    }
    saveDeviceSettings();
    return;
  }

  switch (gSelectedSetting) {
    case 0:
      cycleAudioLanguage(delta >= 0 ? 1 : -1);
      break;
    case 1:
      gDeviceSettings.notificationVolumePct = static_cast<uint8_t>(clampValue<int>(gDeviceSettings.notificationVolumePct + delta * 5, 0, 100));
      applySpeakerVolumeSetting();
      break;
    case 4:
      gDeviceSettings.minBatteryForUpdate = static_cast<uint8_t>(clampValue<int>(gDeviceSettings.minBatteryForUpdate + delta * 5, 5, 95));
      break;
    case 5:
      gDeviceSettings.clockFace = static_cast<uint8_t>((gDeviceSettings.clockFace + kClockFaceCount + (delta >= 0 ? 1 : -1)) % kClockFaceCount);
      break;
    default:
      break;
  }
  saveDeviceSettings();
}

void toggleCurrentSetting() {
  if (gUiMode == UiMode::FocusSettings) {
    switch (gSelectedSetting) {
      case 4:
        gFocusSettings.autoStart = !gFocusSettings.autoStart;
        break;
      case 5:
        gFocusSettings.metronome = !gFocusSettings.metronome;
        break;
      default:
        break;
    }
    saveFocusSettings();
    return;
  }

  if (gUiMode == UiMode::ScreenSettings) {
    switch (gSelectedSetting) {
      case 1:
        gDeviceSettings.screensaverEnabled = !gDeviceSettings.screensaverEnabled;
        break;
      case 5:
        gDeviceSettings.screenAutoOffEnabled = !gDeviceSettings.screenAutoOffEnabled;
        if (!gDeviceSettings.screenAutoOffEnabled && gScreenOffActive) {
          gScreenOffActive = false;
          applyDisplayBrightness();
        }
        break;
      case 7:
        gDeviceSettings.screensaverClockOnWake = !gDeviceSettings.screensaverClockOnWake;
        break;
      default:
        break;
    }
    saveDeviceSettings();
    return;
  }

  switch (gSelectedSetting) {
    case 0:
      cycleAudioLanguage(1);
      break;
    case 2:
      gDeviceSettings.autoUpdateFirmware = !gDeviceSettings.autoUpdateFirmware;
      break;
    case 3:
      gDeviceSettings.checkUpdatesOnBoot = !gDeviceSettings.checkUpdatesOnBoot;
      break;
    case 5:
      gDeviceSettings.clockFace = static_cast<uint8_t>((gDeviceSettings.clockFace + 1) % kClockFaceCount);
      break;
    case 6:
      gDeviceSettings.beepOnUpdate = !gDeviceSettings.beepOnUpdate;
      break;
    default:
      break;
  }
  saveDeviceSettings();
}

bool handleSettingsKey(char key) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  uint8_t count = currentSettingsItemCount();
  if (count == 0) {
    return false;
  }
  if (key == ',' || key == ';') {
    gSelectedSetting = (gSelectedSetting + count - 1) % count;
    return true;
  }
  if (key == '.' || key == '/') {
    gSelectedSetting = (gSelectedSetting + 1) % count;
    return true;
  }
  if (lowerKey == 'r') {
    loadDeviceSettings();
    loadFocusSettings();
    applyDisplayBrightness();
    applySpeakerVolumeSetting();
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
    adjustCurrentSetting(-1);
    setStatus("Setting -", 500);
    return true;
  }
  if (key == '+' || key == '=' || lowerKey == 'x') {
    adjustCurrentSetting(1);
    setStatus("Setting +", 500);
    return true;
  }
  if (key == ' ' || key == '\n' || key == '\r' || lowerKey == 't') {
    toggleCurrentSetting();
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

String rfidHexByte(uint8_t value) {
  char buf[3];
  snprintf(buf, sizeof(buf), "%02X", static_cast<unsigned>(value));
  return String(buf);
}

String rfidVersionName(uint8_t version) {
  if (version == 0x90) {
    return "MFRC522 v0.0";
  }
  if (version == 0x91) {
    return "MFRC522 v1.0";
  }
  if (version == 0x92) {
    return "MFRC522 v2.0";
  }
  if (version == 0x88) {
    return "FM17522 clone";
  }
  if (version == 0x00 || version == 0xFF) {
    return "no chip response";
  }
  return String("ver 0x") + rfidHexByte(version);
}

bool rfidIsClassicType(uint8_t piccType) {
  return piccType == MFRC522_I2C::PICC_TYPE_MIFARE_MINI ||
         piccType == MFRC522_I2C::PICC_TYPE_MIFARE_1K ||
         piccType == MFRC522_I2C::PICC_TYPE_MIFARE_4K;
}

uint16_t rfidClassicBlockCount(uint8_t piccType) {
  if (piccType == MFRC522_I2C::PICC_TYPE_MIFARE_MINI) {
    return 20;
  }
  if (piccType == MFRC522_I2C::PICC_TYPE_MIFARE_1K) {
    return 64;
  }
  if (piccType == MFRC522_I2C::PICC_TYPE_MIFARE_4K) {
    return 256;
  }
  return 0;
}

uint16_t rfidNextSectorFirstBlock(uint16_t firstBlock) {
  if (firstBlock < 128) {
    return static_cast<uint16_t>(firstBlock + 4);
  }
  return static_cast<uint16_t>(firstBlock + 16);
}

bool rfidIsSectorTrailer(uint16_t blockAddr) {
  if (blockAddr < 128) {
    return (blockAddr % 4) == 3;
  }
  return ((blockAddr - 128) % 16) == 15;
}

void rfidDefaultKey(MFRC522_I2C::MIFARE_Key& key) {
  for (uint8_t i = 0; i < MFRC522_I2C::MF_KEY_SIZE; ++i) {
    key.keyByte[i] = 0xFF;
  }
}

void resetRfidDump() {
  gRfidDump.valid = false;
  gRfidDump.piccType = 0;
  gRfidDump.uidSize = 0;
  gRfidDump.sak = 0;
  gRfidDump.blockCount = 0;
  gRfidDump.readableBlocks = 0;
  memset(gRfidDump.uid, 0, sizeof(gRfidDump.uid));
  memset(gRfidDump.blockValid, 0, sizeof(gRfidDump.blockValid));
  memset(gRfidDump.data, 0, sizeof(gRfidDump.data));
  gRfidDump.typeName = "";
  gRfidCopyStatus = "No dump";
}

String rfidUidString(const uint8_t* uid, uint8_t size) {
  String out;
  for (uint8_t i = 0; i < size; ++i) {
    if (i > 0) {
      out += ":";
    }
    out += rfidHexByte(uid[i]);
  }
  return out;
}

bool rfidSelectPresentedCard(String& error) {
  error = "";
  if (!gRfidReady) {
    error = "RFID not ready";
    return false;
  }

  byte atqa[2];
  byte atqaSize = sizeof(atqa);
  byte wake = gRfidReader.PICC_WakeupA(atqa, &atqaSize);
  if (wake != MFRC522_I2C::STATUS_OK && wake != MFRC522_I2C::STATUS_COLLISION) {
    if (!gRfidReader.PICC_IsNewCardPresent()) {
      error = "Present card";
      return false;
    }
  }

  if (!gRfidReader.PICC_ReadCardSerial()) {
    error = "Card select failed";
    return false;
  }
  return true;
}

void updateRfidSnapshotFromSelectedCard(uint8_t piccType, const String& statusText) {
  gRfidUid = rfidUidString(gRfidReader.uid.uidByte, gRfidReader.uid.size);
  gRfidUidSize = gRfidReader.uid.size;
  gRfidSak = gRfidReader.uid.sak;
  gRfidType = String(gRfidReader.PICC_GetTypeName(piccType));
  gRfidStatus = statusText;
}

bool copyRfidClassicDump() {
  String error;
  if (!rfidSelectPresentedCard(error)) {
    gRfidStatus = error;
    return false;
  }

  uint8_t piccType = gRfidReader.PICC_GetType(gRfidReader.uid.sak);
  updateRfidSnapshotFromSelectedCard(piccType, "Copying...");
  if (!rfidIsClassicType(piccType)) {
    gRfidStatus = "Classic only; no dump";
    gRfidCopyStatus = "Unsupported card";
    gRfidReader.PICC_HaltA();
    gRfidReader.PCD_StopCrypto1();
    appendRuntimeLog("RFID", "copy unsupported type=" + gRfidType, false);
    return false;
  }

  resetRfidDump();
  gRfidDump.piccType = piccType;
  gRfidDump.uidSize = min<uint8_t>(gRfidReader.uid.size, sizeof(gRfidDump.uid));
  gRfidDump.sak = gRfidReader.uid.sak;
  gRfidDump.blockCount = rfidClassicBlockCount(piccType);
  gRfidDump.typeName = String(gRfidReader.PICC_GetTypeName(piccType));
  memcpy(gRfidDump.uid, gRfidReader.uid.uidByte, gRfidDump.uidSize);

  MFRC522_I2C::MIFARE_Key key;
  rfidDefaultKey(key);
  uint16_t readBlocks = 0;
  uint16_t failedBlocks = 0;
  byte buffer[18];

  for (uint16_t firstBlock = 0; firstBlock < gRfidDump.blockCount; firstBlock = rfidNextSectorFirstBlock(firstBlock)) {
    uint16_t nextFirst = min<uint16_t>(rfidNextSectorFirstBlock(firstBlock), gRfidDump.blockCount);
    byte auth = gRfidReader.PCD_Authenticate(MFRC522_I2C::PICC_CMD_MF_AUTH_KEY_A,
                                             static_cast<byte>(firstBlock), &key, &gRfidReader.uid);
    if (auth != MFRC522_I2C::STATUS_OK) {
      failedBlocks = static_cast<uint16_t>(failedBlocks + (nextFirst - firstBlock));
      gRfidReader.PCD_StopCrypto1();
      continue;
    }

    for (uint16_t block = firstBlock; block < nextFirst; ++block) {
      byte size = sizeof(buffer);
      byte status = gRfidReader.MIFARE_Read(static_cast<byte>(block), buffer, &size);
      if (status == MFRC522_I2C::STATUS_OK && size >= kRfidBlockBytes) {
        memcpy(gRfidDump.data[block], buffer, kRfidBlockBytes);
        gRfidDump.blockValid[block] = true;
        ++readBlocks;
      } else {
        ++failedBlocks;
      }
    }
    gRfidReader.PCD_StopCrypto1();
  }

  gRfidReader.PICC_HaltA();
  gRfidReader.PCD_StopCrypto1();
  gRfidDump.readableBlocks = readBlocks;
  gRfidDump.valid = readBlocks > 0;
  gRfidUid = rfidUidString(gRfidDump.uid, gRfidDump.uidSize);
  gRfidType = gRfidDump.typeName;
  gRfidUidSize = gRfidDump.uidSize;
  gRfidSak = gRfidDump.sak;

  if (gRfidDump.valid) {
    gRfidCopyStatus = String("Dump ") + String(readBlocks) + "/" + String(gRfidDump.blockCount) + " blocks";
    gRfidStatus = failedBlocks == 0 ? "Copy ready; W writes" : "Partial copy; W writes";
  } else {
    gRfidCopyStatus = "Copy failed";
    gRfidStatus = "Copy failed: key?";
  }
  appendRuntimeLog("RFID", String("copy blocks=") + String(readBlocks) + "/" + String(gRfidDump.blockCount) +
                               " failed=" + String(failedBlocks) + " type=" + gRfidDump.typeName,
                   false);
  return gRfidDump.valid;
}

bool writeRfidClassicDumpToTarget() {
  if (!gRfidDump.valid || gRfidDump.blockCount == 0) {
    gRfidStatus = "No dump: press D first";
    gRfidCopyStatus = "No dump";
    return false;
  }

  String error;
  if (!rfidSelectPresentedCard(error)) {
    gRfidStatus = error;
    return false;
  }

  uint8_t targetType = gRfidReader.PICC_GetType(gRfidReader.uid.sak);
  updateRfidSnapshotFromSelectedCard(targetType, "Writing...");
  uint16_t targetBlocks = rfidClassicBlockCount(targetType);
  if (!rfidIsClassicType(targetType) || targetBlocks < gRfidDump.blockCount) {
    gRfidStatus = "Need Classic blank";
    gRfidCopyStatus = "Target too small";
    gRfidReader.PICC_HaltA();
    gRfidReader.PCD_StopCrypto1();
    appendRuntimeLog("RFID", String("write target incompatible type=") + gRfidType +
                                 " blocks=" + String(targetBlocks), false);
    return false;
  }

  MFRC522_I2C::MIFARE_Key key;
  rfidDefaultKey(key);
  uint16_t writtenBlocks = 0;
  uint16_t skippedBlocks = 0;
  uint16_t failedBlocks = 0;

  for (uint16_t firstBlock = 0; firstBlock < gRfidDump.blockCount; firstBlock = rfidNextSectorFirstBlock(firstBlock)) {
    uint16_t nextFirst = min<uint16_t>(rfidNextSectorFirstBlock(firstBlock), gRfidDump.blockCount);
    byte auth = gRfidReader.PCD_Authenticate(MFRC522_I2C::PICC_CMD_MF_AUTH_KEY_A,
                                             static_cast<byte>(firstBlock), &key, &gRfidReader.uid);
    if (auth != MFRC522_I2C::STATUS_OK) {
      failedBlocks = static_cast<uint16_t>(failedBlocks + (nextFirst - firstBlock));
      gRfidReader.PCD_StopCrypto1();
      continue;
    }

    for (uint16_t block = firstBlock; block < nextFirst; ++block) {
      if (!gRfidDump.blockValid[block]) {
        ++skippedBlocks;
        continue;
      }
      if (block == 0 || rfidIsSectorTrailer(block)) {
        ++skippedBlocks;
        continue;
      }
      byte status = gRfidReader.MIFARE_Write(static_cast<byte>(block), gRfidDump.data[block], kRfidBlockBytes);
      if (status == MFRC522_I2C::STATUS_OK) {
        ++writtenBlocks;
      } else {
        ++failedBlocks;
      }
    }
    gRfidReader.PCD_StopCrypto1();
  }

  gRfidLastUidWriteOk = false;
  if (gRfidDump.uidSize > 0) {
    gRfidLastUidWriteOk = gRfidReader.MIFARE_SetUid(gRfidDump.uid, gRfidDump.uidSize, false);
    gRfidReader.PCD_StopCrypto1();
  }

  gRfidReader.PICC_HaltA();
  gRfidReader.PCD_StopCrypto1();
  gRfidLastWriteBlocks = writtenBlocks;

  String uidNote = gRfidLastUidWriteOk ? "UID ok" : "UID std";
  if (writtenBlocks > 0 && failedBlocks == 0) {
    gRfidStatus = String("Written; ") + uidNote;
  } else if (writtenBlocks > 0) {
    gRfidStatus = String("Partial write; ") + uidNote;
  } else {
    gRfidStatus = String("Write failed; ") + uidNote;
  }
  gRfidCopyStatus = String("Wrote ") + String(writtenBlocks) + " data, skip " + String(skippedBlocks);
  appendRuntimeLog("RFID", String("write data=") + String(writtenBlocks) + " failed=" + String(failedBlocks) +
                               " skipped=" + String(skippedBlocks) +
                               " uid=" + String(gRfidLastUidWriteOk ? "ok" : "not_writable"),
                   false);
  return writtenBlocks > 0 && failedBlocks == 0;
}

bool rfidI2cAddressPresent() {
  if (!gRfidWireReady) {
    return false;
  }
  Wire.beginTransmission(kRfidI2cAddr);
  return Wire.endTransmission() == 0;
}

void clearRfidCardSnapshot(const String& statusText = "Ready: present card") {
  gRfidUid = "-";
  gRfidType = "-";
  gRfidUidSize = 0;
  gRfidSak = 0;
  gRfidStatus = statusText;
}

void initRfidLab(bool force) {
  uint32_t now = millis();
  if (!force && gRfidReady) {
    return;
  }
  if (!force && gRfidLastInitMs != 0 && now - gRfidLastInitMs < kRfidReconnectIntervalMs) {
    return;
  }
  gRfidLastInitMs = now;
  Wire.begin(kRfidSdaPin, kRfidSclPin);
  Wire.setClock(kRfidI2cHz);
  Wire.setTimeOut(50);
  gRfidWireReady = true;

  if (!rfidI2cAddressPresent()) {
    gRfidReady = false;
    gRfidVersionReg = 0;
    clearRfidCardSnapshot("No RFID unit at 0x28");
    return;
  }

  gRfidReader.PCD_Init();
  delay(8);
  gRfidVersionReg = gRfidReader.PCD_ReadRegister(MFRC522_I2C::VersionReg);
  if (gRfidVersionReg == 0x00 || gRfidVersionReg == 0xFF) {
    gRfidReady = false;
    clearRfidCardSnapshot("RFID chip not responding");
    appendRuntimeLog("RFID", "unit addr ack but chip version invalid", true);
    return;
  }

  gRfidReady = true;
  clearRfidCardSnapshot("Ready: present card");
  appendRuntimeLog("RFID", String("ready ") + rfidVersionName(gRfidVersionReg), true);
}

bool readRfidCardSnapshot() {
  if (!gRfidReady) {
    return false;
  }
  if (!gRfidReader.PICC_IsNewCardPresent()) {
    return false;
  }
  if (!gRfidReader.PICC_ReadCardSerial()) {
    gRfidStatus = "Card select failed";
    return false;
  }

  uint8_t piccType = gRfidReader.PICC_GetType(gRfidReader.uid.sak);
  updateRfidSnapshotFromSelectedCard(piccType, "Card read; D copy W write");
  gRfidReader.PICC_HaltA();
  gRfidReader.PCD_StopCrypto1();
  appendRuntimeLog("RFID", String("card type=") + gRfidType + " uid_bytes=" + String(gRfidUidSize), false);
  return true;
}

void serviceRfidLab() {
  if (gUiMode != UiMode::RfidLab) {
    return;
  }
  if (!gRfidReady) {
    initRfidLab(false);
    return;
  }
  uint32_t now = millis();
  if (now - gRfidLastPollMs < kRfidPollIntervalMs) {
    return;
  }
  gRfidLastPollMs = now;
  if (readRfidCardSnapshot()) {
    setStatus("RFID card read", 900);
  }
}

bool handleRfidLabKey(char key, const Keyboard_Class::KeysState& keys) {
  (void)keys;
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'r') {
    gRfidReady = false;
    initRfidLab(true);
    setStatus(gRfidReady ? "RFID reconnected" : "RFID not found", 900);
    return true;
  }
  if (lowerKey == 'c') {
    clearRfidCardSnapshot(gRfidReady ? "Ready: present card" : "No RFID unit at 0x28");
    setStatus("RFID cleared", 700);
    return true;
  }
  if (lowerKey == 'd') {
    bool ok = copyRfidClassicDump();
    setStatus(ok ? "RFID copy ready" : "RFID copy failed", 1000);
    return true;
  }
  if (lowerKey == 'w') {
    bool ok = writeRfidClassicDumpToTarget();
    setStatus(ok ? "RFID write done" : "RFID write partial", 1200);
    return true;
  }
  if (key == '?' || lowerKey == 'h') {
    gRfidHelpVisible = !gRfidHelpVisible;
    setStatus(gRfidHelpVisible ? "RFID wiring help" : "RFID scan", 700);
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
  if (keys.tab || key == '\t' || lowerKey == '?') {
    gLauncherVisible = false;
    gTetrisHelpVisible = !gTetrisHelpVisible;
    setStatus(gTetrisHelpVisible ? "Tetris help" : "Tetris game", 700);
    return true;
  }
  if (lowerKey == 'g') {
    gTetrisRightHandPortrait = !gTetrisRightHandPortrait;
    gTetrisHelpVisible = false;
    gTetrisStatus = gTetrisRightHandPortrait ? "Right grip" : "Landscape";
    setStatus(gTetrisRightHandPortrait ? "Tetris right grip" : "Tetris landscape", 900);
    return true;
  }
  if (!gTetrisStarted || gTetrisGameOver) {
    if (keys.enter || keys.space || key == '\r' || key == '\n' || key == ' ' || lowerKey == 'r') {
      gTetrisHelpVisible = false;
      tetrisResetGame();
    } else {
      setStatus("Enter starts, Tab helps", 800);
    }
    return true;
  }
  if (gTetrisHelpVisible) {
    gTetrisHelpVisible = false;
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
  if (key == ',' || key == ';' || lowerKey == 'a' || lowerKey == 'h' || lowerKey == 'j') {
    tetrisMovePiece(-1, 0);
    return true;
  }
  if (key == '.' || key == '/' || lowerKey == 'd' || lowerKey == 'l') {
    tetrisMovePiece(1, 0);
    return true;
  }
  if (lowerKey == 'w' || lowerKey == 'i') {
    if (tetrisRotatePiece()) {
      gTetrisStatus = "Rotated";
    }
    return true;
  }
  if (lowerKey == 's' || lowerKey == 'k') {
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
constexpr uint8_t kIrRemoteSlotCarrier = 0;
constexpr uint8_t kIrRemoteSlotSamsung = 1;
constexpr uint8_t kIrRemotePresetCount = 2;

// Samsung AA59/A59 TV family. Public databases list these as Samsung 32-bit
// frames like 0xE0E040BF; IRremote sends them as address 0x0707 + LSB command.
constexpr uint16_t kSamsungTvAddress = 0x0707;
constexpr uint8_t kSamsungPowerToggle = 0x02;
constexpr uint8_t kSamsungPowerOn = 0x99;
constexpr uint8_t kSamsungPowerOff = 0x98;
constexpr uint8_t kSamsungSource = 0x01;
constexpr uint8_t kSamsungHome = 0x79;
constexpr uint8_t kSamsungMenu = 0x1A;
constexpr uint8_t kSamsungGuide = 0x4F;
constexpr uint8_t kSamsungInfo = 0x1F;
constexpr uint8_t kSamsungTools = 0x4B;
constexpr uint8_t kSamsungReturn = 0x58;
constexpr uint8_t kSamsungExit = 0x2D;
constexpr uint8_t kSamsungUp = 0x60;
constexpr uint8_t kSamsungDown = 0x61;
constexpr uint8_t kSamsungLeft = 0x65;
constexpr uint8_t kSamsungRight = 0x62;
constexpr uint8_t kSamsungOk = 0x68;
constexpr uint8_t kSamsungMute = 0x0F;
constexpr uint8_t kSamsungVolUp = 0x07;
constexpr uint8_t kSamsungVolDown = 0x0B;
constexpr uint8_t kSamsungChUp = 0x12;
constexpr uint8_t kSamsungChDown = 0x10;

// Carrier WC-UA04NE / 43T6V694 family (device marking also reported as
// Q18D0167859) is sold as a Carrier/Toshiba split A/C remote.
// IRremoteESP8266 documents the matching Toshiba-Carrier A/C protocol
// for Carrier 42NQV/38NYV units and similar Toshiba remotes (WH-TA04NE etc.).
constexpr uint8_t kCarrierToshibaStateLength = 9;
constexpr uint8_t kCarrierToshibaStateLengthShort = 7;
constexpr uint8_t kCarrierToshibaStateLengthLong = 10;
constexpr uint8_t kCarrierToshibaMinLength = 6;
constexpr uint16_t kCarrierToshibaHdrMark = 4400;
constexpr uint16_t kCarrierToshibaHdrSpace = 4300;
constexpr uint16_t kCarrierToshibaBitMark = 580;
constexpr uint16_t kCarrierToshibaOneSpace = 1600;
constexpr uint16_t kCarrierToshibaZeroSpace = 490;
constexpr uint16_t kCarrierToshibaGap = 7400;
constexpr uint8_t kCarrierToshibaAuto = 0;
constexpr uint8_t kCarrierToshibaCool = 1;
constexpr uint8_t kCarrierToshibaDry = 2;
constexpr uint8_t kCarrierToshibaHeat = 3;
constexpr uint8_t kCarrierToshibaFan = 4;
constexpr uint8_t kCarrierToshibaOff = 7;
constexpr uint8_t kCarrierToshibaFanAuto = 0;
constexpr uint8_t kCarrierToshibaFanLow = 1;
constexpr uint8_t kCarrierToshibaFanMedium = 3;
constexpr uint8_t kCarrierToshibaFanHigh = 5;
constexpr uint8_t kCarrierToshibaSwingOn = 1;
constexpr uint8_t kCarrierToshibaSwingOff = 2;
constexpr uint8_t kCarrierToshibaTempMin = 17;
constexpr uint8_t kCarrierToshibaTempMax = 30;
constexpr uint8_t kCarrierToshibaEcoOn = 3;

bool gCarrierPower = false;
uint8_t gCarrierMode = kCarrierToshibaCool;
uint8_t gCarrierTempC = 24;
uint8_t gCarrierFan = kCarrierToshibaFanAuto;
bool gCarrierSwingV = false;
bool gCarrierEco = false;
String gCarrierLastCommand = "Ready";
uint8_t gSamsungLastCommand = kSamsungPowerToggle;
String gSamsungLastCommandLabel = "Ready";

uint8_t carrierToshibaChecksum(const uint8_t* state, uint8_t length) {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i + 1 < length; ++i) {
    checksum ^= state[i];
  }
  return checksum;
}

void carrierToshibaReset(uint8_t* state, uint8_t length) {
  memset(state, 0, kCarrierToshibaStateLengthLong);
  state[0] = 0xF2;
  state[1] = 0x0D;
  state[2] = static_cast<uint8_t>((length - kCarrierToshibaMinLength) & 0x0F);
  state[3] = static_cast<uint8_t>(~state[2]);
  state[4] = 0x01;
  if (length == kCarrierToshibaStateLengthShort) {
    state[4] |= 0x20;  // Short swing message.
  } else if (length == kCarrierToshibaStateLengthLong) {
    state[4] |= 0x08;  // Long Eco/Turbo message.
  }
}

uint8_t carrierToshibaFanRaw() {
  if (gCarrierFan == kCarrierToshibaFanAuto) {
    return kCarrierToshibaFanAuto;
  }
  return static_cast<uint8_t>(clampValue<int>(gCarrierFan, kCarrierToshibaFanLow, kCarrierToshibaFanHigh) + 1);
}

uint8_t carrierToshibaModeRaw() {
  return gCarrierPower ? gCarrierMode : kCarrierToshibaOff;
}

uint8_t carrierBuildToshibaState(uint8_t* state) {
  const uint8_t length = gCarrierEco ? kCarrierToshibaStateLengthLong : kCarrierToshibaStateLength;
  carrierToshibaReset(state, length);
  const uint8_t temp = static_cast<uint8_t>(clampValue<int>(gCarrierTempC, kCarrierToshibaTempMin, kCarrierToshibaTempMax));
  state[5] = static_cast<uint8_t>((temp - kCarrierToshibaTempMin) << 4);
  state[6] = static_cast<uint8_t>((carrierToshibaFanRaw() << 5) | (carrierToshibaModeRaw() & 0x07));
  state[7] = 0x00;
  if (length == kCarrierToshibaStateLengthLong) {
    state[8] = gCarrierEco ? kCarrierToshibaEcoOn : 0x00;
  }
  state[length - 1] = carrierToshibaChecksum(state, length);
  return length;
}

uint8_t carrierBuildToshibaSwingState(uint8_t* state) {
  const uint8_t length = kCarrierToshibaStateLengthShort;
  carrierToshibaReset(state, length);
  state[5] = gCarrierSwingV ? kCarrierToshibaSwingOn : kCarrierToshibaSwingOff;
  state[6] = carrierToshibaChecksum(state, length);
  return length;
}

void carrierSendToshibaMessage(const uint8_t* state, uint8_t length) {
  IrSender.enableIROut(38);
  IrSender.mark(kCarrierToshibaHdrMark);
  IrSender.space(kCarrierToshibaHdrSpace);
  for (uint8_t i = 0; i < length; ++i) {
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
      IrSender.mark(kCarrierToshibaBitMark);
      IrSender.space((state[i] & mask) ? kCarrierToshibaOneSpace : kCarrierToshibaZeroSpace);
    }
  }
  IrSender.mark(kCarrierToshibaBitMark);
  IrSender.space(kCarrierToshibaGap);
  IrSender.IRLedOff();
}

void carrierSendToshibaState(bool includeSwingFrame) {
  uint8_t state[kCarrierToshibaStateLengthLong];
  uint8_t length = carrierBuildToshibaState(state);
  carrierSendToshibaMessage(state, length);
  if (includeSwingFrame) {
    length = carrierBuildToshibaSwingState(state);
    carrierSendToshibaMessage(state, length);
  }
}
}  // namespace

const char* carrierModeName(uint8_t mode) {
  switch (mode) {
    case kCarrierToshibaAuto:
      return "Auto";
    case kCarrierToshibaCool:
      return "Cool";
    case kCarrierToshibaDry:
      return "Dry";
    case kCarrierToshibaFan:
      return "Fan";
    case kCarrierToshibaHeat:
      return "Heat";
    default:
      return "AC";
  }
}

String carrierFanName() {
  switch (gCarrierFan) {
    case kCarrierToshibaFanLow:
      return "Low";
    case kCarrierToshibaFanMedium:
      return "Med";
    case kCarrierToshibaFanHigh:
      return "High";
    default:
      return "Auto";
  }
}

String carrierStateSummary() {
  String state = String(gCarrierPower ? "ON" : "OFF") + " " + carrierModeName(gCarrierMode) + " " +
                 String(gCarrierTempC) + "C F" + carrierFanName();
  if (gCarrierSwingV) {
    state += " Swing";
  }
  if (gCarrierEco) {
    state += " Eco";
  }
  return state;
}

bool isSamsungRemoteSlot() {
  return gSelectedIrRemoteSlot == kIrRemoteSlotSamsung;
}

const char* irRemotePresetName(uint8_t slotIndex) {
  return slotIndex == kIrRemoteSlotSamsung ? "Samsung A59-00818A" : "Carrier WC-UA04NE";
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
  gSelectedIrRemoteSlot = kIrRemoteSlotCarrier;
  gIrRemoteStatus = "Slot 1 Carrier WC-UA04NE, Slot 2 Samsung A59-00818A";
  appendRuntimeLog("IR", gIrRemoteStatus, true);
}

void sendCarrierAcState(const String& commandLabel, bool includeSwingFrame = false) {
  initIrRemote();
  carrierSendToshibaState(includeSwingFrame);
  gCarrierLastCommand = commandLabel;
  gIrRemoteStatus = String("Carrier WC-UA04NE ") + commandLabel + ": " + carrierStateSummary();
  setStatus(gIrRemoteStatus, 1100);
  appendRuntimeLog("IR", gIrRemoteStatus, true);
  flashActivityLed(255, 80, 20, 120);
}

void setCarrierModeAndSend(uint8_t mode, const String& label) {
  gCarrierPower = true;
  gCarrierMode = mode;
  sendCarrierAcState(label);
}

void cycleCarrierFanAndSend() {
  gCarrierPower = true;
  static constexpr uint8_t kFanCycle[] = {kCarrierToshibaFanAuto, kCarrierToshibaFanLow, kCarrierToshibaFanMedium, kCarrierToshibaFanHigh};
  uint8_t next = 0;
  for (uint8_t i = 0; i < sizeof(kFanCycle); ++i) {
    if (gCarrierFan == kFanCycle[i]) {
      next = static_cast<uint8_t>((i + 1) % sizeof(kFanCycle));
      break;
    }
  }
  gCarrierFan = kFanCycle[next];
  sendCarrierAcState(String("Fan ") + carrierFanName());
}

void sendSamsungTvCommand(uint8_t command, const String& label) {
  initIrRemote();
  IrSender.sendSamsung(kSamsungTvAddress, command, NO_REPEATS);
  IrSender.IRLedOff();
  gSamsungLastCommand = command;
  gSamsungLastCommandLabel = label;
  gIrRemoteStatus = String("Samsung A59 ") + label;
  setStatus(gIrRemoteStatus, 950);
  appendRuntimeLog("IR", gIrRemoteStatus, true);
  flashActivityLed(80, 190, 255, 110);
}

bool handleSamsungTvRemoteKey(char key, char lowerKey, const Keyboard_Class::KeysState& keys) {
  if (lowerKey == 'q') {
    sendSamsungTvCommand(kSamsungPowerToggle, "Power");
    return true;
  }
  if (lowerKey == 'w') {
    sendSamsungTvCommand(kSamsungUp, "Up");
    return true;
  }
  if (lowerKey == 'a') {
    sendSamsungTvCommand(kSamsungLeft, "Left");
    return true;
  }
  if (lowerKey == 's') {
    sendSamsungTvCommand(kSamsungDown, "Down");
    return true;
  }
  if (lowerKey == 'd') {
    sendSamsungTvCommand(kSamsungRight, "Right");
    return true;
  }
  if (keys.enter || keys.space || key == ' ' || key == '\r' || key == '\n') {
    sendSamsungTvCommand(kSamsungOk, "OK");
    return true;
  }
  if (lowerKey == 'e') {
    sendSamsungTvCommand(kSamsungSource, "Source");
    return true;
  }
  if (lowerKey == 'r') {
    sendSamsungTvCommand(kSamsungReturn, "Return");
    return true;
  }
  if (lowerKey == 't') {
    sendSamsungTvCommand(kSamsungHome, "Home");
    return true;
  }
  if (lowerKey == 'y') {
    sendSamsungTvCommand(kSamsungMenu, "Menu");
    return true;
  }
  if (lowerKey == 'f') {
    sendSamsungTvCommand(kSamsungMute, "Mute");
    return true;
  }
  if (lowerKey == 'g') {
    sendSamsungTvCommand(kSamsungChDown, "Ch -");
    return true;
  }
  if (lowerKey == 'h') {
    sendSamsungTvCommand(kSamsungChUp, "Ch +");
    return true;
  }
  if (lowerKey == 'z' || key == '-' || key == '_') {
    sendSamsungTvCommand(kSamsungVolDown, "Vol -");
    return true;
  }
  if (lowerKey == 'x' || key == '+' || key == '=') {
    sendSamsungTvCommand(kSamsungVolUp, "Vol +");
    return true;
  }
  if (lowerKey == 'c') {
    sendSamsungTvCommand(kSamsungExit, "Exit");
    return true;
  }
  if (lowerKey == 'v') {
    sendSamsungTvCommand(kSamsungGuide, "Guide");
    return true;
  }
  if (lowerKey == 'b') {
    sendSamsungTvCommand(kSamsungInfo, "Info");
    return true;
  }
  if (lowerKey == 'n') {
    sendSamsungTvCommand(kSamsungTools, "Tools");
    return true;
  }
  if (keys.del || key == '\b') {
    sendSamsungTvCommand(kSamsungPowerOff, "Power off");
    return true;
  }
  if (lowerKey == 'o') {
    sendSamsungTvCommand(kSamsungPowerOn, "Power on");
    return true;
  }
  if (lowerKey == 'p') {
    sendSamsungTvCommand(kSamsungPowerOff, "Power off");
    return true;
  }
  gIrRemoteStatus = "Samsung keys: WASD nav, Enter OK, Z/X volume";
  setStatus("Samsung: WASD Enter Z/X E/R/T/Y", 1200);
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

  if (isSamsungRemoteSlot()) {
    return handleSamsungTvRemoteKey(key, lowerKey, keys);
  }

  if (lowerKey == 'q') {
    gCarrierPower = !gCarrierPower;
    sendCarrierAcState(gCarrierPower ? "Power on" : "Power off");
    return true;
  }
  if (lowerKey == 'w') {
    setCarrierModeAndSend(kCarrierToshibaCool, "Cool");
    return true;
  }
  if (lowerKey == 'e') {
    setCarrierModeAndSend(kCarrierToshibaFan, "Fan only");
    return true;
  }
  if (lowerKey == 'r') {
    setCarrierModeAndSend(kCarrierToshibaDry, "Dry");
    return true;
  }
  if (lowerKey == 't') {
    setCarrierModeAndSend(kCarrierToshibaHeat, "Heat");
    return true;
  }
  if (lowerKey == 'y') {
    setCarrierModeAndSend(kCarrierToshibaAuto, "Auto");
    return true;
  }
  if (lowerKey == 'a' || key == '-' || key == '_') {
    gCarrierPower = true;
    gCarrierTempC = static_cast<uint8_t>(clampValue<int>(gCarrierTempC - 1, kCarrierToshibaTempMin, kCarrierToshibaTempMax));
    sendCarrierAcState("Temp -");
    return true;
  }
  if (lowerKey == 's' || key == '+' || key == '=') {
    gCarrierPower = true;
    gCarrierTempC = static_cast<uint8_t>(clampValue<int>(gCarrierTempC + 1, kCarrierToshibaTempMin, kCarrierToshibaTempMax));
    sendCarrierAcState("Temp +");
    return true;
  }
  if (lowerKey == 'd') {
    cycleCarrierFanAndSend();
    return true;
  }
  if (lowerKey == 'f') {
    gCarrierPower = true;
    gCarrierSwingV = !gCarrierSwingV;
    sendCarrierAcState(gCarrierSwingV ? "Swing on" : "Swing off", true);
    return true;
  }
  if (lowerKey == 'g') {
    gCarrierPower = true;
    gCarrierEco = !gCarrierEco;
    sendCarrierAcState(gCarrierEco ? "Eco on" : "Eco off");
    return true;
  }
  if (lowerKey == 'o') {
    gCarrierPower = true;
    sendCarrierAcState("Power on");
    return true;
  }
  if (lowerKey == 'p' || keys.del || key == '\b') {
    gCarrierPower = false;
    sendCarrierAcState("Power off");
    return true;
  }
  if (keys.enter || keys.space || key == ' ' || key == '\r' || key == '\n' || lowerKey == 'h') {
    sendCarrierAcState("Resend");
    return true;
  }

  gIrRemoteStatus = "Carrier WC-UA04NE keys: Q power, W/E/R/T/Y mode";
  setStatus("WC-UA04NE: Q W/E/R/T/Y A/S D/F/G", 1200);
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
  if (gTetrisHelpVisible) {
    gTetrisHelpVisible = false;
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
