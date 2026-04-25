#include <Arduino.h>
#include <Adafruit_TCA8418.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>

#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <deque>
#include <time.h>
#include <vector>

namespace {

constexpr const char* kDeviceType = "cardputer_adv";
constexpr const char* kDefaultDisplayName = "ADV Cardputer";
constexpr const char* kWsPath = "/ws/stream";
constexpr const char* kDeviceTextTurnPath = "/api/device-text-turn";
constexpr const char* kDeviceAudioTurnPath = "/api/device-audio-turn";
constexpr const char* kDeviceAudioTurnRawPath = "/api/device-audio-turn-raw";
constexpr const char* kDeviceTtsPath = "/api/device-tts";

constexpr const char* kPrefsWifiNs = "wifi_creds";
constexpr const char* kPrefsHubNs = "hub_ep";
constexpr const char* kPrefsDeviceNs = "device_cfg";

constexpr const char* kBleServiceName = "OmniBot Cardputer ADV";
constexpr const char* kBleServiceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
constexpr const char* kBleWifiCharUuid = "12345678-1234-5678-1234-56789abcdef0";

constexpr uint16_t kDefaultHubPort = 8000;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kWifiReconnectIntervalMs = 5000;
constexpr uint32_t kWsReconnectIntervalMs = 3000;
constexpr uint32_t kRenderIntervalMs = 20;
constexpr uint32_t kBusyRenderIntervalMs = 60;
constexpr uint32_t kHttpReconnectTimeoutMs = 9000;
constexpr uint8_t kHttpConnectRetries = 3;
constexpr bool kUseHubWebSocket = false;
constexpr size_t kMaxChatLines = 24;
constexpr size_t kMaxInputChars = 180;
constexpr size_t kWrapColumns = 34;
constexpr uint32_t kMicSampleRate = 8000;
constexpr uint32_t kTtsDefaultSampleRate = 12000;
constexpr float kMaxRecordSeconds = 8.0f;
constexpr float kMaxTtsSeconds = 14.0f;
constexpr float kMinRecordSeconds = 0.35f;
constexpr uint32_t kImuSampleIntervalMs = 30;
constexpr size_t kMaxRecordSamples = static_cast<size_t>(kMicSampleRate * kMaxRecordSeconds);
constexpr size_t kRecordBufferBytes = kMaxRecordSamples * sizeof(int16_t);
constexpr size_t kMinRecordBufferBytes = static_cast<size_t>(kMicSampleRate * 2 * sizeof(int16_t));
constexpr size_t kMaxTtsSamples = static_cast<size_t>(kTtsDefaultSampleRate * kMaxTtsSeconds);
constexpr size_t kTtsBufferBytes = kMaxTtsSamples * sizeof(int16_t);
constexpr size_t kMinTtsBufferBytes = static_cast<size_t>(kTtsDefaultSampleRate * 2 * sizeof(int16_t));
constexpr size_t kMaxHeapRecordFallbackBytes = 160 * 1024;
constexpr size_t kMaxHeapTtsFallbackBytes = 160 * 1024;
constexpr size_t kHeapReserveBytes = 48 * 1024;
constexpr size_t kHttpWriteChunkBytes = 512;
constexpr size_t kHttpStageChunkBytes = 256;
constexpr uint32_t kVoiceSocketSettleMs = 300;
constexpr uint32_t kCtrlDoubleTapWindowMs = 420;
constexpr uint32_t kLauncherToggleDebounceMs = 140;
constexpr uint32_t kClockTickMs = 1000;
constexpr uint32_t kTimeSyncRetryMs = 15000;
constexpr uint32_t kTimeResyncMs = 6UL * 60UL * 60UL * 1000UL;
constexpr size_t kMaxStoredVoiceNotes = 12;
constexpr size_t kMaxStoredPreviewChars = 84;
constexpr size_t kVoicePreviewLineThreshold = 5;
constexpr const char* kVoiceDir = "/voice";
constexpr const char* kVoiceMetaExt = ".json";
constexpr const char* kVoiceAudioExt = ".wav";
constexpr uint8_t kTca8418IntPin = 11;
constexpr uint8_t kTca8418I2cAddr = 0x34;
constexpr uint8_t kTca8418SdaPin = 8;
constexpr uint8_t kTca8418SclPin = 9;
constexpr const char* kBangkokTz = "ICT-7";

#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID ""
#endif

#ifndef DEFAULT_WIFI_PASSWORD
#define DEFAULT_WIFI_PASSWORD ""
#endif

#ifndef DEFAULT_HUB_HOST
#define DEFAULT_HUB_HOST ""
#endif

#ifndef DEFAULT_HUB_PORT_STR
#define DEFAULT_HUB_PORT_STR ""
#endif

#ifndef DEFAULT_DEVICE_TOKEN
#define DEFAULT_DEVICE_TOKEN ""
#endif

enum class LineKind : uint8_t {
  System,
  User,
  Assistant,
  Error,
};

enum class FaceMode : uint8_t {
  Idle,
  Listening,
  Thinking,
  Speaking,
  Error,
};

enum class KeyboardLayout : uint8_t {
  En,
  Ru,
};

enum class UiMode : uint8_t {
  ChatFace,
  Face,
  ChatFull,
  Clock,
  Voice,
};

struct ChatLine {
  String prefix;
  String text;
  LineKind kind;
};

struct VisualLine {
  String text;
  LineKind kind;
};

struct VoiceNote {
  String id;
  String audioPath;
  String metaPath;
  String title;
  String preview;
  uint32_t sampleRate = 16000;
  uint32_t durationMs = 0;
  bool assistant = false;
};

Preferences gPrefs;
WebSocketsClient gWs;
M5Canvas gCanvas(&M5Cardputer.Display);
Adafruit_TCA8418 gTcaKeyboard;

String gDeviceId;
String gDisplayName = kDefaultDisplayName;
String gWifiSsid;
String gWifiPassword;
String gHubHost;
String gDeviceToken;
uint16_t gHubPort = kDefaultHubPort;

std::deque<ChatLine> gChatLines;
String gInputBuffer;
String gStatusText = "Booting";
std::vector<VoiceNote> gVoiceNotes;

bool gWifiReady = false;
bool gWsReady = false;
bool gBleActive = false;
bool gPendingRestart = false;
bool gSubmitting = false;
bool gThinking = false;
bool gTypingKeyHeld = false;
bool gVoiceTriggerHeld = false;
bool gRecording = false;
bool gPlaybackActive = false;
bool gResumeWsAfterVoice = false;
bool gImuEnabled = false;
bool gDebugOverlayVisible = false;
bool gUseTcaKeyboard = false;
bool gStorageReady = false;
bool gLauncherVisible = false;
bool gAssistantPendingVisible = false;
int gChatScrollOffset = 0;
int gLauncherSelection = 0;
int gSelectedVoiceNote = 0;

uint32_t gLastRenderMs = 0;
uint32_t gStatusUntilMs = 0;
uint32_t gRecordStartMs = 0;
uint32_t gBlinkEndMs = 0;
uint32_t gNextBlinkMs = 0;
uint32_t gLastImuSampleMs = 0;
uint32_t gLastWifiReconnectMs = 0;
uint32_t gLastWsReconnectMs = 0;
uint32_t gLastCtrlTapMs = 0;
uint32_t gLastClockRenderMs = 0;
uint32_t gLastTimeSyncAttemptMs = 0;
uint32_t gLastTimeSyncMs = 0;
uint32_t gLastLauncherToggleMs = 0;
uint32_t gLedFlashUntilMs = 0;

FaceMode gFaceMode = FaceMode::Idle;
KeyboardLayout gKeyboardLayout = KeyboardLayout::En;
UiMode gUiMode = UiMode::ChatFace;
Keyboard_Class::KeysState gKeyboardState;
int16_t* gRecordBuffer = nullptr;
int16_t* gTtsBuffer = nullptr;
size_t gRecordedSamples = 0;
size_t gRecordCapacitySamples = 0;
size_t gRecordCapacityBytes = 0;
size_t gTtsCapacitySamples = 0;
size_t gTtsCapacityBytes = 0;
float gEyeLookX = 0.0f;
float gEyeLookY = 0.0f;
float gEyePanelShiftX = 0.0f;
float gEyePanelShiftY = 0.0f;
uint8_t gSpeakerVolume = 255;
String gLastVoiceDiag = "boot";
String gLastHttpDiag = "boot";
String gLastKeyDiag = "-";
String gLastKeySignature;
bool gTcaPressedKeys[4][14] = {};
bool gCtrlSoloHeld = false;
bool gCtrlSoloCandidate = false;
uint8_t* gDynamicPlaybackBuffer = nullptr;
volatile bool gTcaInterruptPending = false;
bool gTimeZoneConfigured = false;
bool gTimeValid = false;

void IRAM_ATTR onTcaKeyboardInterrupt() {
  gTcaInterruptPending = true;
}

void startHubWebSocket();
void pauseHubWebSocketForVoice();
void resumeHubWebSocketAfterVoice();
void render();
std::vector<String> wrapText(const String& raw, int32_t pixelLimit);
String keyboardSignatureFromState();
bool ensureWifiForHttp(const char* reason, uint32_t timeoutMs = kHttpReconnectTimeoutMs);
bool connectHubClient(WiFiClient& client, const char* reason, uint32_t readTimeoutSec = 45);
void synthesizeTcaKeyboardState();
void pollTcaKeyboardEvents();
void submitInput();
void syncClockFromNtp(bool force = false);
void flashActivityLed(uint8_t r, uint8_t g, uint8_t b, uint32_t durationMs = 180);
void updateStatusLed();

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

void logf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
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

const char* uiModeName(UiMode mode) {
  switch (mode) {
    case UiMode::Face:
      return "Face";
    case UiMode::ChatFull:
      return "Chat";
    case UiMode::Clock:
      return "Clock";
    case UiMode::Voice:
      return "Voice";
    case UiMode::ChatFace:
    default:
      return "Chat+Face";
  }
}

const char* layoutName(KeyboardLayout layout) {
  return layout == KeyboardLayout::Ru ? "RU" : "EN";
}

String trimPreview(const String& text, size_t maxChars = kMaxStoredPreviewChars) {
  if (text.length() <= static_cast<int>(maxChars)) {
    return text;
  }
  return text.substring(0, maxChars) + "...";
}

void removeLastUtf8Char(String& value) {
  if (value.isEmpty()) {
    return;
  }
  int pos = value.length() - 1;
  while (pos > 0 && (static_cast<uint8_t>(value[pos]) & 0xC0) == 0x80) {
    --pos;
  }
  value.remove(pos);
}

void removeFirstUtf8Char(String& value) {
  if (value.isEmpty()) {
    return;
  }
  int pos = 1;
  while (pos < value.length() && (static_cast<uint8_t>(value[pos]) & 0xC0) == 0x80) {
    ++pos;
  }
  value.remove(0, pos);
}

const char* translateRuLayoutChar(char key) {
  switch (key) {
    case '`': return u8"ё";
    case '~': return u8"Ё";
    case 'q': return u8"й";
    case 'Q': return u8"Й";
    case 'w': return u8"ц";
    case 'W': return u8"Ц";
    case 'e': return u8"у";
    case 'E': return u8"У";
    case 'r': return u8"к";
    case 'R': return u8"К";
    case 't': return u8"е";
    case 'T': return u8"Е";
    case 'y': return u8"н";
    case 'Y': return u8"Н";
    case 'u': return u8"г";
    case 'U': return u8"Г";
    case 'i': return u8"ш";
    case 'I': return u8"Ш";
    case 'o': return u8"щ";
    case 'O': return u8"Щ";
    case 'p': return u8"з";
    case 'P': return u8"З";
    case '[': return u8"х";
    case '{': return u8"Х";
    case ']': return u8"ъ";
    case '}': return u8"Ъ";
    case 'a': return u8"ф";
    case 'A': return u8"Ф";
    case 's': return u8"ы";
    case 'S': return u8"Ы";
    case 'd': return u8"в";
    case 'D': return u8"В";
    case 'f': return u8"а";
    case 'F': return u8"А";
    case 'g': return u8"п";
    case 'G': return u8"П";
    case 'h': return u8"р";
    case 'H': return u8"Р";
    case 'j': return u8"о";
    case 'J': return u8"О";
    case 'k': return u8"л";
    case 'K': return u8"Л";
    case 'l': return u8"д";
    case 'L': return u8"Д";
    case ';': return u8"ж";
    case ':': return u8"Ж";
    case '\'': return u8"э";
    case '"': return u8"Э";
    case 'z': return u8"я";
    case 'Z': return u8"Я";
    case 'x': return u8"ч";
    case 'X': return u8"Ч";
    case 'c': return u8"с";
    case 'C': return u8"С";
    case 'v': return u8"м";
    case 'V': return u8"М";
    case 'b': return u8"и";
    case 'B': return u8"И";
    case 'n': return u8"т";
    case 'N': return u8"Т";
    case 'm': return u8"ь";
    case 'M': return u8"Ь";
    case ',': return u8"б";
    case '<': return u8"Б";
    case '.': return u8"ю";
    case '>': return u8"Ю";
    case '/': return ".";
    case '?': return ",";
    default: break;
  }
  static char passthrough[2] = {0, 0};
  passthrough[0] = key;
  passthrough[1] = 0;
  return passthrough;
}

String translateInputChar(char key) {
  if (gKeyboardLayout == KeyboardLayout::Ru) {
    return String(translateRuLayoutChar(key));
  }
  return String(key);
}

bool hasValidSystemTime() {
  return time(nullptr) > 1700000000;
}

void ensureBangkokTimezone() {
  if (gTimeZoneConfigured) {
    return;
  }
  setenv("TZ", kBangkokTz, 1);
  tzset();
  gTimeZoneConfigured = true;
}

void persistRtcFromSystemTime() {
  if (!M5.Rtc.isEnabled() || !hasValidSystemTime()) {
    return;
  }
  time_t now = time(nullptr);
  struct tm localNow;
  localtime_r(&now, &localNow);
  M5.Rtc.setDateTime(&localNow);
}

String tailUtf8ToFit(const String& raw, int32_t pixelLimit) {
  String trimmed = raw;
  while (!trimmed.isEmpty() && gCanvas.textWidth(trimmed) > pixelLimit) {
    removeFirstUtf8Char(trimmed);
  }
  return trimmed;
}

void scheduleBlink() {
  gNextBlinkMs = millis() + 1800 + static_cast<uint32_t>(random(1200, 4200));
}

void setStatus(const String& text, uint32_t ttlMs = 0) {
  gStatusText = text;
  gStatusUntilMs = ttlMs ? millis() + ttlMs : 0;
}

void setFaceMode(FaceMode mode) {
  gFaceMode = mode;
}

void setVoiceDiag(const String& text) {
  gLastVoiceDiag = text;
}

void setHttpDiag(const String& text) {
  gLastHttpDiag = text;
}

void setKeyDiag(const String& text) {
  gLastKeyDiag = text;
}

void flashActivityLed(uint8_t r, uint8_t g, uint8_t b, uint32_t durationMs) {
  if (!M5.Led.isEnabled()) {
    return;
  }
  M5.Led.setAllColor(r, g, b);
  M5.Led.display();
  gLedFlashUntilMs = millis() + durationMs;
}

void updateStatusLed() {
  if (!M5.Led.isEnabled()) {
    return;
  }
  if (gLedFlashUntilMs != 0 && millis() >= gLedFlashUntilMs) {
    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();
    gLedFlashUntilMs = 0;
  }
}

uint16_t colorForLine(LineKind kind) {
  switch (kind) {
    case LineKind::User:
      return TFT_CYAN;
    case LineKind::Assistant:
      return TFT_GREENYELLOW;
    case LineKind::Error:
      return TFT_ORANGE;
    case LineKind::System:
    default:
      return TFT_LIGHTGREY;
  }
}

void pushLine(const String& prefix, const String& text, LineKind kind) {
  if (text.isEmpty()) {
    return;
  }
  gChatLines.push_back({prefix, text, kind});
  while (gChatLines.size() > kMaxChatLines) {
    gChatLines.pop_front();
  }
  gChatScrollOffset = 0;
}

void pushSystem(const String& text) { pushLine("SYS", text, LineKind::System); }
void pushError(const String& text) { pushLine("ERR", text, LineKind::Error); }
void pushUser(const String& text) { pushLine("YOU", text, LineKind::User); }
void pushAssistant(const String& text) { pushLine("AI", text, LineKind::Assistant); }

std::vector<VisualLine> buildVisualLines(int32_t pixelLimit) {
  std::vector<VisualLine> lines;
  for (const auto& entry : gChatLines) {
    String full = entry.prefix + ": " + entry.text;
    auto wrapped = wrapText(full, pixelLimit);
    for (const auto& part : wrapped) {
      lines.push_back({part, entry.kind});
    }
  }
  if (gAssistantPendingVisible) {
    String dots = ".";
    uint8_t dotCount = 1 + static_cast<uint8_t>((millis() / 350) % 3);
    while (dots.length() < dotCount) {
      dots += '.';
    }
    auto pendingWrapped = wrapText("AI: " + dots, pixelLimit);
    for (const auto& part : pendingWrapped) {
      lines.push_back({part, LineKind::Assistant});
    }
  }
  return lines;
}

void scrollChatBy(int delta) {
  bool showFace = gUiMode == UiMode::ChatFace;
  bool showStatus = !gStatusText.isEmpty() && gStatusText != "Ready";
  int visibleRows = 0;
  int pixelLimit = 0;
  if (showFace) {
    visibleRows = showStatus ? 3 : 4;
    pixelLimit = 216;
  } else {
    visibleRows = showStatus ? 7 : 8;
    pixelLimit = 228;
  }
  auto lines = buildVisualLines(pixelLimit);
  int maxOffset = static_cast<int>(lines.size()) - visibleRows;
  if (maxOffset < 0) {
    maxOffset = 0;
  }
  gChatScrollOffset = clampValue(gChatScrollOffset + delta, 0, maxOffset);
  setStatus("Scroll " + String(gChatScrollOffset), 600);
}

String voiceAudioPath(const String& id) {
  return String(kVoiceDir) + "/" + id + kVoiceAudioExt;
}

String voiceMetaPath(const String& id) {
  return String(kVoiceDir) + "/" + id + kVoiceMetaExt;
}

bool ensureVoiceStorage() {
  if (!gStorageReady) {
    return false;
  }
  if (!LittleFS.exists(kVoiceDir)) {
    return LittleFS.mkdir(kVoiceDir);
  }
  return true;
}

String makeVoiceNoteId(bool assistant) {
  return String(assistant ? "a_" : "u_") + String(millis()) + "_" + chipIdSuffix();
}

void sortVoiceNotesNewestFirst() {
  std::sort(gVoiceNotes.begin(), gVoiceNotes.end(), [](const VoiceNote& a, const VoiceNote& b) {
    return a.id > b.id;
  });
}

void clampVoiceSelection() {
  if (gVoiceNotes.empty()) {
    gSelectedVoiceNote = 0;
    return;
  }
  if (gSelectedVoiceNote < 0) {
    gSelectedVoiceNote = 0;
  }
  if (gSelectedVoiceNote >= static_cast<int>(gVoiceNotes.size())) {
    gSelectedVoiceNote = static_cast<int>(gVoiceNotes.size()) - 1;
  }
}

void pruneVoiceNotes() {
  sortVoiceNotesNewestFirst();
  while (gVoiceNotes.size() > kMaxStoredVoiceNotes) {
    const auto& note = gVoiceNotes.back();
    LittleFS.remove(note.audioPath);
    LittleFS.remove(note.metaPath);
    gVoiceNotes.pop_back();
  }
  clampVoiceSelection();
}

void loadVoiceNotes() {
  gVoiceNotes.clear();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return;
  }
  File dir = LittleFS.open(kVoiceDir);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  while (true) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }
    String path = file.path();
    file.close();
    if (!path.endsWith(kVoiceMetaExt)) {
      continue;
    }

    File meta = LittleFS.open(path, "r");
    if (!meta) {
      continue;
    }
    DynamicJsonDocument doc(512);
    auto err = deserializeJson(doc, meta);
    meta.close();
    if (err) {
      continue;
    }

    VoiceNote note;
    note.id = doc["id"] | "";
    note.audioPath = doc["audio_path"] | "";
    note.metaPath = path;
    note.title = doc["title"] | "Voice note";
    note.preview = doc["preview"] | "";
    note.sampleRate = doc["sample_rate"] | 16000;
    note.durationMs = doc["duration_ms"] | 0;
    note.assistant = doc["assistant"] | false;
    if (!note.id.isEmpty() && !note.audioPath.isEmpty() && LittleFS.exists(note.audioPath)) {
      gVoiceNotes.push_back(note);
    }
  }
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
}

