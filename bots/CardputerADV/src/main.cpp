#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <WiFiClient.h>
#include <esp_heap_caps.h>

#include <cstdarg>
#include <deque>
#include <vector>

namespace {

constexpr const char* kDeviceType = "cardputer_adv";
constexpr const char* kDefaultDisplayName = "ADV Cardputer";
constexpr const char* kWsPath = "/ws/stream";
constexpr const char* kDeviceTextTurnPath = "/api/device-text-turn";
constexpr const char* kDeviceAudioTurnPath = "/api/device-audio-turn";
constexpr const char* kDeviceTtsPath = "/api/device-tts";

constexpr const char* kPrefsWifiNs = "wifi_creds";
constexpr const char* kPrefsHubNs = "hub_ep";
constexpr const char* kPrefsDeviceNs = "device_cfg";

constexpr const char* kBleServiceName = "OmniBot Cardputer ADV";
constexpr const char* kBleServiceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
constexpr const char* kBleWifiCharUuid = "12345678-1234-5678-1234-56789abcdef0";

constexpr uint16_t kDefaultHubPort = 8000;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kRenderIntervalMs = 50;
constexpr size_t kMaxChatLines = 16;
constexpr size_t kMaxInputChars = 180;
constexpr size_t kWrapColumns = 34;
constexpr uint32_t kMicSampleRate = 16000;
constexpr uint32_t kTtsDefaultSampleRate = 24000;
constexpr float kMaxRecordSeconds = 3.0f;
constexpr float kMinRecordSeconds = 0.35f;
constexpr size_t kMaxRecordSamples = static_cast<size_t>(kMicSampleRate * kMaxRecordSeconds);
constexpr size_t kAudioBufferBytes = kMaxRecordSamples * sizeof(int16_t);
constexpr size_t kMinAudioBufferBytes = static_cast<size_t>(kMicSampleRate * sizeof(int16_t));
constexpr size_t kHttpWriteChunkBytes = 512;

#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID ""
#endif

#ifndef DEFAULT_WIFI_PASSWORD
#define DEFAULT_WIFI_PASSWORD ""
#endif

#ifndef DEFAULT_HUB_HOST
#define DEFAULT_HUB_HOST ""
#endif

#ifndef DEFAULT_HUB_PORT
#define DEFAULT_HUB_PORT 8000
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

struct ChatLine {
  String prefix;
  String text;
  LineKind kind;
};

Preferences gPrefs;
WebSocketsClient gWs;
M5Canvas gCanvas(&M5Cardputer.Display);

String gDeviceId;
String gDisplayName = kDefaultDisplayName;
String gWifiSsid;
String gWifiPassword;
String gHubHost;
uint16_t gHubPort = kDefaultHubPort;

std::deque<ChatLine> gChatLines;
String gInputBuffer;
String gStatusText = "Booting";

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

uint32_t gLastRenderMs = 0;
uint32_t gStatusUntilMs = 0;
uint32_t gRecordStartMs = 0;
uint32_t gBlinkEndMs = 0;
uint32_t gNextBlinkMs = 0;

FaceMode gFaceMode = FaceMode::Idle;
int16_t* gAudioBuffer = nullptr;
size_t gRecordedSamples = 0;
size_t gAudioCapacitySamples = 0;
size_t gAudioCapacityBytes = 0;

void startHubWebSocket();
void pauseHubWebSocketForVoice();
void resumeHubWebSocketAfterVoice();

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
}

void pushSystem(const String& text) { pushLine("SYS", text, LineKind::System); }
void pushError(const String& text) { pushLine("ERR", text, LineKind::Error); }
void pushUser(const String& text) { pushLine("YOU", text, LineKind::User); }
void pushAssistant(const String& text) { pushLine("AI", text, LineKind::Assistant); }

