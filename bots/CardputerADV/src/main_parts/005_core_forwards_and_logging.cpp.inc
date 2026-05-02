void startRealtimeSession(bool startAfterReady);
void startRealtimeRecording();
void stopRealtimeRecordingAndCommit();
void closeRealtimeSession(const String& reason);
void realtimeWsEvent(WStype_t type, uint8_t* payload, size_t length);
void updateG0VoiceTrigger();
void servicePlaybackStream();
bool handleGlobalEscape();
void loadFocusSettings();
void saveFocusSettings();
void loadDeviceSettings();
void saveDeviceSettings();
void loadPetState();
void savePetState(bool force = false);
void resetPet(bool fullReset);
void revivePet(const char* reason = "manual");
void servicePet();
bool handlePetKey(char key, const Keyboard_Class::KeysState& keys);
bool applyPetCare(PetCareAction action);
void setUiMode(UiMode mode);
bool focusStateRuns(FocusState state);
void focusStartOrResume();
void focusReset();
void updateFocusTimer();
void serviceFocusMetronome();
void focusTone(uint16_t freq, uint16_t durationMs);
void updateBatterySnapshot(bool force = false);
void scanAudioAssets();
bool startPlaybackStreamFromWavFile(const String& path, const String& diag);
const char* clockFaceName(uint8_t face);
void loadAssetManifestSummary();
void loadOtaManifestSummary();
void loadConversationContexts();
void scanNotes();
bool handleNotesKey(char key, const Keyboard_Class::KeysState& keys);
String currentConversationKey();
bool fetchRemoteTopicCatalog();
bool fetchSelectedTopicHistory();
bool selectRemoteCurrentTopic();
bool createRemoteTopic(const String& title, String& createdTopicKey);
bool switchTopicRelative(int delta, bool selectAndLoad);
bool startTopicTask(bool syncCatalog, bool selectCurrent, bool loadHistory, const char* statusText = nullptr,
                    bool createTopic = false, const String& createTitle = String());
void processCompletedTopicTask();
void showTopicOverlay(int8_t direction = 0, uint32_t ttlMs = 1600);
void armTopicCreate();
String defaultNewTopicTitle();
void togglePulseChatDisplay();
bool isEmojiAssetCodepoint(uint32_t codepoint);
bool decodeUtf8At(const String& value, int index, uint32_t& codepoint, int& consumed);
String normalizeEmojiDisplayText(const String& raw);
bool emojiAssetAvailable(uint32_t codepoint);
const EmojiCacheEntry* emojiCacheLookup(uint32_t codepoint);
String normalizeTopicTitleForDisplay(const String& raw, bool keepAvailableEmoji = true);
String makeTopicShortName(const String& title, int32_t threadId);
bool fetchRemoteAssetManifest();
bool fetchRemoteOtaManifest();
bool syncAssetsFromManifest();
String fitCurrentFontToWidth(String text, int32_t pixelLimit);
void drawTinyFooter(uint16_t bg, uint16_t fg, const String& text, int16_t x = 12, int16_t y = 124,
                    int16_t w = 216);
String compactBytes(uint32_t bytes);
bool applyOtaFromManifest();
void beginOtaBootGuard();
bool confirmOtaBootIfPending(const char* reason);
void rollbackPendingOta(const char* reason);
void serviceOtaBootGuard();
bool uploadRuntimeLogs();
void processCompletedTextTurn();
void maybeStartNextTextTurn();
void textTurnTask(void* arg);
bool hubUsesTls();
String hubBaseUrl();
fs::FS* activeVoiceFs();
fs::FS* fallbackVoiceFs();
bool ensurePomodoroStorageOn(fs::FS& fs);
uint64_t activeStorageTotalBytes();
uint64_t activeStorageUsedBytes();
void configureEmojiRendering();
void scanEmojiAssets();
bool executeDeviceActions(JsonVariantConst actions);
bool executeDeviceActionsJson(const String& json);
bool executeDeviceAction(JsonObjectConst action);

void mapTcaRawKeyToPhysical(uint8_t keyValue, uint8_t& row, uint8_t& col) {
  uint8_t unit = keyValue % 10;
  uint8_t tens = keyValue / 10;
  if (unit >= 1 && unit <= 8 && tens <= 6) {
    uint8_t unitZero = unit - 1;
    row = unitZero & 0x03;
    col = (tens << 1) | (unitZero >> 2);
    return;
  }
  row = 0xFF;
  col = 0xFF;
}