bool saveVoiceNoteMetadata(const VoiceNote& note) {
  DynamicJsonDocument doc(512);
  doc["id"] = note.id;
  doc["audio_path"] = note.audioPath;
  doc["title"] = note.title;
  doc["preview"] = note.preview;
  doc["sample_rate"] = note.sampleRate;
  doc["duration_ms"] = note.durationMs;
  doc["assistant"] = note.assistant;

  File meta = LittleFS.open(note.metaPath, "w");
  if (!meta) {
    return false;
  }
  serializeJson(doc, meta);
  meta.close();
  return true;
}

bool persistWavFile(const String& path, const uint8_t* header, size_t headerLen, const uint8_t* audioBytes,
                    size_t audioLen) {
  if (!gStorageReady || !ensureVoiceStorage()) {
    return false;
  }
  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }
  size_t writtenHeader = file.write(header, headerLen);
  size_t writtenBody = file.write(audioBytes, audioLen);
  file.close();
  return writtenHeader == headerLen && writtenBody == audioLen;
}

void rememberVoiceNote(const String& title, const String& preview, const uint8_t* header, size_t headerLen,
                       const uint8_t* audioBytes, size_t audioLen, uint32_t sampleRate, bool assistant) {
  if (!gStorageReady || !ensureVoiceStorage()) {
    return;
  }
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  size_t requiredBytes = headerLen + audioLen + 2048;
  if (totalBytes > 0 && usedBytes + requiredBytes >= totalBytes) {
    logf("[VOICE] skip note storage used=%u total=%u required=%u",
         static_cast<unsigned>(usedBytes),
         static_cast<unsigned>(totalBytes),
         static_cast<unsigned>(requiredBytes));
    return;
  }
  VoiceNote note;
  note.id = makeVoiceNoteId(assistant);
  note.audioPath = voiceAudioPath(note.id);
  note.metaPath = voiceMetaPath(note.id);
  note.title = title;
  note.preview = trimPreview(preview);
  note.sampleRate = sampleRate;
  note.durationMs = sampleRate ? static_cast<uint32_t>((audioLen / 2U) * 1000ULL / sampleRate) : 0;
  note.assistant = assistant;

  if (!persistWavFile(note.audioPath, header, headerLen, audioBytes, audioLen)) {
    return;
  }
  if (!saveVoiceNoteMetadata(note)) {
    LittleFS.remove(note.audioPath);
    return;
  }
  gVoiceNotes.push_back(note);
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
  gSelectedVoiceNote = 0;
}

bool playVoiceNoteAt(int index) {
  if (index < 0 || index >= static_cast<int>(gVoiceNotes.size())) {
    return false;
  }
  const auto& note = gVoiceNotes[index];
  File file = LittleFS.open(note.audioPath, "r");
  if (!file) {
    setStatus("Voice file missing", 1200);
    return false;
  }
  size_t fileSize = file.size();
  if (fileSize < 44) {
    file.close();
    setStatus("Voice file invalid", 1200);
    return false;
  }

  uint8_t* wavBuffer = nullptr;
  bool dynamicBuffer = false;
  size_t capacity = max(gTtsCapacityBytes, gRecordCapacityBytes);
  if (capacity >= fileSize && gTtsBuffer) {
    wavBuffer = reinterpret_cast<uint8_t*>(gTtsBuffer);
  } else if (capacity >= fileSize && gRecordBuffer && !gRecording) {
    wavBuffer = reinterpret_cast<uint8_t*>(gRecordBuffer);
  } else {
    wavBuffer = static_cast<uint8_t*>(heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    dynamicBuffer = wavBuffer != nullptr;
  }
  if (!wavBuffer) {
    file.close();
    setStatus("Voice note too large", 1200);
    return false;
  }

  size_t readBytes = file.read(wavBuffer, fileSize);
  file.close();
  if (readBytes != fileSize) {
    if (dynamicBuffer) {
      free(wavBuffer);
    }
    setStatus("Voice read failed", 1200);
    return false;
  }

  M5Cardputer.Speaker.stop();
  if (gDynamicPlaybackBuffer) {
    free(gDynamicPlaybackBuffer);
    gDynamicPlaybackBuffer = nullptr;
  }
  gPlaybackActive = true;
  setFaceMode(FaceMode::Speaking);
  setStatus("Playing note", 1200);
  M5Cardputer.Speaker.playWav(wavBuffer, fileSize, 1, -1, true);
  if (dynamicBuffer) {
    gDynamicPlaybackBuffer = wavBuffer;
    setVoiceDiag("note: dynamic");
  } else {
    setVoiceDiag("note: cached");
  }
  return true;
}

void setUiMode(UiMode mode) {
  gUiMode = mode;
  gLauncherVisible = false;
  setStatus(String("Mode: ") + uiModeName(mode), 900);
}

void moveLauncherSelection(int delta) {
  constexpr int kModeCount = 5;
  gLauncherVisible = true;
  gLauncherSelection = (gLauncherSelection + delta + kModeCount) % kModeCount;
}

void applyLauncherSelection() {
  switch (gLauncherSelection) {
    case 0:
      setUiMode(UiMode::ChatFace);
      break;
    case 1:
      setUiMode(UiMode::Face);
      break;
    case 2:
      setUiMode(UiMode::ChatFull);
      break;
    case 3:
      setUiMode(UiMode::Clock);
      break;
    case 4:
    default:
      setUiMode(UiMode::Voice);
      break;
  }
}

size_t replyVisualLineCount(const String& reply) {
  String full = "AI: " + reply;
  auto wrapped = wrapText(full, 216);
  return wrapped.size();
}

void appendAssistantReply(const String& reply) {
  if (reply.isEmpty()) {
    return;
  }
  flashActivityLed(0, 180, 0);
  if (replyVisualLineCount(reply) > kVoicePreviewLineThreshold) {
    pushAssistant(trimPreview(reply) + " [voice]");
  } else {
    pushAssistant(reply);
  }
}

bool setupKeyboardInput() {
  memset(gTcaPressedKeys, 0, sizeof(gTcaPressedKeys));
  Wire1.begin(kTca8418SdaPin, kTca8418SclPin);
  Wire1.setClock(400000);
  Wire1.setTimeOut(50);
  delay(50);
  gUseTcaKeyboard = gTcaKeyboard.begin(kTca8418I2cAddr, &Wire1);
  if (gUseTcaKeyboard) {
    gTcaKeyboard.matrix(7, 8);
    gTcaKeyboard.flush();
    pinMode(kTca8418IntPin, INPUT_PULLUP);
    gTcaKeyboard.enableInterrupts();
    attachInterrupt(digitalPinToInterrupt(kTca8418IntPin), onTcaKeyboardInterrupt, FALLING);
    gTcaInterruptPending = true;
    logf("[KEY] using TCA8418 keyboard");
    setKeyDiag("kbd:tca");
    return true;
  }

  Wire1.end();
  M5Cardputer.Keyboard.begin();
  logf("[KEY] TCA8418 unavailable, fallback to matrix keyboard");
  setKeyDiag("kbd:matrix");
  return false;
}

void synthesizeTcaKeyboardState() {
  gKeyboardState.reset();

  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 14; ++col) {
      if (!gTcaPressedKeys[row][col]) {
        continue;
      }
      uint8_t keyFirst = static_cast<uint8_t>(_key_value_map[row][col].value_first);
      switch (keyFirst) {
        case KEY_FN:
          gKeyboardState.fn = true;
          break;
        case KEY_LEFT_SHIFT:
          gKeyboardState.shift = true;
          gKeyboardState.modifier_keys.push_back(KEY_LEFT_SHIFT);
          break;
        case KEY_LEFT_CTRL:
          gKeyboardState.ctrl = true;
          gKeyboardState.modifier_keys.push_back(KEY_LEFT_CTRL);
          break;
        case KEY_LEFT_ALT:
          gKeyboardState.alt = true;
          gKeyboardState.modifier_keys.push_back(KEY_LEFT_ALT);
          break;
        case KEY_OPT:
          gKeyboardState.opt = true;
          break;
      }
    }
  }

  for (auto modifier : gKeyboardState.modifier_keys) {
    gKeyboardState.modifiers |= (1 << (modifier - 0x80));
  }

  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 14; ++col) {
      if (!gTcaPressedKeys[row][col]) {
        continue;
      }
      uint8_t keyFirst = static_cast<uint8_t>(_key_value_map[row][col].value_first);
      switch (keyFirst) {
        case KEY_FN:
        case KEY_LEFT_SHIFT:
        case KEY_LEFT_CTRL:
        case KEY_LEFT_ALT:
        case KEY_OPT:
          continue;
        case KEY_TAB:
          gKeyboardState.tab = true;
          gKeyboardState.hid_keys.push_back(KEY_TAB);
          continue;
        case KEY_BACKSPACE:
          gKeyboardState.del = true;
          gKeyboardState.hid_keys.push_back(KEY_BACKSPACE);
          continue;
        case KEY_ENTER:
          gKeyboardState.enter = true;
          gKeyboardState.hid_keys.push_back(KEY_ENTER);
          continue;
        default:
          break;
      }

      char printable = gKeyboardState.shift ? _key_value_map[row][col].value_second
                                            : _key_value_map[row][col].value_first;
      if (printable == ' ') {
        gKeyboardState.space = true;
      }
      if (printable >= 32 && printable < 127) {
        gKeyboardState.word.push_back(printable);
        uint8_t hid = _kb_asciimap[static_cast<uint8_t>(printable)];
        if (hid) {
          gKeyboardState.hid_keys.push_back(hid);
        }
      }
    }
  }
}