std::vector<String> wrapText(const String& raw, size_t limit) {
  std::vector<String> out;
  String remaining = raw;
  remaining.replace('\r', ' ');
  remaining.replace('\n', ' ');
  remaining.trim();

  while (remaining.length() > static_cast<int>(limit)) {
    int split = static_cast<int>(limit);
    while (split > 0 && remaining[split] != ' ') {
      --split;
    }
    if (split <= 0) {
      split = static_cast<int>(limit);
    }
    String part = remaining.substring(0, split);
    part.trim();
    if (!part.isEmpty()) {
      out.push_back(part);
    }
    remaining = remaining.substring(split);
    remaining.trim();
  }

  if (!remaining.isEmpty()) {
    out.push_back(remaining);
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
  if (gHubPort == kDefaultHubPort && static_cast<uint16_t>(DEFAULT_HUB_PORT) != 0) {
    gHubPort = static_cast<uint16_t>(DEFAULT_HUB_PORT);
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

bool ensureAudioBuffer() {
  if (gAudioBuffer) {
    return true;
  }
  size_t targetBytes = kAudioBufferBytes;
  gAudioBuffer = static_cast<int16_t*>(
      heap_caps_malloc(targetBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (gAudioBuffer) {
    gAudioCapacityBytes = targetBytes;
    gAudioCapacitySamples = targetBytes / sizeof(int16_t);
    logf("[VOICE] audio buffer allocated in PSRAM bytes=%u",
         static_cast<unsigned>(gAudioCapacityBytes));
    return true;
  }

  gAudioBuffer = static_cast<int16_t*>(heap_caps_malloc(targetBytes, MALLOC_CAP_8BIT));
  if (gAudioBuffer) {
    gAudioCapacityBytes = targetBytes;
    gAudioCapacitySamples = targetBytes / sizeof(int16_t);
    logf("[VOICE] audio buffer allocated in heap bytes=%u",
         static_cast<unsigned>(gAudioCapacityBytes));
    return true;
  }

  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t fallbackBytes = 0;
  if (largest > 8192) {
    fallbackBytes = (largest - 4096) & ~static_cast<size_t>(1);
  }
  if (fallbackBytes < kMinAudioBufferBytes) {
    logf("[VOICE] audio buffer alloc failed target=%u largest=%u",
         static_cast<unsigned>(targetBytes), static_cast<unsigned>(largest));
    return false;
  }

  gAudioBuffer = static_cast<int16_t*>(heap_caps_malloc(fallbackBytes, MALLOC_CAP_8BIT));
  if (!gAudioBuffer) {
    logf("[VOICE] fallback alloc failed bytes=%u largest=%u",
         static_cast<unsigned>(fallbackBytes), static_cast<unsigned>(largest));
    return false;
  }

  gAudioCapacityBytes = fallbackBytes;
  gAudioCapacitySamples = fallbackBytes / sizeof(int16_t);
  logf("[VOICE] audio buffer fallback bytes=%u samples=%u",
       static_cast<unsigned>(gAudioCapacityBytes),
       static_cast<unsigned>(gAudioCapacitySamples));
  return true;
}

void initSpeaker() {
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(255);
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
    logf("[VOICE] start blocked wifi=%d hub=%s", gWifiReady, gHubHost.c_str());
    return;
  }
  if (!ensureAudioBuffer()) {
    pushError("Audio buffer allocation failed.");
    setFaceMode(FaceMode::Error);
    setStatus("No audio buffer", 1500);
    logf("[VOICE] audio buffer allocation failed");
    return;
  }

  gRecordedSamples = 0;
  gRecording = true;
  gRecordStartMs = millis();
  startMic();
  M5Cardputer.Mic.record(gAudioBuffer, gAudioCapacitySamples, kMicSampleRate);
  setFaceMode(FaceMode::Listening);
  setStatus("Listening...");
  logf("[VOICE] recording started capacity_samples=%u", static_cast<unsigned>(gAudioCapacitySamples));
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
    uint32_t& sampleRate) {
  statusCode = 0;
  contentLength = -1;
  sampleRate = kTtsDefaultSampleRate;

  unsigned long deadline = millis() + 30000;
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
  while (written < length && millis() < deadline) {
    if (!client.connected()) {
      break;
    }
    size_t chunk = length - written;
    if (chunk > kHttpWriteChunkBytes) {
      chunk = kHttpWriteChunkBytes;
    }
    size_t sent = client.write(data + written, chunk);
    if (sent == 0) {
      delay(2);
      continue;
    }
    written += sent;
  }

  if (written != length) {
    logf("[HTTP] short write label=%s written=%u total=%u", label,
         static_cast<unsigned>(written), static_cast<unsigned>(length));
    return false;
  }
  return true;
}

bool writeClientAll(WiFiClient& client, const String& text, const char* label) {
  return writeClientAll(client, reinterpret_cast<const uint8_t*>(text.c_str()), text.length(), label);
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

bool requestAndPlayTts(const String& text) {
  if (!ensureAudioBuffer() || text.isEmpty()) {
    return false;
  }

  bool pausedWsForTts = false;
  if (gWsReady) {
    pauseHubWebSocketForVoice();
    pausedWsForTts = true;
  }

  WiFiClient client;
  client.setTimeout(30);
  client.setNoDelay(true);
  if (!client.connect(gHubHost.c_str(), gHubPort)) {
    logf("[TTS] connect failed host=%s port=%u", gHubHost.c_str(), gHubPort);
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  String body =
      "{\"device_id\":\"" + escapeJsonString(gDeviceId) + "\",\"text\":\"" +
      escapeJsonString(text) + "\"}";

  client.printf(
      "POST %s HTTP/1.1\r\n"
      "Host: %s:%u\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n\r\n",
      kDeviceTtsPath,
      gHubHost.c_str(),
      gHubPort,
      static_cast<unsigned>(body.length()));
  if (!writeClientAll(client, body, "tts_body")) {
    client.stop();
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  int statusCode = 0;
  int contentLength = -1;
  uint32_t sampleRate = kTtsDefaultSampleRate;
  if (!parseHttpStatusAndHeaders(client, statusCode, contentLength, sampleRate) || statusCode != 200) {
    logf("[TTS] bad response status=%d", statusCode);
    client.stop();
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  size_t maxBytes = gAudioCapacityBytes;
  size_t toRead = maxBytes;
  if (contentLength > 0 && static_cast<size_t>(contentLength) < toRead) {
    toRead = static_cast<size_t>(contentLength);
  }

  size_t bytesRead = 0;
  uint8_t* dst = reinterpret_cast<uint8_t*>(gAudioBuffer);
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

  size_t samples = bytesRead / sizeof(int16_t);
  if (samples == 0) {
    logf("[TTS] empty PCM response");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  gPlaybackActive = true;
  setFaceMode(FaceMode::Speaking);
  setStatus("Speaking...");
  M5Cardputer.Speaker.playRaw(gAudioBuffer, samples, sampleRate, false);
  logf("[TTS] playback samples=%u sample_rate=%u", static_cast<unsigned>(samples), sampleRate);
  if (pausedWsForTts) {
    resumeHubWebSocketAfterVoice();
  }
  return true;
}

bool sendTextTurn(const String& text) {
  if (!gWifiReady || gHubHost.isEmpty()) {
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

  gSubmitting = true;
  gThinking = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Thinking...");

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  gSubmitting = false;
  gThinking = false;

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
    pushAssistant(resp["reply"].as<String>());
    logf("[TEXT] reply=%s", resp["reply"].as<String>().c_str());
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
  if (gRecordedSamples > gAudioCapacitySamples) {
    gRecordedSamples = gAudioCapacitySamples;
  }
  stopMic();
  logf("[VOICE] recording stopped elapsed=%.2f samples=%u", elapsed,
       static_cast<unsigned>(gRecordedSamples));

  if (elapsed < kMinRecordSeconds || gRecordedSamples == 0) {
    setFaceMode(FaceMode::Idle);
    setStatus("Voice turn canceled", 1200);
    logf("[VOICE] canceled: too short");
    return false;
  }

  uint32_t dataSize = static_cast<uint32_t>(gRecordedSamples * sizeof(int16_t));
  uint8_t wavHeader[44];
  writeWavHeader(wavHeader, dataSize);

  const char* boundary = "----OmniBotVoiceBoundary";
  String partDevice =
      String("--") + boundary +
      "\r\nContent-Disposition: form-data; name=\"device_id\"\r\n\r\n" +
      gDeviceId + "\r\n";
  String partFileHeader =
      String("--") + boundary +
      "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"cardputer.wav\"\r\n"
      "Content-Type: audio/wav\r\n\r\n";
  String partFooter = String("\r\n--") + boundary + "--\r\n";

  size_t totalSize = partDevice.length() + partFileHeader.length() + sizeof(wavHeader) + dataSize +
                     partFooter.length();

  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (!gWifiReady) {
    pushError("Wi-Fi dropped during recording.");
    setFaceMode(FaceMode::Error);
    setStatus("Wi-Fi lost", 2000);
    logf("[VOICE] wifi lost before upload");
    return false;
  }

  pauseHubWebSocketForVoice();
  auto finishVoiceTurn = [&]() {
    gSubmitting = false;
    gThinking = false;
    resumeHubWebSocketAfterVoice();
  };

  WiFiClient client;
  client.setTimeout(45);
  client.setNoDelay(true);
  if (!client.connect(gHubHost.c_str(), gHubPort)) {
    pushError("Voice upload connect failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice upload failed", 2000);
    logf("[VOICE] upload connect failed host=%s port=%u", gHubHost.c_str(), gHubPort);
    finishVoiceTurn();
    return false;
  }

  gSubmitting = true;
  gThinking = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Transcribing...");

  client.printf(
      "POST %s HTTP/1.1\r\n"
      "Host: %s:%u\r\n"
      "Content-Type: multipart/form-data; boundary=%s\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n\r\n",
      kDeviceAudioTurnPath,
      gHubHost.c_str(),
      gHubPort,
      boundary,
      static_cast<unsigned>(totalSize));

  if (!writeClientAll(client, partDevice, "voice_part_device") ||
      !writeClientAll(client, partFileHeader, "voice_part_header") ||
      !writeClientAll(client, wavHeader, sizeof(wavHeader), "voice_wav_header")) {
    client.stop();
    pushError("Voice upload body failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice upload failed", 2000);
    logf("[VOICE] upload preamble write failed");
    finishVoiceTurn();
    return false;
  }

  const uint8_t* pcmBytes = reinterpret_cast<const uint8_t*>(gAudioBuffer);
  if (!writeClientAll(client, pcmBytes, dataSize, "voice_pcm") ||
      !writeClientAll(client, partFooter, "voice_part_footer")) {
    client.stop();
    pushError("Voice upload body failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice upload failed", 2000);
    logf("[VOICE] upload body write failed bytes=%u", static_cast<unsigned>(dataSize));
    finishVoiceTurn();
    return false;
  }

  int statusCode = 0;
  int contentLength = -1;
  uint32_t ignoredRate = 0;
  if (!parseHttpStatusAndHeaders(client, statusCode, contentLength, ignoredRate)) {
    client.stop();
    pushError("Voice response header failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice turn failed", 2000);
    logf("[VOICE] response header failed");
    finishVoiceTurn();
    return false;
  }

  String response;
  unsigned long deadline = millis() + 90000;
  while ((client.connected() || client.available()) && millis() < deadline) {
    while (client.available()) {
      response += static_cast<char>(client.read());
    }
    delay(1);
  }
  client.stop();
  finishVoiceTurn();

  if (statusCode < 200 || statusCode >= 300) {
    logf("[VOICE] HTTP error status=%d body=%s", statusCode, response.c_str());
    pushError("Voice HTTP error: " + String(statusCode));
    if (!response.isEmpty()) {
      pushError(response);
    }
    setFaceMode(FaceMode::Error);
    setStatus("Voice turn failed", 2000);
    return false;
  }

  DynamicJsonDocument resp(4096);
  auto err = deserializeJson(resp, response);
  if (err) {
    pushError("Could not parse voice reply.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice parse failed", 2000);
    logf("[VOICE] JSON parse failed body=%s", response.c_str());
    return false;
  }

  String transcript = resp["transcript"] | "";
  String reply = resp["reply"] | "";
  logf("[VOICE] transcript=%s", transcript.c_str());
  logf("[VOICE] reply=%s", reply.c_str());
  if (!transcript.isEmpty()) {
    pushUser(transcript);
  }
  if (!reply.isEmpty()) {
    pushAssistant(reply);
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

bool ensureWifi() {
  if (gWifiSsid.isEmpty()) {
    pushSystem("No Wi-Fi credentials. Entering BLE setup.");
    startBleProvisioning();
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
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
  return true;
}

void sendDeviceHello() {
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
  if (gHubHost.isEmpty()) {
    pushError("Hub endpoint is empty. Use BLE provisioning.");
    return;
  }

  gWs.begin(gHubHost.c_str(), gHubPort, kWsPath);
  gWs.onEvent(wsEvent);
  gWs.setReconnectInterval(5000);
  gWs.enableHeartbeat(15000, 3000, 2);
  setStatus("Connecting to hub...");
}

void pauseHubWebSocketForVoice() {
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

bool handleLocalCommand(const String& input) {
  String cmd = input;
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "/help") {
    pushSystem("BtnA/Fn hold: push-to-talk");
    pushSystem("/btsetup /clearwifi /status");
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

void handleTypingKey(char key, const Keyboard_Class::KeysState& keys) {
  if (keys.enter || key == '\r' || key == '\n') {
    submitInput();
    return;
  }
  if (keys.del || key == '\b') {
    if (!gInputBuffer.isEmpty()) {
      gInputBuffer.remove(gInputBuffer.length() - 1);
    }
    return;
  }
  if (keys.tab) {
    handleLocalCommand("/help");
    return;
  }
  if (keys.fn) {
    return;
  }
  if (key >= 32 && key < 127 && gInputBuffer.length() < static_cast<int>(kMaxInputChars)) {
    gInputBuffer += key;
  }
}

void pollTyping() {
  M5Cardputer.Keyboard.updateKeyList();
  M5Cardputer.Keyboard.updateKeysState();
  bool keyPressed = M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed();
  if (!keyPressed) {
    gTypingKeyHeld = false;
    return;
  }
  if (gTypingKeyHeld || gRecording || gSubmitting) {
    return;
  }
  auto keys = M5Cardputer.Keyboard.keysState();
  if (keys.word.size() > 0 || keys.enter || keys.del || keys.tab) {
    char key = 0;
    if (keys.word.size() > 0) {
      key = keys.word[0];
    }
    handleTypingKey(key, keys);
    gTypingKeyHeld = true;
  }
}

bool voiceTriggerPressedNow() {
  M5Cardputer.Keyboard.updateKeyList();
  M5Cardputer.Keyboard.updateKeysState();
  auto keys = M5Cardputer.Keyboard.keysState();
  return keys.fn || M5.BtnA.isPressed();
}

void updateVoiceTrigger() {
  bool held = voiceTriggerPressedNow();
  if (held && gRecording && millis() - gRecordStartMs >= static_cast<uint32_t>(kMaxRecordSeconds * 1000.0f)) {
    logf("[VOICE] auto-stop at max duration");
    stopRecordingAndSend();
    gVoiceTriggerHeld = true;
    return;
  }
  if (held && !gVoiceTriggerHeld) {
    logf("[VOICE] trigger down");
    startRecording();
  } else if (!held && gVoiceTriggerHeld) {
    logf("[VOICE] trigger up");
    stopRecordingAndSend();
  }
  gVoiceTriggerHeld = held;
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

void renderFacePanel() {
  auto& d = gCanvas;
  d.fillRoundRect(8, 18, 224, 58, 14, 0x10A2);

  uint16_t accent = TFT_CYAN;
  int16_t pupilX = 0;
  int16_t pupilY = 0;
  float openness = 1.0f;

  switch (gFaceMode) {
    case FaceMode::Listening:
      accent = 0x4FFF;
      openness = 1.1f;
      pupilY = -1;
      break;
    case FaceMode::Thinking:
      accent = TFT_GOLD;
      openness = 0.9f;
      pupilY = -2;
      pupilX = ((millis() / 220) % 3) - 1;
      break;
    case FaceMode::Speaking:
      accent = TFT_GREENYELLOW;
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

  if (gBlinkEndMs != 0) {
    openness = 0.08f;
  }

  if (gFaceMode == FaceMode::Error) {
    d.drawLine(72, 34, 96, 58, accent);
    d.drawLine(72, 58, 96, 34, accent);
    d.drawLine(144, 34, 168, 58, accent);
    d.drawLine(144, 58, 168, 34, accent);
  } else {
    drawEye(84, 47, 20, 12, openness, accent, pupilX, pupilY);
    drawEye(156, 47, 20, 12, openness, accent, pupilX, pupilY);
  }

  if (gFaceMode == FaceMode::Speaking || gFaceMode == FaceMode::Thinking) {
    int16_t baseY = 66;
    int16_t barH = gFaceMode == FaceMode::Speaking ? (4 + (millis() / 90) % 7) : 4;
    d.fillRect(112, baseY - barH, 16, barH, accent);
  } else if (gRecording) {
    d.fillCircle(120, 63, 4 + ((millis() / 180) % 3), accent);
  }
}

void renderChatUi() {
  auto& d = gCanvas;
  d.fillScreen(0x0841);
  d.setTextSize(1);

  d.fillRect(0, 0, 240, 14, 0x18C3);
  d.setTextColor(TFT_WHITE, 0x18C3);
  d.setCursor(4, 3);
  d.print(gWifiReady ? "WiFi" : "NoWiFi");
  d.setCursor(54, 3);
  d.print(gWsReady ? "Hub" : "NoHub");
  d.setCursor(102, 3);
  if (gRecording) {
    d.print("Listen");
  } else if (gPlaybackActive) {
    d.print("Speak");
  } else if (gThinking || gSubmitting) {
    d.print("Think");
  } else {
    d.print("Idle");
  }

  renderFacePanel();

  int y = 82;
  for (auto it = gChatLines.rbegin(); it != gChatLines.rend(); ++it) {
    String full = it->prefix + ": " + it->text;
    auto wrapped = wrapText(full, kWrapColumns);
    for (auto rit = wrapped.rbegin(); rit != wrapped.rend(); ++rit) {
      if (y > 112) {
        break;
      }
      d.setTextColor(colorForLine(it->kind), 0x0841);
      d.setCursor(4, y);
      d.print(*rit);
      y += 9;
    }
    if (y > 112) {
      break;
    }
  }

  d.drawFastHLine(0, 118, 240, 0x39E7);
  d.setTextColor(TFT_YELLOW, 0x0841);
  d.setCursor(4, 122);
  String inputLine = "> " + gInputBuffer;
  if (gInputBuffer.length() > 28) {
    inputLine = "> " + gInputBuffer.substring(gInputBuffer.length() - 28);
  }
  d.print(inputLine);
}

void renderBleUi() {
  auto& d = gCanvas;
  d.fillScreen(TFT_BLACK);
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
  } else {
    renderChatUi();
  }
  renderStatusBar();
  gCanvas.pushSprite(0, 0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(800);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);
  gCanvas.createSprite(240, 135);
  gCanvas.setTextWrap(false);
  gCanvas.setTextFont(1);
  gCanvas.setTextSize(1);

  initSpeaker();
  randomSeed(micros());
  scheduleBlink();

  loadConfig();
  pushSystem("Device ready: " + gDeviceId);
  pushSystem("BtnA or Fn = push-to-talk");
  logf("[BOOT] device_id=%s", gDeviceId.c_str());

  if (ensureWifi()) {
    startHubWebSocket();
  }

  render();
}

void loop() {
  M5Cardputer.update();

  if (gPendingRestart) {
    delay(600);
    ESP.restart();
  }

  if (!gBleActive) {
    gWifiReady = WiFi.status() == WL_CONNECTED;
    gWs.loop();
    updateVoiceTrigger();
    pollTyping();
  }

  if (gPlaybackActive && !M5Cardputer.Speaker.isPlaying()) {
    gPlaybackActive = false;
    if (!gRecording && !gThinking && !gSubmitting) {
      setFaceMode(FaceMode::Idle);
      setStatus("Reply finished", 1000);
    }
  }

  updateBlink();

  uint32_t now = millis();
  if (now - gLastRenderMs >= kRenderIntervalMs) {
    render();
    gLastRenderMs = now;
  }
}