template <typename T>
T clampValue(T value, T minValue, T maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

String jsonEscapeLine(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\r", " ");
  value.replace("\n", " ");
  return value;
}

void appendRuntimeLog(const String& area, const String& message, bool persist = false) {
  String compact = String(millis() / 1000) + " " + area + " " + message;
  while (compact.length() > 96) {
    compact.remove(compact.length() - 1);
  }
  gRuntimeLogs.push_back(compact);
  while (gRuntimeLogs.size() > kMaxRuntimeLogLines) {
    gRuntimeLogs.pop_front();
  }
  if (!persist || !gStorageReady) {
    return;
  }
  fs::FS* fs = activeVoiceFs();
  if (!fs || !ensurePomodoroStorageOn(*fs)) {
    return;
  }
  File out = fs->open(kRuntimeLogPath, FILE_APPEND);
  if (!out) {
    return;
  }
  out.print("{\"t\":");
  out.print(millis());
  out.print(",\"area\":\"");
  out.print(jsonEscapeLine(area));
  out.print("\",\"msg\":\"");
  out.print(jsonEscapeLine(message));
  out.println("\"}");
  out.close();
}

void logf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
  appendRuntimeLog("SER", String(buf), false);
}

String chipIdSuffix() {
  uint64_t chip = ESP.getEfuseMac();
  char buf[13];
  snprintf(buf, sizeof(buf), "%06llx", (unsigned long long)(chip & 0xFFFFFFULL));
  return String(buf);
}

uint16_t compileTimeHubPort() {
  const char* raw = DEFAULT_HUB_PORT_STR;
  if (!raw || !raw[0]) {
    return kDefaultHubPort;
  }
  long parsed = strtol(raw, nullptr, 10);
  if (parsed <= 0 || parsed > 65535) {
    return kDefaultHubPort;
  }
  return static_cast<uint16_t>(parsed);
}

bool isLocalOrPrivateBridgeHost(const String& host) {
  if (host == "localhost" || host == "127.0.0.1" || host.startsWith("192.168.") || host.startsWith("10.")) {
    return true;
  }
  if (!host.startsWith("172.")) {
    return false;
  }
  int firstDot = host.indexOf('.');
  int secondDot = firstDot >= 0 ? host.indexOf('.', firstDot + 1) : -1;
  if (firstDot < 0 || secondDot < 0) {
    return false;
  }
  int secondOctet = host.substring(firstDot + 1, secondDot).toInt();
  return secondOctet >= 16 && secondOctet <= 31;
}

void migrateBridgeEndpoint() {
  bool legacyBridge = gHubHost == kLegacyBridgeHost && gHubPort == kLegacyBridgePort;
  bool staleDomainPort = gHubHost == kProdBridgeHost && (gHubPort == 80 || gHubPort == kLegacyBridgePort);
  bool localBridge = isLocalOrPrivateBridgeHost(gHubHost);
  bool nonProdSavedEndpoint = !gHubHost.isEmpty() && gHubHost != kProdBridgeHost;
  if (legacyBridge || staleDomainPort || localBridge || nonProdSavedEndpoint) {
    String previous = gHubHost + ":" + String(gHubPort);
    saveHubEndpoint(kProdBridgeHost, kProdBridgePort);
    logf("[CFG] migrated bridge endpoint from=%s to=%s:%u",
         previous.c_str(), gHubHost.c_str(), gHubPort);
  }
}

bool hubUsesTls() {
  return gHubHost == kProdBridgeHost || gHubPort == 443;
}

String hubBaseUrl() {
  return String(hubUsesTls() ? "https://" : "http://") + gHubHost + ":" + String(gHubPort);
}

const char* otaImageStateName(esp_ota_img_states_t state) {
  switch (state) {
    case ESP_OTA_IMG_NEW:
      return "new";
    case ESP_OTA_IMG_PENDING_VERIFY:
      return "pending";
    case ESP_OTA_IMG_VALID:
      return "valid";
    case ESP_OTA_IMG_INVALID:
      return "invalid";
    case ESP_OTA_IMG_ABORTED:
      return "aborted";
    case ESP_OTA_IMG_UNDEFINED:
    default:
      return "undefined";
  }
}

String describeOtaPartition(const esp_partition_t* partition) {
  if (!partition) {
    return "none";
  }
  return String(partition->label) + "@0x" + String(static_cast<uint32_t>(partition->address), HEX) +
         "/" + String(static_cast<unsigned>(partition->size));
}

void otaLog(const String& message, bool persist = true) {
  appendRuntimeLog("OTA", message, persist);
  logf("[OTA] %s", message.c_str());
}

void clearOtaGuardPrefs() {
  gPrefs.begin(kPrefsOtaGuardNs, false);
  gPrefs.clear();
  gPrefs.end();
}