void pollTcaKeyboardEvents() {
  if (!gUseTcaKeyboard) {
    return;
  }

  int safety = 32;
  while (safety-- > 0) {
    bool pendingIrq = gTcaInterruptPending || digitalRead(kTca8418IntPin) == LOW;
    int available = gTcaKeyboard.available();
    if (!pendingIrq && available <= 0) {
      break;
    }
    gTcaInterruptPending = false;

    int keyEvent = gTcaKeyboard.getEvent();
    if (keyEvent <= 0) {
      if (available <= 0) {
        break;
      }
      continue;
    }

    bool pressed = (keyEvent & 0x80) != 0;
    uint8_t rawValue = static_cast<uint8_t>(keyEvent & 0x7F);
    uint8_t row = 0xFF;
    uint8_t col = 0xFF;
    mapTcaRawKeyToPhysical(rawValue, row, col);
    if (row >= 4 || col >= 14) {
      continue;
    }

    gTcaPressedKeys[row][col] = pressed;
    logf("[KEY:TCA] raw=%u pressed=%d row=%u col=%u",
         static_cast<unsigned>(rawValue),
         pressed ? 1 : 0,
         static_cast<unsigned>(row),
         static_cast<unsigned>(col));
  }

  uint8_t intStatus = gTcaKeyboard.readRegister(TCA8418_REG_INT_STAT);
  if (intStatus & TCA8418_REG_STAT_OVR_FLOW_INT) {
    logf("[KEY:TCA] overflow -> flush");
    gTcaKeyboard.flush();
    memset(gTcaPressedKeys, 0, sizeof(gTcaPressedKeys));
  } else if (intStatus != 0) {
    gTcaKeyboard.readRegister(TCA8418_REG_GPIO_INT_STAT_1);
    gTcaKeyboard.readRegister(TCA8418_REG_GPIO_INT_STAT_2);
    gTcaKeyboard.readRegister(TCA8418_REG_GPIO_INT_STAT_3);
    gTcaKeyboard.writeRegister(TCA8418_REG_INT_STAT, 0x0F);
  }

  synthesizeTcaKeyboardState();
}

void refreshKeyboardState() {
  if (gUseTcaKeyboard) {
    pollTcaKeyboardEvents();
  } else {
    M5Cardputer.Keyboard.updateKeyList();
    M5Cardputer.Keyboard.updateKeysState();
    gKeyboardState = M5Cardputer.Keyboard.keysState();
  }
  String sig = keyboardSignatureFromState();
  if (sig != gLastKeySignature) {
    gLastKeySignature = sig;
    setKeyDiag(sig.isEmpty() ? "-" : sig);
    if (!sig.isEmpty()) {
      logf("[KEY] %s", sig.c_str());
    }
  }
}

String keyboardSignatureFromState() {
  String sig;
  sig.reserve(48);
  if (gKeyboardState.fn) {
    sig += "fn|";
  }
  if (gKeyboardState.shift) {
    sig += "shift|";
  }
  if (gKeyboardState.ctrl) {
    sig += "ctrl|";
  }
  if (gKeyboardState.alt) {
    sig += "alt|";
  }
  if (gKeyboardState.opt) {
    sig += "opt|";
  }
  if (gKeyboardState.tab) {
    sig += "tab|";
  }
  if (gKeyboardState.enter) {
    sig += "enter|";
  }
  if (gKeyboardState.del) {
    sig += "del|";
  }
  for (char c : gKeyboardState.word) {
    sig += c;
  }
  return sig;
}

void updateImuMotion() {
  if (!gImuEnabled) {
    gEyeLookX *= 0.9f;
    gEyeLookY *= 0.9f;
    gEyePanelShiftX *= 0.9f;
    gEyePanelShiftY *= 0.9f;
    return;
  }

  uint32_t now = millis();
  if (now - gLastImuSampleMs < kImuSampleIntervalMs) {
    return;
  }
  gLastImuSampleMs = now;

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) {
    return;
  }

  float targetLookX = clampValue(-ay * 18.0f, -18.0f, 18.0f);
  float targetLookY = clampValue(ax * 14.0f, -14.0f, 14.0f);
  float targetPanelX = clampValue(-ay * 6.0f, -6.0f, 6.0f);
  float targetPanelY = clampValue(ax * 4.0f, -4.0f, 4.0f);

  gEyeLookX = gEyeLookX * 0.78f + targetLookX * 0.22f;
  gEyeLookY = gEyeLookY * 0.78f + targetLookY * 0.22f;
  gEyePanelShiftX = gEyePanelShiftX * 0.8f + targetPanelX * 0.2f;
  gEyePanelShiftY = gEyePanelShiftY * 0.8f + targetPanelY * 0.2f;
}

std::vector<String> wrapText(const String& raw, int32_t pixelLimit) {
  std::vector<String> out;
  String normalized = raw;
  normalized.replace('\r', ' ');
  normalized.replace('\n', ' ');
  normalized.trim();

  if (normalized.isEmpty()) {
    out.push_back("");
    return out;
  }

  std::vector<String> words;
  int start = 0;
  while (start < normalized.length()) {
    while (start < normalized.length() && normalized[start] == ' ') {
      ++start;
    }
    if (start >= normalized.length()) {
      break;
    }
    int end = start;
    while (end < normalized.length() && normalized[end] != ' ') {
      ++end;
    }
    words.push_back(normalized.substring(start, end));
    start = end;
  }

  String current;
  for (const auto& word : words) {
    String candidate = current.isEmpty() ? word : current + " " + word;
    if (current.isEmpty() || gCanvas.textWidth(candidate) <= pixelLimit) {
      current = candidate;
      continue;
    }
    out.push_back(current);
    current = word;
  }

  if (!current.isEmpty()) {
    out.push_back(current);
  }
  if (out.empty()) {
    out.push_back("");
  }
  return out;
}

void applyCompileTimeDefaults() {
  if (gWifiSsid.isEmpty() && strlen(DEFAULT_WIFI_SSID) > 0) {
    gWifiSsid = DEFAULT_WIFI_SSID;
  }
  if (gWifiPassword.isEmpty() && strlen(DEFAULT_WIFI_PASSWORD) > 0) {
    gWifiPassword = DEFAULT_WIFI_PASSWORD;
  }
  if (gHubHost.isEmpty() && strlen(DEFAULT_HUB_HOST) > 0) {
    gHubHost = DEFAULT_HUB_HOST;
  }
  uint16_t defaultPort = compileTimeHubPort();
  if (gHubPort == kDefaultHubPort && defaultPort != 0) {
    gHubPort = defaultPort;
  }
  if (gDeviceToken.isEmpty() && strlen(DEFAULT_DEVICE_TOKEN) > 0) {
    gDeviceToken = DEFAULT_DEVICE_TOKEN;
  }
}

void loadConfig() {
  gPrefs.begin(kPrefsWifiNs, true);
  gWifiSsid = gPrefs.getString("ssid", "");
  gWifiPassword = gPrefs.getString("password", "");
  gPrefs.end();

  gPrefs.begin(kPrefsHubNs, true);
  gHubHost = gPrefs.getString("ip", "");
  gHubPort = static_cast<uint16_t>(gPrefs.getUInt("port", kDefaultHubPort));
  gPrefs.end();

  gPrefs.begin(kPrefsDeviceNs, true);
  gDisplayName = gPrefs.getString("display_name", kDefaultDisplayName);
  String savedId = gPrefs.getString("device_id", "");
  gDeviceToken = gPrefs.getString("device_token", "");
  gPrefs.end();

  if (savedId.isEmpty()) {
    gDeviceId = "cardputer_adv_" + chipIdSuffix();
    gPrefs.begin(kPrefsDeviceNs, false);
    gPrefs.putString("device_id", gDeviceId);
    gPrefs.end();
  } else {
    gDeviceId = savedId;
  }

  applyCompileTimeDefaults();
}

void saveWifiCreds(const String& ssid, const String& password) {
  gPrefs.begin(kPrefsWifiNs, false);
  gPrefs.putString("ssid", ssid);
  gPrefs.putString("password", password);
  gPrefs.end();
  gWifiSsid = ssid;
  gWifiPassword = password;
}

void saveHubEndpoint(const String& host, uint16_t port) {
  gPrefs.begin(kPrefsHubNs, false);
  gPrefs.putString("ip", host);
  gPrefs.putUInt("port", port);
  gPrefs.end();
  gHubHost = host;
  gHubPort = port;
}

void clearWifiCreds() {
  gPrefs.begin(kPrefsWifiNs, false);
  gPrefs.remove("ssid");
  gPrefs.remove("password");
  gPrefs.end();
  gPrefs.begin(kPrefsHubNs, false);
  gPrefs.remove("ip");
  gPrefs.remove("port");
  gPrefs.end();
  gWifiSsid = "";
  gWifiPassword = "";
  gHubHost = "";
  gHubPort = kDefaultHubPort;
}

bool ensurePcmBuffer(
    int16_t*& buffer,
    size_t& capacityBytes,
    size_t& capacitySamples,
    size_t targetBytes,
    size_t minBytes,
    size_t heapFallbackCapBytes,
    const char* tag) {
  if (buffer) {
    return true;
  }
  buffer = static_cast<int16_t*>(
      heap_caps_malloc(targetBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (buffer) {
    capacityBytes = targetBytes;
    capacitySamples = targetBytes / sizeof(int16_t);
    logf("[%s] buffer allocated in PSRAM bytes=%u", tag, static_cast<unsigned>(capacityBytes));
    return true;
  }

  size_t heapTargetBytes = targetBytes;
  if (heapFallbackCapBytes > 0 && heapTargetBytes > heapFallbackCapBytes) {
    heapTargetBytes = heapFallbackCapBytes & ~static_cast<size_t>(1);
  }
  size_t freeHeap = ESP.getFreeHeap();
  if (freeHeap > kHeapReserveBytes && heapTargetBytes > (freeHeap - kHeapReserveBytes)) {
    heapTargetBytes = (freeHeap - kHeapReserveBytes) & ~static_cast<size_t>(1);
  }
  if (heapTargetBytes >= minBytes) {
    buffer = static_cast<int16_t*>(heap_caps_malloc(heapTargetBytes, MALLOC_CAP_8BIT));
    if (buffer) {
      capacityBytes = heapTargetBytes;
      capacitySamples = heapTargetBytes / sizeof(int16_t);
      logf("[%s] buffer allocated in heap bytes=%u target=%u",
           tag,
           static_cast<unsigned>(capacityBytes),
           static_cast<unsigned>(targetBytes));
      return true;
    }
  }

  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t fallbackBytes = 0;
  size_t fallbackLimit = largest;
  if (freeHeap > kHeapReserveBytes) {
    size_t freeHeapLimit = freeHeap - kHeapReserveBytes;
    if (fallbackLimit > freeHeapLimit) {
      fallbackLimit = freeHeapLimit;
    }
  } else {
    fallbackLimit = 0;
  }
  if (fallbackLimit > 8192) {
    fallbackBytes = (fallbackLimit - 4096) & ~static_cast<size_t>(1);
  }
  if (heapFallbackCapBytes > 0 && fallbackBytes > heapFallbackCapBytes) {
    fallbackBytes = heapFallbackCapBytes & ~static_cast<size_t>(1);
  }
  if (fallbackBytes < minBytes) {
    logf("[%s] buffer alloc failed target=%u largest=%u",
         tag,
         static_cast<unsigned>(targetBytes), static_cast<unsigned>(largest));
    return false;
  }

  buffer = static_cast<int16_t*>(heap_caps_malloc(fallbackBytes, MALLOC_CAP_8BIT));
  if (!buffer) {
    logf("[%s] fallback alloc failed bytes=%u largest=%u",
         tag,
         static_cast<unsigned>(fallbackBytes), static_cast<unsigned>(largest));
    return false;
  }

  capacityBytes = fallbackBytes;
  capacitySamples = fallbackBytes / sizeof(int16_t);
  logf("[%s] buffer fallback bytes=%u samples=%u",
       tag,
       static_cast<unsigned>(capacityBytes),
       static_cast<unsigned>(capacitySamples));
  return true;
}

bool ensureRecordBuffer() {
  return ensurePcmBuffer(
      gRecordBuffer,
      gRecordCapacityBytes,
      gRecordCapacitySamples,
      kRecordBufferBytes,
      kMinRecordBufferBytes,
      kMaxHeapRecordFallbackBytes,
      "REC");
}

bool ensureTtsBuffer() {
  return ensurePcmBuffer(
      gTtsBuffer,
      gTtsCapacityBytes,
      gTtsCapacitySamples,
      kTtsBufferBytes,
      kMinTtsBufferBytes,
      kMaxHeapTtsFallbackBytes,
      "TTS");
}

void releasePcmBuffer(int16_t*& buffer, size_t& capacityBytes, size_t& capacitySamples, const char* tag) {
  if (!buffer) {
    return;
  }
  free(buffer);
  buffer = nullptr;
  capacityBytes = 0;
  capacitySamples = 0;
  logf("[%s] buffer released", tag);
}

void releaseRecordBuffer() {
  releasePcmBuffer(gRecordBuffer, gRecordCapacityBytes, gRecordCapacitySamples, "REC");
}

void releaseTtsBuffer() {
  releasePcmBuffer(gTtsBuffer, gTtsCapacityBytes, gTtsCapacitySamples, "TTS");
}

void logHeapStats(const char* tag) {
  logf("[HEAP] %s free=%u largest=%u",
       tag,
       static_cast<unsigned>(ESP.getFreeHeap()),
       static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}

uint32_t currentRecordLimitMs() {
  if (!gRecordCapacitySamples) {
    return static_cast<uint32_t>(kMaxRecordSeconds * 1000.0f);
  }
  uint32_t capacityMs = static_cast<uint32_t>((gRecordCapacitySamples * 1000ULL) / kMicSampleRate);
  uint32_t configuredMs = static_cast<uint32_t>(kMaxRecordSeconds * 1000.0f);
  return capacityMs < configuredMs ? capacityMs : configuredMs;
}

void initSpeaker() {
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(gSpeakerVolume);
}

void initStorage() {
  gStorageReady = LittleFS.begin(true);
  if (!gStorageReady) {
    pushError("LittleFS init failed.");
    setStatus("Storage unavailable", 1000);
    return;
  }
  ensureVoiceStorage();
  loadVoiceNotes();
  pushSystem("Storage ready");
}

void startMic() {
  M5Cardputer.Speaker.end();
  auto micCfg = M5Cardputer.Mic.config();
  micCfg.sample_rate = kMicSampleRate;
  micCfg.magnification = 64;
  micCfg.noise_filter_level = 64;
  micCfg.task_priority = 1;
  M5Cardputer.Mic.config(micCfg);
  M5Cardputer.Mic.begin();
}

void stopMic() {
  M5Cardputer.Mic.end();
  initSpeaker();
}

void startRecording() {
  if (gRecording || gSubmitting || gThinking || gPlaybackActive) {
    logf("[VOICE] start ignored rec=%d submit=%d think=%d play=%d", gRecording, gSubmitting,
         gThinking, gPlaybackActive);
    return;
  }
  if (!gWifiReady || gHubHost.isEmpty()) {
    pushError("Hub is not configured for voice.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice unavailable", 1500);
    setVoiceDiag("voice: no wifi/hub");
    logf("[VOICE] start blocked wifi=%d hub=%s", gWifiReady, gHubHost.c_str());
    return;
  }
  if (!gPlaybackActive && gTtsBuffer) {
    releaseTtsBuffer();
  }
  logHeapStats("voice-start");
  if (!ensureRecordBuffer()) {
    pushError("Audio buffer allocation failed.");
    setFaceMode(FaceMode::Error);
    setStatus("No audio buffer", 1500);
    setVoiceDiag("voice: record buffer failed");
    logf("[VOICE] audio buffer allocation failed");
    return;
  }

  gRecordedSamples = 0;
  gRecording = true;
  gRecordStartMs = millis();
  startMic();
  M5Cardputer.Mic.record(gRecordBuffer, gRecordCapacitySamples, kMicSampleRate);
  setFaceMode(FaceMode::Listening);
  setStatus("Listening...");
  setVoiceDiag("voice: recording");
  render();
  logf("[VOICE] recording started capacity_samples=%u", static_cast<unsigned>(gRecordCapacitySamples));
}

void writeWavHeader(uint8_t* h, uint32_t dataSize) {
  uint32_t fileSize = 36 + dataSize;
  uint32_t byteRate = kMicSampleRate * 2;
  uint16_t blockAlign = 2;

  h[0] = 'R'; h[1] = 'I'; h[2] = 'F'; h[3] = 'F';
  h[4] = fileSize & 0xFF;
  h[5] = (fileSize >> 8) & 0xFF;
  h[6] = (fileSize >> 16) & 0xFF;
  h[7] = (fileSize >> 24) & 0xFF;
  h[8] = 'W'; h[9] = 'A'; h[10] = 'V'; h[11] = 'E';
  h[12] = 'f'; h[13] = 'm'; h[14] = 't'; h[15] = ' ';
  h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0;
  h[20] = 1; h[21] = 0;
  h[22] = 1; h[23] = 0;
  h[24] = kMicSampleRate & 0xFF;
  h[25] = (kMicSampleRate >> 8) & 0xFF;
  h[26] = (kMicSampleRate >> 16) & 0xFF;
  h[27] = (kMicSampleRate >> 24) & 0xFF;
  h[28] = byteRate & 0xFF;
  h[29] = (byteRate >> 8) & 0xFF;
  h[30] = (byteRate >> 16) & 0xFF;
  h[31] = (byteRate >> 24) & 0xFF;
  h[32] = blockAlign & 0xFF;
  h[33] = (blockAlign >> 8) & 0xFF;
  h[34] = 16; h[35] = 0;
  h[36] = 'd'; h[37] = 'a'; h[38] = 't'; h[39] = 'a';
  h[40] = dataSize & 0xFF;
  h[41] = (dataSize >> 8) & 0xFF;
  h[42] = (dataSize >> 16) & 0xFF;
  h[43] = (dataSize >> 24) & 0xFF;
}

bool parseHttpStatusAndHeaders(
    WiFiClient& client,
    int& statusCode,
    int& contentLength,
    uint32_t& sampleRate,
    uint32_t timeoutMs = 30000) {
  statusCode = 0;
  contentLength = -1;
  sampleRate = kTtsDefaultSampleRate;

  unsigned long deadline = millis() + timeoutMs;
  String line;
  auto readLine = [&](String& out) -> bool {
    while (millis() < deadline) {
      if (client.available()) {
        out = client.readStringUntil('\n');
        return true;
      }
      if (!client.connected()) {
        break;
      }
      delay(1);
    }
    out = "";
    return false;
  };

  while (millis() < deadline) {
    if (!readLine(line)) {
      break;
    }
    line.trim();
    if (line.isEmpty() || !line.startsWith("HTTP/")) {
      continue;
    }

    int firstSpace = line.indexOf(' ');
    if (firstSpace >= 0) {
      statusCode = line.substring(firstSpace + 1).toInt();
    }
    logf("[HTTP] status line=%s", line.c_str());

    while (readLine(line)) {
      if (line == "\r" || line.length() == 0) {
        break;
      }
      String header = line;
      header.trim();
      String lower = header;
      lower.toLowerCase();
      if (lower.startsWith("content-length:")) {
        contentLength = header.substring(15).toInt();
      } else if (lower.startsWith("x-audio-sample-rate:")) {
        sampleRate = static_cast<uint32_t>(header.substring(20).toInt());
      }
    }

    if (statusCode == 100) {
      statusCode = 0;
      contentLength = -1;
      sampleRate = kTtsDefaultSampleRate;
      continue;
    }
    return statusCode > 0;
  }

  return false;
}

bool writeClientAll(WiFiClient& client, const uint8_t* data, size_t length, const char* label) {
  size_t written = 0;
  unsigned long deadline = millis() + 45000;
  unsigned long lastProgress = millis();
  while (written < length && millis() < deadline) {
    if (WiFi.status() != WL_CONNECTED) {
      setHttpDiag(String(label) + ": wifi lost");
      logf("[HTTP] wifi lost during write label=%s written=%u total=%u",
           label,
           static_cast<unsigned>(written),
           static_cast<unsigned>(length));
      break;
    }
    if (!client.connected()) {
      break;
    }
    size_t chunk = length - written;
    if (chunk > kHttpWriteChunkBytes) {
      chunk = kHttpWriteChunkBytes;
    }
    size_t sent = client.write(data + written, chunk);
    if (sent == 0) {
      if (millis() - lastProgress > 3000) {
        setHttpDiag(String(label) + ": stalled");
        logf("[HTTP] stalled write label=%s written=%u total=%u",
             label,
             static_cast<unsigned>(written),
             static_cast<unsigned>(length));
        break;
      }
      yield();
      delay(2);
      continue;
    }
    written += sent;
    lastProgress = millis();
    yield();
    delay(1);
  }

  if (written != length) {
    setHttpDiag(String(label) + ": short " + String(static_cast<unsigned>(written)) + "/" +
                String(static_cast<unsigned>(length)));
    logf("[HTTP] short write label=%s written=%u total=%u", label,
         static_cast<unsigned>(written), static_cast<unsigned>(length));
    return false;
  }
  setHttpDiag(String(label) + ": ok");
  return true;
}

bool writeClientAll(WiFiClient& client, const String& text, const char* label) {
  return writeClientAll(client, reinterpret_cast<const uint8_t*>(text.c_str()), text.length(), label);
}

bool writeClientAllStaged(WiFiClient& client, const uint8_t* data, size_t length, const char* label) {
  uint8_t stage[kHttpStageChunkBytes];
  size_t written = 0;
  unsigned long deadline = millis() + 45000;
  unsigned long lastProgress = millis();

  while (written < length && millis() < deadline) {
    if (WiFi.status() != WL_CONNECTED) {
      setHttpDiag(String(label) + ": wifi lost");
      logf("[HTTP] staged wifi lost label=%s written=%u total=%u",
           label,
           static_cast<unsigned>(written),
           static_cast<unsigned>(length));
      break;
    }
    if (!client.connected()) {
      setHttpDiag(String(label) + ": disconnected");
      break;
    }

    size_t chunk = length - written;
    if (chunk > sizeof(stage)) {
      chunk = sizeof(stage);
    }
    memcpy(stage, data + written, chunk);
    size_t sent = client.write(stage, chunk);
    if (sent == 0) {
      if (millis() - lastProgress > 3000) {
        setHttpDiag(String(label) + ": stalled");
        logf("[HTTP] staged stalled label=%s written=%u total=%u",
             label,
             static_cast<unsigned>(written),
             static_cast<unsigned>(length));
        break;
      }
      yield();
      delay(2);
      continue;
    }
    written += sent;
    lastProgress = millis();
    if ((written % (8 * 1024)) == 0 || written == length) {
      setHttpDiag(String(label) + ": " + String(static_cast<unsigned>(written / 1024)) + "k/" +
                  String(static_cast<unsigned>(length / 1024)) + "k");
    }
    yield();
    delay(1);
  }

  if (written != length) {
    setHttpDiag(String(label) + ": short " + String(static_cast<unsigned>(written)) + "/" +
                String(static_cast<unsigned>(length)));
    logf("[HTTP] staged short write label=%s written=%u total=%u",
         label,
         static_cast<unsigned>(written),
         static_cast<unsigned>(length));
    return false;
  }
  setHttpDiag(String(label) + ": ok " + String(static_cast<unsigned>(length / 1024)) + "k");
  return true;
}

bool connectHubClient(WiFiClient& client, const char* reason, uint32_t readTimeoutSec) {
  if (!ensureWifiForHttp(reason)) {
    setHttpDiag(String(reason ? reason : "?") + ": wifi unavailable");
    return false;
  }
  for (uint8_t attempt = 0; attempt < kHttpConnectRetries; ++attempt) {
    client.stop();
    delay(30);
    if (attempt > 0) {
      ensureWifiForHttp(reason);
      delay(150);
    }
    if (client.connect(gHubHost.c_str(), gHubPort)) {
      client.setTimeout(readTimeoutSec);
      client.setNoDelay(true);
      setHttpDiag(String(reason ? reason : "?") + ": connect ok");
      logf("[HTTP] connect ok reason=%s attempt=%u ip=%s",
           reason ? reason : "?",
           static_cast<unsigned>(attempt + 1),
           WiFi.localIP().toString().c_str());
      return true;
    }
    setHttpDiag(String(reason ? reason : "?") + ": connect fail");
    logf("[HTTP] connect fail reason=%s attempt=%u wifi=%d ip=%s",
         reason ? reason : "?",
         static_cast<unsigned>(attempt + 1),
         static_cast<int>(WiFi.status()),
         WiFi.localIP().toString().c_str());
  }
  return false;
}

String escapeJsonString(const String& raw) {
  String out;
  out.reserve(raw.length() + 16);
  for (size_t i = 0; i < raw.length(); ++i) {
    char c = raw[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<uint8_t>(c) >= 0x20) {
          out += c;
        }
        break;
    }
  }
  return out;
}

String authHeaderLine() {
  if (gDeviceToken.isEmpty()) {
    return "";
  }
  return "Authorization: Bearer " + gDeviceToken + "\r\n";
}

bool requestAndPlayTts(const String& text) {
  if (text.isEmpty()) {
    setVoiceDiag("tts: empty/buffer");
    return false;
  }
  if (!gWifiReady || gHubHost.isEmpty()) {
    setVoiceDiag("tts: wifi unavailable");
    return false;
  }

  if (!gRecording && gRecordBuffer) {
    releaseRecordBuffer();
  }

  logHeapStats("tts-start");

  int16_t* playbackBuffer = gTtsBuffer;
  size_t playbackCapacityBytes = gTtsCapacityBytes;
  if (!playbackBuffer) {
    if (ensureTtsBuffer()) {
      playbackBuffer = gTtsBuffer;
      playbackCapacityBytes = gTtsCapacityBytes;
    } else {
      setVoiceDiag("tts: empty/buffer");
      return false;
    }
  }

  if (gDynamicPlaybackBuffer && !gPlaybackActive) {
    free(gDynamicPlaybackBuffer);
    gDynamicPlaybackBuffer = nullptr;
  }

  bool pausedWsForTts = false;
  if (gWsReady) {
    pauseHubWebSocketForVoice();
    pausedWsForTts = true;
  }

  WiFiClient client;
  client.setTimeout(30);
  if (!client.connect(gHubHost.c_str(), gHubPort)) {
    logf("[TTS] connect failed host=%s port=%u", gHubHost.c_str(), gHubPort);
    setVoiceDiag("tts: connect failed");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }
  client.setNoDelay(true);

  String body =
      "{\"device_id\":\"" + escapeJsonString(gDeviceId) + "\",\"text\":\"" +
      escapeJsonString(text) + "\"}";
  String authHeader = authHeaderLine();

  client.printf(
      "POST %s HTTP/1.1\r\n"
      "Host: %s:%u\r\n"
      "%s"
      "Content-Type: application/json\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n\r\n",
      kDeviceTtsPath,
      gHubHost.c_str(),
      gHubPort,
      authHeader.c_str(),
      static_cast<unsigned>(body.length()));
  if (!writeClientAll(client, body, "tts_body")) {
    client.stop();
    setVoiceDiag("tts: body write failed");
    releaseTtsBuffer();
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  int statusCode = 0;
  int contentLength = -1;
  uint32_t sampleRate = kTtsDefaultSampleRate;
  if (!parseHttpStatusAndHeaders(client, statusCode, contentLength, sampleRate, 45000) || statusCode != 200) {
    logf("[TTS] bad response status=%d", statusCode);
    client.stop();
    setVoiceDiag("tts: bad status " + String(statusCode));
    releaseTtsBuffer();
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  size_t maxBytes = playbackCapacityBytes;
  size_t toRead = maxBytes;
  uint8_t* dynamicPcmBuffer = nullptr;
  bool dynamicPlayback = false;
  if (contentLength > 0 && static_cast<size_t>(contentLength) < toRead) {
    toRead = static_cast<size_t>(contentLength);
  } else if (contentLength > 0 && static_cast<size_t>(contentLength) > maxBytes) {
    dynamicPcmBuffer = static_cast<uint8_t*>(
        heap_caps_malloc(static_cast<size_t>(contentLength), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!dynamicPcmBuffer) {
      dynamicPcmBuffer = static_cast<uint8_t*>(
          heap_caps_malloc(static_cast<size_t>(contentLength), MALLOC_CAP_8BIT));
    }
    if (dynamicPcmBuffer) {
      if (gTtsBuffer) {
        releaseTtsBuffer();
      }
      playbackBuffer = reinterpret_cast<int16_t*>(dynamicPcmBuffer);
      playbackCapacityBytes = static_cast<size_t>(contentLength);
      toRead = static_cast<size_t>(contentLength);
      dynamicPlayback = true;
      logf("[TTS] dynamic buffer content_length=%u", static_cast<unsigned>(contentLength));
      setVoiceDiag("tts: dyn " + String(static_cast<unsigned>(contentLength / 1024)) + "k");
    } else {
      logf("[TTS] clipping response content_length=%u max_bytes=%u",
           static_cast<unsigned>(contentLength),
           static_cast<unsigned>(maxBytes));
      setVoiceDiag("tts: clip " + String(static_cast<unsigned>(maxBytes / 1024)) + "k");
    }
  }

  size_t bytesRead = 0;
  uint8_t* dst = reinterpret_cast<uint8_t*>(playbackBuffer);
  unsigned long deadline = millis() + 45000;
  while (bytesRead < toRead && (client.connected() || client.available()) && millis() < deadline) {
    if (!client.available()) {
      delay(1);
      continue;
    }
    int avail = client.available();
    int chunk = avail;
    if (bytesRead + static_cast<size_t>(chunk) > toRead) {
      chunk = static_cast<int>(toRead - bytesRead);
    }
    int got = client.read(dst + bytesRead, chunk);
    if (got > 0) {
      bytesRead += static_cast<size_t>(got);
    }
  }
  client.stop();
  delay(20);

  size_t samples = bytesRead / sizeof(int16_t);
  if (samples == 0) {
    logf("[TTS] empty PCM response");
    setVoiceDiag("tts: empty pcm");
    if (dynamicPcmBuffer) {
      free(dynamicPcmBuffer);
    }
    releaseTtsBuffer();
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  uint8_t wavHeader[44];
  writeWavHeader(wavHeader, static_cast<uint32_t>(bytesRead));
  rememberVoiceNote("Assistant voice", text, wavHeader, sizeof(wavHeader),
                    reinterpret_cast<const uint8_t*>(playbackBuffer), bytesRead, sampleRate, true);

  gPlaybackActive = true;
  setFaceMode(FaceMode::Speaking);
  setStatus("Speaking...");
  M5Cardputer.Speaker.playRaw(playbackBuffer, samples, sampleRate, false);
  if (dynamicPlayback) {
    gDynamicPlaybackBuffer = dynamicPcmBuffer;
  }
  logf("[TTS] playback samples=%u sample_rate=%u", static_cast<unsigned>(samples), sampleRate);
  setVoiceDiag("tts: playing " + String(static_cast<unsigned>(samples)));
  if (pausedWsForTts) {
    resumeHubWebSocketAfterVoice();
  }
  return true;
}

bool sendTextTurn(const String& text) {
  if ((!gWifiReady && !ensureWifiForHttp("text-turn", 3000)) || gHubHost.isEmpty()) {
    pushError("Hub is not configured.");
    return false;
  }

  HTTPClient http;
  String url = "http://" + gHubHost + ":" + String(gHubPort) + kDeviceTextTurnPath;
  http.setTimeout(60000);
  if (!http.begin(url)) {
    pushError("Failed to open HTTP connection.");
    return false;
  }

  DynamicJsonDocument doc(512);
  doc["message"] = text;
  doc["device_id"] = gDeviceId;

  String body;
  serializeJson(doc, body);
  http.addHeader("Content-Type", "application/json");
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }

  gSubmitting = true;
  gThinking = true;
  gAssistantPendingVisible = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Sending...");
  render();
  gLastRenderMs = millis();
  delay(10);

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  gSubmitting = false;
  gThinking = false;
  gAssistantPendingVisible = false;

  if (code < 200 || code >= 300) {
    logf("[TEXT] hub error code=%d body=%s", code, response.c_str());
    pushError("Hub HTTP error: " + String(code));
    if (!response.isEmpty()) {
      pushError(response);
    }
    setFaceMode(FaceMode::Error);
    setStatus("Text turn failed", 2000);
    return false;
  }

  DynamicJsonDocument resp(2048);
  auto err = deserializeJson(resp, response);
  if (err) {
    pushError("Could not parse hub response.");
    setFaceMode(FaceMode::Error);
    setStatus("Bad response", 2000);
    return false;
  }

  if (!resp["reply"].isNull()) {
    String reply = resp["reply"].as<String>();
    appendAssistantReply(reply);
    requestAndPlayTts(reply);
    logf("[TEXT] reply=%s", reply.c_str());
  } else {
    pushSystem("Hub accepted the turn.");
    logf("[TEXT] accepted without reply");
  }
  setFaceMode(FaceMode::Idle);
  setStatus("Reply received", 1200);
  return true;
}

bool stopRecordingAndSend() {
  if (!gRecording) {
    return false;
  }
  gRecording = false;

  float elapsed = (millis() - gRecordStartMs) / 1000.0f;
  gRecordedSamples = static_cast<size_t>(elapsed * kMicSampleRate);
  if (gRecordedSamples > gRecordCapacitySamples) {
    gRecordedSamples = gRecordCapacitySamples;
  }
  stopMic();
  logf("[VOICE] recording stopped elapsed=%.2f samples=%u", elapsed,
       static_cast<unsigned>(gRecordedSamples));

  if (elapsed < kMinRecordSeconds || gRecordedSamples == 0) {
    setFaceMode(FaceMode::Idle);
    setStatus("Voice turn canceled", 1200);
    logf("[VOICE] canceled: too short");
    releaseRecordBuffer();
    return false;
  }

  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (!gWifiReady) {
    pushError("Wi-Fi dropped during recording.");
    setFaceMode(FaceMode::Error);
    setStatus("Wi-Fi lost", 2000);
    setVoiceDiag("voice: wifi lost");
    logf("[VOICE] wifi lost before upload");
    releaseRecordBuffer();
    return false;
  }

  gSubmitting = true;
  gThinking = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Uploading...");
  render();
  gLastRenderMs = millis();
  delay(10);
  pauseHubWebSocketForVoice();
  auto finishVoiceTurn = [&]() {
    gSubmitting = false;
    gThinking = false;
    resumeHubWebSocketAfterVoice();
  };

  uint32_t dataSize = static_cast<uint32_t>(gRecordedSamples * sizeof(int16_t));
  uint8_t wavHeader[44];
  writeWavHeader(wavHeader, dataSize);
  rememberVoiceNote("My voice", "Outbound voice note", wavHeader, sizeof(wavHeader),
                    reinterpret_cast<const uint8_t*>(gRecordBuffer), dataSize, kMicSampleRate, false);
  logHeapStats("voice-upload");

  WiFiClient client;
  if (!connectHubClient(client, "voice-upload", 45)) {
    pushError("Voice upload connect failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice upload failed", 2000);
    setVoiceDiag("voice: upload connect failed");
    logf("[VOICE] upload connect failed host=%s port=%u", gHubHost.c_str(), gHubPort);
    finishVoiceTurn();
    releaseRecordBuffer();
    return false;
  }

  String authHeader = authHeaderLine();
  String request =
      String("POST ") + kDeviceAudioTurnRawPath + " HTTP/1.1\r\n" +
      "Host: " + gHubHost + ":" + String(gHubPort) + "\r\n" +
      authHeader +
      "Content-Type: application/octet-stream\r\n" +
      "X-Audio-Sample-Rate: " + String(kMicSampleRate) + "\r\n" +
      "Content-Length: " + String(dataSize) + "\r\n" +
      "Connection: close\r\n\r\n";

  if (!writeClientAll(client, request, "voice_raw_header")) {
    client.stop();
    pushError("Voice upload body failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice upload failed", 2000);
    setVoiceDiag("voice: upload header failed");
    logf("[VOICE] upload raw header write failed");
    finishVoiceTurn();
    releaseRecordBuffer();
    return false;
  }

  setStatus("Transcribing...");
  render();
  gLastRenderMs = millis();
  const uint8_t* pcmBytes = reinterpret_cast<const uint8_t*>(gRecordBuffer);
  if (!writeClientAllStaged(client, pcmBytes, dataSize, "voice_pcm")) {
    client.stop();
    pushError("Voice upload body failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice upload failed", 2000);
    setVoiceDiag("voice: upload body failed");
    logf("[VOICE] upload body write failed bytes=%u", static_cast<unsigned>(dataSize));
    finishVoiceTurn();
    releaseRecordBuffer();
    return false;
  }

  int statusCode = 0;
  int contentLength = -1;
  uint32_t ignoredRate = 0;
  if (!parseHttpStatusAndHeaders(client, statusCode, contentLength, ignoredRate, 90000)) {
    client.stop();
    pushError("Voice response header failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice turn failed", 2000);
    setVoiceDiag("voice: response header failed");
    logf("[VOICE] response header failed");
    finishVoiceTurn();
    releaseRecordBuffer();
    return false;
  }

  String response;
  unsigned long deadline = millis() + 45000;
  while ((client.connected() || client.available()) && millis() < deadline) {
    while (client.available()) {
      response += static_cast<char>(client.read());
    }
    delay(1);
  }
  client.stop();
  finishVoiceTurn();
  releaseRecordBuffer();

  if (statusCode < 200 || statusCode >= 300) {
    logf("[VOICE] HTTP error status=%d body=%s", statusCode, response.c_str());
    pushError("Voice HTTP error: " + String(statusCode));
    if (!response.isEmpty()) {
      pushError(response);
    }
    setFaceMode(FaceMode::Error);
    setStatus("Voice turn failed", 2000);
    setVoiceDiag("voice: http " + String(statusCode));
    return false;
  }

  DynamicJsonDocument resp(4096);
  auto err = deserializeJson(resp, response);
  if (err) {
    pushError("Could not parse voice reply.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice parse failed", 2000);
    setVoiceDiag("voice: json parse failed");
    logf("[VOICE] JSON parse failed body=%s", response.c_str());
    return false;
  }

  String transcript = resp["transcript"] | "";
  String reply = resp["reply"] | "";
  logf("[VOICE] transcript=%s", transcript.c_str());
  logf("[VOICE] reply=%s", reply.c_str());
  if (!transcript.isEmpty()) {
    pushUser(transcript);
    setVoiceDiag("voice: transcript ok");
  }
  if (!reply.isEmpty()) {
    appendAssistantReply(reply);
    if (!requestAndPlayTts(reply)) {
      logf("[VOICE] TTS request failed");
      setFaceMode(FaceMode::Idle);
      setStatus("Reply text only", 1200);
    }
  } else {
    setFaceMode(FaceMode::Idle);
    setStatus("No reply", 1200);
  }
  return true;
}

class ProvisionCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    std::string rx = characteristic->getValue();
    if (rx.empty() || rx.size() > 900) {
      return;
    }

    DynamicJsonDocument doc(768);
    DeserializationError err = deserializeJson(doc, rx);
    if (err) {
      return;
    }

    bool changed = false;

    if (!doc["ssid"].isNull()) {
      String ssid = doc["ssid"].as<String>();
      ssid.trim();
      if (!ssid.isEmpty()) {
        saveWifiCreds(ssid, doc["password"].as<String>());
        changed = true;
      }
    }

    if (!doc["hub_ip"].isNull()) {
      String host = doc["hub_ip"].as<String>();
      host.trim();
      uint16_t port = static_cast<uint16_t>(doc["hub_port"] | kDefaultHubPort);
      if (!host.isEmpty()) {
        saveHubEndpoint(host, port);
        changed = true;
      }
    }

    if (changed) {
      setStatus("Provisioning saved, rebooting...");
      gPendingRestart = true;
    }
  }
};

void startBleProvisioning() {
  if (gBleActive) {
    return;
  }

  if (gWsReady) {
    gWs.disconnect();
    gWsReady = false;
  }
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  delay(100);

  BLEDevice::init(kBleServiceName);
  BLEServer* server = BLEDevice::createServer();
  BLEService* service = server->createService(kBleServiceUuid);
  BLECharacteristic* characteristic = service->createCharacteristic(
      kBleWifiCharUuid, BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new ProvisionCallbacks());
  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kBleServiceUuid);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  gBleActive = true;
  gWifiReady = false;
  setStatus("Waiting for BLE provisioning");
  pushSystem("Bluetooth provisioning started.");
}

void syncClockFromNtp(bool force) {
  ensureBangkokTimezone();
  if (!gWifiReady) {
    return;
  }
  if (!force) {
    if (gTimeValid && gLastTimeSyncMs != 0 && millis() - gLastTimeSyncMs < kTimeResyncMs) {
      return;
    }
    if (gLastTimeSyncAttemptMs != 0 && millis() - gLastTimeSyncAttemptMs < kTimeSyncRetryMs) {
      return;
    }
  }

  gLastTimeSyncAttemptMs = millis();
  configTzTime(kBangkokTz, "time.google.com", "pool.ntp.org", "time.cloudflare.com");
  uint32_t start = millis();
  while (millis() - start < 8000) {
    if (hasValidSystemTime()) {
      gTimeValid = true;
      gLastTimeSyncMs = millis();
      persistRtcFromSystemTime();
      setStatus("Clock synced", 1000);
      logf("[TIME] synced epoch=%lu", static_cast<unsigned long>(time(nullptr)));
      return;
    }
    delay(100);
  }
  logf("[TIME] sync timed out");
}

bool ensureWifi() {
  if (gWifiSsid.isEmpty()) {
    pushSystem("No Wi-Fi credentials. Entering BLE setup.");
    startBleProvisioning();
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.begin(gWifiSsid.c_str(), gWifiPassword.c_str());
  setStatus("Connecting to Wi-Fi...");

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiConnectTimeoutMs) {
    delay(120);
  }

  if (WiFi.status() != WL_CONNECTED) {
    pushError("Wi-Fi connection failed. Entering BLE setup.");
    startBleProvisioning();
    logf("[WIFI] connect failed ssid=%s", gWifiSsid.c_str());
    return false;
  }

  gWifiReady = true;
  setStatus("Wi-Fi connected", 1000);
  pushSystem("Wi-Fi connected: " + WiFi.localIP().toString());
  logf("[WIFI] connected ssid=%s ip=%s", gWifiSsid.c_str(), WiFi.localIP().toString().c_str());
  syncClockFromNtp(true);
  return true;
}

bool ensureWifiForHttp(const char* reason, uint32_t timeoutMs) {
  (void)reason;
  (void)timeoutMs;
  if (gWifiSsid.isEmpty() || gHubHost.isEmpty()) {
    gWifiReady = false;
    return false;
  }

  gWifiReady = WiFi.status() == WL_CONNECTED;
  return gWifiReady;
}

void sendDeviceHello() {
  if (!kUseHubWebSocket) {
    return;
  }
  DynamicJsonDocument doc(512);
  doc["type"] = "device_hello";
  doc["device_id"] = gDeviceId;
  doc["display_name"] = gDisplayName;
  doc["device_type"] = kDeviceType;

  JsonObject caps = doc.createNestedObject("capabilities");
  caps["text_input"] = true;
  caps["voice_input"] = true;
  caps["vision"] = false;
  caps["face_animation"] = true;
  caps["presence_scan"] = false;
  caps["face_enrollment"] = false;
  caps["ble_provisioning"] = true;

  String payload;
  serializeJson(doc, payload);
  gWs.sendTXT(payload);
}

void handleHubJson(const JsonDocument& doc) {
  const String msgType = doc["type"] | "";
  const String status = doc["status"] | "";

  if (msgType == "assistant_speech_face") {
    const String event = doc["event"] | "";
    if (event == "start") {
      setFaceMode(FaceMode::Speaking);
    } else if (event == "stop" && !gPlaybackActive) {
      setFaceMode(FaceMode::Idle);
    }
    return;
  }

  if (msgType == "gemini_first_token") {
    gThinking = true;
    setFaceMode(FaceMode::Thinking);
    setStatus("Hub is thinking...");
    return;
  }

  if (msgType == "runtime_timezone" || msgType == "runtime_vision" ||
      msgType == "runtime_presence_scan" || msgType == "runtime_wake_word" ||
      msgType == "runtime_live_voice" || msgType == "runtime_sleep_timeout") {
    setStatus("Runtime settings updated", 1200);
    return;
  }

  if (status == "success" && !doc["reply"].isNull()) {
    gSubmitting = false;
    gThinking = false;
    pushAssistant(doc["reply"].as<String>());
    setFaceMode(FaceMode::Idle);
    setStatus("Reply received", 1200);
    return;
  }

  if (status == "error" && !doc["message"].isNull()) {
    gSubmitting = false;
    gThinking = false;
    pushError(doc["message"].as<String>());
    setFaceMode(FaceMode::Error);
    setStatus("Hub error", 1800);
    logf("[HUB] error=%s", doc["message"].as<String>().c_str());
  }
}

void wsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      gWsReady = false;
      setStatus("Hub disconnected");
      logf("[WS] disconnected");
      break;
    case WStype_CONNECTED:
      gWsReady = true;
      setStatus("Hub connected", 1200);
      pushSystem("Hub WebSocket connected.");
      sendDeviceHello();
      logf("[WS] connected host=%s port=%u", gHubHost.c_str(), gHubPort);
      break;
    case WStype_TEXT: {
      DynamicJsonDocument doc(1024);
      auto err = deserializeJson(doc, payload, length);
      if (!err) {
        handleHubJson(doc);
      }
      break;
    }
    default:
      break;
  }
}

void startHubWebSocket() {
  if (!kUseHubWebSocket) {
    gWsReady = false;
    setStatus("Hub HTTP mode", 1200);
    return;
  }
  if (gHubHost.isEmpty()) {
    pushError("Hub endpoint is empty. Use BLE provisioning.");
    return;
  }

  gWs.begin(gHubHost.c_str(), gHubPort, kWsPath);
  gWs.onEvent(wsEvent);
  gWs.setReconnectInterval(5000);
  gWs.enableHeartbeat(15000, 3000, 2);
  gLastWsReconnectMs = millis();
  setStatus("Connecting to hub...");
}

void pauseHubWebSocketForVoice() {
  if (!kUseHubWebSocket) {
    gResumeWsAfterVoice = false;
    return;
  }
  if (gBleActive || gHubHost.isEmpty()) {
    return;
  }
  gResumeWsAfterVoice = true;
  if (gWsReady) {
    logf("[WS] pause for voice");
    gWs.disconnect();
    gWsReady = false;
  }
}

void resumeHubWebSocketAfterVoice() {
  if (!kUseHubWebSocket) {
    gResumeWsAfterVoice = false;
    return;
  }
  if (!gResumeWsAfterVoice) {
    return;
  }
  gResumeWsAfterVoice = false;
  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (!gBleActive && gWifiReady && !gHubHost.isEmpty()) {
    logf("[WS] resume after voice");
    startHubWebSocket();
  }
}

void maintainConnectivity() {
  bool wifiNow = WiFi.status() == WL_CONNECTED;

  if (!wifiNow) {
    if (gWifiReady) {
      gWifiReady = false;
      gWsReady = false;
      setStatus("Wi-Fi lost", 1500);
      pushError("Wi-Fi disconnected.");
      logf("[WIFI] disconnected");
    }
    uint32_t now = millis();
    if (now - gLastWifiReconnectMs >= kWifiReconnectIntervalMs && !gSubmitting && !gRecording) {
      gLastWifiReconnectMs = now;
      logf("[WIFI] background reconnect attempt");
      ensureWifiForHttp("background-reconnect", 3000);
    }
    return;
  }

  if (!gWifiReady) {
    gWifiReady = true;
    setStatus("Wi-Fi restored", 1200);
    pushSystem("Wi-Fi restored: " + WiFi.localIP().toString());
    logf("[WIFI] restored ip=%s", WiFi.localIP().toString().c_str());
    syncClockFromNtp(true);
    if (kUseHubWebSocket && !gBleActive && !gHubHost.isEmpty() && !gWsReady && !gResumeWsAfterVoice) {
      logf("[WS] restart after wifi restore");
      startHubWebSocket();
    }
  }
}

bool handleLocalCommand(const String& input) {
  String cmd = input;
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "/help") {
    pushSystem("Ctrl+Space = EN/RU");
    pushSystem("Ctrl x2 = start/stop voice");
    pushSystem("Tab + < > = mode switch, Ctrl+D = debug");
    pushSystem("/face /chat /clock /voice /dbg");
    return true;
  }

  if (cmd == "/status") {
    pushSystem("device_id=" + gDeviceId);
    pushSystem("wifi=" + String(gWifiReady ? WiFi.localIP().toString() : "offline"));
    pushSystem("hub=" + (gHubHost.isEmpty() ? String("unset") : gHubHost + ":" + String(gHubPort)));
    return true;
  }

  if (cmd == "/btsetup") {
    pushSystem("Switching to BLE provisioning mode.");
    startBleProvisioning();
    return true;
  }

  if (cmd == "/clearwifi") {
    pushSystem("Clearing Wi-Fi credentials and rebooting.");
    clearWifiCreds();
    delay(400);
    ESP.restart();
    return true;
  }

  if (cmd == "/dbg") {
    gDebugOverlayVisible = !gDebugOverlayVisible;
    setStatus(gDebugOverlayVisible ? "Debug overlay on" : "Debug overlay off", 1000);
    return true;
  }

  if (cmd == "/face") {
    setUiMode(UiMode::Face);
    return true;
  }

  if (cmd == "/chat") {
    setUiMode(UiMode::ChatFull);
    return true;
  }

  if (cmd == "/clock") {
    setUiMode(UiMode::Clock);
    return true;
  }

  if (cmd == "/voice") {
    setUiMode(UiMode::Voice);
    return true;
  }

  if (cmd == "/scrollup") {
    scrollChatBy(1);
    return true;
  }

  if (cmd == "/scrolldown") {
    scrollChatBy(-1);
    return true;
  }

  return false;
}

void submitInput() {
  String text = gInputBuffer;
  text.trim();
  if (text.isEmpty()) {
    return;
  }
  gInputBuffer = "";
  if (handleLocalCommand(text)) {
    return;
  }
  pushUser(text);
  sendTextTurn(text);
}

void handleLauncherKey(char key, const Keyboard_Class::KeysState& keys) {
  if (key >= '1' && key <= '5') {
    gLauncherSelection = key - '1';
    applyLauncherSelection();
    return;
  }
  if (keys.enter || key == '\n' || key == '\r') {
    applyLauncherSelection();
    return;
  }
  if (key == ',' || key == ';') {
    moveLauncherSelection(-1);
    applyLauncherSelection();
    return;
  }
  if (key == '.' || key == '/') {
    moveLauncherSelection(1);
    applyLauncherSelection();
    return;
  }
}

void handleVoicePlayerKey(char key, const Keyboard_Class::KeysState& keys) {
  if (gVoiceNotes.empty()) {
    setStatus("No voice notes", 1000);
    return;
  }
  if (key == ',' || key == ';') {
    gSelectedVoiceNote = max(0, gSelectedVoiceNote - 1);
    setStatus("Voice " + String(gSelectedVoiceNote + 1), 600);
    return;
  }
  if (key == '.' || key == '/') {
    gSelectedVoiceNote = min(static_cast<int>(gVoiceNotes.size()) - 1, gSelectedVoiceNote + 1);
    setStatus("Voice " + String(gSelectedVoiceNote + 1), 600);
    return;
  }
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    if (gPlaybackActive) {
      M5Cardputer.Speaker.stop();
      gPlaybackActive = false;
      setFaceMode(FaceMode::Idle);
      setStatus("Playback stopped", 800);
      return;
    }
    if (!playVoiceNoteAt(gSelectedVoiceNote)) {
      setStatus("Playback failed", 1000);
    }
    return;
  }
  if (key == '-' && gSpeakerVolume >= 32) {
    gSpeakerVolume -= 32;
    M5Cardputer.Speaker.setVolume(gSpeakerVolume);
    setStatus("Vol " + String(gSpeakerVolume), 600);
    return;
  }
  if ((key == '=' || key == '+') && gSpeakerVolume <= 223) {
    gSpeakerVolume += 32;
    M5Cardputer.Speaker.setVolume(gSpeakerVolume);
    setStatus("Vol " + String(gSpeakerVolume), 600);
  }
}

void handleTypingKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  setKeyDiag(String("key=") + String(static_cast<int>(key)));
  if (keys.ctrl && keys.space) {
    gKeyboardLayout = (gKeyboardLayout == KeyboardLayout::En) ? KeyboardLayout::Ru : KeyboardLayout::En;
    setStatus(String("Layout ") + layoutName(gKeyboardLayout), 900);
    return;
  }
  if (keys.ctrl && lowerKey == 'd') {
    gDebugOverlayVisible = !gDebugOverlayVisible;
    setStatus(gDebugOverlayVisible ? "Debug overlay on" : "Debug overlay off", 1000);
    setKeyDiag(gDebugOverlayVisible ? "dbg:on" : "dbg:off");
    return;
  }
  if (keys.tab) {
    uint32_t now = millis();
    if (now - gLastLauncherToggleMs < kLauncherToggleDebounceMs) {
      return;
    }
    gLastLauncherToggleMs = now;
    gLauncherVisible = !gLauncherVisible;
    setStatus(gLauncherVisible ? "Launcher" : "Launcher closed", 700);
    return;
  }
  if (gLauncherVisible) {
    handleLauncherKey(key, keys);
    return;
  }
  if (gUiMode == UiMode::Voice) {
    handleVoicePlayerKey(key, keys);
    return;
  }
  if (gInputBuffer.isEmpty() && (key == ',' || key == '<' || key == ';' ||
                                 (keys.ctrl && (lowerKey == 'p' || lowerKey == 'k')))) {
    logf("[CHAT] scroll up key=%d", static_cast<int>(key));
    scrollChatBy(1);
    return;
  }
  if (gInputBuffer.isEmpty() && (key == '.' || key == '>' || key == '/' ||
                                 (keys.ctrl && (lowerKey == 'n' || lowerKey == 'j')))) {
    logf("[CHAT] scroll down key=%d", static_cast<int>(key));
    scrollChatBy(-1);
    return;
  }
  if (keys.enter || key == '\r' || key == '\n') {
    submitInput();
    return;
  }
  if (keys.del || key == '\b') {
    if (!gInputBuffer.isEmpty()) {
      removeLastUtf8Char(gInputBuffer);
    }
    return;
  }
  if (key >= 32 && key < 127 && gInputBuffer.length() < static_cast<int>(kMaxInputChars)) {
    gInputBuffer += translateInputChar(key);
  }
}

bool hasHidKey(const Keyboard_Class::KeysState& keys, uint8_t keyCode) {
  for (uint8_t code : keys.hid_keys) {
    if (code == keyCode) {
      return true;
    }
  }
  return false;
}

void pollTyping() {
  if (gRecording || gSubmitting) {
    return;
  }

  auto& keys = gKeyboardState;
  static bool pEnter = false;
  static bool pDel = false;
  static bool pTab = false;
  static bool pSpace = false;
  static bool pUp = false;
  static bool pDown = false;
  static bool pLeft = false;
  static bool pRight = false;
  static String pWordSignature;

  bool enterDown = keys.enter && !pEnter;
  bool delDown = keys.del && !pDel;
  bool tabDown = keys.tab && !pTab;
  bool spaceDown = keys.space && !pSpace;
  bool upHeld = hasHidKey(keys, 0xDA);
  bool downHeld = hasHidKey(keys, 0xD9);
  bool leftHeld = hasHidKey(keys, 0xD8);
  bool rightHeld = hasHidKey(keys, 0xD7);
  bool upDown = upHeld && !pUp;
  bool downDown = downHeld && !pDown;
  bool leftDown = leftHeld && !pLeft;
  bool rightDown = rightHeld && !pRight;
  String wordSignature;
  wordSignature.reserve(keys.word.size());
  for (char c : keys.word) {
    wordSignature += c;
  }
  char curWordChar = keys.word.empty() ? 0 : keys.word[0];
  bool charDown = !wordSignature.isEmpty() && wordSignature != pWordSignature;
  bool tabPrevCombo = keys.tab && (leftDown || upDown ||
                                   (charDown && (curWordChar == ',' || curWordChar == ';' || curWordChar == '<')));
  bool tabNextCombo = keys.tab && (rightDown || downDown ||
                                   (charDown && (curWordChar == '.' || curWordChar == '/' || curWordChar == '>')));

  if (tabPrevCombo) {
    gLauncherVisible = true;
    handleLauncherKey(',', keys);
  } else if (tabNextCombo) {
    gLauncherVisible = true;
    handleLauncherKey('.', keys);
  } else if (tabDown) {
    handleTypingKey(0, keys);
  } else if (gLauncherVisible && (leftDown || upDown)) {
    handleLauncherKey(',', keys);
  } else if (gLauncherVisible && (rightDown || downDown)) {
    handleLauncherKey('.', keys);
  } else if (spaceDown && keys.ctrl) {
    handleTypingKey(' ', keys);
  } else if (enterDown) {
    handleTypingKey('\n', keys);
  } else if (delDown) {
    handleTypingKey('\b', keys);
  } else if (charDown) {
    handleTypingKey(curWordChar, keys);
  } else if (keys.fn || keys.ctrl || keys.alt || keys.opt) {
    setKeyDiag("mods=" + keyboardSignatureFromState());
  }

  pEnter = keys.enter;
  pDel = keys.del;
  pTab = keys.tab;
  pSpace = keys.space;
  pUp = upHeld;
  pDown = downHeld;
  pLeft = leftHeld;
  pRight = rightHeld;
  pWordSignature = wordSignature;
}

bool ctrlAlonePressedNow() {
  return gKeyboardState.ctrl && !gKeyboardState.fn && !gKeyboardState.shift &&
         !gKeyboardState.alt && !gKeyboardState.opt && !gKeyboardState.tab &&
         !gKeyboardState.enter && !gKeyboardState.del && !gKeyboardState.space &&
         gKeyboardState.word.empty();
}

void updateVoiceTrigger() {
  bool held = ctrlAlonePressedNow();
  bool ctrlComboActive = gKeyboardState.ctrl &&
                         (!gKeyboardState.word.empty() || gKeyboardState.space ||
                          gKeyboardState.tab || gKeyboardState.enter ||
                          gKeyboardState.del || gKeyboardState.fn ||
                          gKeyboardState.shift || gKeyboardState.alt ||
                          gKeyboardState.opt);
  if (gRecording && millis() - gRecordStartMs >= currentRecordLimitMs()) {
    logf("[VOICE] auto-stop at max duration");
    stopRecordingAndSend();
    return;
  }
  if (ctrlComboActive) {
    gCtrlSoloCandidate = false;
    gLastCtrlTapMs = 0;
  }
  if (held && !gCtrlSoloHeld) {
    gCtrlSoloHeld = true;
    gCtrlSoloCandidate = !ctrlComboActive;
    uint32_t now = millis();
    if (gCtrlSoloCandidate && gLastCtrlTapMs != 0 &&
        now - gLastCtrlTapMs <= kCtrlDoubleTapWindowMs) {
      gLastCtrlTapMs = 0;
      gCtrlSoloCandidate = false;
      if (gRecording) {
        logf("[VOICE] ctrl double tap stop");
        stopRecordingAndSend();
      } else {
        logf("[VOICE] ctrl double tap start");
        startRecording();
      }
      return;
    }
    if (gCtrlSoloCandidate) {
      gLastCtrlTapMs = now;
      setStatus(gRecording ? "Ctrl x2 stop" : "Ctrl x2 start", 600);
    }
    return;
  }
  if (gCtrlSoloHeld && !held) {
    gCtrlSoloHeld = false;
    gCtrlSoloCandidate = false;
  }
  if (gLastCtrlTapMs != 0 && millis() - gLastCtrlTapMs > kCtrlDoubleTapWindowMs) {
    gLastCtrlTapMs = 0;
  }
}

void updateBlink() {
  uint32_t now = millis();
  if (gBlinkEndMs == 0 && now >= gNextBlinkMs) {
    gBlinkEndMs = now + 120;
  } else if (gBlinkEndMs != 0 && now >= gBlinkEndMs) {
    gBlinkEndMs = 0;
    scheduleBlink();
  }
}

void drawEye(int16_t cx, int16_t cy, int16_t halfW, int16_t halfH, float openness,
             uint16_t irisColor, int16_t pupilOffsetX, int16_t pupilOffsetY) {
  auto& d = gCanvas;
  int16_t eyeH = max<int16_t>(3, static_cast<int16_t>(halfH * openness));
  if (eyeH <= 4) {
    d.drawFastHLine(cx - halfW, cy, halfW * 2, irisColor);
    return;
  }

  d.fillRoundRect(cx - halfW, cy - eyeH, halfW * 2, eyeH * 2, 8, TFT_BLACK);
  d.drawRoundRect(cx - halfW, cy - eyeH, halfW * 2, eyeH * 2, 8, irisColor);
  d.fillRoundRect(cx - halfW + 2, cy - eyeH + 2, halfW * 2 - 4, eyeH * 2 - 4, 7, TFT_DARKGREY);
  d.fillCircle(cx + pupilOffsetX, cy + pupilOffsetY, max<int16_t>(3, eyeH / 3), irisColor);
  d.fillCircle(cx + pupilOffsetX, cy + pupilOffsetY, max<int16_t>(2, eyeH / 5), TFT_BLACK);
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

  uint16_t accent = TFT_CYAN;
  int16_t pupilX = 0;
  int16_t pupilY = 0;
  float openness = 1.0f;

  switch (gFaceMode) {
    case FaceMode::Listening:
      accent = TFT_RED;
      openness = 1.08f;
      pupilY = -1;
      break;
    case FaceMode::Thinking:
      accent = TFT_GOLD;
      openness = 0.9f;
      pupilY = -5;
      pupilX = ((millis() / 220) % 3) - 1;
      break;
    case FaceMode::Speaking:
      accent = 0x55FF;
      openness = 0.95f;
      pupilX = ((millis() / 160) % 5) - 2;
      break;
    case FaceMode::Error:
      accent = TFT_ORANGE;
      openness = 0.6f;
      break;
    case FaceMode::Idle:
    default:
      accent = TFT_CYAN;
      openness = 1.0f;
      pupilX = ((millis() / 700) % 5) - 2;
      break;
  }

  pupilX += static_cast<int16_t>(gEyeLookX);
  pupilY += static_cast<int16_t>(gEyeLookY);
  pupilX = clampValue<int16_t>(pupilX, -14, 14);
  pupilY = clampValue<int16_t>(pupilY, -10, 10);

  if (gBlinkEndMs != 0) {
    openness = 0.08f;
  }

  if (gFaceMode == FaceMode::Error) {
    int16_t leftStartX = fullscreen ? panelX + 24 : panelX + 30;
    int16_t leftEndX = fullscreen ? panelX + 98 : panelX + 84;
    int16_t rightStartX = fullscreen ? panelX + 138 : panelX + 148;
    int16_t rightEndX = fullscreen ? panelX + 212 : panelX + 202;
    int16_t topY = fullscreen ? panelY + 26 : panelY + 14;
    int16_t bottomY = fullscreen ? panelY + 98 : panelY + 54;
    d.drawLine(leftStartX, topY, leftEndX, bottomY, accent);
    d.drawLine(leftStartX, bottomY, leftEndX, topY, accent);
    d.drawLine(rightStartX, topY, rightEndX, bottomY, accent);
    d.drawLine(rightStartX, bottomY, rightEndX, topY, accent);
  } else {
    int16_t eyeCenterY = fullscreen ? panelY + 63 : panelY + 35;
    int16_t leftEyeX = fullscreen ? panelX + 64 : panelX + 60;
    int16_t rightEyeX = fullscreen ? panelX + 172 : panelX + 172;
    int16_t eyeHalfW = fullscreen ? 52 : 41;
    int16_t eyeHalfH = fullscreen ? 36 : 25;
    drawEye(leftEyeX, eyeCenterY, eyeHalfW, eyeHalfH, openness, accent, pupilX, pupilY);
    drawEye(rightEyeX, eyeCenterY, eyeHalfW, eyeHalfH, openness, accent, pupilX, pupilY);
  }

  if (gSubmitting || gThinking) {
    int16_t arrowY = fullscreen ? panelY + 12 : panelY + 10;
    int16_t centerX = panelX + panelW / 2;
    d.fillTriangle(centerX - 16, arrowY + 10, centerX - 10, arrowY, centerX - 4, arrowY + 10, accent);
    d.fillTriangle(centerX + 4, arrowY + 10, centerX + 10, arrowY, centerX + 16, arrowY + 10, accent);
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
  d.fillRoundRect(8, 8, 224, 94, 10, 0x0000);
  d.drawRoundRect(8, 8, 224, 94, 10, TFT_DARKGREY);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setCursor(14, 14);
  d.print("DBG");
  d.setCursor(48, 14);
  d.print(gWifiReady ? "wifi=ok" : "wifi=off");
  d.setCursor(118, 14);
  d.print(gWsReady ? "ws=ok" : "ws=off");
  d.setCursor(14, 28);
  d.print("face=");
  d.print(static_cast<int>(gFaceMode));
  d.setCursor(86, 28);
  d.print("scroll=");
  d.print(gChatScrollOffset);
  d.setCursor(150, 28);
  d.print(layoutName(gKeyboardLayout));
  d.setCursor(14, 42);
  d.print("rec=");
  d.print(static_cast<unsigned>(gRecordCapacityBytes / 1024));
  d.print("k");
  d.setCursor(86, 42);
  d.print("tts=");
  d.print(static_cast<unsigned>(gTtsCapacityBytes / 1024));
  d.print("k");
  d.setCursor(150, 42);
  d.print(gStorageReady ? "fs=ok" : "fs=off");
  d.setCursor(14, 56);
  String diag = gLastVoiceDiag;
  if (diag.length() > 28) {
    diag = diag.substring(diag.length() - 28);
  }
  d.print(diag);
  d.setCursor(14, 70);
  String httpDiag = gLastHttpDiag;
  if (httpDiag.length() > 28) {
    httpDiag = httpDiag.substring(httpDiag.length() - 28);
  }
  d.print(httpDiag);
  d.setCursor(14, 84);
  String keyDiag = gLastKeyDiag;
  if (keyDiag.length() > 28) {
    keyDiag = keyDiag.substring(keyDiag.length() - 28);
  }
  d.print(keyDiag);
  d.setCursor(150, 84);
  d.print(String(uiModeName(gUiMode)).substring(0, 6));
}

void renderLauncherOverlay() {
  if (!gLauncherVisible) {
    return;
  }
  static const char* kEntries[] = {"Chat+Face", "Face", "Chat", "Clock", "Voice"};
  auto& d = gCanvas;
  d.fillRoundRect(34, 16, 172, 104, 12, 0x0000);
  d.drawRoundRect(34, 16, 172, 104, 12, TFT_DARKGREY);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_YELLOW, TFT_BLACK);
  d.setCursor(44, 24);
  d.print("Launcher");
  for (int i = 0; i < 5; ++i) {
    int y = 42 + i * 14;
    bool selected = i == gLauncherSelection;
    if (selected) {
      d.fillRoundRect(42, y - 2, 156, 12, 6, 0x2945);
    }
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, selected ? 0x2945 : TFT_BLACK);
    d.setCursor(50, y);
    d.print(kEntries[i]);
  }
}

void renderModeFooter(const String& left, const String& right = "") {
  auto& d = gCanvas;
  d.fillRect(0, 123, 240, 12, 0x18C3);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_LIGHTGREY, 0x18C3);
  d.setCursor(4, 125);
  d.print(left);
  if (!right.isEmpty()) {
    int rightWidth = d.textWidth(right);
    d.setCursor(max<int>(4, 236 - rightWidth), 125);
    d.print(right);
  }
}

void renderFaceOnlyUi() {
  auto& d = gCanvas;
  d.fillScreen(0x0841);
  renderFacePanel(true);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderClockUi() {
  auto& d = gCanvas;
  d.fillScreen(0x0000);
  ensureBangkokTimezone();
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  String hhmm = "--:--";
  String ss = "--";
  time_t now = time(nullptr);
  if (now > 100000) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[6];
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    hhmm = buf;
    char secBuf[3];
    strftime(secBuf, sizeof(secBuf), "%S", &timeinfo);
    ss = secBuf;
  } else {
    hhmm = "--:--";
    ss = "--";
  }
  d.setFont(&fonts::Font7);
  d.setCursor(18, 16);
  d.print(hhmm);
  d.setFont(&fonts::Font6);
  d.setTextColor(TFT_ORANGE, TFT_BLACK);
  d.setCursor(88, 82);
  d.print(ss);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderVoicePlayerUi() {
  auto& d = gCanvas;
  d.fillScreen(0x0841);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_YELLOW, 0x0841);
  d.setCursor(6, 8);
  d.print("Voice Library");
  d.setTextColor(TFT_LIGHTGREY, 0x0841);
  d.setCursor(150, 8);
  d.print(String(gSelectedVoiceNote + 1) + "/" + String(max<int>(1, static_cast<int>(gVoiceNotes.size()))));

  if (gVoiceNotes.empty()) {
    d.setCursor(6, 38);
    d.print("No saved voice notes");
  } else {
    int start = max(0, gSelectedVoiceNote - 2);
    int end = min(static_cast<int>(gVoiceNotes.size()), start + 4);
    int y = 24;
    for (int i = start; i < end; ++i) {
      bool selected = i == gSelectedVoiceNote;
      uint16_t bg = selected ? 0x2945 : 0x0841;
      d.fillRoundRect(4, y - 2, 232, 18, 6, bg);
      d.setTextColor(selected ? TFT_WHITE : (gVoiceNotes[i].assistant ? TFT_GREENYELLOW : TFT_CYAN), bg);
      d.setCursor(8, y);
      d.print(String(gVoiceNotes[i].assistant ? "AI " : "ME ") + gVoiceNotes[i].title);
      d.setTextColor(TFT_LIGHTGREY, bg);
      d.setCursor(8, y + 8);
      d.print(trimPreview(gVoiceNotes[i].preview, 28));
      y += 20;
    }
  }
  d.setTextColor(TFT_LIGHTGREY, 0x0841);
  d.setCursor(6, 112);
  d.print("Enter/Space play-stop");
  d.setCursor(6, 124);
  d.print(", . select   +/- volume");
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderChatUi() {
  auto& d = gCanvas;
  d.fillScreen(0x0841);
  d.setTextSize(1);
  d.setFont(&fonts::Font2);

  bool showFace = gUiMode == UiMode::ChatFace;
  if (showFace) {
    renderFacePanel(false);
  }

  bool showStatus = !gStatusText.isEmpty() && gStatusText != "Ready";
  d.setFont(&fonts::efontCN_12);
  int lineHeight = d.fontHeight() + 1;
  int visibleRows = 0;
  int y = 0;
  if (showFace) {
    visibleRows = showStatus ? 3 : 4;
    y = 80;
  } else {
    visibleRows = showStatus ? 7 : 8;
    y = 12;
  }
  auto lines = buildVisualLines(showFace ? 216 : 228);
  int maxOffset = max<int>(0, static_cast<int>(lines.size()) - visibleRows);
  if (gChatScrollOffset > maxOffset) {
    gChatScrollOffset = maxOffset;
  }
  int startIndex = max<int>(0, static_cast<int>(lines.size()) - visibleRows - gChatScrollOffset);
  int endIndex = min<int>(static_cast<int>(lines.size()), startIndex + visibleRows);

  for (int i = startIndex; i < endIndex; ++i) {
    d.setTextColor(colorForLine(lines[i].kind), 0x0841);
    d.setCursor(4, y);
    d.print(lines[i].text);
    y += lineHeight;
  }
  if (showFace) {
    drawScrollIndicators(gChatScrollOffset < maxOffset, gChatScrollOffset > 0);
  }

  if (showStatus) {
    d.setFont(&fonts::Font2);
    d.setTextColor(TFT_LIGHTGREY, 0x0841);
    d.setCursor(4, showFace ? 112 : 106);
    d.print(gStatusText);
  }

  d.setFont(&fonts::efontCN_12);
  d.setTextColor(TFT_YELLOW, 0x0841);
  d.setCursor(4, 122);
  String inputLine = tailUtf8ToFit("> " + gInputBuffer, 232);
  d.print(inputLine);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderBleUi() {
  auto& d = gCanvas;
  d.fillScreen(TFT_BLACK);
  d.setFont(&fonts::Font2);
  d.setTextSize(2);
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  d.setCursor(18, 18);
  d.print("BT SETUP");

  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setCursor(10, 52);
  d.print("Open OmniBot hub -> Add New Bot");
  d.setCursor(10, 64);
  d.print("Select this device over BLE.");
  d.setCursor(10, 88);
  d.print("The hub can send Wi-Fi + endpoint.");
  d.setCursor(10, 100);
  d.print("After provisioning, device reboots.");
  d.setCursor(10, 124);
  d.print("Use /clearwifi later to reset.");
}

void renderStatusBar() {
  auto& d = gCanvas;
  d.fillRect(0, 126, 240, 9, 0x18C3);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_LIGHTGREY, 0x18C3);
  d.setCursor(4, 127);
  d.print(gStatusText);
}

void render() {
  if (gStatusUntilMs != 0 && millis() > gStatusUntilMs) {
    gStatusUntilMs = 0;
    if (gBleActive) {
      gStatusText = "Waiting for BLE provisioning";
    } else if (gRecording) {
      gStatusText = "Listening...";
    } else if (gPlaybackActive) {
      gStatusText = "Speaking...";
    } else {
      gStatusText = "Ready";
    }
  }

  if (gBleActive) {
    renderBleUi();
    renderStatusBar();
  } else {
    switch (gUiMode) {
      case UiMode::Face:
        renderFaceOnlyUi();
        break;
      case UiMode::Clock:
        renderClockUi();
        break;
      case UiMode::Voice:
        renderVoicePlayerUi();
        break;
      case UiMode::ChatFull:
      case UiMode::ChatFace:
      default:
        renderChatUi();
        break;
    }
  }
  gCanvas.pushSprite(0, 0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(800);
  ensureBangkokTimezone();

  auto cfg = M5.config();
  cfg.internal_imu = true;
  M5Cardputer.begin(cfg, false);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setFont(&fonts::Font2);
  M5Cardputer.Display.setTextSize(1);
  gCanvas.createSprite(240, 135);
  gCanvas.setTextWrap(false);
  gCanvas.setFont(&fonts::Font2);
  gCanvas.setTextSize(1);

  initSpeaker();
  initStorage();
  randomSeed(micros());
  scheduleBlink();
  gImuEnabled = M5.Imu.isEnabled();
  logf("[IMU] enabled=%d type=%d", gImuEnabled, static_cast<int>(M5.Imu.getType()));
  setupKeyboardInput();
  gTimeValid = hasValidSystemTime();

  loadConfig();
  pushSystem("Device ready: " + gDeviceId);
  pushSystem("Ctrl+Space = EN/RU");
  pushSystem("Ctrl x2 = voice");
  pushSystem("Tab + < > mode, Ctrl+D debug");
  logf("[BOOT] device_id=%s", gDeviceId.c_str());

  if (ensureWifi()) {
    startHubWebSocket();
  }

  render();
}

void loop() {
  M5Cardputer.update();
  refreshKeyboardState();
  updateImuMotion();
  updateStatusLed();
  gTimeValid = hasValidSystemTime();

  if (gPendingRestart) {
    delay(600);
    ESP.restart();
  }

  if (!gBleActive) {
    maintainConnectivity();
    if (kUseHubWebSocket) {
      gWs.loop();
    }
    syncClockFromNtp(false);
    updateVoiceTrigger();
    pollTyping();
  }

  if (gPlaybackActive && !M5Cardputer.Speaker.isPlaying()) {
    gPlaybackActive = false;
    if (gDynamicPlaybackBuffer) {
      free(gDynamicPlaybackBuffer);
      gDynamicPlaybackBuffer = nullptr;
    }
    if (gTtsBuffer) {
      releaseTtsBuffer();
    }
    if (gRecordBuffer && !gRecording && !gSubmitting && !gThinking) {
      releaseRecordBuffer();
    }
    if (!gRecording && !gThinking && !gSubmitting) {
      setFaceMode(FaceMode::Idle);
      setStatus("Reply finished", 1000);
    }
  }

  updateBlink();

  uint32_t now = millis();
  uint32_t renderInterval = (gRecording || gSubmitting || gThinking) ? kBusyRenderIntervalMs : kRenderIntervalMs;
  if (now - gLastRenderMs >= renderInterval) {
    render();
    gLastRenderMs = now;
  }
}
