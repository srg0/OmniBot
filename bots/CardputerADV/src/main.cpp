#include <Arduino.h>
#include <Adafruit_TCA8418.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>

#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <deque>
#include <cmath>
#include <time.h>
#include <vector>
#include <memory>

namespace {

constexpr const char* kDeviceType = "cardputer_adv";
constexpr const char* kDefaultDisplayName = "ADV Cardputer";
#ifndef APP_VERSION
#define APP_VERSION "0.2.3-dev"
#endif
#ifndef BUILD_GIT_SHA
#define BUILD_GIT_SHA "local"
#endif
#ifndef BUILD_TIME
#define BUILD_TIME __DATE__ " " __TIME__
#endif
constexpr const char* kAppVersion = APP_VERSION;
constexpr const char* kBuildGitSha = BUILD_GIT_SHA;
constexpr const char* kBuildTime = BUILD_TIME;
constexpr const char* kWsPath = "/ws/stream";
constexpr const char* kDeviceTextTurnPath = "/api/device-text-turn";
constexpr const char* kDeviceAudioTurnPath = "/api/device-audio-turn";
constexpr const char* kDeviceAudioTurnRawPath = "/api/device-audio-turn-raw";
constexpr const char* kDeviceTtsPath = "/api/device-tts";
constexpr const char* kCardputerCommandPath = "/api/cardputer/command";
constexpr const char* kCardputerLogsPath = "/api/cardputer/logs";
constexpr const char* kAssetManifestFetchPath = "/api/cardputer/assets/manifest";
constexpr const char* kFirmwareManifestFetchPath = "/api/cardputer/firmware/manifest";

constexpr const char* kPrefsWifiNs = "wifi_creds";
constexpr const char* kPrefsHubNs = "hub_ep";
constexpr const char* kPrefsDeviceNs = "device_cfg";
constexpr const char* kPrefsFocusNs = "focus_cfg";
constexpr const char* kPrefsContextNs = "ctx_cfg";
constexpr const char* kLegacyBridgeHost = "109.199.103.176";
constexpr uint16_t kLegacyBridgePort = 31889;
constexpr const char* kProdBridgeHost = "bridge.ai.k-digital.pro";
constexpr uint16_t kProdBridgePort = 443;

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
constexpr float kMaxRecordSeconds = 600.0f;
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
constexpr size_t kHttpStageChunkBytes = 1024;
constexpr uint32_t kVoiceSocketSettleMs = 300;
constexpr uint32_t kCtrlDoubleTapWindowMs = 420;
constexpr uint32_t kLauncherToggleDebounceMs = 140;
constexpr uint8_t kLauncherGroupCount = 6;
constexpr uint8_t kLauncherMaxSubItems = 4;
constexpr uint8_t kSettingsItemCount = 10;
constexpr uint8_t kClockFaceCount = 1;
constexpr uint8_t kHidKeyEscape = 0x29;
constexpr float kPi = 3.14159265358979323846f;
constexpr uint32_t kClockTickMs = 1000;
constexpr uint32_t kBatterySampleIntervalMs = 2000;
constexpr uint32_t kTimeSyncRetryMs = 15000;
constexpr uint32_t kTimeResyncMs = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kFocusMinDurationSec = 60;
constexpr uint32_t kFocusMaxDurationSec = 60UL * 60UL;
constexpr uint32_t kFocusDefaultSec = 25UL * 60UL;
constexpr uint32_t kFocusShortBreakDefaultSec = 5UL * 60UL;
constexpr uint32_t kFocusLongBreakDefaultSec = 15UL * 60UL;
constexpr uint8_t kFocusDefaultCycles = 4;
constexpr uint32_t kFocusSnoozeSec = 5UL * 60UL;
constexpr uint16_t kFocusMinBpm = 30;
constexpr uint16_t kFocusMaxBpm = 160;
constexpr uint32_t kTextTurnTaskStackSize = 8192;
constexpr uint32_t kTopicTaskStackSize = 8192;
constexpr size_t kMaxFlashVoiceNotes = 12;
constexpr size_t kMaxSdVoiceNotes = 256;
constexpr size_t kMaxStoredPreviewChars = 84;
constexpr size_t kMaxAudioAssets = 96;
constexpr size_t kMaxRuntimeLogLines = 36;
constexpr size_t kMaxContexts = 32;
constexpr size_t kMaxContextPreviewLines = 5;
constexpr size_t kVoicePreviewLineThreshold = 5;
constexpr const char* kVoiceDir = "/voice";
constexpr const char* kVoiceMetaExt = ".json";
constexpr const char* kVoiceAudioExt = ".wav";
constexpr const char* kPomodoroDir = "/pomodoro";
constexpr const char* kPomodoroAudioDir = "/pomodoro/audio";
constexpr const char* kSdAudioDir = "/audio";
constexpr const char* kPomodoroLogsDir = "/pomodoro/logs";
constexpr const char* kAssetManifestPath = "/pomodoro/manifest.json";
constexpr const char* kAudioIndexPath = "/pomodoro/audio/index.json";
constexpr const char* kOtaManifestPath = "/pomodoro/firmware.json";
constexpr const char* kContextRegistryPath = "/pomodoro/contexts.json";
constexpr const char* kEmojiDir = "/emoji";
constexpr const char* kRuntimeLogPath = "/pomodoro/logs/runtime.ndjson";
constexpr const char* kOtaTempPath = "/pomodoro/firmware.tmp.bin";
constexpr const char* kFocusCuePath = "/pomodoro/audio/focus_ru.wav";
constexpr const char* kBreakCuePath = "/pomodoro/audio/break_ru.wav";
constexpr const char* kReflectionCuePath = "/pomodoro/audio/reflection_ru.wav";
constexpr const char* kActiveRecordPcmPath = "/voice/__active_record.pcm";
constexpr const char* kActiveTtsPcmPath = "/voice/__active_tts.pcm";
constexpr uint8_t kSdSckPin = 40;
constexpr uint8_t kSdMisoPin = 39;
constexpr uint8_t kSdMosiPin = 14;
constexpr uint8_t kSdCsPin = 12;
constexpr uint32_t kSdFrequencyHz = 25000000;
constexpr size_t kRecordChunkSamples = 1024;
constexpr size_t kRecordChunkBytes = kRecordChunkSamples * sizeof(int16_t);
constexpr size_t kRecordChunkBufferCount = 3;
constexpr uint32_t kRecordDrainTimeoutMs = 900;
constexpr size_t kPlaybackChunkSamples = 2048;
constexpr size_t kPlaybackChunkBytes = kPlaybackChunkSamples * sizeof(int16_t);
constexpr size_t kPlaybackBufferCount = 2;
constexpr size_t kEmojiCacheCap = 10;
constexpr size_t kEmojiMaxPngBytes = 64 * 1024;
constexpr uint8_t kTca8418IntPin = 11;
constexpr uint8_t kTca8418I2cAddr = 0x34;
constexpr uint8_t kTca8418SdaPin = 8;
constexpr uint8_t kTca8418SclPin = 9;
constexpr const char* kBangkokTz = "ICT-7";

constexpr uint16_t rgb565_local(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

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

#ifndef DEFAULT_DEVICE_ID
#define DEFAULT_DEVICE_ID ""
#endif

#ifndef DEFAULT_DEVICE_TOKEN
#define DEFAULT_DEVICE_TOKEN ""
#endif

constexpr int kHeroSpriteW = 16;
constexpr int kHeroSpriteH = 16;
constexpr uint16_t HCLEAR = 0xF81F;
constexpr uint16_t HBLACK = TFT_BLACK;
constexpr uint16_t HWHITE = TFT_WHITE;
constexpr uint16_t HRED = rgb565_local(210, 50, 40);
constexpr uint16_t HDARK = rgb565_local(160, 30, 25);
constexpr uint16_t HHIGHLIGHT = rgb565_local(240, 100, 80);
constexpr uint16_t HORANGE = rgb565_local(230, 140, 60);
constexpr uint16_t HEYE = rgb565_local(20, 20, 20);
constexpr uint16_t HCLAW = rgb565_local(190, 40, 35);
constexpr uint16_t HTAIL = rgb565_local(180, 60, 50);

const uint16_t PROGMEM kHeroIdle1[kHeroSpriteW * kHeroSpriteH] = {
    HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HWHITE, HWHITE, HRED, HRED, HWHITE, HWHITE, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HWHITE, HEYE, HRED, HRED, HWHITE, HEYE, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HORANGE, HORANGE, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HTAIL, HBLACK, HDARK, HDARK, HBLACK, HTAIL, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HTAIL, HBLACK, HTAIL, HTAIL, HBLACK, HTAIL, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
};

const uint16_t PROGMEM kHeroIdle2[kHeroSpriteW * kHeroSpriteH] = {
    HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HBLACK, HBLACK, HRED, HRED, HBLACK, HBLACK, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HORANGE, HORANGE, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HTAIL, HBLACK, HDARK, HDARK, HBLACK, HTAIL, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HTAIL, HBLACK, HTAIL, HTAIL, HBLACK, HTAIL, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
};

const uint16_t PROGMEM kHeroIdle3[kHeroSpriteW * kHeroSpriteH] = {
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HWHITE, HWHITE, HRED, HRED, HWHITE, HWHITE, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HWHITE, HEYE, HRED, HRED, HWHITE, HEYE, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HORANGE, HORANGE, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HTAIL, HBLACK, HDARK, HDARK, HBLACK, HTAIL, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
};

const uint16_t PROGMEM kHeroHappy1[kHeroSpriteW * kHeroSpriteH] = {
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HHIGHLIGHT, HHIGHLIGHT, HRED, HRED, HHIGHLIGHT, HHIGHLIGHT, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HHIGHLIGHT, HEYE, HRED, HRED, HHIGHLIGHT, HEYE, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HORANGE, HBLACK, HBLACK, HORANGE, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HBLACK, HBLACK, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
};

const uint16_t PROGMEM kHeroTalk1[kHeroSpriteW * kHeroSpriteH] = {
    HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HWHITE, HWHITE, HRED, HRED, HWHITE, HWHITE, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HWHITE, HEYE, HRED, HRED, HWHITE, HEYE, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HBLACK, HORANGE, HORANGE, HBLACK, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HBLACK, HBLACK, HBLACK, HBLACK, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HTAIL, HBLACK, HDARK, HDARK, HBLACK, HTAIL, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HTAIL, HBLACK, HTAIL, HTAIL, HBLACK, HTAIL, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
};

const uint16_t PROGMEM kHeroTalk2[kHeroSpriteW * kHeroSpriteH] = {
    HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HWHITE, HWHITE, HRED, HRED, HWHITE, HWHITE, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HWHITE, HEYE, HRED, HRED, HWHITE, HEYE, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HBLACK, HBLACK, HBLACK, HBLACK, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HRED, HRED, HRED, HRED, HRED, HRED, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HBLACK, HCLAW, HCLAW, HBLACK, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HBLACK, HCLAW, HCLAW, HBLACK,
    HCLEAR, HBLACK, HBLACK, HCLEAR, HCLEAR, HBLACK, HDARK, HDARK, HDARK, HDARK, HBLACK, HCLEAR, HCLEAR, HBLACK, HBLACK, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HTAIL, HBLACK, HDARK, HDARK, HBLACK, HTAIL, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HTAIL, HBLACK, HTAIL, HTAIL, HBLACK, HTAIL, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
    HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HBLACK, HCLEAR, HBLACK, HBLACK, HCLEAR, HBLACK, HCLEAR, HCLEAR, HCLEAR, HCLEAR, HCLEAR,
};

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
  Home,
  ChatFace,
  Face,
  Hero,
  ChatFull,
  Clock,
  Battery,
  Voice,
  Focus,
  Library,
  AudioFolders,
  Assets,
  Ota,
  Contexts,
  Settings,
  Logs,
};

enum class FocusState : uint8_t {
  Stopped,
  Focus,
  ShortBreak,
  LongBreak,
  Reflect,
  Paused,
};

enum class EyeEmotion : uint8_t {
  Neutral,
  Speaking,
  Happy,
  Mad,
  Sad,
  Surprised,
  Sleepy,
  Thinking,
  Confused,
  Excited,
  Love,
};

enum class AssistantPulseState : uint8_t {
  Ready,
  Recording,
  Thinking,
  Playback,
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

struct AudioAsset {
  String path;
  String title;
  String category;
  String language;
  uint32_t size = 0;
  uint32_t durationMs = 0;
};

struct AudioFolder {
  String label;
  String category;
  uint16_t count = 0;
  uint32_t totalBytes = 0;
};

struct AssetManifestSummary {
  bool present = false;
  bool parsed = false;
  String version;
  String generatedAt;
  uint16_t fileCount = 0;
  uint32_t totalBytes = 0;
  String error;
};

struct OtaManifestSummary {
  bool present = false;
  bool parsed = false;
  String version;
  String channel;
  String url;
  String sha256;
  uint32_t size = 0;
  uint8_t minBattery = 50;
  bool confirmRequired = true;
  String error;
};

struct ConversationContext {
  String label;
  String key;
  String shortName;
  String topicKey;
  int32_t threadId = 0;
  uint32_t headSeq = 0;
  uint16_t unread = 0;
  bool remote = false;
};

struct ContextPreviewLine {
  String role;
  String text;
  bool voice = false;
};

struct BatterySnapshot {
  int32_t level = -1;
  int16_t voltageMv = -1;
  int32_t currentMa = 0;
  m5::Power_Class::is_charging_t charging = m5::Power_Class::charge_unknown;
};

struct TextTurnTaskPayload {
  String text;
  String url;
  String deviceId;
  String deviceToken;
  String conversationKey;
};

struct TextTurnTaskResult {
  int statusCode = 0;
  bool ok = false;
  bool parsed = false;
  bool hasReply = false;
  bool telegramDelivered = false;
  String reply;
  String shortText;
  String audioUrl;
  String audioPath;
  String audioTitle;
  String audioSha256;
  uint32_t audioSize = 0;
  String error;
  String response;
};

struct TopicTaskPayload {
  bool syncCatalog = false;
  bool selectCurrent = false;
  bool loadHistory = false;
};

struct TopicTaskResult {
  bool catalogOk = false;
  bool selectOk = false;
  bool historyOk = false;
  String label;
};

struct FocusSettings {
  uint32_t focusSec = kFocusDefaultSec;
  uint32_t shortBreakSec = kFocusShortBreakDefaultSec;
  uint32_t longBreakSec = kFocusLongBreakDefaultSec;
  uint8_t cyclesPerRound = kFocusDefaultCycles;
  bool autoStart = false;
  bool metronome = false;
  uint16_t bpm = 60;
};

struct DeviceSettings {
  bool autoUpdateFirmware = false;
  bool checkUpdatesOnBoot = true;
  bool beepOnUpdate = true;
  uint8_t minBatteryForUpdate = 40;
  uint8_t clockFace = 0;
  String updateChannel = "dev";
  String audioLanguage = "ru";
};

struct FocusRuntime {
  FocusState state = FocusState::Stopped;
  FocusState stateBeforePause = FocusState::Focus;
  uint32_t remainingSec = kFocusDefaultSec;
  uint32_t sessionTotalSec = kFocusDefaultSec;
  uint32_t lastTickMs = 0;
  uint16_t cycle = 1;
  uint16_t completedToday = 0;
  uint32_t focusedTodaySec = 0;
  uint32_t nextMetronomeMs = 0;
  bool helpVisible = false;
};

struct LauncherSubItem {
  const char* label;
  UiMode mode;
};

struct LauncherGroup {
  const char* title;
  const char* glyph;
  const char* hint;
  uint16_t accent;
  uint8_t count;
  LauncherSubItem items[kLauncherMaxSubItems];
};

struct EmojiCacheEntry {
  uint32_t codepoint = 0;
  uint8_t* data = nullptr;
  uint32_t len = 0;
  int16_t width = 0;
  int16_t height = 0;
};

constexpr LauncherGroup kLauncherGroups[kLauncherGroupCount] = {
    {"Assistant", "AI", "Voice, face, hero", rgb565_local(47, 227, 255), 4,
     {{"Pulse", UiMode::Home}, {"Big Eyes", UiMode::Face}, {"Klo Hero", UiMode::Hero}, {"Chat Eyes", UiMode::ChatFace}}},
    {"Chat", "CH", "Chat in active topic", rgb565_local(88, 210, 255), 2,
     {{"Full Chat", UiMode::ChatFull}, {"Chat Eyes", UiMode::ChatFace}}},
    {"Audio", "WAV", "Library, folders, voice", rgb565_local(88, 240, 141), 4,
     {{"Library", UiMode::Library}, {"Folders", UiMode::AudioFolders}, {"Voice Notes", UiMode::Voice}, {"Assets", UiMode::Assets}}},
    {"Focus", "25", "Timer and focus setup", rgb565_local(246, 194, 74), 1,
     {{"Focus Space", UiMode::Focus}}},
    {"OpenClaw", "OC", "Bridge runtime", rgb565_local(180, 150, 255), 2,
     {{"Logs", UiMode::Logs}, {"Firmware", UiMode::Ota}}},
    {"System", "SYS", "Power and settings", rgb565_local(203, 216, 226), 3,
     {{"Battery", UiMode::Battery}, {"Settings", UiMode::Settings}, {"Firmware", UiMode::Ota}}},
};

Preferences gPrefs;
WebSocketsClient gWs;
M5Canvas gCanvas(&M5Cardputer.Display);
Adafruit_TCA8418 gTcaKeyboard;
SPIClass gSdSpi(FSPI);

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
std::vector<AudioAsset> gAudioAssets;
std::vector<AudioFolder> gAudioFolders;
std::vector<ConversationContext> gContexts;
std::deque<ContextPreviewLine> gContextPreview;
std::deque<String> gRuntimeLogs;

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
bool gLittleFsReady = false;
bool gSdReady = false;
bool gLauncherVisible = false;
bool gAssistantPendingVisible = false;
int gChatScrollOffset = 0;
int gLauncherSelection = 0;
int gLauncherSubSelection = 0;
uint8_t gLauncherLastSub[kLauncherGroupCount] = {};
int gSelectedVoiceNote = 0;
int gSelectedAudioAsset = 0;
int gSelectedAudioFolder = 0;
String gAudioFolderFilter;
int gSelectedContext = 0;
String gSavedContextKey;
String gContextHistoryKey;
String gContextError;
bool gContextsRemoteLoaded = false;
uint32_t gContextsFetchedAtMs = 0;
uint32_t gContextHistoryFetchedAtMs = 0;
uint32_t gContextAnimStartMs = 0;
uint32_t gTopicOverlayUntilMs = 0;
uint32_t gTopicOverlayActionMs = 0;
int8_t gContextAnimDir = 0;
int gLogScrollOffset = 0;
int gSelectedSetting = 0;

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
uint32_t gLastBatterySampleMs = 0;

FaceMode gFaceMode = FaceMode::Idle;
KeyboardLayout gKeyboardLayout = KeyboardLayout::En;
UiMode gUiMode = UiMode::Home;
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
String gStorageLabel = "off";
String gLastKeySignature;
EmojiCacheEntry gEmojiCache[kEmojiCacheCap];
uint8_t gEmojiCacheSize = 0;
size_t gEmojiAssetCount = 0;
bool gTcaPressedKeys[4][14] = {};
bool gCtrlSoloHeld = false;
bool gCtrlSoloCandidate = false;
bool gG0VoiceHeld = false;
bool gRecordingToFile = false;
bool gPlaybackStreaming = false;
uint8_t* gDynamicPlaybackBuffer = nullptr;
volatile bool gTcaInterruptPending = false;
bool gTimeZoneConfigured = false;
bool gTimeValid = false;
File gActiveRecordFile;
String gActiveRecordPath;
size_t gActiveRecordBytes = 0;
size_t gActiveRecordCommittedBytes = 0;
size_t gActiveRecordExpectedBytes = 0;
std::deque<uint8_t> gRecordInflightChunks;
uint8_t gRecordNextChunkIndex = 0;
int16_t gRecordChunkBuffers[kRecordChunkBufferCount][kRecordChunkSamples];
File gPlaybackStreamFile;
String gPlaybackStreamPath;
uint32_t gPlaybackStreamSampleRate = 0;
size_t gPlaybackStreamRemainingBytes = 0;
uint8_t gPlaybackStreamNextBuffer = 0;
bool gPlaybackStreamRawPcm = false;
uint8_t gPlaybackStreamBuffers[kPlaybackBufferCount][kPlaybackChunkBytes];
int16_t gVoiceTickerX = 90;
uint8_t gVoiceGraphSpeed = 0;
uint8_t gVoiceLevel8 = 28;
uint32_t gAssistantPulsePressUntilMs = 0;
int gVoiceEqBars[14] = {};
BatterySnapshot gBatterySnapshot;
FocusSettings gFocusSettings;
DeviceSettings gDeviceSettings;
FocusRuntime gFocus;
AssetManifestSummary gAssetManifest;
OtaManifestSummary gOtaManifest;
TaskHandle_t gTextTurnTaskHandle = nullptr;
portMUX_TYPE gTextTurnMux = portMUX_INITIALIZER_UNLOCKED;
TextTurnTaskResult* gCompletedTextTurn = nullptr;
TaskHandle_t gTopicTaskHandle = nullptr;
portMUX_TYPE gTopicTaskMux = portMUX_INITIALIZER_UNLOCKED;
TopicTaskResult* gCompletedTopicTask = nullptr;
std::deque<String> gPendingTextTurns;
String gActiveTextTurn;

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
bool hasHidKey(const Keyboard_Class::KeysState& keys, uint8_t keyCode);
void submitInput();
void saveHubEndpoint(const String& host, uint16_t port);
void syncClockFromNtp(bool force = false);
void flashActivityLed(uint8_t r, uint8_t g, uint8_t b, uint32_t durationMs = 180);
void updateStatusLed();
void writeWavHeader(uint8_t* h, uint32_t dataSize, uint32_t sampleRate);
void serviceRecordingToFile(bool refill = true);
void updateG0VoiceTrigger();
void servicePlaybackStream();
bool handleGlobalEscape();
void loadFocusSettings();
void saveFocusSettings();
void loadDeviceSettings();
void saveDeviceSettings();
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
String currentConversationKey();
bool fetchRemoteTopicCatalog();
bool fetchSelectedTopicHistory();
bool selectRemoteCurrentTopic();
bool switchTopicRelative(int delta, bool selectAndLoad);
bool startTopicTask(bool syncCatalog, bool selectCurrent, bool loadHistory, const char* statusText = nullptr);
void processCompletedTopicTask();
void showTopicOverlay(int8_t direction = 0, uint32_t ttlMs = 1600);
bool isEmojiAssetCodepoint(uint32_t codepoint);
bool fetchRemoteAssetManifest();
bool fetchRemoteOtaManifest();
bool syncAssetsFromManifest();
String fitCurrentFontToWidth(String text, int32_t pixelLimit);
void drawTinyFooter(uint16_t bg, uint16_t fg, const String& text, int16_t x = 12, int16_t y = 124,
                    int16_t w = 216);
bool applyOtaFromManifest();
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
  if (legacyBridge || staleDomainPort || localBridge) {
    saveHubEndpoint(kProdBridgeHost, kProdBridgePort);
    logf("[CFG] migrated bridge endpoint host=%s port=%u", gHubHost.c_str(), gHubPort);
  }
}

bool hubUsesTls() {
  return gHubHost == kProdBridgeHost || gHubPort == 443;
}

String hubBaseUrl() {
  return String(hubUsesTls() ? "https://" : "http://") + gHubHost + ":" + String(gHubPort);
}

const char* uiModeName(UiMode mode) {
  switch (mode) {
    case UiMode::Home:
      return "Pulse";
    case UiMode::Face:
      return "Face";
    case UiMode::Hero:
      return "Hero";
    case UiMode::ChatFull:
      return "Chat";
    case UiMode::Clock:
      return "Pulse";
    case UiMode::Battery:
      return "Battery";
    case UiMode::Voice:
      return "Voice";
    case UiMode::Focus:
      return "Focus";
    case UiMode::Library:
      return "Library";
    case UiMode::AudioFolders:
      return "Folders";
    case UiMode::Assets:
      return "Assets";
    case UiMode::Ota:
      return "OTA";
    case UiMode::Contexts:
      return "Topics";
    case UiMode::Settings:
      return "Settings";
    case UiMode::Logs:
      return "Logs";
    case UiMode::ChatFace:
    default:
      return "Chat+Face";
  }
}

const char* eyeEmotionName(EyeEmotion emotion) {
  switch (emotion) {
    case EyeEmotion::Speaking:
      return "speaking";
    case EyeEmotion::Happy:
      return "happy";
    case EyeEmotion::Mad:
      return "mad";
    case EyeEmotion::Sad:
      return "sad";
    case EyeEmotion::Surprised:
      return "surprised";
    case EyeEmotion::Sleepy:
      return "sleepy";
    case EyeEmotion::Thinking:
      return "thinking";
    case EyeEmotion::Confused:
      return "confused";
    case EyeEmotion::Excited:
      return "excited";
    case EyeEmotion::Love:
      return "love";
    case EyeEmotion::Neutral:
    default:
      return "neutral";
  }
}

EyeEmotion currentEyeEmotion() {
  switch (gFaceMode) {
    case FaceMode::Listening:
      return EyeEmotion::Excited;
    case FaceMode::Thinking:
      return EyeEmotion::Thinking;
    case FaceMode::Speaking:
      return EyeEmotion::Speaking;
    case FaceMode::Error:
      return EyeEmotion::Mad;
    case FaceMode::Idle:
    default:
      break;
  }

  switch ((millis() / 3800) % 8) {
    case 1:
      return EyeEmotion::Happy;
    case 2:
      return EyeEmotion::Love;
    case 3:
      return EyeEmotion::Sleepy;
    case 4:
      return EyeEmotion::Confused;
    case 5:
      return EyeEmotion::Surprised;
    case 6:
      return EyeEmotion::Excited;
    default:
      return EyeEmotion::Neutral;
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

size_t voiceNoteLimit() {
  return gSdReady ? kMaxSdVoiceNotes : kMaxFlashVoiceNotes;
}

String cleanReplyForDevice(const String& text) {
  String out = text;
  out.replace("[[audio_as_voice]]", "");
  out.replace("[[audio_as_text]]", "");
  out.trim();
  return out;
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

AssistantPulseState assistantPulseState() {
  if (gRecording) {
    return AssistantPulseState::Recording;
  }
  if (gSubmitting || gThinking) {
    return AssistantPulseState::Thinking;
  }
  if (gPlaybackActive) {
    return AssistantPulseState::Playback;
  }
  return AssistantPulseState::Ready;
}

void pulseButtonPress(uint32_t durationMs = 120) {
  gAssistantPulsePressUntilMs = millis() + durationMs;
}

void showTopicOverlay(int8_t direction, uint32_t ttlMs) {
  uint32_t now = millis();
  gTopicOverlayActionMs = now;
  gTopicOverlayUntilMs = now + ttlMs;
  if (direction != 0) {
    gContextAnimDir = direction;
    gContextAnimStartMs = now;
  }
}

void updateVoiceLevelFromSamples(const int16_t* samples, size_t sampleCount) {
  if (!samples || sampleCount == 0) {
    return;
  }
  uint32_t acc = 0;
  size_t reads = 0;
  for (size_t i = 0; i < sampleCount; i += 16) {
    int32_t v = samples[i];
    acc += static_cast<uint32_t>(v < 0 ? -v : v);
    reads++;
  }
  if (reads == 0) {
    return;
  }
  uint32_t avg = acc / reads;
  uint8_t level = static_cast<uint8_t>(min<uint32_t>(255, avg >> 4));
  gVoiceLevel8 = static_cast<uint8_t>((static_cast<uint16_t>(gVoiceLevel8) * 3 + level) / 4);
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
void pushError(const String& text) {
  pushLine("ERR", text, LineKind::Error);
  appendRuntimeLog("ERR", text, true);
}
void pushUser(const String& text) { pushLine("YOU", text, LineKind::User); }
void pushAssistant(const String& text) { pushLine("AI", text, LineKind::Assistant); }

bool decodeUtf8At(const String& value, int index, uint32_t& codepoint, int& consumed) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(value.c_str());
  int len = value.length();
  uint8_t b0 = bytes[index];
  if (b0 < 0x80) {
    codepoint = b0;
    consumed = 1;
    return true;
  }
  if ((b0 & 0xE0) == 0xC0 && index + 1 < len && (bytes[index + 1] & 0xC0) == 0x80) {
    codepoint = ((b0 & 0x1F) << 6) | (bytes[index + 1] & 0x3F);
    consumed = 2;
    return true;
  }
  if ((b0 & 0xF0) == 0xE0 && index + 2 < len &&
      (bytes[index + 1] & 0xC0) == 0x80 && (bytes[index + 2] & 0xC0) == 0x80) {
    codepoint = ((b0 & 0x0F) << 12) | ((bytes[index + 1] & 0x3F) << 6) |
                (bytes[index + 2] & 0x3F);
    consumed = 3;
    return true;
  }
  if ((b0 & 0xF8) == 0xF0 && index + 3 < len &&
      (bytes[index + 1] & 0xC0) == 0x80 && (bytes[index + 2] & 0xC0) == 0x80 &&
      (bytes[index + 3] & 0xC0) == 0x80) {
    codepoint = ((b0 & 0x07) << 18) | ((bytes[index + 1] & 0x3F) << 12) |
                ((bytes[index + 2] & 0x3F) << 6) | (bytes[index + 3] & 0x3F);
    consumed = 4;
    return true;
  }
  codepoint = b0;
  consumed = 1;
  return false;
}

bool shouldSuppressEmojiModifier(uint32_t codepoint) {
  return codepoint == 0x200D || codepoint == 0x20E3 || codepoint == 0xFE0E ||
         codepoint == 0xFE0F || (codepoint >= 0x1F3FB && codepoint <= 0x1F3FF) ||
         (codepoint >= 0xE0100 && codepoint <= 0xE01EF);
}

String normalizeEmojiDisplayText(const String& raw) {
  String out;
  out.reserve(raw.length());
  int i = 0;
  while (i < raw.length()) {
    uint32_t codepoint = 0;
    int consumed = 1;
    bool ok = decodeUtf8At(raw, i, codepoint, consumed);
    if (ok && shouldSuppressEmojiModifier(codepoint)) {
      i += consumed;
      continue;
    }
    for (int n = 0; n < consumed && i + n < raw.length(); ++n) {
      out += raw[i + n];
    }
    i += consumed;
  }
  return out;
}

bool emojiAssetAvailable(uint32_t codepoint) {
  if (!gSdReady || !isEmojiAssetCodepoint(codepoint)) {
    return false;
  }
  char path[40];
  snprintf(path, sizeof(path), "%s/u%lX.png", kEmojiDir, static_cast<unsigned long>(codepoint));
  if (SD.exists(path)) {
    return true;
  }
  snprintf(path, sizeof(path), "%s/u%lx.png", kEmojiDir, static_cast<unsigned long>(codepoint));
  return SD.exists(path);
}

String normalizeTopicTitleForDisplay(const String& raw, bool keepAvailableEmoji = true) {
  String cleaned = raw;
  cleaned.replace("to:root-main", "");
  cleaned.replace("root-main", "");
  cleaned.trim();

  String out;
  out.reserve(cleaned.length());
  int i = 0;
  while (i < cleaned.length()) {
    uint32_t codepoint = 0;
    int consumed = 1;
    bool ok = decodeUtf8At(cleaned, i, codepoint, consumed);
    if (ok && shouldSuppressEmojiModifier(codepoint)) {
      i += consumed;
      continue;
    }
    if (ok && isEmojiAssetCodepoint(codepoint) && !emojiAssetAvailable(codepoint)) {
      i += consumed;
      continue;
    }
    for (int n = 0; n < consumed && i + n < cleaned.length(); ++n) {
      out += cleaned[i + n];
    }
    i += consumed;
  }
  out.trim();
  if (out.isEmpty()) {
    out = "Topic";
  }
  (void)keepAvailableEmoji;
  return out;
}

std::vector<VisualLine> buildVisualLines(int32_t pixelLimit) {
  std::vector<VisualLine> lines;
  for (const auto& entry : gChatLines) {
    String full = entry.prefix + ": " + normalizeEmojiDisplayText(entry.text);
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

fs::FS* activeVoiceFs() {
  if (gSdReady) {
    return &SD;
  }
  if (gLittleFsReady) {
    return &LittleFS;
  }
  return nullptr;
}

fs::FS* fallbackVoiceFs() {
  if (gSdReady && gLittleFsReady) {
    return &LittleFS;
  }
  return nullptr;
}

String activeVoiceStorageLabel() {
  if (gSdReady) {
    return "sd";
  }
  if (gLittleFsReady) {
    return "flash";
  }
  return "off";
}

bool fsExists(fs::FS& fs, const String& path) {
  return fs.exists(path.c_str());
}

bool fsRemove(fs::FS& fs, const String& path) {
  return fs.remove(path.c_str());
}

bool fsRename(fs::FS& fs, const String& from, const String& to) {
  return fs.rename(from.c_str(), to.c_str());
}

String parentDirOf(const String& path) {
  int slash = path.lastIndexOf('/');
  if (slash <= 0) {
    return "/";
  }
  return path.substring(0, slash);
}

bool ensureDirRecursive(fs::FS& fs, const String& dirPath) {
  if (dirPath.isEmpty() || dirPath == "/" || fsExists(fs, dirPath)) {
    return true;
  }
  int start = 1;
  while (start < dirPath.length()) {
    int slash = dirPath.indexOf('/', start);
    String part = slash < 0 ? dirPath : dirPath.substring(0, slash);
    if (!part.isEmpty() && !fsExists(fs, part) && !fs.mkdir(part)) {
      return false;
    }
    if (slash < 0) {
      break;
    }
    start = slash + 1;
  }
  return fsExists(fs, dirPath) || fs.mkdir(dirPath);
}

bool ensureVoiceStorageOn(fs::FS& fs) {
  if (!fsExists(fs, kVoiceDir)) {
    return fs.mkdir(kVoiceDir);
  }
  return true;
}

bool ensureVoiceStorage() {
  fs::FS* fs = activeVoiceFs();
  if (!fs) {
    return false;
  }
  return ensureVoiceStorageOn(*fs);
}

bool ensurePomodoroStorageOn(fs::FS& fs) {
  if (!fsExists(fs, kPomodoroDir) && !fs.mkdir(kPomodoroDir)) {
    return false;
  }
  if (!fsExists(fs, kPomodoroAudioDir) && !fs.mkdir(kPomodoroAudioDir)) {
    return false;
  }
  if (!fsExists(fs, kPomodoroLogsDir) && !fs.mkdir(kPomodoroLogsDir)) {
    return false;
  }
  return true;
}

uint64_t activeStorageTotalBytes() {
  if (gSdReady) {
    return SD.totalBytes();
  }
  if (gLittleFsReady) {
    return LittleFS.totalBytes();
  }
  return 0;
}

uint64_t activeStorageUsedBytes() {
  if (gSdReady) {
    return SD.usedBytes();
  }
  if (gLittleFsReady) {
    return LittleFS.usedBytes();
  }
  return 0;
}

bool isEmojiAssetCodepoint(uint32_t codepoint) {
  return codepoint == 0x00A9 || codepoint == 0x00AE || codepoint == 0x203C ||
         codepoint == 0x2049 || codepoint == 0x2122 || codepoint == 0x2139 ||
         (codepoint >= 0x2194 && codepoint <= 0x21AA) ||
         (codepoint >= 0x2300 && codepoint <= 0x23FF) ||
         (codepoint >= 0x2460 && codepoint <= 0x27BF) ||
         (codepoint >= 0x2900 && codepoint <= 0x2BFF) ||
         (codepoint >= 0x3000 && codepoint <= 0x32FF) ||
         (codepoint >= 0x1F000 && codepoint <= 0x1FAFF) ||
         (codepoint >= 0xFE000 && codepoint <= 0xFFFFD);
}

void releaseEmojiCacheEntry(EmojiCacheEntry& entry) {
  if (entry.data) {
    free(entry.data);
  }
  entry = EmojiCacheEntry{};
}

void clearEmojiCache() {
  for (uint8_t i = 0; i < gEmojiCacheSize; ++i) {
    releaseEmojiCacheEntry(gEmojiCache[i]);
  }
  gEmojiCacheSize = 0;
}

const EmojiCacheEntry* rememberEmojiEntry(const EmojiCacheEntry& entry) {
  if (gEmojiCacheSize < kEmojiCacheCap) {
    gEmojiCache[gEmojiCacheSize] = entry;
    return &gEmojiCache[gEmojiCacheSize++];
  }
  releaseEmojiCacheEntry(gEmojiCache[0]);
  memmove(&gEmojiCache[0], &gEmojiCache[1], sizeof(gEmojiCache[0]) * (kEmojiCacheCap - 1));
  gEmojiCache[kEmojiCacheCap - 1] = entry;
  return &gEmojiCache[kEmojiCacheCap - 1];
}

const EmojiCacheEntry* emojiCacheLookup(uint32_t codepoint) {
  for (uint8_t i = 0; i < gEmojiCacheSize; ++i) {
    if (gEmojiCache[i].codepoint == codepoint) {
      return &gEmojiCache[i];
    }
  }

  EmojiCacheEntry entry;
  entry.codepoint = codepoint;
  if (!gSdReady || !isEmojiAssetCodepoint(codepoint)) {
    return rememberEmojiEntry(entry);
  }

  char path[40];
  snprintf(path, sizeof(path), "%s/u%lX.png", kEmojiDir, static_cast<unsigned long>(codepoint));
  File file = SD.open(path, "r");
  if (!file) {
    snprintf(path, sizeof(path), "%s/u%lx.png", kEmojiDir, static_cast<unsigned long>(codepoint));
    file = SD.open(path, "r");
  }
  if (!file) {
    return rememberEmojiEntry(entry);
  }

  size_t size = file.size();
  if (size > 24 && size <= kEmojiMaxPngBytes) {
    uint8_t* data = static_cast<uint8_t*>(malloc(size));
    if (data && file.read(data, size) == size &&
        memcmp(data, "\x89PNG\r\n\x1A\n", 8) == 0) {
      uint32_t width = (static_cast<uint32_t>(data[16]) << 24) |
                       (static_cast<uint32_t>(data[17]) << 16) |
                       (static_cast<uint32_t>(data[18]) << 8) | data[19];
      uint32_t height = (static_cast<uint32_t>(data[20]) << 24) |
                        (static_cast<uint32_t>(data[21]) << 16) |
                        (static_cast<uint32_t>(data[22]) << 8) | data[23];
      if (width > 0 && width <= 240 && height > 0 && height <= 240) {
        entry.data = data;
        entry.len = static_cast<uint32_t>(size);
        entry.width = static_cast<int16_t>(width);
        entry.height = static_cast<int16_t>(height);
        data = nullptr;
      }
    }
    if (data) {
      free(data);
    }
  }
  file.close();
  return rememberEmojiEntry(entry);
}

int32_t drawEmojiFromSd(lgfx::LGFXBase* gfx, int32_t x, int32_t y, uint32_t codepoint,
                        int32_t fontHeight) {
  const EmojiCacheEntry* entry = emojiCacheLookup(codepoint);
  if (!gfx || !entry || !entry->data || entry->height <= 0 || fontHeight <= 0) {
    return 0;
  }

  float scale = static_cast<float>(fontHeight) / static_cast<float>(entry->height);
  int32_t drawY = y - static_cast<int32_t>((fontHeight * 90) / 100);
  if (!gfx->drawPng(entry->data, entry->len, x, drawY, 0, 0, 0, 0, scale, 0)) {
    return 0;
  }
  return max<int32_t>(1, static_cast<int32_t>(entry->width * scale));
}

void configureEmojiRendering() {
  M5Cardputer.Display.setEmojiCallback(drawEmojiFromSd);
  gCanvas.setEmojiCallback(drawEmojiFromSd);
}

void scanEmojiAssets() {
  gEmojiAssetCount = 0;
  clearEmojiCache();
  if (!gSdReady) {
    logf("[EMOJI] SD not mounted; emoji assets disabled");
    return;
  }
  // Directory enumeration can block boot on large or fragmented SD cards.
  // Emoji PNG files are loaded by exact codepoint path on demand.
  logf("[EMOJI] lazy SD lookup enabled");
}

String bytesToHex(const uint8_t* bytes, size_t len) {
  static const char* hex = "0123456789abcdef";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out += hex[(bytes[i] >> 4) & 0x0F];
    out += hex[bytes[i] & 0x0F];
  }
  return out;
}

String sha256File(fs::FS& fs, const String& path) {
  File file = fs.open(path, "r");
  if (!file) {
    return "";
  }
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  uint8_t buffer[1024];
  while (file.available()) {
    size_t n = file.read(buffer, sizeof(buffer));
    if (n == 0) {
      break;
    }
    mbedtls_sha256_update(&ctx, buffer, n);
  }
  file.close();
  uint8_t hash[32];
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);
  return bytesToHex(hash, sizeof(hash));
}

bool configureHttpClient(HTTPClient& http, std::unique_ptr<WiFiClientSecure>& secureClient, const String& url,
                         uint32_t timeoutMs = 60000) {
  http.setTimeout(timeoutMs);
  http.setReuse(false);
  bool tls = url.startsWith("https://");
  if (tls) {
    secureClient.reset(new WiFiClientSecure());
    secureClient->setInsecure();
    secureClient->setHandshakeTimeout(12);
    return http.begin(*secureClient, url);
  }
  return http.begin(url);
}

bool downloadUrlToFile(const String& url, const String& path, const String& expectedSha = "", uint32_t expectedSize = 0) {
  if ((!gWifiReady && !ensureWifiForHttp("download", 3000)) || url.isEmpty()) {
    setStatus("Download no Wi-Fi", 1200);
    return false;
  }
  fs::FS* fs = activeVoiceFs();
  if (!fs || !ensurePomodoroStorageOn(*fs) || !ensureDirRecursive(*fs, parentDirOf(path))) {
    setStatus("Download no storage", 1200);
    return false;
  }

  String tempPath = path + ".tmp";
  fsRemove(*fs, tempPath);
  File out = fs->open(tempPath, "w");
  if (!out) {
    setStatus("Download open failed", 1200);
    return false;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  if (!configureHttpClient(http, secureClient, url, 90000)) {
    out.close();
    fsRemove(*fs, tempPath);
    setStatus("Download connect failed", 1200);
    return false;
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  int code = http.GET();
  if (code < 200 || code >= 300) {
    http.end();
    out.close();
    fsRemove(*fs, tempPath);
    setStatus("HTTP " + String(code), 1500);
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[1024];
  uint32_t written = 0;
  int contentLength = http.getSize();
  uint32_t lastProgress = millis();
  while (http.connected() && (contentLength < 0 || written < static_cast<uint32_t>(contentLength))) {
    size_t available = stream->available();
    if (!available) {
      if (millis() - lastProgress > 15000) {
        break;
      }
      delay(2);
      continue;
    }
    size_t chunk = min(available, sizeof(buffer));
    int readBytes = stream->readBytes(buffer, chunk);
    if (readBytes <= 0) {
      break;
    }
    if (out.write(buffer, readBytes) != static_cast<size_t>(readBytes)) {
      http.end();
      out.close();
      fsRemove(*fs, tempPath);
      setStatus("Download write failed", 1200);
      return false;
    }
    written += readBytes;
    lastProgress = millis();
  }
  http.end();
  out.close();

  if ((expectedSize > 0 && written != expectedSize) ||
      (contentLength > 0 && written != static_cast<uint32_t>(contentLength))) {
    fsRemove(*fs, tempPath);
    setStatus("Download short", 1200);
    return false;
  }
  if (!expectedSha.isEmpty()) {
    String actualSha = sha256File(*fs, tempPath);
    actualSha.toLowerCase();
    String expected = expectedSha;
    expected.toLowerCase();
    if (actualSha != expected) {
      fsRemove(*fs, tempPath);
      setStatus("SHA mismatch", 1600);
      appendRuntimeLog("SHA", path + " mismatch", true);
      return false;
    }
  }
  fsRemove(*fs, path);
  if (!fsRename(*fs, tempPath, path)) {
    fsRemove(*fs, tempPath);
    setStatus("Rename failed", 1200);
    return false;
  }
  return true;
}

String voiceAudioPath(const String& id) {
  return String(kVoiceDir) + "/" + id + kVoiceAudioExt;
}

String voiceMetaPath(const String& id) {
  return String(kVoiceDir) + "/" + id + kVoiceMetaExt;
}

String makeVoiceNoteId(bool assistant) {
  return String(assistant ? "a_" : "u_") + String(millis()) + "_" + chipIdSuffix();
}

String makeVoiceNoteTitle(bool assistant, size_t audioLen, uint32_t sampleRate) {
  String who = assistant ? "Claw" : "Me";
  String stamp = String(millis() / 1000);
  if (hasValidSystemTime()) {
    time_t now = time(nullptr);
    struct tm localNow;
    localtime_r(&now, &localNow);
    char buf[5];
    strftime(buf, sizeof(buf), "%H%M", &localNow);
    stamp = buf;
  }
  uint32_t durationSec = sampleRate ? static_cast<uint32_t>((audioLen / 2U) / sampleRate) : 0;
  durationSec = max<uint32_t>(1, durationSec);
  return who + "-" + stamp + "-" + String(durationSec) + "s";
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
  fs::FS* fs = activeVoiceFs();
  size_t limit = voiceNoteLimit();
  while (gVoiceNotes.size() > limit) {
    const auto& note = gVoiceNotes.back();
    if (fs && !gSdReady) {
      fsRemove(*fs, note.audioPath);
      fsRemove(*fs, note.metaPath);
    }
    gVoiceNotes.pop_back();
  }
  clampVoiceSelection();
}

void loadVoiceNotes() {
  gVoiceNotes.clear();
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return;
  }
  File dir = fs->open(kVoiceDir);
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

    File meta = fs->open(path, "r");
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
    if (!note.id.isEmpty() && !note.audioPath.isEmpty() && fsExists(*fs, note.audioPath)) {
      gVoiceNotes.push_back(note);
    }
  }
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
}

String filenameFromPath(const String& path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

String audioCategoryFromPath(const String& path) {
  String tail;
  String pomodoroPrefix = String(kPomodoroAudioDir) + "/";
  String userPrefix = String(kSdAudioDir) + "/";
  if (path.startsWith(userPrefix)) {
    tail = path.substring(userPrefix.length());
  } else if (path.startsWith(pomodoroPrefix)) {
    tail = path.substring(pomodoroPrefix.length());
  } else {
    return "voice";
  }
  int slash = tail.indexOf('/');
  if (slash > 0) {
    return tail.substring(0, slash);
  }
  return "root";
}

String audioFolderLabel(const String& category) {
  if (category.isEmpty()) {
    return "All audio";
  }
  if (category == "root") {
    return "Root";
  }
  return category;
}

bool audioAssetMatchesFolder(const AudioAsset& asset) {
  return gAudioFolderFilter.isEmpty() || asset.category == gAudioFolderFilter;
}

int audioAssetCountForCurrentFolder() {
  if (gAudioFolderFilter.isEmpty()) {
    return static_cast<int>(gAudioAssets.size());
  }
  int count = 0;
  for (const auto& asset : gAudioAssets) {
    if (audioAssetMatchesFolder(asset)) {
      count++;
    }
  }
  return count;
}

int firstAudioAssetInCurrentFolder() {
  for (int i = 0; i < static_cast<int>(gAudioAssets.size()); ++i) {
    if (audioAssetMatchesFolder(gAudioAssets[i])) {
      return i;
    }
  }
  return -1;
}

int audioAssetRankInCurrentFolder(int assetIndex) {
  int rank = 0;
  for (int i = 0; i < static_cast<int>(gAudioAssets.size()); ++i) {
    if (!audioAssetMatchesFolder(gAudioAssets[i])) {
      continue;
    }
    if (i == assetIndex) {
      return rank;
    }
    rank++;
  }
  return -1;
}

void clampSelectedAudioAssetToFolder() {
  if (gAudioAssets.empty()) {
    gSelectedAudioAsset = 0;
    return;
  }
  gSelectedAudioAsset = clampValue<int>(gSelectedAudioAsset, 0, static_cast<int>(gAudioAssets.size()) - 1);
  if (!audioAssetMatchesFolder(gAudioAssets[gSelectedAudioAsset])) {
    int first = firstAudioAssetInCurrentFolder();
    if (first >= 0) {
      gSelectedAudioAsset = first;
    }
  }
}

void moveSelectedAudioAssetInFolder(int delta) {
  if (gAudioAssets.empty() || audioAssetCountForCurrentFolder() == 0) {
    return;
  }
  clampSelectedAudioAssetToFolder();
  int index = gSelectedAudioAsset;
  int total = static_cast<int>(gAudioAssets.size());
  for (int step = 0; step < total; ++step) {
    index = (index + delta + total) % total;
    if (audioAssetMatchesFolder(gAudioAssets[index])) {
      gSelectedAudioAsset = index;
      return;
    }
  }
}

void rebuildAudioFolders() {
  gAudioFolders.clear();
  AudioFolder all;
  all.label = "All audio";
  all.category = "";
  for (const auto& asset : gAudioAssets) {
    all.count++;
    all.totalBytes += asset.size;
    int folderIndex = -1;
    for (int i = 0; i < static_cast<int>(gAudioFolders.size()); ++i) {
      if (gAudioFolders[i].category == asset.category) {
        folderIndex = i;
        break;
      }
    }
    if (folderIndex < 0) {
      AudioFolder folder;
      folder.category = asset.category;
      folder.label = audioFolderLabel(asset.category);
      gAudioFolders.push_back(folder);
      folderIndex = static_cast<int>(gAudioFolders.size()) - 1;
    }
    gAudioFolders[folderIndex].count++;
    gAudioFolders[folderIndex].totalBytes += asset.size;
  }
  gAudioFolders.insert(gAudioFolders.begin(), all);
  gSelectedAudioFolder = clampValue<int>(gSelectedAudioFolder, 0, max<int>(0, static_cast<int>(gAudioFolders.size()) - 1));
  if (!gAudioFolderFilter.isEmpty()) {
    bool filterStillExists = false;
    for (const auto& folder : gAudioFolders) {
      if (folder.category == gAudioFolderFilter) {
        filterStillExists = true;
        break;
      }
    }
    if (!filterStillExists) {
      gAudioFolderFilter = "";
      gSelectedAudioFolder = 0;
    }
  }
  clampSelectedAudioAssetToFolder();
}

void scanAudioAssetsRecursive(fs::FS& fs, const String& dirPath, uint8_t depth) {
  if (depth > 4 || gAudioAssets.size() >= kMaxAudioAssets) {
    return;
  }
  File dir = fs.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  while (gAudioAssets.size() < kMaxAudioAssets) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }
    String path = file.path();
    bool directory = file.isDirectory();
    uint32_t size = static_cast<uint32_t>(file.size());
    file.close();
    if (directory) {
      scanAudioAssetsRecursive(fs, path, depth + 1);
      continue;
    }
    String lower = path;
    lower.toLowerCase();
    if (!lower.endsWith(".wav")) {
      continue;
    }
    AudioAsset asset;
    asset.path = path;
    asset.title = filenameFromPath(path);
    asset.category = audioCategoryFromPath(path);
    asset.language = path.indexOf("/ru/") >= 0 || path.endsWith("_ru.wav") ? "ru" : "";
    asset.size = size;
    gAudioAssets.push_back(asset);
  }
}

void scanAudioAssets() {
  gAudioAssets.clear();
  fs::FS* fs = activeVoiceFs();
  if (!fs || !ensurePomodoroStorageOn(*fs)) {
    rebuildAudioFolders();
    appendRuntimeLog("ASSET", "storage unavailable", true);
    return;
  }
  if (fsExists(*fs, kSdAudioDir)) {
    scanAudioAssetsRecursive(*fs, kSdAudioDir, 0);
  }
  scanAudioAssetsRecursive(*fs, kPomodoroAudioDir, 0);
  rebuildAudioFolders();
  if (gSelectedAudioAsset >= static_cast<int>(gAudioAssets.size())) {
    gSelectedAudioAsset = max<int>(0, static_cast<int>(gAudioAssets.size()) - 1);
  }
  clampSelectedAudioAssetToFolder();
  appendRuntimeLog("LIB", "audio scan count=" + String(gAudioAssets.size()), true);
}

void loadAssetManifestSummary() {
  gAssetManifest = AssetManifestSummary{};
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, kAssetManifestPath)) {
    gAssetManifest.error = "missing";
    return;
  }
  gAssetManifest.present = true;
  File file = fs->open(kAssetManifestPath, "r");
  if (!file) {
    gAssetManifest.error = "open failed";
    return;
  }
  DynamicJsonDocument doc(8192);
  auto err = deserializeJson(doc, file);
  file.close();
  if (err) {
    gAssetManifest.error = err.c_str();
    return;
  }
  gAssetManifest.parsed = true;
  gAssetManifest.version = doc["asset_version"] | doc["version"] | "";
  gAssetManifest.generatedAt = doc["generated_at"] | doc["generated"] | "";
  JsonArray files = doc["files"].as<JsonArray>();
  gAssetManifest.fileCount = files.size();
  uint32_t total = 0;
  for (JsonObject item : files) {
    total += item["size"] | 0;
  }
  gAssetManifest.totalBytes = total;
  appendRuntimeLog("ASSET", "manifest files=" + String(gAssetManifest.fileCount), true);
}

void loadOtaManifestSummary() {
  gOtaManifest = OtaManifestSummary{};
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, kOtaManifestPath)) {
    gOtaManifest.error = "missing";
    return;
  }
  gOtaManifest.present = true;
  File file = fs->open(kOtaManifestPath, "r");
  if (!file) {
    gOtaManifest.error = "open failed";
    return;
  }
  DynamicJsonDocument doc(3072);
  auto err = deserializeJson(doc, file);
  file.close();
  if (err) {
    gOtaManifest.error = err.c_str();
    return;
  }
  gOtaManifest.parsed = true;
  gOtaManifest.version = doc["version"] | "";
  gOtaManifest.channel = doc["channel"] | "";
  gOtaManifest.url = doc["firmware_url"] | doc["url"] | "";
  gOtaManifest.sha256 = doc["sha256"] | "";
  gOtaManifest.size = doc["size"] | 0;
  gOtaManifest.minBattery = doc["min_battery"] | 50;
  gOtaManifest.confirmRequired = doc["confirm_required"] | true;
  appendRuntimeLog("OTA", "manifest version=" + gOtaManifest.version, true);
}

bool fetchRemoteJsonToFile(const String& path, const String& localPath, const String& label) {
  String url = hubBaseUrl() + path;
  bool ok = downloadUrlToFile(url, localPath);
  appendRuntimeLog(label, ok ? "fetch ok" : "fetch failed", true);
  return ok;
}

bool fetchRemoteAssetManifest() {
  if (fetchRemoteJsonToFile(kAssetManifestFetchPath, kAssetManifestPath, "ASSET")) {
    loadAssetManifestSummary();
    return true;
  }
  return false;
}

bool fetchRemoteOtaManifest() {
  if (fetchRemoteJsonToFile(kFirmwareManifestFetchPath, kOtaManifestPath, "OTA")) {
    loadOtaManifestSummary();
    if (gDeviceSettings.beepOnUpdate && gOtaManifest.parsed && !gOtaManifest.version.isEmpty() &&
        gOtaManifest.version != kAppVersion) {
      focusTone(1320, 120);
    }
    return true;
  }
  return false;
}

bool syncAssetsFromManifest() {
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, kAssetManifestPath)) {
    setStatus("No asset manifest", 1200);
    return false;
  }
  File file = fs->open(kAssetManifestPath, "r");
  if (!file) {
    return false;
  }
  DynamicJsonDocument doc(12288);
  auto err = deserializeJson(doc, file);
  file.close();
  if (err || !doc["files"].is<JsonArray>()) {
    setStatus("Bad manifest", 1200);
    return false;
  }
  uint16_t okCount = 0;
  uint16_t failCount = 0;
  for (JsonObject item : doc["files"].as<JsonArray>()) {
    String path = item["path"] | "";
    String url = item["url"] | "";
    String sha = item["sha256"] | "";
    uint32_t size = item["size"] | 0;
    if (path.isEmpty() || url.isEmpty()) {
      continue;
    }
    bool alreadyOk = false;
    if (!sha.isEmpty() && fsExists(*fs, path)) {
      String actual = sha256File(*fs, path);
      actual.toLowerCase();
      sha.toLowerCase();
      alreadyOk = actual == sha;
    }
    if (alreadyOk || downloadUrlToFile(url, path, sha, size)) {
      ++okCount;
    } else {
      ++failCount;
    }
    setStatus("Assets " + String(okCount) + "/" + String(okCount + failCount), 500);
    render();
  }
  scanAudioAssets();
  loadAssetManifestSummary();
  appendRuntimeLog("ASSET", "sync ok=" + String(okCount) + " fail=" + String(failCount), true);
  setStatus(failCount ? "Asset sync partial" : "Asset sync done", 1600);
  return failCount == 0;
}

bool uploadRuntimeLogs() {
  if ((!gWifiReady && !ensureWifiForHttp("logs", 3000)) || gHubHost.isEmpty()) {
    return false;
  }
  DynamicJsonDocument doc(8192);
  doc["device_id"] = gDeviceId;
  doc["firmware_version"] = kAppVersion;
  doc["git"] = kBuildGitSha;
  JsonArray events = doc.createNestedArray("events");
  for (const auto& line : gRuntimeLogs) {
    JsonObject ev = events.createNestedObject();
    ev["ts"] = millis();
    ev["level"] = "info";
    ev["message"] = line;
    ev["free_heap"] = ESP.getFreeHeap();
    ev["rssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
    ev["storage"] = gStorageLabel;
  }
  String body;
  serializeJson(doc, body);

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + kCardputerLogsPath;
  if (!configureHttpClient(http, secureClient, url, 30000)) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Conversation-Key", currentConversationKey());
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  int code = http.POST(body);
  http.end();
  bool ok = code >= 200 && code < 300;
  setStatus(ok ? "Logs uploaded" : ("Logs HTTP " + String(code)), 1400);
  appendRuntimeLog("LOG", ok ? "upload ok" : ("upload failed " + String(code)), true);
  return ok;
}

bool applyOtaFromManifest() {
  if (!gOtaManifest.parsed || gOtaManifest.url.isEmpty() || gOtaManifest.sha256.isEmpty()) {
    setStatus("OTA manifest incomplete", 1500);
    appendRuntimeLog("OTA", "manifest incomplete", true);
    return false;
  }
  appendRuntimeLog("OTA", "apply start version=" + gOtaManifest.version +
                            " size=" + String(gOtaManifest.size), true);
  setStatus("OTA downloading...", 0);
  render();
  updateBatterySnapshot(true);
  int level = gBatterySnapshot.level < 0 ? 100 : gBatterySnapshot.level;
  uint8_t minBattery = max<uint8_t>(gDeviceSettings.minBatteryForUpdate, gOtaManifest.minBattery);
  if (level < minBattery) {
    setStatus("Battery too low", 1600);
    appendRuntimeLog("OTA", "battery too low", true);
    return false;
  }
  if (!gSdReady) {
    pushError("SD card required for OTA.");
    setStatus("Insert SD card", 2500);
    appendRuntimeLog("OTA", "sd required", true);
    return false;
  }
  if (!downloadUrlToFile(gOtaManifest.url, kOtaTempPath, gOtaManifest.sha256, gOtaManifest.size)) {
    appendRuntimeLog("OTA", "firmware download failed", true);
    return false;
  }
  setStatus("OTA applying...", 0);
  render();
  fs::FS* fs = activeVoiceFs();
  File firmware = fs ? fs->open(kOtaTempPath, "r") : File();
  if (!firmware) {
    setStatus("OTA file missing", 1200);
    appendRuntimeLog("OTA", "file missing after download", true);
    return false;
  }
  size_t size = firmware.size();
  if (!Update.begin(size)) {
    uint8_t updateError = Update.getError();
    firmware.close();
    setStatus("OTA begin err " + String(updateError), 1800);
    appendRuntimeLog("OTA", "begin failed err=" + String(updateError) +
                              " size=" + String(static_cast<unsigned>(size)), true);
    return false;
  }
  size_t written = Update.writeStream(firmware);
  firmware.close();
  if (written != size) {
    uint8_t updateError = Update.getError();
    Update.abort();
    setStatus("OTA short err " + String(updateError), 1800);
    appendRuntimeLog("OTA", "short write err=" + String(updateError) +
                              " written=" + String(static_cast<unsigned>(written)) +
                              " size=" + String(static_cast<unsigned>(size)), true);
    return false;
  }
  if (!Update.end(true)) {
    uint8_t updateError = Update.getError();
    setStatus("OTA end err " + String(updateError), 1800);
    appendRuntimeLog("OTA", "end failed err=" + String(updateError), true);
    return false;
  }
  appendRuntimeLog("OTA", "apply ok reboot", true);
  setStatus("OTA applied", 800);
  delay(500);
  ESP.restart();
  return true;
}

String urlEncodePathSegment(const String& raw) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  out.reserve(raw.length() + 8);
  for (size_t i = 0; i < raw.length(); ++i) {
    uint8_t c = static_cast<uint8_t>(raw[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

String makeTopicShortName(const String& title, int32_t threadId) {
  (void)threadId;
  String displayTitle = normalizeTopicTitleForDisplay(title, false);
  String out;
  for (size_t i = 0; i < displayTitle.length() && out.length() < 4; ++i) {
    char c = displayTitle[i];
    if (isalnum(static_cast<unsigned char>(c))) {
      out += static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
  }
  if (!out.isEmpty()) {
    return out;
  }
  return "TG";
}

void clearContextPreviewIfSelectionChanged() {
  String selectedKey = currentConversationKey();
  if (gContextHistoryKey != selectedKey) {
    gContextPreview.clear();
    gContextHistoryKey = "";
    gContextHistoryFetchedAtMs = 0;
  }
}

bool addContextItem(const String& label, const String& key, const String& shortName,
                    const String& topicKey, int32_t threadId, uint32_t headSeq,
                    uint16_t unread, bool remote) {
  if (gContexts.size() >= kMaxContexts) {
    return false;
  }
  ConversationContext ctx;
  ctx.label = label;
  ctx.key = key;
  ctx.shortName = shortName.isEmpty() ? makeTopicShortName(label, threadId) : shortName;
  ctx.topicKey = topicKey.isEmpty() ? key : topicKey;
  ctx.threadId = threadId;
  ctx.headSeq = headSeq;
  ctx.unread = unread;
  ctx.remote = remote;
  gContexts.push_back(ctx);
  return true;
}

bool addDefaultContext(const String& label, const String& key, const String& shortName) {
  return addContextItem(label, key, shortName, key, 0, 0, 0, false);
}

void loadDefaultConversationContexts() {
  gContexts.clear();
  addDefaultContext("Помощник", "tg:root-main:default:-1003665527854:54", "TG");
  addDefaultContext("Marathon", "tg:root-main:default:-1003665527854:741", "RUN");
  addDefaultContext("Cardputer", "tg:root-main:default:-1003665527854:323", "ADV");
}

void selectSavedContextOrDefault(const String& savedKey) {
  if (!savedKey.isEmpty()) {
    for (size_t i = 0; i < gContexts.size(); ++i) {
      if (gContexts[i].key == savedKey || gContexts[i].topicKey == savedKey) {
        gSelectedContext = static_cast<int>(i);
        return;
      }
    }
  }
  if (gSelectedContext >= static_cast<int>(gContexts.size())) {
    gSelectedContext = 0;
  }
}

void loadConversationContexts() {
  loadDefaultConversationContexts();

  fs::FS* fs = activeVoiceFs();
  if (fs && fsExists(*fs, kContextRegistryPath)) {
    File file = fs->open(kContextRegistryPath, "r");
    if (file) {
      DynamicJsonDocument doc(4096);
      auto err = deserializeJson(doc, file);
      file.close();
      if (!err && doc["contexts"].is<JsonArray>()) {
        std::vector<ConversationContext> loaded;
        for (JsonObject item : doc["contexts"].as<JsonArray>()) {
          if (loaded.size() >= kMaxContexts) {
            break;
          }
          String label = item["label"] | item["name"] | "";
          String topicKey = item["topic_key"] | "";
          String key = item["key"] | item["conversation_key"] | topicKey;
          int32_t threadId = item["thread_id"] | 0;
          String shortName = item["short"] | "";
          label = normalizeTopicTitleForDisplay(label);
          if (!label.isEmpty() && !key.isEmpty()) {
            ConversationContext ctx;
            ctx.label = label;
            ctx.key = key;
            ctx.shortName = shortName.isEmpty() ? makeTopicShortName(label, threadId) : shortName;
            ctx.topicKey = topicKey.isEmpty() ? key : topicKey;
            ctx.threadId = threadId;
            ctx.headSeq = item["head_seq"] | 0;
            ctx.unread = item["unread"] | 0;
            ctx.remote = item["remote"] | false;
            loaded.push_back(ctx);
          }
        }
        if (!loaded.empty()) {
          gContexts = loaded;
        }
      }
    }
  }

  gPrefs.begin(kPrefsContextNs, true);
  String savedKey = gPrefs.getString("key", "");
  gPrefs.end();
  gSavedContextKey = savedKey;
  selectSavedContextOrDefault(savedKey);
  if (gSavedContextKey.isEmpty() && !gContexts.empty()) {
    gSavedContextKey = currentConversationKey();
  }
  clearContextPreviewIfSelectionChanged();
  appendRuntimeLog("CTX", "loaded count=" + String(gContexts.size()), true);
}

void saveCurrentContext() {
  if (gContexts.empty()) {
    return;
  }
  gPrefs.begin(kPrefsContextNs, false);
  gPrefs.putString("key", currentConversationKey());
  gPrefs.end();
  gSavedContextKey = currentConversationKey();
  appendRuntimeLog("CTX", "selected " + gContexts[gSelectedContext].shortName, true);
}

String currentConversationKey() {
  if (gContexts.empty()) {
    return "tg:root-main:default:-1003665527854:54";
  }
  gSelectedContext = clampValue<int>(gSelectedContext, 0, static_cast<int>(gContexts.size()) - 1);
  return gContexts[gSelectedContext].key;
}

bool fetchRemoteTopicCatalog() {
  if ((!gWifiReady && !ensureWifiForHttp("topics", 3000)) || gHubHost.isEmpty()) {
    gContextError = "Bridge offline";
    setStatus("Topics offline", 1200);
    return false;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + "/api/cardputer/v1/topics";
  if (!configureHttpClient(http, secureClient, url, 15000)) {
    gContextError = "Connect failed";
    setStatus("Topics connect failed", 1200);
    return false;
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  int code = http.GET();
  String body = code > 0 ? http.getString() : "";
  http.end();
  if (code < 200 || code >= 300) {
    gContextError = "HTTP " + String(code);
    setStatus("Topics HTTP " + String(code), 1400);
    appendRuntimeLog("CTX", "topics http " + String(code), true);
    return false;
  }

  DynamicJsonDocument doc(16384);
  auto err = deserializeJson(doc, body);
  if (err || !doc["items"].is<JsonArray>()) {
    gContextError = err ? String(err.c_str()) : "Bad topics";
    setStatus("Topics parse failed", 1400);
    appendRuntimeLog("CTX", "topics parse failed", true);
    return false;
  }

  std::vector<ConversationContext> loaded;
  for (JsonObject item : doc["items"].as<JsonArray>()) {
    if (loaded.size() >= kMaxContexts) {
      break;
    }
    String topicKey = item["topic_key"] | "";
    String title = item["title"] | "";
    int32_t threadId = item["thread_id"] | 0;
    if (topicKey.isEmpty()) {
      continue;
    }
    if (title.isEmpty()) {
      title = "Topic " + String(threadId > 0 ? threadId : static_cast<int>(loaded.size() + 1));
    }
    title = normalizeTopicTitleForDisplay(title);
    ConversationContext ctx;
    ctx.label = title;
    ctx.key = topicKey;
    ctx.shortName = makeTopicShortName(title, threadId);
    ctx.topicKey = topicKey;
    ctx.threadId = threadId;
    ctx.headSeq = item["head_seq"] | 0;
    ctx.unread = item["unread_count"] | 0;
    ctx.remote = true;
    loaded.push_back(ctx);
  }
  if (loaded.empty()) {
    gContextError = "No topics";
    setStatus("No topics", 1200);
    return false;
  }

  gContexts = loaded;
  gContextsRemoteLoaded = true;
  gContextsFetchedAtMs = millis();
  gContextError = "";
  selectSavedContextOrDefault(gSavedContextKey);
  clearContextPreviewIfSelectionChanged();
  appendRuntimeLog("CTX", "remote topics=" + String(gContexts.size()), true);
  setStatus("Topics synced", 900);
  return true;
}

bool selectRemoteCurrentTopic() {
  if (gContexts.empty() || (!gWifiReady && !ensureWifiForHttp("topic-select", 2000)) || gHubHost.isEmpty()) {
    return false;
  }
  String topicKey = gContexts[gSelectedContext].topicKey.isEmpty() ? gContexts[gSelectedContext].key : gContexts[gSelectedContext].topicKey;
  if (!topicKey.startsWith("tg:")) {
    return false;
  }
  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + "/api/cardputer/v1/topics/" + urlEncodePathSegment(topicKey) + "/select";
  if (!configureHttpClient(http, secureClient, url, 8000)) {
    appendRuntimeLog("CTX", "select connect failed", true);
    return false;
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{}");
  http.end();
  bool ok = code >= 200 && code < 300;
  appendRuntimeLog("CTX", ok ? "remote selected" : ("select http " + String(code)), true);
  return ok;
}

bool fetchSelectedTopicHistory() {
  if (gContexts.empty()) {
    return false;
  }
  if ((!gWifiReady && !ensureWifiForHttp("topic-history", 3000)) || gHubHost.isEmpty()) {
    gContextError = "Bridge offline";
    setStatus("History offline", 1200);
    return false;
  }
  gSelectedContext = clampValue<int>(gSelectedContext, 0, static_cast<int>(gContexts.size()) - 1);
  String topicKey = gContexts[gSelectedContext].topicKey.isEmpty() ? gContexts[gSelectedContext].key : gContexts[gSelectedContext].topicKey;
  String url = hubBaseUrl() + "/api/cardputer/v1/topics/" + urlEncodePathSegment(topicKey) + "/history?limit=5";

  auto getHistoryBody = [&](String& outBody) -> int {
    HTTPClient http;
    std::unique_ptr<WiFiClientSecure> secureClient;
    if (!configureHttpClient(http, secureClient, url, 15000)) {
      return -1000;
    }
    if (!gDeviceToken.isEmpty()) {
      http.addHeader("Authorization", "Bearer " + gDeviceToken);
    }
    int httpCode = http.GET();
    outBody = httpCode > 0 ? http.getString() : "";
    if (httpCode <= 0) {
      appendRuntimeLog("CTX", "history transport " + String(httpCode), true);
      setHttpDiag("history transport " + String(httpCode));
    }
    http.end();
    return httpCode;
  };

  String body;
  int code = getHistoryBody(body);
  if (code <= 0) {
    ensureWifiForHttp("topic-history-retry", 3000);
    delay(80);
    code = getHistoryBody(body);
  }
  if (code < 200 || code >= 300) {
    gContextError = "History HTTP " + String(code);
    setStatus("History HTTP " + String(code), 1400);
    appendRuntimeLog("CTX", "history http " + String(code), true);
    return false;
  }

  DynamicJsonDocument doc(8192);
  auto err = deserializeJson(doc, body);
  if (err || !doc["items"].is<JsonArray>()) {
    gContextError = err ? String(err.c_str()) : "Bad history";
    setStatus("History parse failed", 1400);
    return false;
  }

  gContextPreview.clear();
  for (JsonObject item : doc["items"].as<JsonArray>()) {
    if (gContextPreview.size() >= kMaxContextPreviewLines) {
      break;
    }
    JsonObject sender = item["sender"].as<JsonObject>();
    JsonObject content = item["content"].as<JsonObject>();
    String kind = content["kind"] | "";
    ContextPreviewLine line;
    line.role = sender["kind"] | "";
    line.voice = kind == "voice";
    if (line.role.isEmpty()) {
      line.role = "msg";
    }
    if (kind == "voice") {
      line.text = content["transcript"] | "";
      if (line.text.isEmpty()) {
        uint32_t durationMs = content["voice"]["duration_ms"] | 0;
        line.text = "Voice " + String(durationMs / 1000) + "s";
      }
    } else {
      line.text = content["text"] | "";
    }
    line.text = trimPreview(line.text, 92);
    if (!line.text.isEmpty()) {
      gContextPreview.push_back(line);
    }
  }
  gContextHistoryKey = topicKey;
  gContextHistoryFetchedAtMs = millis();
  gContextError = "";
  appendRuntimeLog("CTX", "history " + gContexts[gSelectedContext].shortName +
                            " lines=" + String(gContextPreview.size()), true);
  setStatus("History loaded", 800);
  return true;
}

bool saveVoiceNoteMetadata(const VoiceNote& note) {
  fs::FS* fs = activeVoiceFs();
  if (!fs) {
    return false;
  }
  DynamicJsonDocument doc(768);
  doc["id"] = note.id;
  doc["audio_path"] = note.audioPath;
  doc["title"] = note.title;
  doc["preview"] = note.preview;
  doc["sample_rate"] = note.sampleRate;
  doc["duration_ms"] = note.durationMs;
  doc["assistant"] = note.assistant;

  File meta = fs->open(note.metaPath, "w");
  if (!meta) {
    return false;
  }
  serializeJson(doc, meta);
  meta.close();
  return true;
}

bool persistWavFile(const String& path, const uint8_t* header, size_t headerLen, const uint8_t* audioBytes,
                    size_t audioLen) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return false;
  }
  File file = fs->open(path, "w");
  if (!file) {
    return false;
  }
  size_t writtenHeader = file.write(header, headerLen);
  size_t writtenBody = file.write(audioBytes, audioLen);
  file.close();
  return writtenHeader == headerLen && writtenBody == audioLen;
}

bool persistFileToFile(File& src, File& dst, size_t bytesToCopy) {
  uint8_t buffer[1024];
  size_t copied = 0;
  while (copied < bytesToCopy) {
    size_t chunk = bytesToCopy - copied;
    if (chunk > sizeof(buffer)) {
      chunk = sizeof(buffer);
    }
    size_t readBytes = src.read(buffer, chunk);
    if (readBytes == 0) {
      return false;
    }
    size_t written = dst.write(buffer, readBytes);
    if (written != readBytes) {
      return false;
    }
    copied += readBytes;
  }
  return copied == bytesToCopy;
}

bool persistWavFromPcmFile(const String& outPath, const String& pcmPath, uint32_t sampleRate, size_t audioLen) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return false;
  }
  File src = fs->open(pcmPath, "r");
  if (!src) {
    return false;
  }
  File dst = fs->open(outPath, "w");
  if (!dst) {
    src.close();
    return false;
  }

  uint8_t header[44];
  writeWavHeader(header, static_cast<uint32_t>(audioLen), sampleRate);
  bool ok = dst.write(header, sizeof(header)) == sizeof(header) && persistFileToFile(src, dst, audioLen);
  src.close();
  dst.close();
  if (!ok) {
    fsRemove(*fs, outPath);
  }
  return ok;
}

void rememberVoiceNote(const String& title, const String& preview, const uint8_t* header, size_t headerLen,
                       const uint8_t* audioBytes, size_t audioLen, uint32_t sampleRate, bool assistant) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return;
  }
  uint64_t totalBytes = activeStorageTotalBytes();
  uint64_t usedBytes = activeStorageUsedBytes();
  size_t requiredBytes = headerLen + audioLen + 2048;
  if (totalBytes > 0 && usedBytes + requiredBytes >= totalBytes) {
    logf("[VOICE] skip note storage used=%llu total=%llu required=%u",
         static_cast<unsigned long long>(usedBytes),
         static_cast<unsigned long long>(totalBytes),
         static_cast<unsigned>(requiredBytes));
    return;
  }
  VoiceNote note;
  note.id = makeVoiceNoteId(assistant);
  note.audioPath = voiceAudioPath(note.id);
  note.metaPath = voiceMetaPath(note.id);
  note.title = makeVoiceNoteTitle(assistant, audioLen, sampleRate);
  note.preview = trimPreview(preview);
  note.sampleRate = sampleRate;
  note.durationMs = sampleRate ? static_cast<uint32_t>((audioLen / 2U) * 1000ULL / sampleRate) : 0;
  note.assistant = assistant;

  if (!persistWavFile(note.audioPath, header, headerLen, audioBytes, audioLen)) {
    return;
  }
  if (!saveVoiceNoteMetadata(note)) {
    fsRemove(*fs, note.audioPath);
    return;
  }
  gVoiceNotes.push_back(note);
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
  gSelectedVoiceNote = 0;
}

void rememberVoiceNoteFromPcmFile(const String& title, const String& preview, const String& pcmPath,
                                  size_t audioLen, uint32_t sampleRate, bool assistant) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage() || !fs || !fsExists(*fs, pcmPath)) {
    return;
  }
  uint64_t totalBytes = activeStorageTotalBytes();
  uint64_t usedBytes = activeStorageUsedBytes();
  size_t requiredBytes = 44 + audioLen + 2048;
  if (totalBytes > 0 && usedBytes + requiredBytes >= totalBytes) {
    logf("[VOICE] skip note storage used=%llu total=%llu required=%u",
         static_cast<unsigned long long>(usedBytes),
         static_cast<unsigned long long>(totalBytes),
         static_cast<unsigned>(requiredBytes));
    return;
  }

  VoiceNote note;
  note.id = makeVoiceNoteId(assistant);
  note.audioPath = voiceAudioPath(note.id);
  note.metaPath = voiceMetaPath(note.id);
  note.title = makeVoiceNoteTitle(assistant, audioLen, sampleRate);
  note.preview = trimPreview(preview);
  note.sampleRate = sampleRate;
  note.durationMs = sampleRate ? static_cast<uint32_t>((audioLen / 2U) * 1000ULL / sampleRate) : 0;
  note.assistant = assistant;

  if (!persistWavFromPcmFile(note.audioPath, pcmPath, sampleRate, audioLen)) {
    return;
  }
  if (!saveVoiceNoteMetadata(note)) {
    fsRemove(*fs, note.audioPath);
    return;
  }
  gVoiceNotes.push_back(note);
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
  gSelectedVoiceNote = 0;
}

uint32_t wavDurationMs(const String& path, uint32_t* sampleRateOut = nullptr) {
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, path)) {
    return 0;
  }
  File file = fs->open(path, "r");
  if (!file || file.size() < 44) {
    return 0;
  }
  uint8_t header[44];
  if (file.read(header, sizeof(header)) != sizeof(header)) {
    file.close();
    return 0;
  }
  file.close();
  uint32_t sampleRate = static_cast<uint32_t>(header[24]) |
                        (static_cast<uint32_t>(header[25]) << 8) |
                        (static_cast<uint32_t>(header[26]) << 16) |
                        (static_cast<uint32_t>(header[27]) << 24);
  uint16_t bitsPerSample = static_cast<uint16_t>(header[34]) |
                           (static_cast<uint16_t>(header[35]) << 8);
  uint16_t channels = static_cast<uint16_t>(header[22]) |
                      (static_cast<uint16_t>(header[23]) << 8);
  uint32_t dataBytes = static_cast<uint32_t>(header[40]) |
                       (static_cast<uint32_t>(header[41]) << 8) |
                       (static_cast<uint32_t>(header[42]) << 16) |
                       (static_cast<uint32_t>(header[43]) << 24);
  if (sampleRateOut) {
    *sampleRateOut = sampleRate;
  }
  uint32_t bytesPerSampleFrame = max<uint32_t>(1, (bitsPerSample / 8U) * max<uint16_t>(1, channels));
  if (sampleRate == 0 || dataBytes == 0) {
    return 0;
  }
  return static_cast<uint32_t>((static_cast<uint64_t>(dataBytes) * 1000ULL) /
                               (static_cast<uint64_t>(sampleRate) * bytesPerSampleFrame));
}

bool registerDownloadedVoiceNote(const String& wavPath, const String& titleHint, const String& preview, bool assistant) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage() || !fs || !fsExists(*fs, wavPath)) {
    return false;
  }

  uint32_t sampleRate = 16000;
  uint32_t durationMs = wavDurationMs(wavPath, &sampleRate);
  VoiceNote note;
  note.id = filenameFromPath(wavPath);
  int dot = note.id.lastIndexOf('.');
  if (dot > 0) {
    note.id = note.id.substring(0, dot);
  }
  note.audioPath = wavPath;
  note.metaPath = voiceMetaPath(note.id);
  note.title = titleHint.isEmpty() ? makeVoiceNoteTitle(assistant, max<uint32_t>(1, durationMs) * sampleRate / 500U,
                                                        sampleRate)
                                   : trimPreview(titleHint, 24);
  note.preview = trimPreview(preview);
  note.sampleRate = sampleRate;
  note.durationMs = durationMs;
  note.assistant = assistant;
  if (!saveVoiceNoteMetadata(note)) {
    return false;
  }
  bool existsInList = false;
  for (auto& existing : gVoiceNotes) {
    if (existing.audioPath == note.audioPath) {
      existing = note;
      existsInList = true;
      break;
    }
  }
  if (!existsInList) {
    gVoiceNotes.push_back(note);
  }
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
  gSelectedVoiceNote = 0;
  return true;
}

bool downloadAndPlayResponseAudio(const String& url, const String& localPath, const String& sha256,
                                  uint32_t expectedSize, const String& title, const String& preview) {
  if (url.isEmpty()) {
    if (!localPath.isEmpty() && registerDownloadedVoiceNote(localPath, title, preview, true)) {
      return startPlaybackStreamFromWavFile(localPath, "reply: " + title);
    }
    return false;
  }
  if (!gStorageReady || !ensureVoiceStorage()) {
    return false;
  }
  String id = makeVoiceNoteId(true);
  String outPath = voiceAudioPath(id);
  if (!downloadUrlToFile(url, outPath, sha256, expectedSize)) {
    appendRuntimeLog("AUDIO", "reply audio download failed", true);
    return false;
  }
  if (!registerDownloadedVoiceNote(outPath, title, preview, true)) {
    return false;
  }
  return startPlaybackStreamFromWavFile(outPath, "reply: " + title);
}

void stopPlaybackStream() {
  if (gPlaybackStreamFile) {
    gPlaybackStreamFile.close();
  }
  gPlaybackStreamPath = "";
  gPlaybackStreaming = false;
  gPlaybackStreamRemainingBytes = 0;
  gPlaybackStreamSampleRate = 0;
  gPlaybackStreamNextBuffer = 0;
  gPlaybackStreamRawPcm = false;
  if (gDynamicPlaybackBuffer) {
    free(gDynamicPlaybackBuffer);
    gDynamicPlaybackBuffer = nullptr;
  }
  M5Cardputer.Speaker.stop();
}

bool startPlaybackStreamFromRawPcmFile(const String& path, uint32_t sampleRate, size_t audioLen, const String& diag) {
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, path)) {
    setStatus("Voice file missing", 1200);
    return false;
  }
  stopPlaybackStream();
  gPlaybackStreamFile = fs->open(path, "r");
  if (!gPlaybackStreamFile) {
    setStatus("Voice file missing", 1200);
    return false;
  }
  gPlaybackStreamPath = path;
  gPlaybackStreamRemainingBytes = audioLen;
  gPlaybackStreamSampleRate = sampleRate;
  gPlaybackStreamNextBuffer = 0;
  gPlaybackStreamRawPcm = true;
  gPlaybackStreaming = true;
  gPlaybackActive = true;
  pulseButtonPress();
  setFaceMode(FaceMode::Speaking);
  setStatus("Playing voice...", 1200);
  setVoiceDiag(diag);
  return true;
}

bool startPlaybackStreamFromWavFile(const String& path, const String& diag) {
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, path)) {
    setStatus("Voice file missing", 1200);
    return false;
  }
  stopPlaybackStream();
  File file = fs->open(path, "r");
  if (!file) {
    setStatus("Voice file missing", 1200);
    return false;
  }
  uint8_t header[44];
  if (file.read(header, sizeof(header)) != sizeof(header)) {
    file.close();
    setStatus("Voice file invalid", 1200);
    return false;
  }
  if (!(header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F')) {
    file.close();
    setStatus("Voice file invalid", 1200);
    return false;
  }
  uint32_t sampleRate = static_cast<uint32_t>(header[24]) |
                        (static_cast<uint32_t>(header[25]) << 8) |
                        (static_cast<uint32_t>(header[26]) << 16) |
                        (static_cast<uint32_t>(header[27]) << 24);
  uint32_t audioLen = static_cast<uint32_t>(header[40]) |
                      (static_cast<uint32_t>(header[41]) << 8) |
                      (static_cast<uint32_t>(header[42]) << 16) |
                      (static_cast<uint32_t>(header[43]) << 24);
  gPlaybackStreamFile = file;
  gPlaybackStreamPath = path;
  gPlaybackStreamRemainingBytes = audioLen;
  gPlaybackStreamSampleRate = sampleRate;
  gPlaybackStreamNextBuffer = 0;
  gPlaybackStreamRawPcm = false;
  gPlaybackStreaming = true;
  gPlaybackActive = true;
  pulseButtonPress();
  setFaceMode(FaceMode::Speaking);
  setStatus("Playing voice...", 1200);
  setVoiceDiag(diag);
  return true;
}

void servicePlaybackStream() {
  if (!gPlaybackStreaming) {
    return;
  }
  constexpr int kPlaybackChannel = 0;
  while (gPlaybackStreamRemainingBytes > 0 && M5Cardputer.Speaker.isPlaying(kPlaybackChannel) < 2) {
    size_t chunk = gPlaybackStreamRemainingBytes;
    if (chunk > kPlaybackChunkBytes) {
      chunk = kPlaybackChunkBytes;
    }
    uint8_t* buffer = gPlaybackStreamBuffers[gPlaybackStreamNextBuffer];
    size_t readBytes = gPlaybackStreamFile.read(buffer, chunk);
    if (readBytes == 0) {
      gPlaybackStreamRemainingBytes = 0;
      break;
    }
    bool firstChunk = M5Cardputer.Speaker.isPlaying(kPlaybackChannel) == 0;
    bool queued = M5Cardputer.Speaker.playRaw(reinterpret_cast<const int16_t*>(buffer),
                                              readBytes / sizeof(int16_t),
                                              gPlaybackStreamSampleRate,
                                              false,
                                              1,
                                              kPlaybackChannel,
                                              firstChunk);
    if (!queued) {
      setVoiceDiag("playback: queue fail");
      setFaceMode(FaceMode::Error);
      setStatus("Playback failed", 1500);
      stopPlaybackStream();
      gPlaybackActive = false;
      return;
    }
    gPlaybackStreamRemainingBytes -= readBytes;
    gPlaybackStreamNextBuffer = (gPlaybackStreamNextBuffer + 1) % kPlaybackBufferCount;
  }

  if (gPlaybackStreamRemainingBytes == 0 && M5Cardputer.Speaker.isPlaying(kPlaybackChannel) == 0) {
    bool removeTempPcm = gPlaybackStreamPath == kActiveTtsPcmPath;
    fs::FS* fs = activeVoiceFs();
    stopPlaybackStream();
    gPlaybackActive = false;
    setFaceMode(FaceMode::Idle);
    setStatus("Reply finished", 1000);
    if (removeTempPcm && fs) {
      fsRemove(*fs, kActiveTtsPcmPath);
    }
  }
}

bool playVoiceNoteAt(int index) {
  if (index < 0 || index >= static_cast<int>(gVoiceNotes.size())) {
    return false;
  }
  const auto& note = gVoiceNotes[index];
  return startPlaybackStreamFromWavFile(note.audioPath, String("note: ") + note.title);
}

bool findLauncherEntryForMode(UiMode mode, int& groupIndex, int& subIndex) {
  for (uint8_t g = 0; g < kLauncherGroupCount; ++g) {
    for (uint8_t s = 0; s < kLauncherGroups[g].count; ++s) {
      if (kLauncherGroups[g].items[s].mode == mode) {
        groupIndex = g;
        subIndex = s;
        return true;
      }
    }
  }
  return false;
}

void syncLauncherToMode(UiMode mode) {
  int groupIndex = 0;
  int subIndex = 0;
  if (!findLauncherEntryForMode(mode, groupIndex, subIndex)) {
    return;
  }
  gLauncherSelection = groupIndex;
  gLauncherSubSelection = subIndex;
  gLauncherLastSub[groupIndex] = subIndex;
}

void setUiMode(UiMode mode) {
  gUiMode = mode;
  gLauncherVisible = false;
  if (gLauncherSelection >= 0 && gLauncherSelection < kLauncherGroupCount &&
      gLauncherSubSelection >= 0 && gLauncherSubSelection < kLauncherGroups[gLauncherSelection].count &&
      kLauncherGroups[gLauncherSelection].items[gLauncherSubSelection].mode == mode) {
    gLauncherLastSub[gLauncherSelection] = gLauncherSubSelection;
  } else {
    syncLauncherToMode(mode);
  }
  setStatus(String("Mode: ") + uiModeName(mode), 900);
  if (mode == UiMode::Contexts) {
    bool needCatalog = !gContextsRemoteLoaded;
    bool needHistory = !gContexts.empty() && gContextHistoryKey != currentConversationKey();
    if (needCatalog || needHistory) {
      startTopicTask(needCatalog, false, true, needCatalog ? "Syncing topics..." : "Loading history...");
    }
  }
}

void moveLauncherSelection(int delta) {
  gLauncherVisible = true;
  gLauncherSelection = (gLauncherSelection + delta + kLauncherGroupCount) % kLauncherGroupCount;
  uint8_t count = kLauncherGroups[gLauncherSelection].count;
  gLauncherSubSelection = clampValue<int>(gLauncherLastSub[gLauncherSelection], 0, count - 1);
}

void moveLauncherSubSelection(int delta) {
  gLauncherVisible = true;
  uint8_t count = kLauncherGroups[gLauncherSelection].count;
  if (count <= 1) {
    moveLauncherSelection(delta);
    return;
  }
  gLauncherSubSelection = (gLauncherSubSelection + delta + count) % count;
  gLauncherLastSub[gLauncherSelection] = gLauncherSubSelection;
}

void applyLauncherSelection() {
  gLauncherSelection = clampValue<int>(gLauncherSelection, 0, kLauncherGroupCount - 1);
  uint8_t count = kLauncherGroups[gLauncherSelection].count;
  gLauncherSubSelection = clampValue<int>(gLauncherSubSelection, 0, count - 1);
  gLauncherLastSub[gLauncherSelection] = gLauncherSubSelection;
  UiMode target = kLauncherGroups[gLauncherSelection].items[gLauncherSubSelection].mode;
  setUiMode(target);
}

void cycleCurrentAppScreen(int delta) {
  syncLauncherToMode(gUiMode);
  gLauncherVisible = false;
  uint8_t count = kLauncherGroups[gLauncherSelection].count;
  gLauncherSubSelection = (gLauncherSubSelection + delta + count) % count;
  gLauncherLastSub[gLauncherSelection] = gLauncherSubSelection;
  applyLauncherSelection();
}

void switchLauncherGroupAndApply(int delta) {
  syncLauncherToMode(gUiMode);
  gLauncherVisible = false;
  gLauncherSelection = (gLauncherSelection + delta + kLauncherGroupCount) % kLauncherGroupCount;
  uint8_t count = kLauncherGroups[gLauncherSelection].count;
  gLauncherSubSelection = clampValue<int>(gLauncherLastSub[gLauncherSelection], 0, count - 1);
  applyLauncherSelection();
}

size_t replyVisualLineCount(const String& reply) {
  String full = "AI: " + reply;
  auto wrapped = wrapText(full, 216);
  return wrapped.size();
}

void appendAssistantReply(const String& reply) {
  String displayReply = cleanReplyForDevice(reply);
  if (displayReply.isEmpty()) {
    return;
  }
  flashActivityLed(0, 180, 0);
  if (replyVisualLineCount(displayReply) > kVoicePreviewLineThreshold) {
    pushAssistant(trimPreview(displayReply) + " [voice]");
  } else {
    pushAssistant(displayReply);
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
  if (gDeviceId.isEmpty() && strlen(DEFAULT_DEVICE_ID) > 0) {
    gDeviceId = DEFAULT_DEVICE_ID;
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

  if (strlen(DEFAULT_DEVICE_ID) > 0 && savedId != DEFAULT_DEVICE_ID) {
    savedId = DEFAULT_DEVICE_ID;
    gPrefs.begin(kPrefsDeviceNs, false);
    gPrefs.putString("device_id", savedId);
    gPrefs.end();
  }
  if (savedId.isEmpty()) {
    gDeviceId = "cardputer_adv_" + chipIdSuffix();
    gPrefs.begin(kPrefsDeviceNs, false);
    gPrefs.putString("device_id", gDeviceId);
    gPrefs.end();
  } else {
    gDeviceId = savedId;
  }

  applyCompileTimeDefaults();
  migrateBridgeEndpoint();
}

void loadDeviceSettings() {
  gPrefs.begin(kPrefsDeviceNs, true);
  gDeviceSettings.autoUpdateFirmware = gPrefs.getBool("fw_auto", false);
  gDeviceSettings.checkUpdatesOnBoot = gPrefs.getBool("fw_boot", true);
  gDeviceSettings.beepOnUpdate = gPrefs.getBool("fw_beep", true);
  gDeviceSettings.minBatteryForUpdate = static_cast<uint8_t>(gPrefs.getUChar("fw_bat", 40));
  gDeviceSettings.clockFace = static_cast<uint8_t>(gPrefs.getUChar("clk_face", 0));
  gDeviceSettings.updateChannel = gPrefs.getString("fw_chan", "dev");
  gDeviceSettings.audioLanguage = gPrefs.getString("aud_lang", "ru");
  gPrefs.end();
  gDeviceSettings.minBatteryForUpdate = clampValue<uint8_t>(gDeviceSettings.minBatteryForUpdate, 5, 95);
  gDeviceSettings.clockFace %= kClockFaceCount;
  if (gDeviceSettings.updateChannel.isEmpty()) {
    gDeviceSettings.updateChannel = "dev";
  }
  if (gDeviceSettings.audioLanguage.isEmpty()) {
    gDeviceSettings.audioLanguage = "ru";
  }
}

void saveDeviceSettings() {
  gPrefs.begin(kPrefsDeviceNs, false);
  gPrefs.putBool("fw_auto", gDeviceSettings.autoUpdateFirmware);
  gPrefs.putBool("fw_boot", gDeviceSettings.checkUpdatesOnBoot);
  gPrefs.putBool("fw_beep", gDeviceSettings.beepOnUpdate);
  gPrefs.putUChar("fw_bat", gDeviceSettings.minBatteryForUpdate);
  gPrefs.putUChar("clk_face", gDeviceSettings.clockFace % kClockFaceCount);
  gPrefs.putString("fw_chan", gDeviceSettings.updateChannel);
  gPrefs.putString("aud_lang", gDeviceSettings.audioLanguage);
  gPrefs.end();
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
  if (gRecordingToFile && gStorageReady) {
    uint64_t total = activeStorageTotalBytes();
    uint64_t used = activeStorageUsedBytes();
    uint64_t availableBytes = 0;
    if (total > used + 8192) {
      availableBytes = total - used - 8192;
    }
    uint32_t storageMs = static_cast<uint32_t>((availableBytes * 1000ULL) / (kMicSampleRate * sizeof(int16_t)));
    uint32_t configuredMs = static_cast<uint32_t>(kMaxRecordSeconds * 1000.0f);
    return storageMs < configuredMs ? storageMs : configuredMs;
  }
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

const char* focusStateName(FocusState state) {
  switch (state) {
    case FocusState::Focus:
      return "FOCUS";
    case FocusState::ShortBreak:
      return "BREAK";
    case FocusState::LongBreak:
      return "LONG";
    case FocusState::Reflect:
      return "REFLECT";
    case FocusState::Paused:
      return "PAUSED";
    case FocusState::Stopped:
    default:
      return "READY";
  }
}

bool focusStateRuns(FocusState state) {
  return state == FocusState::Focus || state == FocusState::ShortBreak ||
         state == FocusState::LongBreak || state == FocusState::Reflect;
}

uint32_t focusDurationForState(FocusState state) {
  switch (state) {
    case FocusState::Focus:
      return gFocusSettings.focusSec;
    case FocusState::ShortBreak:
      return gFocusSettings.shortBreakSec;
    case FocusState::LongBreak:
      return gFocusSettings.longBreakSec;
    case FocusState::Reflect:
      return 60;
    case FocusState::Paused:
    case FocusState::Stopped:
    default:
      return gFocusSettings.focusSec;
  }
}

String focusTimeString(uint32_t seconds) {
  uint32_t minutes = seconds / 60;
  uint8_t secs = seconds % 60;
  char buf[10];
  snprintf(buf, sizeof(buf), "%02lu:%02u", static_cast<unsigned long>(minutes), secs);
  return String(buf);
}

void focusTone(uint16_t freq, uint16_t durationMs) {
  if (gRecording || gPlaybackActive) {
    return;
  }
  M5Cardputer.Speaker.tone(freq, durationMs);
}

bool playFocusCue(FocusState state) {
  if (gRecording || gSubmitting || gThinking || gPlaybackActive) {
    return false;
  }
  const char* path = nullptr;
  switch (state) {
    case FocusState::Focus:
      path = kFocusCuePath;
      break;
    case FocusState::ShortBreak:
    case FocusState::LongBreak:
      path = kBreakCuePath;
      break;
    case FocusState::Reflect:
      path = kReflectionCuePath;
      break;
    case FocusState::Paused:
    case FocusState::Stopped:
    default:
      return false;
  }
  fs::FS* fs = activeVoiceFs();
  if (fs && fsExists(*fs, path)) {
    return startPlaybackStreamFromWavFile(path, String("focus cue: ") + focusStateName(state));
  }
  return false;
}

void loadFocusSettings() {
  gPrefs.begin(kPrefsFocusNs, true);
  gFocusSettings.focusSec = gPrefs.getUInt("focus", kFocusDefaultSec);
  gFocusSettings.shortBreakSec = gPrefs.getUInt("short", kFocusShortBreakDefaultSec);
  gFocusSettings.longBreakSec = gPrefs.getUInt("long", kFocusLongBreakDefaultSec);
  gFocusSettings.cyclesPerRound = static_cast<uint8_t>(gPrefs.getUChar("cycles", kFocusDefaultCycles));
  gFocusSettings.autoStart = gPrefs.getBool("auto", false);
  gFocusSettings.metronome = gPrefs.getBool("metro", false);
  gFocusSettings.bpm = static_cast<uint16_t>(gPrefs.getUShort("bpm", 60));
  gPrefs.end();

  gFocusSettings.focusSec = clampValue<uint32_t>(gFocusSettings.focusSec, kFocusMinDurationSec, kFocusMaxDurationSec);
  gFocusSettings.shortBreakSec = clampValue<uint32_t>(gFocusSettings.shortBreakSec, kFocusMinDurationSec, kFocusMaxDurationSec);
  gFocusSettings.longBreakSec = clampValue<uint32_t>(gFocusSettings.longBreakSec, kFocusMinDurationSec, kFocusMaxDurationSec);
  gFocusSettings.cyclesPerRound = clampValue<uint8_t>(gFocusSettings.cyclesPerRound, 1, 9);
  gFocusSettings.bpm = clampValue<uint16_t>(gFocusSettings.bpm, kFocusMinBpm, kFocusMaxBpm);
  gFocus.remainingSec = gFocusSettings.focusSec;
  gFocus.sessionTotalSec = gFocusSettings.focusSec;
}

void saveFocusSettings() {
  gPrefs.begin(kPrefsFocusNs, false);
  gPrefs.putUInt("focus", gFocusSettings.focusSec);
  gPrefs.putUInt("short", gFocusSettings.shortBreakSec);
  gPrefs.putUInt("long", gFocusSettings.longBreakSec);
  gPrefs.putUChar("cycles", gFocusSettings.cyclesPerRound);
  gPrefs.putBool("auto", gFocusSettings.autoStart);
  gPrefs.putBool("metro", gFocusSettings.metronome);
  gPrefs.putUShort("bpm", gFocusSettings.bpm);
  gPrefs.end();
}

void focusEnterState(FocusState state, bool resetTimer = true) {
  gFocus.state = state;
  if (state != FocusState::Paused && state != FocusState::Stopped) {
    gFocus.stateBeforePause = state;
  }
  if (resetTimer) {
    gFocus.sessionTotalSec = focusDurationForState(state);
    gFocus.remainingSec = gFocus.sessionTotalSec;
  }
  gFocus.lastTickMs = millis();
  gFocus.nextMetronomeMs = millis() + 100;
  setStatus(String("Focus ") + focusStateName(state), 1000);
  if (!playFocusCue(state)) {
    if (state == FocusState::Focus) {
      focusTone(1047, 120);
    } else if (state == FocusState::Reflect) {
      focusTone(659, 160);
    } else if (state == FocusState::ShortBreak || state == FocusState::LongBreak) {
      focusTone(880, 120);
    }
  }
  logf("[FOCUS] state=%s remain=%u cycle=%u/%u",
       focusStateName(state),
       static_cast<unsigned>(gFocus.remainingSec),
       static_cast<unsigned>(gFocus.cycle),
       static_cast<unsigned>(gFocusSettings.cyclesPerRound));
}

void focusStartOrResume() {
  if (gFocus.state == FocusState::Paused) {
    focusEnterState(gFocus.stateBeforePause, false);
    return;
  }
  if (gFocus.state == FocusState::Stopped) {
    if (gFocus.remainingSec == 0) {
      gFocus.remainingSec = gFocusSettings.focusSec;
    }
    gFocus.sessionTotalSec = gFocus.remainingSec;
    focusEnterState(FocusState::Focus, false);
    return;
  }
  if (focusStateRuns(gFocus.state)) {
    gFocus.stateBeforePause = gFocus.state;
    gFocus.state = FocusState::Paused;
    setStatus("Focus paused", 1000);
    focusTone(523, 90);
  }
}

void focusReset() {
  gFocus.state = FocusState::Stopped;
  gFocus.stateBeforePause = FocusState::Focus;
  gFocus.remainingSec = gFocusSettings.focusSec;
  gFocus.sessionTotalSec = gFocusSettings.focusSec;
  gFocus.lastTickMs = millis();
  setStatus("Focus reset", 1000);
  focusTone(440, 90);
}

void focusSnooze(uint32_t seconds = kFocusSnoozeSec) {
  gFocus.remainingSec = min<uint32_t>(gFocus.remainingSec + seconds, kFocusMaxDurationSec);
  gFocus.sessionTotalSec = max<uint32_t>(gFocus.sessionTotalSec, gFocus.remainingSec);
  setStatus("+5 min", 900);
}

void focusAdvance() {
  FocusState ended = gFocus.state;
  if (ended == FocusState::Focus) {
    ++gFocus.completedToday;
    gFocus.focusedTodaySec += gFocus.sessionTotalSec;
    if ((gFocus.cycle % gFocusSettings.cyclesPerRound) == 0) {
      focusEnterState(FocusState::LongBreak);
    } else {
      focusEnterState(FocusState::ShortBreak);
    }
    return;
  }
  if (ended == FocusState::ShortBreak) {
    ++gFocus.cycle;
    if (gFocusSettings.autoStart) {
      focusEnterState(FocusState::Focus);
    } else {
      gFocus.state = FocusState::Stopped;
      gFocus.remainingSec = gFocusSettings.focusSec;
      gFocus.sessionTotalSec = gFocusSettings.focusSec;
      setStatus("Next focus ready", 1200);
    }
    return;
  }
  if (ended == FocusState::LongBreak) {
    focusEnterState(FocusState::Reflect);
    return;
  }
  if (ended == FocusState::Reflect) {
    gFocus.cycle = 1;
    if (gFocusSettings.autoStart) {
      focusEnterState(FocusState::Focus);
    } else {
      gFocus.state = FocusState::Stopped;
      gFocus.remainingSec = gFocusSettings.focusSec;
      gFocus.sessionTotalSec = gFocusSettings.focusSec;
      setStatus("Round complete", 1400);
    }
  }
}

void updateFocusTimer() {
  if (!focusStateRuns(gFocus.state)) {
    return;
  }
  uint32_t now = millis();
  if (now < gFocus.lastTickMs) {
    gFocus.lastTickMs = now;
    return;
  }
  uint32_t elapsedSec = (now - gFocus.lastTickMs) / 1000;
  if (elapsedSec == 0) {
    return;
  }
  gFocus.lastTickMs += elapsedSec * 1000;
  if (elapsedSec < gFocus.remainingSec) {
    gFocus.remainingSec -= elapsedSec;
    return;
  }
  gFocus.remainingSec = 0;
  focusAdvance();
}

void serviceFocusMetronome() {
  if (!gFocusSettings.metronome || gFocus.state != FocusState::Focus || gRecording ||
      gPlaybackActive || gSubmitting || gThinking) {
    return;
  }
  uint32_t now = millis();
  if (now < gFocus.nextMetronomeMs) {
    return;
  }
  uint32_t beatMs = 60000UL / max<uint16_t>(1, gFocusSettings.bpm);
  gFocus.nextMetronomeMs = now + beatMs;
  focusTone(660, 35);
}

bool copyFileBetweenFs(fs::FS& fromFs, fs::FS& toFs, const String& path) {
  if (!fsExists(fromFs, path)) {
    return false;
  }
  if (fsExists(toFs, path)) {
    return true;
  }
  File src = fromFs.open(path, "r");
  if (!src) {
    return false;
  }
  File dst = toFs.open(path, "w");
  if (!dst) {
    src.close();
    return false;
  }
  uint8_t buffer[1024];
  bool ok = true;
  while (src.available()) {
    size_t readBytes = src.read(buffer, sizeof(buffer));
    if (readBytes == 0) {
      break;
    }
    if (dst.write(buffer, readBytes) != readBytes) {
      ok = false;
      break;
    }
  }
  src.close();
  dst.close();
  if (!ok) {
    fsRemove(toFs, path);
  }
  return ok;
}

void migrateLittleFsVoiceNotesToSd() {
  if (!gSdReady || !gLittleFsReady || !ensureVoiceStorageOn(SD) || !LittleFS.exists(kVoiceDir)) {
    return;
  }
  File dir = LittleFS.open(kVoiceDir);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  uint16_t copied = 0;
  while (true) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }
    String path = file.path();
    bool regular = !file.isDirectory();
    file.close();
    if (!regular || path.indexOf("__active_") >= 0 ||
        (!path.endsWith(kVoiceMetaExt) && !path.endsWith(kVoiceAudioExt))) {
      continue;
    }
    if (copyFileBetweenFs(LittleFS, SD, path)) {
      ++copied;
    }
  }
  if (copied > 0) {
    logf("[SD] migrated %u voice files from LittleFS", static_cast<unsigned>(copied));
  }
}

void initStorage() {
  Serial.println("[BOOT] storage: LittleFS begin");
  gLittleFsReady = LittleFS.begin(true);
  if (!gLittleFsReady) {
    pushError("LittleFS init failed.");
  }

  Serial.println("[BOOT] storage: SD begin");
  gSdSpi.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  gSdReady = SD.begin(kSdCsPin, gSdSpi, kSdFrequencyHz);
  if (gSdReady && SD.cardType() == CARD_NONE) {
    SD.end();
    gSdReady = false;
  }
  if (gSdReady) {
    ensureVoiceStorageOn(SD);
    ensurePomodoroStorageOn(SD);
    logf("[SD] ready type=%u total=%lluMB used=%lluMB pins=sck%u miso%u mosi%u cs%u",
         static_cast<unsigned>(SD.cardType()),
         static_cast<unsigned long long>(SD.totalBytes() / (1024ULL * 1024ULL)),
         static_cast<unsigned long long>(SD.usedBytes() / (1024ULL * 1024ULL)),
         static_cast<unsigned>(kSdSckPin),
         static_cast<unsigned>(kSdMisoPin),
         static_cast<unsigned>(kSdMosiPin),
         static_cast<unsigned>(kSdCsPin));
    // Keep boot deterministic: migration can stall on corrupted LittleFS/SD metadata.
    logf("[SD] LittleFS voice migration skipped on boot");
  } else {
    logf("[SD] not mounted; using LittleFS fallback");
  }

  Serial.println("[BOOT] storage: scan emoji");
  scanEmojiAssets();
  gStorageReady = gSdReady || gLittleFsReady;
  gStorageLabel = activeVoiceStorageLabel();
  if (!gStorageReady) {
    setStatus("Storage unavailable", 1000);
    return;
  }
  Serial.println("[BOOT] storage: ensure voice");
  ensureVoiceStorage();
  if (!gSdReady && gLittleFsReady) {
    ensurePomodoroStorageOn(LittleFS);
  }
  Serial.println("[BOOT] storage: load voice notes");
  loadVoiceNotes();
  Serial.println("[BOOT] storage: scan audio");
  scanAudioAssets();
  Serial.println("[BOOT] storage: load manifests");
  loadAssetManifestSummary();
  loadOtaManifestSummary();
  Serial.println("[BOOT] storage: load contexts");
  loadConversationContexts();
  pushSystem("Storage: " + gStorageLabel);
  if (gEmojiAssetCount > 0) {
    pushSystem("Emoji SD: " + String(gEmojiAssetCount));
  }
  Serial.println("[BOOT] storage: ready");
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

void flushCompletedRecordChunksToFile(size_t completedCount) {
  for (size_t i = 0; i < completedCount && !gRecordInflightChunks.empty(); ++i) {
    uint8_t chunkIndex = gRecordInflightChunks.front();
    gRecordInflightChunks.pop_front();
    updateVoiceLevelFromSamples(gRecordChunkBuffers[chunkIndex], kRecordChunkSamples);
    if (gActiveRecordFile) {
      gActiveRecordFile.write(reinterpret_cast<const uint8_t*>(gRecordChunkBuffers[chunkIndex]), kRecordChunkBytes);
      gActiveRecordBytes += kRecordChunkBytes;
      gActiveRecordCommittedBytes += kRecordChunkBytes;
    }
  }
}

bool queueRecordChunk(uint8_t chunkIndex) {
  return M5Cardputer.Mic.record(gRecordChunkBuffers[chunkIndex], kRecordChunkSamples, kMicSampleRate);
}

void serviceRecordingToFile(bool refill) {
  if (!gRecording || !gRecordingToFile) {
    return;
  }
  size_t micQueued = M5Cardputer.Mic.isRecording();
  if (gRecordInflightChunks.size() > micQueued) {
    flushCompletedRecordChunksToFile(gRecordInflightChunks.size() - micQueued);
  }
  if (!refill) {
    return;
  }
  while (gRecordInflightChunks.size() < 2) {
    uint8_t chunkIndex = gRecordNextChunkIndex;
    gRecordNextChunkIndex = (gRecordNextChunkIndex + 1) % kRecordChunkBufferCount;
    bool alreadyInflight = false;
    for (uint8_t inflight : gRecordInflightChunks) {
      if (inflight == chunkIndex) {
        alreadyInflight = true;
        break;
      }
    }
    if (alreadyInflight) {
      continue;
    }
    if (!queueRecordChunk(chunkIndex)) {
      break;
    }
    gRecordInflightChunks.push_back(chunkIndex);
  }
}

void drainRecordingToFile() {
  unsigned long deadline = millis() + kRecordDrainTimeoutMs;
  while (millis() < deadline) {
    serviceRecordingToFile(false);
    if (M5Cardputer.Mic.isRecording() == 0 && gRecordInflightChunks.empty()) {
      return;
    }
    yield();
    delay(4);
  }
  serviceRecordingToFile(false);
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
  gRecordedSamples = 0;
  gRecording = true;
  gRecordStartMs = millis();
  gVoiceLevel8 = 36;
  pulseButtonPress();

  gRecordingToFile = gStorageReady && ensureVoiceStorage();
  if (gRecordingToFile) {
    fs::FS* fs = activeVoiceFs();
    fsRemove(*fs, kActiveRecordPcmPath);
    gActiveRecordFile = fs->open(kActiveRecordPcmPath, "w");
    if (!gActiveRecordFile) {
      gRecordingToFile = false;
    } else {
      gActiveRecordPath = kActiveRecordPcmPath;
      gActiveRecordBytes = 0;
      gActiveRecordCommittedBytes = 0;
      gActiveRecordExpectedBytes = 0;
      gRecordInflightChunks.clear();
      gRecordNextChunkIndex = 0;
    }
  }

  if (!gRecordingToFile) {
    logHeapStats("voice-start");
    if (!ensureRecordBuffer()) {
      pushError("Audio buffer allocation failed.");
      setFaceMode(FaceMode::Error);
      setStatus("No audio buffer", 1500);
      setVoiceDiag("voice: record buffer failed");
      logf("[VOICE] audio buffer allocation failed");
      gRecording = false;
      return;
    }
  }

  startMic();
  if (gRecordingToFile) {
    serviceRecordingToFile();
    setVoiceDiag("voice: recording fs");
  } else {
    M5Cardputer.Mic.record(gRecordBuffer, gRecordCapacitySamples, kMicSampleRate);
    setVoiceDiag("voice: recording");
  }
  setFaceMode(FaceMode::Listening);
  setStatus("Listening...");
  render();
  logf("[VOICE] recording started mode=%s", gRecordingToFile ? "file" : "ram");
}

void writeWavHeader(uint8_t* h, uint32_t dataSize, uint32_t sampleRate) {
  uint32_t fileSize = 36 + dataSize;
  uint32_t byteRate = sampleRate * 2;
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
  h[24] = sampleRate & 0xFF;
  h[25] = (sampleRate >> 8) & 0xFF;
  h[26] = (sampleRate >> 16) & 0xFF;
  h[27] = (sampleRate >> 24) & 0xFF;
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

bool writeClientAllFileStaged(WiFiClient& client, File& file, size_t length, const char* label) {
  uint8_t stage[kHttpStageChunkBytes];
  size_t written = 0;
  unsigned long deadline = millis() + 90000;
  unsigned long lastProgress = millis();

  while (written < length && millis() < deadline) {
    if (WiFi.status() != WL_CONNECTED) {
      setHttpDiag(String(label) + ": wifi lost");
      return false;
    }
    if (!client.connected()) {
      setHttpDiag(String(label) + ": disconnected");
      return false;
    }
    size_t chunk = length - written;
    if (chunk > sizeof(stage)) {
      chunk = sizeof(stage);
    }
    size_t readBytes = file.read(stage, chunk);
    if (readBytes == 0) {
      break;
    }
    size_t sentFromStage = 0;
    while (sentFromStage < readBytes && millis() < deadline) {
      if (WiFi.status() != WL_CONNECTED) {
        setHttpDiag(String(label) + ": wifi lost");
        return false;
      }
      if (!client.connected()) {
        setHttpDiag(String(label) + ": disconnected");
        return false;
      }
      size_t sent = client.write(stage + sentFromStage, readBytes - sentFromStage);
      if (sent == 0) {
        if (millis() - lastProgress > 5000) {
          setHttpDiag(String(label) + ": stalled");
          return false;
        }
        yield();
        delay(2);
        continue;
      }
      sentFromStage += sent;
      written += sent;
      lastProgress = millis();
      if ((written % (8 * 1024)) == 0 || written == length) {
        setHttpDiag(String(label) + ": " + String(static_cast<unsigned>(written / 1024)) + "k/" +
                    String(static_cast<unsigned>(length / 1024)) + "k");
      }
      yield();
      delay(1);
    }
  }

  if (written != length) {
    setHttpDiag(String(label) + ": short " + String(static_cast<unsigned>(written)) + "/" +
                String(static_cast<unsigned>(length)));
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
      if (!hubUsesTls()) {
        client.setNoDelay(true);
      }
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
    return "X-Device-Id: " + gDeviceId + "\r\n";
  }
  return "Authorization: Bearer " + gDeviceToken + "\r\n" +
         "X-Device-Token: " + gDeviceToken + "\r\n" +
         "X-Device-Id: " + gDeviceId + "\r\n";
}

bool requestAndPlayTts(const String& text) {
  String speakText = cleanReplyForDevice(text);
  if (speakText.isEmpty()) {
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
  if (gTtsBuffer) {
    releaseTtsBuffer();
  }

  bool pausedWsForTts = false;
  if (gWsReady) {
    pauseHubWebSocketForVoice();
    pausedWsForTts = true;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + kDeviceTtsPath;
  http.setTimeout(60000);
  http.setReuse(false);
  const char* responseHeaders[] = {"X-Audio-Sample-Rate"};
  http.collectHeaders(responseHeaders, 1);

  bool beginOk = false;
  if (hubUsesTls()) {
    secureClient.reset(new WiFiClientSecure());
    secureClient->setInsecure();
    secureClient->setHandshakeTimeout(12);
    beginOk = http.begin(*secureClient, url);
  } else {
    beginOk = http.begin(url);
  }
  if (!beginOk) {
    logf("[TTS] begin failed url=%s", url.c_str());
    setVoiceDiag("tts: connect failed");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  String body =
      "{\"device_id\":\"" + escapeJsonString(gDeviceId) + "\",\"text\":\"" +
      escapeJsonString(speakText) + "\"}";
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Id", gDeviceId);
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
    http.addHeader("X-Device-Token", gDeviceToken);
  }

  int statusCode = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body.c_str())), body.length());
  if (statusCode <= 0) {
    logf("[TTS] HTTPClient failed code=%d err=%s",
         statusCode,
         HTTPClient::errorToString(statusCode).c_str());
    http.end();
    setVoiceDiag("tts: httpclient " + String(statusCode));
    releaseTtsBuffer();
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  if (statusCode == 204) {
    http.end();
    setVoiceDiag("tts: empty pcm");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  if (statusCode != 200) {
    String errorBody = http.getString();
    logf("[TTS] bad response status=%d body=%s", statusCode, errorBody.c_str());
    http.end();
    setVoiceDiag("tts: bad status " + String(statusCode));
    releaseTtsBuffer();
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage() || !fs) {
    http.end();
    setVoiceDiag("tts: fs unavailable");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  uint32_t sampleRate = kTtsDefaultSampleRate;
  String sampleRateHeader = http.header("X-Audio-Sample-Rate");
  if (!sampleRateHeader.isEmpty()) {
    uint32_t parsed = static_cast<uint32_t>(sampleRateHeader.toInt());
    if (parsed >= 8000 && parsed <= 48000) {
      sampleRate = parsed;
    }
  }

  fsRemove(*fs, kActiveTtsPcmPath);
  File pcmFile = fs->open(kActiveTtsPcmPath, "w");
  if (!pcmFile) {
    http.end();
    setVoiceDiag("tts: fs open failed");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  size_t bytesRead = 0;
  uint8_t ioBuffer[1024];
  int contentLength = http.getSize();
  WiFiClient* stream = http.getStreamPtr();
  unsigned long deadline = millis() + 90000;
  while (stream && http.connected() && millis() < deadline &&
         (contentLength < 0 || bytesRead < static_cast<size_t>(contentLength))) {
    int avail = stream->available();
    if (avail <= 0) {
      delay(1);
      yield();
      continue;
    }
    size_t chunk = static_cast<size_t>(avail);
    if (chunk > sizeof(ioBuffer)) {
      chunk = sizeof(ioBuffer);
    }
    if (contentLength > 0) {
      size_t left = static_cast<size_t>(contentLength) - bytesRead;
      if (chunk > left) {
        chunk = left;
      }
    }
    int got = stream->readBytes(ioBuffer, chunk);
    if (got > 0) {
      size_t written = pcmFile.write(ioBuffer, static_cast<size_t>(got));
      if (written != static_cast<size_t>(got)) {
        pcmFile.close();
        http.end();
        fsRemove(*fs, kActiveTtsPcmPath);
        setVoiceDiag("tts: fs write failed");
        if (pausedWsForTts) {
          resumeHubWebSocketAfterVoice();
        }
        return false;
      }
      bytesRead += static_cast<size_t>(got);
      if ((bytesRead % (16 * 1024)) == 0 || (contentLength > 0 && bytesRead == static_cast<size_t>(contentLength))) {
        setVoiceDiag("tts: rx " + String(static_cast<unsigned>(bytesRead / 1024)) + "k");
      }
    }
  }
  pcmFile.close();
  http.end();

  if (bytesRead == 0) {
    logf("[TTS] empty PCM response");
    setVoiceDiag("tts: empty pcm");
    fsRemove(*fs, kActiveTtsPcmPath);
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  if (contentLength > 0 && bytesRead < static_cast<size_t>(contentLength)) {
    logf("[TTS] short PCM response bytes=%u expected=%u",
         static_cast<unsigned>(bytesRead),
         static_cast<unsigned>(contentLength));
    fsRemove(*fs, kActiveTtsPcmPath);
    setVoiceDiag("tts: short pcm");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  rememberVoiceNoteFromPcmFile("Assistant voice", speakText, kActiveTtsPcmPath, bytesRead, sampleRate, true);
  bool started = startPlaybackStreamFromRawPcmFile(kActiveTtsPcmPath, sampleRate, bytesRead,
                                                   "tts: file " + String(static_cast<unsigned>(bytesRead / 1024)) + "k");
  if (!started) {
    fsRemove(*fs, kActiveTtsPcmPath);
  }
  if (pausedWsForTts) {
    resumeHubWebSocketAfterVoice();
  }
  return started;
}

bool sendTextTurn(const String& text) {
  if ((!gWifiReady && !ensureWifiForHttp("text-turn", 3000)) || gHubHost.isEmpty()) {
    pushError("Hub is not configured.");
    return false;
  }

  HTTPClient http;
  String url = hubBaseUrl() + kDeviceTextTurnPath;
  http.setTimeout(60000);
  std::unique_ptr<WiFiClientSecure> secureClient;
  bool beginOk = false;
  if (hubUsesTls()) {
    secureClient.reset(new WiFiClientSecure());
    secureClient->setInsecure();
    secureClient->setHandshakeTimeout(12);
    beginOk = http.begin(*secureClient, url);
  } else {
    beginOk = http.begin(url);
  }
  if (!beginOk) {
    pushError("Failed to open HTTP connection.");
    return false;
  }

  DynamicJsonDocument doc(768);
  doc["message"] = text;
  doc["device_id"] = gDeviceId;
  doc["firmware_version"] = kAppVersion;
  doc["conversation_key"] = currentConversationKey();
  doc["input_type"] = "text";
  doc["reply_audio"] = true;
  doc["save_reply_audio"] = true;

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

  String reply = resp["reply"] | resp["full_text"] | resp["text"] | "";
  String shortText = resp["short_text"] | "";
  if (reply.isEmpty()) {
    reply = shortText;
  }
  if (!reply.isEmpty()) {
    appendAssistantReply(reply);
    String audioUrl;
    String audioPath;
    String audioTitle;
    String audioSha;
    uint32_t audioSize = 0;
    if (resp["audio"].is<JsonObject>()) {
      JsonObject audio = resp["audio"].as<JsonObject>();
      audioUrl = audio["url"] | "";
      audioPath = audio["path"] | "";
      audioTitle = audio["title"] | "";
      audioSha = audio["sha256"] | "";
      audioSize = audio["size"] | 0;
    }
    if (!downloadAndPlayResponseAudio(audioUrl, audioPath, audioSha, audioSize, audioTitle, reply)) {
      requestAndPlayTts(reply);
    }
    logf("[TEXT] reply=%s", reply.c_str());
  } else {
    pushSystem("Hub accepted the turn.");
    logf("[TEXT] accepted without reply");
  }
  setFaceMode(FaceMode::Idle);
  setStatus("Reply received", 1200);
  return true;
}

const char* batteryChargeLabel(m5::Power_Class::is_charging_t charging) {
  switch (charging) {
    case m5::Power_Class::is_charging:
      return "Charging";
    case m5::Power_Class::is_discharging:
      return "Discharging";
    case m5::Power_Class::charge_unknown:
    default:
      return "Unknown";
  }
}

void updateBatterySnapshot(bool force) {
  uint32_t now = millis();
  if (!force && gLastBatterySampleMs != 0 && now - gLastBatterySampleMs < kBatterySampleIntervalMs) {
    return;
  }
  gLastBatterySampleMs = now;
  gBatterySnapshot.level = M5.Power.getBatteryLevel();
  gBatterySnapshot.voltageMv = M5.Power.getBatteryVoltage();
  gBatterySnapshot.currentMa = M5.Power.getBatteryCurrent();
  gBatterySnapshot.charging = M5.Power.isCharging();
}

void textTurnTask(void* arg) {
  std::unique_ptr<TextTurnTaskPayload> payload(static_cast<TextTurnTaskPayload*>(arg));
  std::unique_ptr<TextTurnTaskResult> result(new TextTurnTaskResult());
  if (!result) {
    gTextTurnTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  result->response.reserve(2048);

  if (payload->url.isEmpty()) {
    result->error = "Hub is not configured.";
  } else if (!ensureWifiForHttp("text-turn", 3000)) {
    result->error = "Wi-Fi unavailable.";
  } else {
    HTTPClient http;
    std::unique_ptr<WiFiClientSecure> secureClient;
    http.setTimeout(60000);
    bool beginOk = false;
    if (hubUsesTls()) {
      secureClient.reset(new WiFiClientSecure());
      secureClient->setInsecure();
      secureClient->setHandshakeTimeout(12);
      beginOk = http.begin(*secureClient, payload->url);
    } else {
      beginOk = http.begin(payload->url);
    }
    if (!beginOk) {
      result->error = "Failed to open HTTP connection.";
    } else {
      DynamicJsonDocument doc(768);
      doc["message"] = payload->text;
      doc["device_id"] = payload->deviceId;
      doc["firmware_version"] = kAppVersion;
      doc["conversation_key"] = payload->conversationKey;
      doc["input_type"] = "text";
      doc["reply_audio"] = true;
      doc["save_reply_audio"] = true;

      String body;
      serializeJson(doc, body);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("X-Conversation-Key", payload->conversationKey);
      if (!payload->deviceToken.isEmpty()) {
        http.addHeader("Authorization", "Bearer " + payload->deviceToken);
      }
      int code = http.POST(body);
      result->statusCode = code;
      result->response = http.getString();
      http.end();

      if (code < 200 || code >= 300) {
        result->error = code <= 0 ? "Hub request failed." : "Hub HTTP error: " + String(code);
      } else {
        DynamicJsonDocument resp(4096);
        auto err = deserializeJson(resp, result->response);
        result->parsed = !err;
        if (err) {
          result->error = "Could not parse hub response.";
        } else {
          String reply = resp["reply"] | resp["full_text"] | resp["text"] | "";
          String shortText = resp["short_text"] | "";
          if (reply.isEmpty()) {
            reply = shortText;
          }
          result->shortText = shortText;
          result->telegramDelivered = resp["telegram"]["delivered"] | false;
          if (resp["audio"].is<JsonObject>()) {
            JsonObject audio = resp["audio"].as<JsonObject>();
            result->audioUrl = audio["url"] | "";
            result->audioPath = audio["path"] | "";
            result->audioTitle = audio["title"] | "";
            result->audioSha256 = audio["sha256"] | "";
            result->audioSize = audio["size"] | 0;
          }
          if (!reply.isEmpty()) {
          result->hasReply = true;
            result->reply = reply;
          result->ok = true;
        } else {
          result->ok = true;
          }
        }
      }
    }
  }

  portENTER_CRITICAL(&gTextTurnMux);
  if (gCompletedTextTurn == nullptr) {
    gCompletedTextTurn = result.release();
  } else {
    delete gCompletedTextTurn;
    gCompletedTextTurn = result.release();
  }
  portEXIT_CRITICAL(&gTextTurnMux);
  vTaskDelete(nullptr);
}

void maybeStartNextTextTurn() {
  if (gTextTurnTaskHandle != nullptr || gCompletedTextTurn != nullptr || !gActiveTextTurn.isEmpty()) {
    return;
  }
  if (gPendingTextTurns.empty()) {
    if (!gRecording) {
      gSubmitting = false;
      gThinking = false;
      gAssistantPendingVisible = false;
      if (!gPlaybackActive) {
        setFaceMode(FaceMode::Idle);
      }
    }
    return;
  }

  std::unique_ptr<TextTurnTaskPayload> payload(new TextTurnTaskPayload());
  if (!payload) {
    pushError("Send queue allocation failed.");
    gPendingTextTurns.pop_front();
    gSubmitting = false;
    gThinking = false;
    gAssistantPendingVisible = false;
    setFaceMode(FaceMode::Error);
    setStatus("Queue failed", 1500);
    return;
  }

  payload->text = gPendingTextTurns.front();
  gPendingTextTurns.pop_front();
  payload->url = hubBaseUrl() + kDeviceTextTurnPath;
  payload->deviceId = gDeviceId;
  payload->deviceToken = gDeviceToken;
  payload->conversationKey = currentConversationKey();

  gActiveTextTurn = payload->text;
  gSubmitting = true;
  gThinking = true;
  gAssistantPendingVisible = true;
  setFaceMode(FaceMode::Thinking);
  setStatus(gPendingTextTurns.empty() ? "Sending..." : ("Sending... +" + String(gPendingTextTurns.size())));

  BaseType_t taskOk = xTaskCreate(
      textTurnTask,
      "text-turn",
      kTextTurnTaskStackSize,
      payload.release(),
      1,
      &gTextTurnTaskHandle);

  if (taskOk != pdPASS) {
    gActiveTextTurn = "";
    gTextTurnTaskHandle = nullptr;
    gSubmitting = false;
    gThinking = false;
    gAssistantPendingVisible = false;
    pushError("Failed to start send task.");
    setFaceMode(FaceMode::Error);
    setStatus("Send failed", 1500);
  }
}

void processCompletedTextTurn() {
  TextTurnTaskResult* result = nullptr;
  portENTER_CRITICAL(&gTextTurnMux);
  result = gCompletedTextTurn;
  gCompletedTextTurn = nullptr;
  portEXIT_CRITICAL(&gTextTurnMux);
  if (!result) {
    return;
  }

  std::unique_ptr<TextTurnTaskResult> done(result);
  gTextTurnTaskHandle = nullptr;
  gActiveTextTurn = "";

  if (!done->error.isEmpty()) {
    logf("[TEXT] async error code=%d body=%s", done->statusCode, done->response.c_str());
    pushError(done->error);
    if (!done->response.isEmpty()) {
      pushError(done->response);
    }
    setFaceMode(FaceMode::Error);
    setStatus("Text turn failed", 2000);
  } else if (done->ok && done->hasReply) {
    appendAssistantReply(done->reply);
    bool playedAudio = downloadAndPlayResponseAudio(done->audioUrl, done->audioPath, done->audioSha256,
                                                    done->audioSize, done->audioTitle, done->reply);
    if (!playedAudio) {
      requestAndPlayTts(done->reply);
    }
    logf("[TEXT] async reply=%s", done->reply.c_str());
    setStatus("Reply received", 1200);
  } else if (done->ok) {
    pushSystem("Hub accepted the turn.");
    logf("[TEXT] async accepted without reply");
    if (!gPlaybackActive) {
      setFaceMode(FaceMode::Idle);
    }
    setStatus("Turn accepted", 1200);
  }

  if (!gPendingTextTurns.empty()) {
    maybeStartNextTextTurn();
    return;
  }

  gSubmitting = false;
  gThinking = false;
  gAssistantPendingVisible = false;
  if (!gRecording && !gPlaybackActive) {
    setFaceMode(FaceMode::Idle);
  }
}

bool stopRecordingAndSend() {
  if (!gRecording) {
    return false;
  }
  gRecording = false;
  pulseButtonPress();

  float elapsed = (millis() - gRecordStartMs) / 1000.0f;
  size_t dataSize = 0;
  String pcmUploadPath;

  setStatus("Finalizing...");
  render();
  gLastRenderMs = millis();

  if (gRecordingToFile) {
    drainRecordingToFile();
    size_t queuedBeforeStop = M5Cardputer.Mic.isRecording();
    if (queuedBeforeStop > 0 || !gRecordInflightChunks.empty()) {
      logf("[VOICE] dropping tail queued=%u inflight=%u",
           static_cast<unsigned>(queuedBeforeStop),
           static_cast<unsigned>(gRecordInflightChunks.size()));
    }
    stopMic();
    gRecordInflightChunks.clear();
    if (gActiveRecordFile) {
      gActiveRecordFile.flush();
      gActiveRecordFile.close();
    }
    gRecordedSamples = gActiveRecordBytes / sizeof(int16_t);
    dataSize = static_cast<uint32_t>(gActiveRecordBytes);
    pcmUploadPath = gActiveRecordPath;
  } else {
    size_t expectedSamples = static_cast<size_t>(elapsed * kMicSampleRate);
    gRecordedSamples = expectedSamples;
    if (gRecordedSamples > gRecordCapacitySamples) {
      gRecordedSamples = gRecordCapacitySamples;
    }
    stopMic();
    dataSize = static_cast<uint32_t>(gRecordedSamples * sizeof(int16_t));
  }

  logf("[VOICE] recording stopped elapsed=%.2f samples=%u mode=%s", elapsed,
       static_cast<unsigned>(gRecordedSamples),
       gRecordingToFile ? "file" : "ram");

  if (elapsed < kMinRecordSeconds || gRecordedSamples == 0) {
    setFaceMode(FaceMode::Idle);
    setStatus("Voice turn canceled", 1200);
    logf("[VOICE] canceled: too short");
    if (gRecordingToFile) {
      fs::FS* fs = activeVoiceFs();
      if (!pcmUploadPath.isEmpty()) {
        if (fs) {
          fsRemove(*fs, pcmUploadPath);
        }
      }
      gRecordingToFile = false;
      gActiveRecordPath = "";
      gActiveRecordBytes = 0;
      gActiveRecordCommittedBytes = 0;
      gActiveRecordExpectedBytes = 0;
    } else {
      releaseRecordBuffer();
    }
    return false;
  }

  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (!gWifiReady) {
    pushError("Wi-Fi dropped during recording.");
    setFaceMode(FaceMode::Error);
    setStatus("Wi-Fi lost", 2000);
    setVoiceDiag("voice: wifi lost");
    logf("[VOICE] wifi lost before upload");
    if (gRecordingToFile) {
      fs::FS* fs = activeVoiceFs();
      if (!pcmUploadPath.isEmpty()) {
        rememberVoiceNoteFromPcmFile("My voice", "Outbound voice note", pcmUploadPath, dataSize, kMicSampleRate, false);
        if (fs) {
          fsRemove(*fs, pcmUploadPath);
        }
      }
      gRecordingToFile = false;
      gActiveRecordPath = "";
      gActiveRecordBytes = 0;
      gActiveRecordCommittedBytes = 0;
      gActiveRecordExpectedBytes = 0;
    } else {
      releaseRecordBuffer();
    }
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

  if (gRecordingToFile) {
    rememberVoiceNoteFromPcmFile("My voice", "Outbound voice note", pcmUploadPath, dataSize, kMicSampleRate, false);
  } else {
    uint8_t wavHeader[44];
    writeWavHeader(wavHeader, dataSize, kMicSampleRate);
    rememberVoiceNote("My voice", "Outbound voice note", wavHeader, sizeof(wavHeader),
                      reinterpret_cast<const uint8_t*>(gRecordBuffer), dataSize, kMicSampleRate, false);
  }
  logHeapStats("voice-upload");

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + kDeviceAudioTurnRawPath;
  http.setTimeout(60000);
  http.setReuse(false);
  bool beginOk = false;
  if (hubUsesTls()) {
    secureClient.reset(new WiFiClientSecure());
    secureClient->setInsecure();
    secureClient->setHandshakeTimeout(12);
    beginOk = http.begin(*secureClient, url);
  } else {
    beginOk = http.begin(url);
  }
  if (!beginOk) {
    pushError("Voice upload connect failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice upload failed", 2000);
    setVoiceDiag("voice: upload connect failed");
    logf("[VOICE] upload begin failed url=%s", url.c_str());
    finishVoiceTurn();
    if (gRecordingToFile) {
      fs::FS* fs = activeVoiceFs();
      if (fs) {
        fsRemove(*fs, pcmUploadPath);
      }
      gRecordingToFile = false;
      gActiveRecordPath = "";
      gActiveRecordBytes = 0;
      gActiveRecordCommittedBytes = 0;
      gActiveRecordExpectedBytes = 0;
    } else {
      releaseRecordBuffer();
    }
    return false;
  }

  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Audio-Sample-Rate", String(kMicSampleRate));
  http.addHeader("X-Device-Id", gDeviceId);
  http.addHeader("X-Firmware-Version", kAppVersion);
  http.addHeader("X-Conversation-Key", currentConversationKey());
  http.addHeader("X-Reply-Audio", "true");
  http.addHeader("X-Save-Reply-Audio", "true");
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
    http.addHeader("X-Device-Token", gDeviceToken);
  }

  setStatus("Transcribing...");
  render();
  gLastRenderMs = millis();
  int statusCode = -1;
  if (gRecordingToFile) {
    fs::FS* fs = activeVoiceFs();
    File pcmFile = fs ? fs->open(pcmUploadPath, "r") : File();
    if (pcmFile) {
      setHttpDiag("voice_pcm: http stream " + String(static_cast<unsigned>(dataSize / 1024)) + "k");
      statusCode = http.sendRequest("POST", &pcmFile, dataSize);
      pcmFile.close();
    }
  } else {
    setHttpDiag("voice_pcm: http ram " + String(static_cast<unsigned>(dataSize / 1024)) + "k");
    statusCode = http.POST(reinterpret_cast<uint8_t*>(gRecordBuffer), dataSize);
  }
  String response = statusCode > 0 ? http.getString() : "";
  http.end();
  finishVoiceTurn();
  if (gRecordingToFile) {
    fs::FS* fs = activeVoiceFs();
    if (fs) {
      fsRemove(*fs, pcmUploadPath);
    }
    gRecordingToFile = false;
    gActiveRecordPath = "";
    gActiveRecordBytes = 0;
    gActiveRecordCommittedBytes = 0;
    gActiveRecordExpectedBytes = 0;
  } else {
    releaseRecordBuffer();
  }

  if (statusCode <= 0) {
    pushError("Voice upload body failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice upload failed", 2000);
    setVoiceDiag("voice: httpclient " + String(statusCode));
    setHttpDiag(HTTPClient::errorToString(statusCode));
    logf("[VOICE] HTTPClient upload failed code=%d err=%s bytes=%u",
         statusCode,
         HTTPClient::errorToString(statusCode).c_str(),
         static_cast<unsigned>(dataSize));
    return false;
  }

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

  String transcript = resp["transcript"] | resp["text"] | "";
  String reply = resp["reply"] | resp["full_text"] | "";
  String shortText = resp["short_text"] | "";
  if (reply.isEmpty()) {
    reply = shortText;
  }
  String audioUrl;
  String audioPath;
  String audioTitle;
  String audioSha;
  uint32_t audioSize = 0;
  if (resp["audio"].is<JsonObject>()) {
    JsonObject audio = resp["audio"].as<JsonObject>();
    audioUrl = audio["url"] | "";
    audioPath = audio["path"] | "";
    audioTitle = audio["title"] | "";
    audioSha = audio["sha256"] | "";
    audioSize = audio["size"] | 0;
  }
  logf("[VOICE] transcript=%s", transcript.c_str());
  logf("[VOICE] reply=%s", reply.c_str());
  if (!transcript.isEmpty()) {
    pushUser(transcript);
    setVoiceDiag("voice: transcript ok");
  }
  if (!reply.isEmpty()) {
    appendAssistantReply(reply);
    if (!downloadAndPlayResponseAudio(audioUrl, audioPath, audioSha, audioSize, audioTitle, reply) &&
        !requestAndPlayTts(reply)) {
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
    pushSystem("Voice: Ctrl x2, Ctrl+V, or hold G0");
    pushSystem("Tab = next screen, Tab+Down/Right = next app");
    pushSystem("Ctrl+L = launcher map, arrows = group/item");
    pushSystem("Ctrl+D = debug, /voice /focus /topics");
    return true;
  }

  if (cmd == "/home" || cmd == "/pulse") {
    setUiMode(UiMode::Home);
    return true;
  }

  if (cmd == "/status") {
    pushSystem("device_id=" + gDeviceId);
    pushSystem("wifi=" + String(gWifiReady ? WiFi.localIP().toString() : "offline"));
    pushSystem("hub=" + (gHubHost.isEmpty() ? String("unset") : gHubHost + ":" + String(gHubPort)));
    pushSystem("ctx=" + currentConversationKey());
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

  if (cmd == "/hero") {
    setUiMode(UiMode::Hero);
    return true;
  }

  if (cmd == "/clock") {
    setUiMode(UiMode::Home);
    return true;
  }

  if (cmd == "/voice") {
    setUiMode(UiMode::Voice);
    return true;
  }

  if (cmd == "/battery") {
    setUiMode(UiMode::Battery);
    return true;
  }

  if (cmd == "/focus") {
    setUiMode(UiMode::Focus);
    return true;
  }

  if (cmd == "/focusreset" || cmd == "/freset") {
    focusReset();
    setUiMode(UiMode::Focus);
    return true;
  }

  if (cmd == "/library") {
    scanAudioAssets();
    setUiMode(UiMode::Library);
    return true;
  }

  if (cmd == "/assets") {
    loadAssetManifestSummary();
    scanAudioAssets();
    setUiMode(UiMode::Assets);
    return true;
  }

  if (cmd == "/ota") {
    loadOtaManifestSummary();
    setUiMode(UiMode::Ota);
    return true;
  }

  if (cmd == "/contexts" || cmd == "/topics") {
    loadConversationContexts();
    setUiMode(UiMode::Contexts);
    startTopicTask(true, false, true, "Syncing topics...");
    return true;
  }

  if (cmd == "/settings") {
    setUiMode(UiMode::Settings);
    return true;
  }

  if (cmd == "/syncassets") {
    fetchRemoteAssetManifest();
    syncAssetsFromManifest();
    setUiMode(UiMode::Assets);
    return true;
  }

  if (cmd == "/fetchota") {
    fetchRemoteOtaManifest();
    setUiMode(UiMode::Ota);
    return true;
  }

  if (cmd == "/sendlogs") {
    uploadRuntimeLogs();
    setUiMode(UiMode::Logs);
    return true;
  }

  if (cmd == "/logs") {
    setUiMode(UiMode::Logs);
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
  gPendingTextTurns.push_back(text);
  gAssistantPendingVisible = true;
  if (gSubmitting || gTextTurnTaskHandle != nullptr || !gActiveTextTurn.isEmpty()) {
    setStatus("Queued +" + String(gPendingTextTurns.size()), 1000);
  }
  maybeStartNextTextTurn();
}

void handleLauncherKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (key >= '1' && key <= '9') {
    int target = key - '1';
    if (target >= kLauncherGroupCount) {
      return;
    }
    gLauncherSelection = target;
    uint8_t count = kLauncherGroups[gLauncherSelection].count;
    if (gLauncherSubSelection >= count) {
      gLauncherSubSelection = count - 1;
    }
    return;
  }
  if (keys.enter || key == '\n' || key == '\r') {
    applyLauncherSelection();
    return;
  }
  if (key == ',' || key == ';' || lowerKey == 'a' || lowerKey == 'h') {
    moveLauncherSubSelection(-1);
    return;
  }
  if (key == '.' || key == '/' || lowerKey == 'd' || lowerKey == 'l') {
    moveLauncherSubSelection(1);
    return;
  }
  if (lowerKey == 'w' || lowerKey == 'k') {
    moveLauncherSelection(-1);
    return;
  }
  if (lowerKey == 's' || lowerKey == 'j') {
    moveLauncherSelection(1);
    return;
  }
}

void handleVoicePlayerKey(char key, const Keyboard_Class::KeysState& keys) {
  if (gVoiceNotes.empty()) {
    setStatus("No voice notes", 1000);
    return;
  }
  auto playSelected = [&]() {
    if (gPlaybackActive) {
      stopPlaybackStream();
      gPlaybackActive = false;
    }
    gVoiceTickerX = 90;
    if (!playVoiceNoteAt(gSelectedVoiceNote)) {
      setStatus("Playback failed", 1000);
    }
  };

  if (key == ',' || key == ';' || key == 'p' || key == 'P') {
    gSelectedVoiceNote = max(0, gSelectedVoiceNote - 1);
    gVoiceTickerX = 230;
    setStatus("Voice " + String(gSelectedVoiceNote + 1), 600);
    if (key == 'p' || key == 'P') {
      playSelected();
    }
    return;
  }
  if (key == '.' || key == '/' || key == 'n' || key == 'N') {
    gSelectedVoiceNote = min(static_cast<int>(gVoiceNotes.size()) - 1, gSelectedVoiceNote + 1);
    gVoiceTickerX = 230;
    setStatus("Voice " + String(gSelectedVoiceNote + 1), 600);
    if (key == 'n' || key == 'N') {
      playSelected();
    }
    return;
  }
  if (key == 'b' || key == 'B') {
    gSelectedVoiceNote = random(0, static_cast<int>(gVoiceNotes.size()));
    gVoiceTickerX = 230;
    playSelected();
    return;
  }
  if (key == 'v' || key == 'V') {
    gSpeakerVolume = gSpeakerVolume >= 224 ? 64 : gSpeakerVolume + 32;
    M5Cardputer.Speaker.setVolume(gSpeakerVolume);
    setStatus("Vol " + String(gSpeakerVolume), 600);
    return;
  }
  if (key == 'a' || key == 'A' || keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    if (gPlaybackActive) {
      stopPlaybackStream();
      gPlaybackActive = false;
      setFaceMode(FaceMode::Idle);
      setStatus("Playback stopped", 800);
      return;
    }
    playSelected();
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

bool handleFocusKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    focusStartOrResume();
    return true;
  }
  if (lowerKey == 'r' || keys.del || key == '\b') {
    focusReset();
    return true;
  }
  if (lowerKey == 's') {
    focusSnooze();
    return true;
  }
  if (lowerKey == 'n') {
    if (focusStateRuns(gFocus.state)) {
      gFocus.remainingSec = 0;
      focusAdvance();
    } else {
      focusEnterState(FocusState::Focus);
    }
    return true;
  }
  if (lowerKey == 'm') {
    gFocusSettings.metronome = !gFocusSettings.metronome;
    saveFocusSettings();
    setStatus(gFocusSettings.metronome ? "Metronome on" : "Metronome off", 900);
    return true;
  }
  if (lowerKey == 'a') {
    gFocusSettings.autoStart = !gFocusSettings.autoStart;
    saveFocusSettings();
    setStatus(gFocusSettings.autoStart ? "Auto on" : "Auto off", 900);
    return true;
  }
  if (lowerKey == 'z') {
    gFocusSettings.bpm = gFocusSettings.bpm <= kFocusMinBpm + 5 ? kFocusMinBpm : gFocusSettings.bpm - 5;
    saveFocusSettings();
    setStatus("BPM " + String(gFocusSettings.bpm), 700);
    return true;
  }
  if (lowerKey == 'x') {
    gFocusSettings.bpm = min<uint16_t>(kFocusMaxBpm, gFocusSettings.bpm + 5);
    saveFocusSettings();
    setStatus("BPM " + String(gFocusSettings.bpm), 700);
    return true;
  }
  if (lowerKey == 'q') {
    gFocusSettings.focusSec = max<uint32_t>(kFocusMinDurationSec, gFocusSettings.focusSec - 5UL * 60UL);
    if (gFocus.state == FocusState::Stopped) {
      focusReset();
    }
    saveFocusSettings();
    setStatus("Focus " + String(gFocusSettings.focusSec / 60) + "m", 900);
    return true;
  }
  if (lowerKey == 'w') {
    gFocusSettings.focusSec = min<uint32_t>(kFocusMaxDurationSec, gFocusSettings.focusSec + 5UL * 60UL);
    if (gFocus.state == FocusState::Stopped) {
      focusReset();
    }
    saveFocusSettings();
    setStatus("Focus " + String(gFocusSettings.focusSec / 60) + "m", 900);
    return true;
  }
  if (key == '-' && gSpeakerVolume >= 32) {
    gSpeakerVolume -= 32;
    M5Cardputer.Speaker.setVolume(gSpeakerVolume);
    setStatus("Vol " + String(gSpeakerVolume), 600);
    return true;
  }
  if ((key == '=' || key == '+') && gSpeakerVolume <= 223) {
    gSpeakerVolume += 32;
    M5Cardputer.Speaker.setVolume(gSpeakerVolume);
    setStatus("Vol " + String(gSpeakerVolume), 600);
    return true;
  }
  if (key == '?' || lowerKey == 'h') {
    gFocus.helpVisible = !gFocus.helpVisible;
    setStatus(gFocus.helpVisible ? "Focus help" : "Focus clean", 700);
    return true;
  }
  return false;
}

bool handleLibraryKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'r') {
    scanAudioAssets();
    setStatus("Library rescanned", 900);
    return true;
  }
  if (lowerKey == 'f') {
    setUiMode(UiMode::AudioFolders);
    return true;
  }
  int folderCount = audioAssetCountForCurrentFolder();
  if (gAudioAssets.empty() || folderCount == 0) {
    return lowerKey == 'r' || lowerKey == 'f';
  }
  if (key == ',' || key == ';') {
    moveSelectedAudioAssetInFolder(-1);
    setStatus("Asset " + String(audioAssetRankInCurrentFolder(gSelectedAudioAsset) + 1) + "/" + String(folderCount), 500);
    return true;
  }
  if (key == '.' || key == '/') {
    moveSelectedAudioAssetInFolder(1);
    setStatus("Asset " + String(audioAssetRankInCurrentFolder(gSelectedAudioAsset) + 1) + "/" + String(folderCount), 500);
    return true;
  }
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    if (gPlaybackActive) {
      stopPlaybackStream();
      gPlaybackActive = false;
      setStatus("Playback stopped", 800);
      return true;
    }
    clampSelectedAudioAssetToFolder();
    if (!startPlaybackStreamFromWavFile(gAudioAssets[gSelectedAudioAsset].path, "asset: " + gAudioAssets[gSelectedAudioAsset].title)) {
      setStatus("Asset play failed", 1200);
    }
    return true;
  }
  return false;
}

bool handleAudioFoldersKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'r') {
    scanAudioAssets();
    setStatus("Folders rescanned", 900);
    return true;
  }
  if (gAudioFolders.empty()) {
    rebuildAudioFolders();
  }
  if (key == ',' || key == ';') {
    gSelectedAudioFolder = max<int>(0, gSelectedAudioFolder - 1);
    setStatus(audioFolderLabel(gAudioFolders[gSelectedAudioFolder].category), 700);
    return true;
  }
  if (key == '.' || key == '/') {
    gSelectedAudioFolder = min<int>(static_cast<int>(gAudioFolders.size()) - 1, gSelectedAudioFolder + 1);
    setStatus(audioFolderLabel(gAudioFolders[gSelectedAudioFolder].category), 700);
    return true;
  }
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    gSelectedAudioFolder = clampValue<int>(gSelectedAudioFolder, 0, max<int>(0, static_cast<int>(gAudioFolders.size()) - 1));
    gAudioFolderFilter = gAudioFolders.empty() ? "" : gAudioFolders[gSelectedAudioFolder].category;
    clampSelectedAudioAssetToFolder();
    setUiMode(UiMode::Library);
    return true;
  }
  return false;
}

bool handleAssetsKey(char key) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'f') {
    fetchRemoteAssetManifest();
    setStatus("Asset manifest fetched", 900);
    return true;
  }
  if (lowerKey == 's') {
    syncAssetsFromManifest();
    return true;
  }
  if (lowerKey == 'r') {
    loadAssetManifestSummary();
    scanAudioAssets();
    setStatus("Assets reloaded", 900);
    return true;
  }
  return false;
}

bool handleOtaKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'f') {
    fetchRemoteOtaManifest();
    return true;
  }
  if (lowerKey == 'r') {
    loadOtaManifestSummary();
    setStatus("OTA manifest reloaded", 900);
    return true;
  }
  if (lowerKey == 'a' || keys.enter || key == '\n' || key == '\r') {
    setStatus("OTA apply requested", 700);
    appendRuntimeLog("OTA", "apply requested by keyboard", true);
    render();
    applyOtaFromManifest();
    return true;
  }
  return false;
}

bool handleContextsKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'r') {
    startTopicTask(true, false, true, "Syncing topics...");
    return true;
  }
  if (lowerKey == 'h') {
    startTopicTask(false, false, true, "Loading history...");
    return true;
  }
  if (gContexts.empty()) {
    return lowerKey == 'r';
  }
  if (key == ',' || key == ';') {
    return switchTopicRelative(-1, false);
  }
  if (key == '.' || key == '/') {
    return switchTopicRelative(1, false);
  }
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    saveCurrentContext();
    startTopicTask(false, true, true, "Topic loading...");
    return true;
  }
  return false;
}

bool switchTopicRelative(int delta, bool selectAndLoad) {
  if (gContexts.empty()) {
    startTopicTask(true, false, true, "Syncing topics...");
    setStatus("No topics", 900);
    return false;
  }
  gSelectedContext = (gSelectedContext + delta + static_cast<int>(gContexts.size())) %
                     static_cast<int>(gContexts.size());
  showTopicOverlay(delta < 0 ? -1 : 1, 1800);
  clearContextPreviewIfSelectionChanged();
  if (selectAndLoad) {
    saveCurrentContext();
    startTopicTask(!gContextsRemoteLoaded, true, true, "Topic loading...");
    setStatus("Topic " + normalizeTopicTitleForDisplay(gContexts[gSelectedContext].label), 900);
  } else {
    setStatus(normalizeTopicTitleForDisplay(gContexts[gSelectedContext].label), 700);
  }
  return true;
}

void topicTask(void* arg) {
  std::unique_ptr<TopicTaskPayload> payload(static_cast<TopicTaskPayload*>(arg));
  std::unique_ptr<TopicTaskResult> result(new TopicTaskResult());
  if (!result) {
    gTopicTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }
  if (payload->syncCatalog) {
    result->catalogOk = fetchRemoteTopicCatalog();
  }
  if (payload->selectCurrent) {
    result->selectOk = selectRemoteCurrentTopic();
  }
  if (payload->loadHistory) {
    result->historyOk = fetchSelectedTopicHistory();
  }
  if (!gContexts.empty()) {
    result->label = gContexts[gSelectedContext].shortName;
  }

  portENTER_CRITICAL(&gTopicTaskMux);
  if (gCompletedTopicTask == nullptr) {
    gCompletedTopicTask = result.release();
  } else {
    delete gCompletedTopicTask;
    gCompletedTopicTask = result.release();
  }
  portEXIT_CRITICAL(&gTopicTaskMux);
  vTaskDelete(nullptr);
}

bool startTopicTask(bool syncCatalog, bool selectCurrent, bool loadHistory, const char* statusText) {
  if (gTopicTaskHandle != nullptr || gCompletedTopicTask != nullptr) {
    setStatus("Topic busy", 500);
    return false;
  }
  std::unique_ptr<TopicTaskPayload> payload(new TopicTaskPayload());
  if (!payload) {
    setStatus("Topic alloc failed", 900);
    return false;
  }
  payload->syncCatalog = syncCatalog;
  payload->selectCurrent = selectCurrent;
  payload->loadHistory = loadHistory;
  setStatus(statusText ? statusText : "Topic sync...");
  BaseType_t ok = xTaskCreate(topicTask, "topic-sync", kTopicTaskStackSize, payload.release(), 1, &gTopicTaskHandle);
  if (ok != pdPASS) {
    gTopicTaskHandle = nullptr;
    setStatus("Topic task failed", 1200);
    return false;
  }
  return true;
}

void processCompletedTopicTask() {
  TopicTaskResult* result = nullptr;
  portENTER_CRITICAL(&gTopicTaskMux);
  result = gCompletedTopicTask;
  gCompletedTopicTask = nullptr;
  portEXIT_CRITICAL(&gTopicTaskMux);
  if (!result) {
    return;
  }
  std::unique_ptr<TopicTaskResult> done(result);
  gTopicTaskHandle = nullptr;
  if (done->historyOk) {
    setStatus(done->label.isEmpty() ? "History loaded" : ("Topic " + done->label), 900);
  } else if (done->catalogOk) {
    setStatus("Topics synced", 900);
  } else if (!gContextError.isEmpty()) {
    setStatus(gContextError, 1200);
  }
}

bool handleLogsKey(char key) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (key == ',' || key == ';') {
    gLogScrollOffset = min<int>(gLogScrollOffset + 1, max<int>(0, static_cast<int>(gRuntimeLogs.size()) - 5));
    return true;
  }
  if (key == '.' || key == '/') {
    gLogScrollOffset = max<int>(0, gLogScrollOffset - 1);
    return true;
  }
  if (lowerKey == 'r') {
    scanAudioAssets();
    loadAssetManifestSummary();
    loadOtaManifestSummary();
    loadConversationContexts();
    appendRuntimeLog("LOG", "manual refresh", true);
    setStatus("Diagnostics refreshed", 900);
    return true;
  }
  if (lowerKey == 'u') {
    uploadRuntimeLogs();
    return true;
  }
  return false;
}

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

bool handleGlobalEscape() {
  bool changed = false;

  if (gRecording) {
    cancelRecordingDiscard();
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
  if (gChatScrollOffset != 0 || gLogScrollOffset != 0) {
    gChatScrollOffset = 0;
    gLogScrollOffset = 0;
    changed = true;
  }

  gLastCtrlTapMs = 0;
  gCtrlSoloHeld = false;
  gCtrlSoloCandidate = false;

  if (!gSubmitting && !gThinking) {
    gAssistantPendingVisible = false;
    setFaceMode(FaceMode::Idle);
  }

  if (!gBleActive) {
    setUiMode(UiMode::Home);
  }
  setStatus("Ready", 1000);
  setKeyDiag("esc:reset");
  appendRuntimeLog("UI", "escape reset", false);
  return changed || gUiMode != UiMode::Home;
}

void handleTypingKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  setKeyDiag(String("key=") + String(static_cast<int>(key)));
  if (key == '`' || key == '~' || key == 27 || hasHidKey(keys, kHidKeyEscape)) {
    handleGlobalEscape();
    return;
  }
  if (keys.ctrl && keys.space) {
    gKeyboardLayout = (gKeyboardLayout == KeyboardLayout::En) ? KeyboardLayout::Ru : KeyboardLayout::En;
    setStatus(String("Layout ") + layoutName(gKeyboardLayout), 900);
    return;
  }
  if (keys.ctrl && lowerKey == 'v') {
    if (gRecording) {
      logf("[VOICE] Ctrl+V stop");
      stopRecordingAndSend();
    } else {
      logf("[VOICE] Ctrl+V start");
      startRecording();
    }
    return;
  }
  if (keys.ctrl && lowerKey == 'd') {
    gDebugOverlayVisible = !gDebugOverlayVisible;
    setStatus(gDebugOverlayVisible ? "Debug overlay on" : "Debug overlay off", 1000);
    setKeyDiag(gDebugOverlayVisible ? "dbg:on" : "dbg:off");
    return;
  }
  if (keys.ctrl && lowerKey == 'l') {
    uint32_t now = millis();
    if (now - gLastLauncherToggleMs < kLauncherToggleDebounceMs) {
      return;
    }
    gLastLauncherToggleMs = now;
    gLauncherVisible = !gLauncherVisible;
    setStatus(gLauncherVisible ? "Launcher" : "Launcher closed", 700);
    return;
  }
  if (keys.tab) {
    if (gLauncherVisible) {
      moveLauncherSubSelection(1);
      const LauncherGroup& group = kLauncherGroups[gLauncherSelection];
      setStatus(group.items[gLauncherSubSelection].label, 500);
    } else {
      gLauncherVisible = true;
      setStatus("Menu", 700);
    }
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
  if (gUiMode == UiMode::Focus && handleFocusKey(key, keys)) {
    return;
  }
  if (gUiMode == UiMode::Library && handleLibraryKey(key, keys)) {
    return;
  }
  if (gUiMode == UiMode::AudioFolders && handleAudioFoldersKey(key, keys)) {
    return;
  }
  if (gUiMode == UiMode::Assets && handleAssetsKey(key)) {
    return;
  }
  if (gUiMode == UiMode::Ota && handleOtaKey(key, keys)) {
    return;
  }
  if (gUiMode == UiMode::Contexts && handleContextsKey(key, keys)) {
    return;
  }
  if (gUiMode == UiMode::Settings && handleSettingsKey(key)) {
    return;
  }
  if (gUiMode == UiMode::Logs && handleLogsKey(key)) {
    return;
  }
  if (gUiMode == UiMode::Home) {
    if (keys.enter || key == '\r' || key == '\n') {
      if (gInputBuffer.isEmpty()) {
        setUiMode(UiMode::ChatFull);
      } else {
        submitInput();
      }
      return;
    }
    if (key >= 32 && key < 127 && gInputBuffer.length() < static_cast<int>(kMaxInputChars)) {
      gInputBuffer += translateInputChar(key);
      return;
    }
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
  if (gRecording) {
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
  static bool pEsc = false;
  static String pWordSignature;

  bool enterDown = keys.enter && !pEnter;
  bool delDown = keys.del && !pDel;
  bool tabDown = keys.tab && !pTab;
  bool spaceDown = keys.space && !pSpace;
  bool upHeld = hasHidKey(keys, 0xDA);
  bool downHeld = hasHidKey(keys, 0xD9);
  bool leftHeld = hasHidKey(keys, 0xD8);
  bool rightHeld = hasHidKey(keys, 0xD7);
  bool escHeld = hasHidKey(keys, kHidKeyEscape);
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
  bool escDown = (escHeld && !pEsc) || (charDown && (curWordChar == '`' || curWordChar == '~'));
  bool tabPrevCombo = keys.tab && (leftDown || upDown ||
                                   (charDown && (curWordChar == ',' || curWordChar == ';' || curWordChar == '<')));
  bool tabNextCombo = keys.tab && (rightDown || downDown ||
                                   (charDown && (curWordChar == '.' || curWordChar == '/' || curWordChar == '>')));
  bool topicPrevCombo = (keys.alt || keys.opt) &&
                        (leftDown || (charDown && (curWordChar == ',' || curWordChar == ';' ||
                                                   curWordChar == '<' || curWordChar == '[')));
  bool topicNextCombo = (keys.alt || keys.opt) &&
                        (rightDown || (charDown && (curWordChar == '.' || curWordChar == '/' ||
                                                    curWordChar == '>' || curWordChar == ']')));

  if (escDown) {
    handleGlobalEscape();
  } else if (topicPrevCombo) {
    bool ok = switchTopicRelative(-1, true);
    if (ok && gUiMode != UiMode::Contexts) {
      setStatus("Topic " + gContexts[gSelectedContext].shortName, 900);
    }
  } else if (topicNextCombo) {
    bool ok = switchTopicRelative(1, true);
    if (ok && gUiMode != UiMode::Contexts) {
      setStatus("Topic " + gContexts[gSelectedContext].shortName, 900);
    }
  } else if (tabPrevCombo) {
    switchLauncherGroupAndApply(-1);
  } else if (tabNextCombo) {
    switchLauncherGroupAndApply(1);
  } else if (tabDown) {
    if (gLauncherVisible) {
      moveLauncherSubSelection(1);
      const LauncherGroup& group = kLauncherGroups[gLauncherSelection];
      setStatus(group.items[gLauncherSubSelection].label, 500);
      setKeyDiag("menu:sub");
    } else {
      gLauncherVisible = true;
      setStatus("Menu", 700);
      setKeyDiag("menu:on");
    }
  } else if (gLauncherVisible && leftDown) {
    moveLauncherSubSelection(-1);
  } else if (gLauncherVisible && rightDown) {
    moveLauncherSubSelection(1);
  } else if (gLauncherVisible && upDown) {
    moveLauncherSelection(-1);
  } else if (gLauncherVisible && downDown) {
    moveLauncherSelection(1);
  } else if (gUiMode == UiMode::Contexts && (leftDown || upDown)) {
    handleTypingKey(',', keys);
  } else if (gUiMode == UiMode::Contexts && (rightDown || downDown)) {
    handleTypingKey('.', keys);
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
  pEsc = escHeld;
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

void updateG0VoiceTrigger() {
  bool pressed = M5.BtnA.isPressed();
  if (gUiMode == UiMode::Ota) {
    if (pressed && !gG0VoiceHeld) {
      gG0VoiceHeld = true;
      pulseButtonPress();
      setStatus("Release G0 to apply OTA", 1200);
      appendRuntimeLog("OTA", "G0 press", false);
      return;
    }
    if (!pressed && gG0VoiceHeld) {
      gG0VoiceHeld = false;
      setStatus("OTA apply requested", 700);
      appendRuntimeLog("OTA", "apply requested by G0", true);
      render();
      applyOtaFromManifest();
      return;
    }
    return;
  }
  if (pressed && !gG0VoiceHeld) {
    gG0VoiceHeld = true;
    gLastCtrlTapMs = 0;
    gCtrlSoloCandidate = false;
    if (!gRecording) {
      logf("[VOICE] G0 hold start");
      startRecording();
    }
    return;
  }
  if (!pressed && gG0VoiceHeld) {
    gG0VoiceHeld = false;
    if (gRecording) {
      logf("[VOICE] G0 hold stop");
      stopRecordingAndSend();
    }
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

void drawHeartGlyph(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  auto& d = gCanvas;
  int16_t r = max<int16_t>(1, size / 3);
  d.fillCircle(cx - r, cy - r / 2, r, color);
  d.fillCircle(cx + r, cy - r / 2, r, color);
  d.fillTriangle(cx - size, cy, cx + size, cy, cx, cy + size, color);
}

void drawSparkGlyph(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  auto& d = gCanvas;
  d.drawLine(cx - size, cy, cx + size, cy, color);
  d.drawLine(cx, cy - size, cx, cy + size, color);
  d.drawLine(cx - size + 1, cy - size + 1, cx + size - 1, cy + size - 1, color);
  d.drawLine(cx - size + 1, cy + size - 1, cx + size - 1, cy - size + 1, color);
}

void drawArcEye(int16_t cx, int16_t cy, int16_t halfW, int16_t arch, uint16_t color,
                bool centerHigh, uint8_t thickness = 2) {
  auto& d = gCanvas;
  if (halfW <= 1) {
    d.drawPixel(cx, cy, color);
    return;
  }
  for (int16_t dx = -halfW; dx <= halfW; ++dx) {
    float ratio = 1.0f - (static_cast<float>(dx * dx) / static_cast<float>(halfW * halfW));
    if (ratio < 0.0f) {
      ratio = 0.0f;
    }
    int16_t curve = static_cast<int16_t>(arch * ratio);
    int16_t py = centerHigh ? (cy - curve) : (cy + curve);
    d.fillRect(cx + dx, py - thickness / 2, 1, max<uint8_t>(1, thickness), color);
  }
}

void drawBrowStroke(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, uint8_t thickness = 2) {
  auto& d = gCanvas;
  int16_t half = max<int16_t>(0, thickness / 2);
  for (int16_t t = -half; t <= half; ++t) {
    d.drawLine(x1, y1 + t, x2, y2 + t, color);
  }
}

void renderOmniEyePair(int16_t leftEyeX, int16_t rightEyeX, int16_t eyeCenterY,
                       int16_t eyeHalfW, int16_t eyeHalfH, uint16_t panelBg) {
  (void)panelBg;
  auto& d = gCanvas;
  EyeEmotion emotion = currentEyeEmotion();
  uint16_t accent = TFT_CYAN;
  float opennessL = 1.0f;
  float opennessR = 1.0f;
  int16_t pupilX = clampValue<int16_t>(static_cast<int16_t>(gEyeLookX), -max<int16_t>(2, eyeHalfW / 3), max<int16_t>(2, eyeHalfW / 3));
  int16_t pupilY = clampValue<int16_t>(static_cast<int16_t>(gEyeLookY), -max<int16_t>(2, eyeHalfH / 3), max<int16_t>(2, eyeHalfH / 3));
  bool happyArc = false;
  bool sleepyArc = false;
  bool angryBrows = false;
  bool sadBrows = false;
  bool lovePupils = false;
  bool surprisedRings = false;
  bool excitedSparks = false;

  if (gFaceMode == FaceMode::Error) {
    accent = TFT_ORANGE;
    d.drawLine(leftEyeX - eyeHalfW, eyeCenterY - eyeHalfH, leftEyeX + eyeHalfW, eyeCenterY + eyeHalfH, accent);
    d.drawLine(leftEyeX - eyeHalfW, eyeCenterY + eyeHalfH, leftEyeX + eyeHalfW, eyeCenterY - eyeHalfH, accent);
    d.drawLine(rightEyeX - eyeHalfW, eyeCenterY - eyeHalfH, rightEyeX + eyeHalfW, eyeCenterY + eyeHalfH, accent);
    d.drawLine(rightEyeX - eyeHalfW, eyeCenterY + eyeHalfH, rightEyeX + eyeHalfW, eyeCenterY - eyeHalfH, accent);
    return;
  }

  switch (emotion) {
    case EyeEmotion::Speaking:
      accent = rgb565_local(90, 255, 185);
      opennessL = 0.92f;
      opennessR = 0.92f;
      pupilX += static_cast<int16_t>(((millis() / 120) % 5) - 2);
      break;
    case EyeEmotion::Happy:
      accent = rgb565_local(130, 255, 220);
      happyArc = true;
      pupilY = 0;
      break;
    case EyeEmotion::Mad:
      accent = rgb565_local(255, 110, 64);
      opennessL = 0.74f;
      opennessR = 0.74f;
      angryBrows = true;
      pupilY -= max<int16_t>(1, eyeHalfH / 6);
      break;
    case EyeEmotion::Sad:
      accent = rgb565_local(110, 195, 255);
      opennessL = 0.76f;
      opennessR = 0.76f;
      sadBrows = true;
      pupilY += max<int16_t>(1, eyeHalfH / 7);
      break;
    case EyeEmotion::Surprised:
      accent = rgb565_local(165, 235, 255);
      opennessL = 1.18f;
      opennessR = 1.18f;
      surprisedRings = true;
      pupilY -= max<int16_t>(1, eyeHalfH / 8);
      break;
    case EyeEmotion::Sleepy:
      accent = rgb565_local(120, 180, 225);
      sleepyArc = true;
      pupilY = 0;
      break;
    case EyeEmotion::Thinking:
      accent = TFT_GOLD;
      opennessL = 0.88f;
      opennessR = 0.88f;
      pupilY -= max<int16_t>(2, eyeHalfH / 4);
      pupilX += static_cast<int16_t>(((millis() / 220) % 3) - 1);
      break;
    case EyeEmotion::Confused:
      accent = rgb565_local(170, 220, 255);
      opennessL = 0.5f;
      opennessR = 1.08f;
      pupilX -= 1;
      break;
    case EyeEmotion::Excited:
      accent = rgb565_local(120, 255, 170);
      opennessL = 1.12f;
      opennessR = 1.12f;
      excitedSparks = true;
      break;
    case EyeEmotion::Love:
      accent = rgb565_local(255, 90, 180);
      opennessL = 1.02f;
      opennessR = 1.02f;
      lovePupils = true;
      break;
    case EyeEmotion::Neutral:
    default:
      accent = TFT_CYAN;
      pupilX += static_cast<int16_t>(((millis() / 520) % 5) - 2);
      break;
  }

  if (gBlinkEndMs != 0) {
    happyArc = false;
    sleepyArc = true;
    lovePupils = false;
    surprisedRings = false;
    excitedSparks = false;
    opennessL = 0.08f;
    opennessR = 0.08f;
  }

  pupilX = clampValue<int16_t>(pupilX, -max<int16_t>(2, eyeHalfW / 3), max<int16_t>(2, eyeHalfW / 3));
  pupilY = clampValue<int16_t>(pupilY, -max<int16_t>(2, eyeHalfH / 3), max<int16_t>(2, eyeHalfH / 3));

  if (happyArc) {
    drawArcEye(leftEyeX, eyeCenterY + eyeHalfH / 5, eyeHalfW, max<int16_t>(2, eyeHalfH / 2), accent, true,
               eyeHalfW > 14 ? 3 : 2);
    drawArcEye(rightEyeX, eyeCenterY + eyeHalfH / 5, eyeHalfW, max<int16_t>(2, eyeHalfH / 2), accent, true,
               eyeHalfW > 14 ? 3 : 2);
  } else if (sleepyArc) {
    drawArcEye(leftEyeX, eyeCenterY - eyeHalfH / 8, eyeHalfW, max<int16_t>(1, eyeHalfH / 5), accent, false,
               eyeHalfW > 14 ? 3 : 2);
    drawArcEye(rightEyeX, eyeCenterY - eyeHalfH / 8, eyeHalfW, max<int16_t>(1, eyeHalfH / 5), accent, false,
               eyeHalfW > 14 ? 3 : 2);
  } else {
    drawEye(leftEyeX, eyeCenterY, eyeHalfW, eyeHalfH, opennessL, accent, pupilX, pupilY);
    drawEye(rightEyeX, eyeCenterY, eyeHalfW, eyeHalfH, opennessR, accent, pupilX, pupilY);
  }

  if (lovePupils) {
    drawHeartGlyph(leftEyeX + pupilX, eyeCenterY + pupilY + max<int16_t>(1, eyeHalfH / 10), max<int16_t>(2, eyeHalfW / 5), accent);
    drawHeartGlyph(rightEyeX + pupilX, eyeCenterY + pupilY + max<int16_t>(1, eyeHalfH / 10), max<int16_t>(2, eyeHalfW / 5), accent);
  }

  if (surprisedRings) {
    d.drawCircle(leftEyeX, eyeCenterY, max<int16_t>(3, eyeHalfH - 2), accent);
    d.drawCircle(rightEyeX, eyeCenterY, max<int16_t>(3, eyeHalfH - 2), accent);
  }

  if (excitedSparks) {
    drawSparkGlyph(leftEyeX - eyeHalfW + 1, eyeCenterY - eyeHalfH - 4, max<int16_t>(1, eyeHalfW / 5), accent);
    drawSparkGlyph(rightEyeX + eyeHalfW - 1, eyeCenterY - eyeHalfH - 4, max<int16_t>(1, eyeHalfW / 5), accent);
  }

  int16_t browLift = max<int16_t>(3, eyeHalfH / 2);
  if (angryBrows) {
    drawBrowStroke(leftEyeX - eyeHalfW - 2, eyeCenterY - browLift - 2, leftEyeX + eyeHalfW / 2, eyeCenterY - eyeHalfH / 2, accent,
                   eyeHalfW > 14 ? 3 : 2);
    drawBrowStroke(rightEyeX + eyeHalfW + 2, eyeCenterY - browLift - 2, rightEyeX - eyeHalfW / 2, eyeCenterY - eyeHalfH / 2, accent,
                   eyeHalfW > 14 ? 3 : 2);
  } else if (sadBrows) {
    drawBrowStroke(leftEyeX - eyeHalfW - 2, eyeCenterY - eyeHalfH / 2, leftEyeX + eyeHalfW / 2, eyeCenterY - browLift - 2, accent,
                   eyeHalfW > 14 ? 3 : 2);
    drawBrowStroke(rightEyeX + eyeHalfW + 2, eyeCenterY - eyeHalfH / 2, rightEyeX - eyeHalfW / 2, eyeCenterY - browLift - 2, accent,
                   eyeHalfW > 14 ? 3 : 2);
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
  d.drawRoundRect(x, y, w, h, 10, 0x31C7);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_CYAN, 0x0841);
  d.setCursor(x + 8, y + 8);
  d.print("Live");

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

  drawHeroSprite(heroX + 3, cellY + 3, 2, currentHeroFrame());

  d.setTextColor(TFT_LIGHTGREY, 0x0841);
  d.setCursor(x + 8, y + h - 18);
  d.print(uiModeName(gUiMode));
  d.setTextColor(TFT_WHITE, 0x0841);
  d.setCursor(x + 8, y + h - 8);
  d.print(gFaceMode == FaceMode::Error ? "error" : eyeEmotionName(currentEyeEmotion()));
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
  d.print(gStorageReady ? gStorageLabel : "fs=off");
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

void drawLauncherIcon(uint8_t group, int16_t x, int16_t y, int16_t s, uint16_t accent, uint16_t bg) {
  auto& d = gCanvas;
  int16_t cx = x + s / 2;
  int16_t cy = y + s / 2;
  d.fillRoundRect(x, y, s, s, 14, bg);
  d.drawRoundRect(x, y, s, s, 14, accent);
  d.fillCircle(cx, cy, s / 3, rgb565_local(10, 14, 26));
  d.drawCircle(cx, cy, s / 3 + 4, accent);
  switch (group) {
    case 0:
      d.drawRoundRect(x + 12, y + 18, s - 24, 28, 8, accent);
      d.fillTriangle(x + 28, y + 46, x + 38, y + 46, x + 28, y + 56, accent);
      d.drawFastHLine(x + 24, y + 30, s - 48, accent);
      d.drawFastHLine(x + 24, y + 38, s - 58, accent);
      break;
    case 1:
      d.drawRoundRect(x + 10, y + 22, 26, 22, 10, accent);
      d.drawRoundRect(x + s - 36, y + 22, 26, 22, 10, accent);
      d.fillCircle(x + 23, y + 33, 5, accent);
      d.fillCircle(x + s - 23, y + 33, 5, accent);
      break;
    case 2:
      d.setFont(&fonts::Font4);
      d.setTextColor(accent, bg);
      d.setCursor(x + 19, y + 24);
      d.print("25");
      d.drawArc(cx, cy, 31, 25, 35, 310, accent);
      d.setFont(&fonts::Font2);
      break;
    case 3:
      for (int i = 0; i < 8; ++i) {
        int h = 8 + ((i * 7 + millis() / 80) % 26);
        d.fillRoundRect(x + 14 + i * 7, y + 52 - h, 4, h, 2, accent);
      }
      d.drawCircle(cx, cy, 18, accent);
      break;
    case 4:
      d.drawRoundRect(x + 17, y + 18, s - 34, 32, 8, accent);
      d.drawLine(x + 28, y + 34, x + 42, y + 46, accent);
      d.drawLine(x + 42, y + 46, x + 57, y + 24, accent);
      d.fillCircle(x + 57, y + 24, 4, accent);
      break;
    case 5:
      d.drawCircle(cx, cy, 25, accent);
      d.drawFastHLine(cx - 25, cy, 50, accent);
      d.drawFastVLine(cx, cy - 25, 50, accent);
      d.fillCircle(cx, cy, 5, accent);
      break;
    case 6:
    default:
      d.drawRoundRect(x + 15, y + 22, s - 30, 28, 6, accent);
      d.drawFastHLine(x + 23, y + 34, s - 46, accent);
      d.drawFastHLine(x + 23, y + 43, s - 56, accent);
      d.fillCircle(x + s - 22, y + 36, 5, accent);
      break;
  }
}

void renderLauncherOverlay() {
  if (!gLauncherVisible) {
    return;
  }
  auto& d = gCanvas;
  gLauncherSelection = clampValue<int>(gLauncherSelection, 0, kLauncherGroupCount - 1);
  const LauncherGroup& group = kLauncherGroups[gLauncherSelection];
  gLauncherSubSelection = clampValue<int>(gLauncherSubSelection, 0, group.count - 1);

  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(5, 13, 18);
  const uint16_t rowBg = rgb565_local(8, 17, 23);
  const uint16_t selectedBg = rgb565_local(14, 31, 39);
  const uint16_t line = rgb565_local(42, 55, 63);
  const uint16_t muted = rgb565_local(128, 145, 155);

  d.fillRoundRect(5, 5, 230, 125, 8, bg);
  d.drawRoundRect(5, 5, 230, 125, 8, rgb565_local(48, 54, 61));
  d.fillRoundRect(10, 10, 220, 18, 5, panel);

  d.setFont(&fonts::Font2);
  d.setTextColor(group.accent, panel);
  d.setCursor(17, 14);
  d.print(group.title);
  String page = String(gLauncherSelection + 1) + "/" + String(kLauncherGroupCount);
  d.setTextColor(muted, panel);
  d.setCursor(220 - d.textWidth(page), 14);
  d.print(page);

  const int16_t railX = 10;
  const int16_t railY = 33;
  const int16_t railW = 78;
  const int16_t railH = 13;
  const int16_t railGap = 3;

  for (int idx = 0; idx < kLauncherGroupCount; ++idx) {
    const LauncherGroup& item = kLauncherGroups[idx];
    const bool isSelected = idx == gLauncherSelection;
    const int16_t y = railY + idx * (railH + railGap);
    const uint16_t fill = isSelected ? selectedBg : rowBg;
    d.fillRoundRect(railX, y, railW, railH, 4, fill);
    d.drawRoundRect(railX, y, railW, railH, 4, isSelected ? item.accent : line);

    d.setTextColor(isSelected ? TFT_WHITE : muted, fill);
    d.setCursor(railX + 5, y + 1);
    d.print(String(idx + 1));

    d.setTextColor(isSelected ? item.accent : TFT_LIGHTGREY, fill);
    d.setCursor(railX + 19, y + 1);
    d.print(trimPreview(item.title, 8));

    if (isSelected) {
      d.fillCircle(railX + railW - 7, y + railH / 2, 3, item.accent);
    }
  }

  d.fillRoundRect(94, 33, 136, 78, 7, panel);
  d.drawRoundRect(94, 33, 136, 78, 7, group.accent);

  d.setTextColor(group.accent, panel);
  d.setCursor(102, 40);
  d.print(group.glyph);
  d.setTextColor(muted, panel);
  d.setCursor(102, 54);
  d.print(trimPreview(group.hint, 18));

  for (int i = 0; i < group.count; ++i) {
    const int16_t y = 68 + i * 10;
    const bool selected = i == gLauncherSubSelection;
    const uint16_t fill = selected ? rgb565_local(18, 39, 48) : panel;
    if (selected) {
      d.fillRoundRect(100, y - 1, 124, 10, 3, fill);
    }
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, fill);
    d.setCursor(104, y);
    d.print(selected ? "> " : "  ");
    d.print(trimPreview(group.items[i].label, 16));
  }

  d.fillRoundRect(10, 116, 220, 10, 4, panel);
  drawTinyFooter(panel, muted, "Up/Down app  Left/Right screen  Ent", 15, 118, 210);
}

void renderBatteryUi() {
  updateBatterySnapshot();
  auto& d = gCanvas;
  d.fillScreen(0x0210);

  d.fillRoundRect(8, 8, 224, 118, 16, 0x0841);
  d.drawRoundRect(8, 8, 224, 118, 16, 0x31C7);

  int level = gBatterySnapshot.level < 0 ? 0 : gBatterySnapshot.level;
  level = clampValue(level, 0, 100);
  int barX = 22;
  int barY = 28;
  int barW = 140;
  int barH = 28;
  int fillW = (barW - 8) * level / 100;
  uint16_t barColor = level >= 60 ? rgb565_local(90, 255, 150)
                     : level >= 25 ? TFT_GOLD
                                   : rgb565_local(255, 110, 80);

  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_YELLOW, 0x0841);
  d.setCursor(22, 16);
  d.print("Battery");

  d.drawRoundRect(barX, barY, barW, barH, 8, TFT_LIGHTGREY);
  d.fillRect(barX + barW, barY + 8, 6, 12, TFT_LIGHTGREY);
  d.fillRoundRect(barX + 4, barY + 4, max(0, fillW), barH - 8, 5, barColor);

  d.setFont(&fonts::Font7);
  d.setTextColor(TFT_WHITE, 0x0841);
  d.setCursor(166, 22);
  d.print(String(level));
  d.setFont(&fonts::Font4);
  d.setCursor(208, 36);
  d.print("%");

  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_CYAN, 0x0841);
  d.setCursor(22, 70);
  d.print("State");
  d.setCursor(92, 70);
  d.setTextColor(TFT_WHITE, 0x0841);
  d.print(batteryChargeLabel(gBatterySnapshot.charging));

  d.setTextColor(TFT_CYAN, 0x0841);
  d.setCursor(22, 84);
  d.print("Voltage");
  d.setCursor(92, 84);
  d.setTextColor(TFT_WHITE, 0x0841);
  if (gBatterySnapshot.voltageMv > 0) {
    d.print(String(gBatterySnapshot.voltageMv / 1000.0f, 2) + "V");
  } else {
    d.print("--");
  }

  d.setTextColor(TFT_CYAN, 0x0841);
  d.setCursor(22, 98);
  d.print("Current");
  d.setCursor(92, 98);
  d.setTextColor(TFT_WHITE, 0x0841);
  d.print(String(gBatterySnapshot.currentMa) + "mA");

  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_CYAN, 0x0841);
  d.setCursor(22, 113);
  d.print("Storage");
  d.setCursor(92, 113);
  d.setTextColor(TFT_WHITE, 0x0841);
  if (gStorageReady) {
    uint64_t totalKb = activeStorageTotalBytes() / 1024ULL;
    uint64_t usedKb = activeStorageUsedBytes() / 1024ULL;
    String unit = "KB";
    uint64_t totalShown = totalKb;
    uint64_t usedShown = usedKb;
    if (totalKb > 1024ULL * 10ULL) {
      unit = "MB";
      totalShown = totalKb / 1024ULL;
      usedShown = usedKb / 1024ULL;
    }
    d.print(gStorageLabel + " " + String(static_cast<unsigned long>(usedShown)) + "/" +
            String(static_cast<unsigned long>(totalShown)) + unit);
  } else {
    d.print("off");
  }
  d.setFont(&fonts::Font2);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void drawCardHeader(const String& title, uint16_t accent) {
  auto& d = gCanvas;
  d.fillScreen(0x0208);
  d.fillRoundRect(6, 6, 228, 123, 14, 0x0841);
  d.drawRoundRect(6, 6, 228, 123, 14, accent);
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, 0x0841);
  d.setCursor(14, 14);
  d.print(title);
}

void renderSdRequiredBanner(const String& title) {
  drawCardHeader(title, rgb565_local(255, 180, 60));
  auto& d = gCanvas;
  d.setFont(&fonts::Font2);
  d.fillRoundRect(18, 34, 204, 72, 10, rgb565_local(26, 18, 6));
  d.drawRoundRect(18, 34, 204, 72, 10, rgb565_local(255, 180, 60));
  d.setTextColor(TFT_ORANGE, rgb565_local(26, 18, 6));
  d.setCursor(32, 45);
  d.print("SD card required");
  d.setTextColor(TFT_WHITE, rgb565_local(26, 18, 6));
  d.setCursor(32, 62);
  d.print("Insert FAT32 card");
  d.setCursor(32, 76);
  d.print("then reboot device");
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "Pulse/chat still work without SD", 18, 116, 204);
}

String compactBytes(uint32_t bytes) {
  if (bytes >= 1024UL * 1024UL) {
    return String(bytes / (1024UL * 1024UL)) + "MB";
  }
  if (bytes >= 1024UL) {
    return String(bytes / 1024UL) + "KB";
  }
  return String(bytes) + "B";
}

String marqueeText(const String& text, int32_t pixelLimit, uint16_t stepMs = 240) {
  if (gCanvas.textWidth(text) <= pixelLimit) {
    return text;
  }
  String padded = text + "    " + text;
  int len = text.length() + 4;
  int offset = len > 0 ? static_cast<int>((millis() / stepMs) % len) : 0;
  String out = padded.substring(offset);
  while (out.length() > 1 && gCanvas.textWidth(out) > pixelLimit) {
    out.remove(out.length() - 1);
  }
  return out;
}

String fitCurrentFontToWidth(String text, int32_t pixelLimit) {
  if (gCanvas.textWidth(text) <= pixelLimit) {
    return text;
  }
  while (text.length() > 1 && gCanvas.textWidth(text + "..") > pixelLimit) {
    text.remove(text.length() - 1);
  }
  return text + "..";
}

void drawTinyFooter(uint16_t bg, uint16_t fg, const String& text, int16_t x, int16_t y, int16_t w) {
  auto& d = gCanvas;
  d.fillRect(max<int16_t>(0, x - 2), max<int16_t>(0, y - 2), min<int16_t>(240, w + 4), 10, bg);
  d.setFont(&fonts::Font0);
  d.setTextColor(fg, bg);
  d.setCursor(x, y);
  d.print(fitCurrentFontToWidth(text, w));
  d.setFont(&fonts::Font2);
}

void renderFocusUi() {
  auto& d = gCanvas;
  uint16_t bg = 0x0000;
  uint16_t panel = 0x0841;
  uint16_t accent = 0x07FF;
  uint16_t soft = 0x39E7;
  if (gFocus.state == FocusState::ShortBreak || gFocus.state == FocusState::LongBreak) {
    bg = 0x0208;
    panel = 0x0320;
    accent = 0x07E0;
    soft = 0x2589;
  } else if (gFocus.state == FocusState::Reflect) {
    bg = 0x0808;
    panel = 0x180C;
    accent = 0xFD20;
    soft = 0x7BEF;
  } else if (gFocus.state == FocusState::Paused || gFocus.state == FocusState::Stopped) {
    bg = 0x0008;
    panel = 0x1082;
    accent = 0x867F;
    soft = 0x4208;
  }

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 12, panel);
  d.drawRoundRect(6, 5, 228, 124, 12, accent);

  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(14, 12);
  d.print(focusStateName(gFocus.state));
  d.setTextColor(TFT_LIGHTGREY, panel);
  d.setCursor(88, 12);
  d.print("cycle ");
  d.print(gFocus.cycle);
  d.print("/");
  d.print(gFocusSettings.cyclesPerRound);
  d.setCursor(171, 12);
  d.print(String(gFocus.completedToday) + " done");

  String timeText = focusTimeString(gFocus.remainingSec);
  d.setFont(&fonts::Font7);
  d.setTextColor(accent, panel);
  int timeWidth = d.textWidth(timeText);
  d.setCursor(max<int>(8, (240 - timeWidth) / 2), 31);
  d.print(timeText);

  uint16_t pulse = (millis() / 500) % 2 ? accent : soft;
  int avatarX = 204;
  int avatarY = 83;
  d.drawCircle(avatarX, avatarY, 13, soft);
  d.drawCircle(avatarX, avatarY, 8, pulse);
  d.fillCircle(avatarX - 4, avatarY - 2, 2, accent);
  d.fillCircle(avatarX + 4, avatarY - 2, 2, accent);
  if (gFocus.state == FocusState::Focus) {
    d.drawLine(avatarX - 5, avatarY + 5, avatarX + 5, avatarY + 5, accent);
  } else if (gFocus.state == FocusState::Reflect) {
    d.drawLine(avatarX - 5, avatarY + 5, avatarX, avatarY + 8, accent);
    d.drawLine(avatarX, avatarY + 8, avatarX + 5, avatarY + 5, accent);
  } else {
    d.drawLine(avatarX - 4, avatarY + 6, avatarX + 4, avatarY + 6, accent);
  }

  uint32_t elapsed = gFocus.sessionTotalSec > gFocus.remainingSec ? gFocus.sessionTotalSec - gFocus.remainingSec : 0;
  int progress = gFocus.sessionTotalSec ? static_cast<int>((elapsed * 208ULL) / gFocus.sessionTotalSec) : 0;
  progress = clampValue<int>(progress, 0, 208);
  d.drawRoundRect(16, 96, 208, 9, 5, soft);
  d.fillRoundRect(17, 97, max<int>(1, progress), 7, 4, accent);

  if (gFocus.helpVisible) {
    drawTinyFooter(panel, TFT_LIGHTGREY, "Ent pause  R reset  S +5  N next", 16, 114, 208);
  } else {
    String footer = String("Ent start  M ") + (gFocusSettings.metronome ? "metro" : "quiet") +
                    "  " + String(gFocusSettings.bpm) + "bpm  H setup";
    drawTinyFooter(panel, TFT_LIGHTGREY, footer, 16, 114, 208);
  }

  if (gFocus.helpVisible) {
    d.fillRoundRect(18, 28, 204, 86, 10, 0x0000);
    d.drawRoundRect(18, 28, 204, 86, 10, accent);
    d.setTextColor(TFT_WHITE, 0x0000);
    d.setCursor(28, 38);
    d.print("Focus setup");
    d.setTextColor(TFT_LIGHTGREY, 0x0000);
    d.setCursor(28, 54);
    d.print("Q/W focus ");
    d.print(gFocusSettings.focusSec / 60);
    d.print("m");
    d.setCursor(28, 68);
    d.print("A auto ");
    d.print(gFocusSettings.autoStart ? "on" : "off");
    d.print("  M ");
    d.print(gFocusSettings.metronome ? "metro" : "quiet");
    d.setCursor(28, 82);
    d.print("Z/X bpm ");
    d.print(gFocusSettings.bpm);
    d.print("  S +5m");
    d.setCursor(28, 96);
    d.print("Enter pause  R reset  N next");
  }

  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderLibraryUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Audio Library");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(6, 16, 22);
  const uint16_t line = rgb565_local(34, 78, 88);
  const uint16_t accent = rgb565_local(88, 240, 141);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t amber = rgb565_local(246, 194, 74);
  const uint16_t muted = rgb565_local(130, 145, 155);
  d.fillScreen(bg);
  d.fillRoundRect(4, 4, 232, 127, 8, panel);
  d.drawRoundRect(4, 4, 232, 127, 8, rgb565_local(48, 54, 61));

  int folderCount = audioAssetCountForCurrentFolder();
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(13, 12);
  d.print("AUDIO");
  String folder = audioFolderLabel(gAudioFolderFilter);
  String folderLabel = trimPreview(folder, 14);
  d.setTextColor(muted, panel);
  d.setCursor(226 - d.textWidth(folderLabel), 12);
  d.print(folderLabel);

  if (gAudioAssets.empty() || folderCount == 0) {
    d.drawRoundRect(20, 38, 54, 54, 27, line);
    d.drawArc(47, 65, 21, 15, 220, 35, accent);
    d.setTextColor(accent, panel);
    d.setCursor(88, 44);
    d.print("No WAV assets");
    d.setTextColor(muted, panel);
    d.setCursor(88, 60);
    d.print("Put files in /audio");
    d.setCursor(88, 76);
    d.print("or /pomodoro/audio");
    d.setCursor(88, 92);
    d.print("Press R to rescan");
  } else {
    clampSelectedAudioAssetToFolder();
    const AudioAsset& asset = gAudioAssets[gSelectedAudioAsset];
    int rank = max<int>(0, audioAssetRankInCurrentFolder(gSelectedAudioAsset));
    int cx = 48;
    int cy = 66;
    d.fillCircle(cx, cy, 35, bg);
    d.drawCircle(cx, cy, 35, line);
    d.drawCircle(cx, cy, 26, rgb565_local(14, 41, 48));
    d.drawArc(cx, cy, 29, 23, 210, 55, gPlaybackActive ? accent : cyan);
    d.drawArc(cx, cy, 18, 13, 25, 295, amber);
    d.fillCircle(cx, cy, 9 + ((gPlaybackActive && ((millis() / 260) % 2)) ? 1 : 0), TFT_WHITE);
    if (gPlaybackActive) {
      for (int i = 0; i < 9; ++i) {
        int h = 4 + ((i * 9 + millis() / 70) % 18);
        d.fillRoundRect(17 + i * 7, 112 - h, 4, h, 2, i % 2 ? accent : cyan);
      }
    }

    d.setFont(&fonts::Font2);
    d.setTextColor(TFT_WHITE, panel);
    d.setCursor(92, 34);
    d.print(marqueeText(asset.title, 126, 260));
    d.setTextColor(muted, panel);
    d.setCursor(92, 49);
    d.print(trimPreview(asset.category, 13));
    d.setTextColor(accent, panel);
    String countText = String(rank + 1) + "/" + String(folderCount);
    d.setCursor(224 - d.textWidth(countText), 49);
    d.print(countText);

    d.drawRoundRect(92, 64, 132, 8, 4, line);
    int progress = gPlaybackActive ? 10 + ((millis() / 140) % 100) : 18;
    d.fillRoundRect(95, 67, min<int>(126, progress), 3, 2, gPlaybackActive ? accent : muted);

    d.fillRoundRect(92, 82, 33, 22, 6, bg);
    d.drawRoundRect(92, 82, 33, 22, 6, line);
    d.fillRoundRect(132, 79, 48, 27, 8, bg);
    d.drawRoundRect(132, 79, 48, 27, 8, gPlaybackActive ? accent : cyan);
    d.fillRoundRect(187, 82, 33, 22, 6, bg);
    d.drawRoundRect(187, 82, 33, 22, 6, line);
    d.setTextColor(muted, bg);
    d.setCursor(102, 89);
    d.print("<<");
    d.setTextColor(gPlaybackActive ? accent : cyan, bg);
    d.setCursor(148, 88);
    d.print(gPlaybackActive ? "PAU" : "PLAY");
    d.setTextColor(muted, bg);
    d.setCursor(197, 89);
    d.print(">>");
  }
  drawTinyFooter(panel, muted, "< > track  Ent play  F folder  R scan", 14, 118, 210);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderAudioFoldersUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Audio Folders");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(6, 16, 22);
  const uint16_t rowBg = rgb565_local(7, 17, 22);
  const uint16_t line = rgb565_local(39, 51, 58);
  const uint16_t accent = rgb565_local(34, 209, 189);
  const uint16_t selected = rgb565_local(12, 32, 34);
  const uint16_t muted = rgb565_local(130, 145, 155);
  d.fillScreen(bg);
  d.fillRoundRect(5, 5, 230, 125, 6, panel);
  d.drawRoundRect(5, 5, 230, 125, 6, rgb565_local(48, 54, 61));

  if (gAudioFolders.empty()) {
    rebuildAudioFolders();
  }
  gSelectedAudioFolder = clampValue<int>(gSelectedAudioFolder, 0, max<int>(0, static_cast<int>(gAudioFolders.size()) - 1));

  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE, panel);
  d.setCursor(15, 14);
  d.print("Meditation folders");
  d.setTextColor(accent, panel);
  d.setCursor(196, 14);
  d.print(gSdReady ? "SD" : "FS");

  d.fillCircle(44, 68, 36, bg);
  d.drawCircle(44, 68, 36, rgb565_local(23, 77, 90));
  d.drawArc(44, 68, 28, 23, 220, 38, accent);
  d.drawArc(44, 68, 17, 13, 35, 300, rgb565_local(47, 227, 255));
  d.fillCircle(44, 68, 9, TFT_WHITE);
  d.setTextColor(bg, TFT_WHITE);
  d.setCursor(38, 64);
  d.print("DIR");

  int first = gSelectedAudioFolder - 1;
  first = clampValue<int>(first, 0, max<int>(0, static_cast<int>(gAudioFolders.size()) - 4));
  for (int r = 0; r < 4; ++r) {
    int idx = first + r;
    if (idx >= static_cast<int>(gAudioFolders.size())) {
      break;
    }
    const AudioFolder& folder = gAudioFolders[idx];
    bool isSelected = idx == gSelectedAudioFolder;
    int y = 35 + r * 20;
    uint16_t fill = isSelected ? selected : rowBg;
    d.fillRoundRect(84, y, 137, 17, 6, fill);
    d.drawRoundRect(84, y, 137, 17, 6, isSelected ? accent : line);
    d.setTextColor(isSelected ? TFT_WHITE : TFT_LIGHTGREY, fill);
    d.setCursor(92, y + 3);
    d.print(trimPreview(folder.label, 13));
    String count = String(folder.count);
    d.setTextColor(isSelected ? accent : muted, fill);
    d.setCursor(214 - d.textWidth(count), y + 3);
    d.print(count);
  }

  drawTinyFooter(panel, muted, "< > folder  Ent open  R scan", 15, 118, 210);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderAssetsUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Asset Sync");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  drawCardHeader("Asset Sync", 0x07E0);
  auto& d = gCanvas;
  d.setTextColor(TFT_WHITE, 0x0841);
  d.setCursor(16, 34);
  d.print("manifest ");
  d.print(gAssetManifest.present ? (gAssetManifest.parsed ? "ok" : "bad") : "missing");
  d.setCursor(16, 48);
  d.print("version ");
  d.print(gAssetManifest.version.isEmpty() ? "-" : gAssetManifest.version.substring(0, 16));
  d.setCursor(16, 62);
  d.print("files ");
  d.print(gAssetManifest.fileCount);
  d.print("  ");
  d.print(compactBytes(gAssetManifest.totalBytes));
  d.setCursor(16, 76);
  d.print("storage ");
  d.print(gStorageReady ? gStorageLabel : "off");
  d.setCursor(16, 90);
  d.print("audio index ");
  fs::FS* fs = activeVoiceFs();
  d.print((fs && fsExists(*fs, kAudioIndexPath)) ? "yes" : "no");
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "F fetch  S sync  R load", 16, 116, 208);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderOtaUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Firmware OTA");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  drawCardHeader("Firmware OTA", 0xF800);
  auto& d = gCanvas;
  d.setTextColor(TFT_WHITE, 0x0841);
  d.setCursor(16, 34);
  d.print("version ");
  d.print(kAppVersion);
  d.setCursor(16, 48);
  d.print("git ");
  d.print(String(kBuildGitSha).substring(0, 10));
  d.setCursor(16, 62);
  d.print("manifest ");
  d.print(gOtaManifest.present ? (gOtaManifest.parsed ? "ok" : "bad") : "missing");
  d.setCursor(16, 76);
  d.print("candidate ");
  d.print(gOtaManifest.version.isEmpty() ? "-" : gOtaManifest.version.substring(0, 14));
  d.setCursor(16, 90);
  d.print("policy ");
  d.print(gOtaManifest.confirmRequired ? "manual confirm" : "auto allowed");
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "F fetch  R load  A/G0 apply", 16, 116, 208);
  renderLauncherOverlay();
  renderDebugOverlay();
}

String settingLabel(uint8_t index) {
  switch (index) {
    case 0:
      return "Focus";
    case 1:
      return "Short break";
    case 2:
      return "Long break";
    case 3:
      return "Cycles";
    case 4:
      return "Auto focus";
    case 5:
      return "Auto OTA";
    case 6:
      return "BPM";
    case 7:
      return "OTA battery";
    case 8:
      return "Audio lang";
    case 9:
      return "Home face";
    default:
      return "-";
  }
}

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
  int start = max<int>(0, min<int>(gSelectedSetting - 2, kSettingsItemCount - 5));
  for (int i = 0; i < 5; ++i) {
    int idx = start + i;
    if (idx >= kSettingsItemCount) {
      break;
    }
    int y = 35 + i * 15;
    bool selected = idx == gSelectedSetting;
    uint16_t bg = selected ? 0x4208 : 0x0841;
    d.fillRoundRect(14, y - 2, 212, 14, 4, bg);
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, bg);
    d.setCursor(18, y);
    d.print(settingLabel(idx));
    String value = settingValue(idx);
    int w = d.textWidth(value);
    d.setCursor(max<int>(120, 222 - w), y);
    d.print(value);
  }
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "< > pick  +/- edit  Space toggle", 16, 116, 208);
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
  d.drawString(ctx.shortName, cardX + 34, 59);
  d.setTextDatum(top_left);
  d.setFont(&fonts::efontCN_12);

  d.setTextColor(TFT_WHITE, panel);
  String title = marqueeText(normalizeTopicTitleForDisplay(ctx.label), 146, 260);
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
      if (shown >= 5) {
        break;
      }
      uint16_t roleColor = line.role == "assistant" ? green : (line.voice ? amber : cyan);
      d.setTextColor(roleColor, rgb565_local(4, 10, 16));
      d.setCursor(18, y);
      String prefix = line.role == "assistant" ? "AI" : (line.voice ? "VO" : "ME");
      d.print(prefix);
      d.setTextColor(TFT_LIGHTGREY, rgb565_local(4, 10, 16));
      d.setCursor(34, y);
      d.print(trimPreview(line.text, 34));
      y += 6;
      shown++;
    }
    if (gContextPreview.size() > 5) {
      d.setTextColor(muted, rgb565_local(4, 10, 16));
      d.setFont(&fonts::Font0);
      d.setCursor(205, 115);
      d.print("+");
      d.print(static_cast<int>(gContextPreview.size()) - 5);
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
  d.setTextColor(TFT_WHITE, 0x0841);
  d.setCursor(16, 34);
  d.print(WiFi.isConnected() ? WiFi.localIP().toString() : "wifi off");
  d.print(" ");
  d.print(gStorageLabel);
  int visible = 5;
  int maxOffset = max<int>(0, static_cast<int>(gRuntimeLogs.size()) - visible);
  gLogScrollOffset = clampValue<int>(gLogScrollOffset, 0, maxOffset);
  int start = max<int>(0, static_cast<int>(gRuntimeLogs.size()) - visible - gLogScrollOffset);
  for (int i = 0; i < visible; ++i) {
    int idx = start + i;
    if (idx >= static_cast<int>(gRuntimeLogs.size())) {
      break;
    }
    d.setTextColor(TFT_LIGHTGREY, 0x0841);
    d.setCursor(16, 50 + i * 12);
    d.print(trimPreview(gRuntimeLogs[idx], 34));
  }
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "< > scroll  R reload  U upload", 16, 116, 208);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderTopicOverlayIfVisible() {
  if (millis() > gTopicOverlayUntilMs || gContexts.empty()) {
    return;
  }
  auto& d = gCanvas;
  gSelectedContext = clampValue<int>(gSelectedContext, 0, static_cast<int>(gContexts.size()) - 1);
  const ConversationContext& ctx = gContexts[gSelectedContext];

  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(5, 13, 18);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t violet = rgb565_local(157, 102, 255);
  const uint16_t muted = rgb565_local(128, 145, 155);
  const uint16_t amber = rgb565_local(246, 194, 74);

  uint32_t now = millis();
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
  d.print(fitCurrentFontToWidth(ctx.shortName, 30));

  d.setFont(&fonts::efontCN_12);
  d.setTextColor(cyan, bg);
  d.setCursor(60 + dx, 9);
  d.print(fitCurrentFontToWidth(normalizeTopicTitleForDisplay(ctx.label), 132));

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

void renderHeroUi() {
  auto& d = gCanvas;
  d.fillScreen(0x0210);

  int16_t arenaX = 7;
  int16_t arenaY = 5;
  int16_t arenaW = 226;
  int16_t arenaH = 125;
  d.fillRoundRect(arenaX, arenaY, arenaW, arenaH, 16, 0x0841);
  d.drawRoundRect(arenaX, arenaY, arenaW, arenaH, 16, 0x31C7);

  for (int16_t gx = arenaX + 12; gx < arenaX + arenaW - 8; gx += 24) {
    d.drawFastVLine(gx, arenaY + 14, arenaH - 28, 0x18C3);
  }
  for (int16_t gy = arenaY + 14; gy < arenaY + arenaH - 8; gy += 18) {
    d.drawFastHLine(arenaX + 12, gy, arenaW - 24, 0x18C3);
  }

  int scale = 5;
  int16_t heroW = kHeroSpriteW * scale;
  int16_t heroH = kHeroSpriteH * scale;
  int16_t margin = 8;
  int16_t rangeX = max<int16_t>(0, arenaW - heroW - margin * 2);
  int16_t rangeY = max<int16_t>(0, arenaH - heroH - margin * 2);
  int16_t heroOffsetX = static_cast<int16_t>((gEyeLookX / 18.0f) * (rangeX / 2.0f));
  int16_t heroOffsetY = static_cast<int16_t>((gEyeLookY / 14.0f) * (rangeY / 2.0f));
  int16_t bob = abs(static_cast<int>((millis() / 170) % 10) - 5);
  int16_t heroX = arenaX + margin + rangeX / 2 - heroW / 2 + heroOffsetX;
  int16_t heroY = arenaY + margin + rangeY / 2 - heroH / 2 + heroOffsetY + bob / 2;
  heroX = clampValue<int16_t>(heroX, arenaX + margin, arenaX + arenaW - margin - heroW);
  heroY = clampValue<int16_t>(heroY, arenaY + margin, arenaY + arenaH - margin - heroH);

  int16_t shadowW = heroW - 18;
  int16_t shadowX = heroX + (heroW - shadowW) / 2;
  int16_t shadowY = heroY + heroH - 6;
  d.fillRoundRect(shadowX, shadowY, shadowW, 10, 5, 0x0000);
  drawHeroSprite(heroX, heroY, scale, currentHeroFrame());

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
  d.fillCircle(70, 58, 86, rgb565_local(4, 16, 20));
  d.drawRoundRect(0, 0, 240, 135, 6, rgb565_local(48, 54, 61));

  uint32_t nowMs = millis();
  float t = nowMs / 1000.0f;
  float breathe = sinf(t * 2.2f) * 0.9f;
  float spin = t * 0.55f;
  if (state == AssistantPulseState::Thinking) {
    spin = t * 1.9f;
  } else if (state == AssistantPulseState::Recording) {
    spin = t * 1.25f;
  }
  float level = max<uint8_t>(18, gVoiceLevel8) / 255.0f;
  if (!gRecording && gVoiceLevel8 > 28) {
    gVoiceLevel8 = static_cast<uint8_t>(max<int>(28, static_cast<int>(gVoiceLevel8) - 2));
  }

  const int16_t cx = 58;
  const int16_t cy = 68;
  d.fillCircle(cx, cy, 50 + static_cast<int16_t>(breathe), rgb565_local(4, 16, 21));
  d.drawCircle(cx, cy, 49 + static_cast<int16_t>(breathe), c3);
  drawAssistantPulseArc(cx, cy, 43, 36, -2.25f + spin, 1.00f + spin, c1);
  drawAssistantPulseArc(cx, cy, 29, 23, 0.58f - spin * 1.08f, 4.54f - spin * 1.08f, c2);

  if (state == AssistantPulseState::Recording) {
    int16_t pulseR = 18 + static_cast<int16_t>(level * 14.0f + sinf(t * 12.0f));
    d.fillCircle(cx, cy, pulseR, c1);
    d.fillCircle(cx, cy, 9, fg);
  } else if (state == AssistantPulseState::Thinking) {
    drawAssistantPulseArc(cx, cy, 19, 14, 0.2f + spin * 1.7f, 5.3f + spin * 1.7f, fg);
    d.fillCircle(cx + static_cast<int16_t>(cosf(spin * 2.4f) * 27.0f),
                 cy + static_cast<int16_t>(sinf(spin * 2.4f) * 21.0f), 4, c1);
    d.fillCircle(cx + static_cast<int16_t>(cosf(spin * 2.0f + 2.1f) * 26.0f),
                 cy + static_cast<int16_t>(sinf(spin * 2.0f + 2.1f) * 23.0f), 3, c2);
  } else if (state == AssistantPulseState::Playback) {
    d.fillCircle(cx, cy, 16, fg);
    d.fillTriangle(cx - 4, cy - 8, cx - 4, cy + 8, cx + 9, cy, bg);
  } else {
    d.fillCircle(cx, cy, 14 + static_cast<int16_t>(sinf(t * 2.2f) * 1.4f), fg);
  }

  String hhmm = "--:--";
  time_t now = time(nullptr);
  if (now > 100000) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[6];
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    hhmm = buf;
  }
  if (state == AssistantPulseState::Recording) {
    uint32_t elapsed = (millis() - gRecordStartMs) / 1000;
    char rec[6];
    snprintf(rec, sizeof(rec), "%02u:%02u", static_cast<unsigned>(elapsed / 60),
             static_cast<unsigned>(elapsed % 60));
    hhmm = rec;
  }

  d.setFont(&fonts::Font7);
  d.setTextColor(fg, bg);
  int tw = d.textWidth(hhmm);
  d.setCursor(171 - tw / 2, state == AssistantPulseState::Recording ? 18 : 20);
  d.print(hhmm);

  int16_t waveX = state == AssistantPulseState::Ready ? 121 : 117;
  int16_t waveY = state == AssistantPulseState::Ready ? 88 : 86;
  int bars = state == AssistantPulseState::Ready ? 15 : 16;
  for (int i = 0; i < bars; ++i) {
    int16_t bx = waveX + i * 7;
    if (state == AssistantPulseState::Thinking) {
      int16_t dotY = waveY + static_cast<int16_t>(sinf(t * 5.0f + i * 0.7f) * 6.0f);
      d.fillCircle(bx, dotY, (i % 3 == 0) ? 3 : 2, (i % 2) ? c1 : c2);
      continue;
    }
    float amp = state == AssistantPulseState::Recording ? 1.05f :
                state == AssistantPulseState::Playback ? 0.90f : 0.68f;
    int16_t h = static_cast<int16_t>((8.0f + fabsf(sinf(t * 3.6f + i * 0.74f)) * 22.0f +
                                      fabsf(cosf(t * 1.7f + i * 0.31f)) * 7.0f) *
                                     amp * (0.65f + level * 0.75f));
    h = clampValue<int16_t>(h, 3, 42);
    d.fillRoundRect(bx - 1, waveY - h / 2, 3, h, 2, (i % 2) ? c1 : c2);
  }

  if (state == AssistantPulseState::Playback) {
    d.fillRoundRect(117, 110, 88, 2, 1, rgb565_local(23, 61, 69));
    int16_t progress = 117 + static_cast<int16_t>(88.0f * fmodf(t * 0.17f, 1.0f));
    d.fillRoundRect(117, 110, max<int16_t>(1, progress - 117), 3, 1, c1);
  }
  if (state == AssistantPulseState::Recording) {
    d.fillCircle(133, 112, 4 + static_cast<int16_t>(sinf(t * 8.0f)), c1);
  }
  drawAssistantPulseButton(154, 105, 62, 21, label, c1, millis() < gAssistantPulsePressUntilMs);
  if (gAssistantPendingVisible && state == AssistantPulseState::Thinking) {
    d.setFont(&fonts::Font2);
    d.setTextColor(amber, bg);
    d.setCursor(123, 13);
    d.print("agent thinking");
  }
  renderAssistantPulseChatHint();
  renderTopicOverlayIfVisible();
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderClockUi() {
  renderAssistantPulseUi();
}

void renderVoicePlayerUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Voice Notes");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(74, 78, 94);
  const uint16_t panel = TFT_BLACK;
  const uint16_t edge = rgb565_local(190, 196, 220);
  const uint16_t dim = rgb565_local(86, 94, 112);
  const uint16_t orange = TFT_ORANGE;
  d.fillScreen(bg);
  d.setFont(&fonts::Font2);
  d.drawFastHLine(0, 0, 240, edge);
  d.drawFastHLine(0, 134, 240, edge);
  d.fillRect(4, 8, 130, 122, panel);
  d.drawRect(3, 7, 132, 124, edge);
  d.fillRect(138, 0, 4, 135, TFT_BLACK);
  d.drawFastVLine(143, 0, 135, edge);

  d.setTextColor(orange, bg);
  d.setCursor(8, 0);
  d.print("VOICE");
  d.setTextColor(TFT_LIGHTGREY, bg);
  d.setCursor(55, 0);
  d.print(gStorageLabel);
  d.setCursor(194, 0);
  d.print(String(gSelectedVoiceNote + 1) + "/" + String(max<int>(1, static_cast<int>(gVoiceNotes.size()))));

  if (gVoiceNotes.empty()) {
    d.setTextColor(TFT_YELLOW, panel);
    d.setCursor(10, 54);
    d.print("No saved voice notes");
  } else {
    int start = max(0, gSelectedVoiceNote - 4);
    int end = min(static_cast<int>(gVoiceNotes.size()), start + 10);
    int y = 11;
    for (int i = start; i < end; ++i) {
      bool selected = i == gSelectedVoiceNote;
      uint16_t rowBg = selected ? rgb565_local(30, 70, 50) : panel;
      d.fillRect(5, y - 1, 128, 11, rowBg);
      d.setTextColor(selected ? TFT_WHITE : (gVoiceNotes[i].assistant ? TFT_GREENYELLOW : TFT_CYAN), rowBg);
      d.setCursor(8, y);
      String rowTitle = String(gVoiceNotes[i].assistant ? "AI " : "ME ") + gVoiceNotes[i].title;
      d.print(selected ? marqueeText(rowTitle, 118, 260) : trimPreview(rowTitle, 18));
      y += 12;
    }
    int sliderY = map(gSelectedVoiceNote, 0, max<int>(1, static_cast<int>(gVoiceNotes.size()) - 1), 10, 108);
    d.fillRect(129, 8, 5, 122, dim);
    d.fillRect(129, sliderY, 5, 18, edge);
  }

  d.fillRect(148, 14, 86, 42, panel);
  d.drawRect(147, 13, 88, 44, edge);
  d.fillRect(148, 59, 86, 16, panel);
  d.drawRect(147, 58, 88, 18, edge);
  d.setTextColor(TFT_GREEN, panel);
  d.setCursor(153, 17);
  d.print(gPlaybackActive ? "PLAY" : "STOP");

  d.setTextColor(TFT_GREEN, panel);
  String timeText = "--:--";
  if (gPlaybackActive && gPlaybackStreamSampleRate > 0) {
    uint32_t leftMs = static_cast<uint32_t>((gPlaybackStreamRemainingBytes / 2ULL) * 1000ULL / gPlaybackStreamSampleRate);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02u:%02u", static_cast<unsigned>(leftMs / 60000U),
             static_cast<unsigned>((leftMs / 1000U) % 60U));
    timeText = buf;
  }
  d.setCursor(190, 17);
  d.print(timeText);

  for (int i = 0; i < 14; ++i) {
    if (gPlaybackActive && (gVoiceGraphSpeed % 4 == 0)) {
      gVoiceEqBars[i] = random(1, 6);
    }
    for (int j = 0; j < gVoiceEqBars[i]; ++j) {
      d.fillRect(172 + (i * 4), 51 - j * 3, 3, 2, rgb565_local(165, 255, 120));
    }
  }
  gVoiceGraphSpeed++;

  String ticker = "NO VOICE NOTES";
  if (!gVoiceNotes.empty()) {
    const auto& note = gVoiceNotes[gSelectedVoiceNote];
    ticker = note.title + " :: " + note.preview;
  }
  d.fillRect(149, 60, 84, 14, panel);
  d.setTextColor(TFT_GREEN, panel);
  d.setCursor(gVoiceTickerX, 63);
  d.print(ticker);
  if (d.textWidth(ticker) > 84 || gPlaybackActive) {
    gVoiceTickerX -= 2;
    if (gVoiceTickerX < -static_cast<int16_t>(max<int>(80, d.textWidth(ticker)))) {
      gVoiceTickerX = 230;
    }
  }

  d.setTextColor(TFT_LIGHTGREY, bg);
  d.setCursor(150, 80);
  d.print("VOL");
  d.fillRoundRect(172, 83, 60, 3, 2, TFT_YELLOW);
  int volX = map(gSpeakerVolume, 0, 255, 172, 224);
  d.fillRoundRect(volX, 80, 9, 8, 2, edge);

  for (int i = 0; i < 4; ++i) {
    d.fillRoundRect(148 + (i * 22), 94, 18, 18, 3, rgb565_local(150, 156, 176));
  }
  d.setTextColor(TFT_BLACK, rgb565_local(150, 156, 176));
  d.setCursor(154, 97);
  d.print("A");
  d.setCursor(176, 97);
  d.print("P");
  d.setCursor(198, 97);
  d.print("N");
  d.setCursor(220, 97);
  d.print("B");
  drawTinyFooter(bg, TFT_LIGHTGREY, "Ent play  V vol", 149, 122, 84);

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
    visibleRows = showStatus ? 2 : 4;
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
    d.setFont(&fonts::Font0);
    d.setTextColor(TFT_LIGHTGREY, 0x0841);
    d.setCursor(4, showFace ? 111 : 109);
    d.print(fitCurrentFontToWidth(gStatusText, 228));
  }

  d.setFont(&fonts::efontCN_12);
  d.setTextColor(TFT_YELLOW, 0x0841);
  d.setCursor(4, 121);
  String inputLine = tailUtf8ToFit("> " + gInputBuffer, 232);
  d.print(inputLine);
  renderTopicOverlayIfVisible();
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
  d.print("Open OmniBot hub");
  d.setCursor(10, 64);
  d.print("Add New Bot over BLE");
  d.setCursor(10, 88);
  d.print("Sends Wi-Fi + endpoint.");
  d.setCursor(10, 100);
  d.print("After provisioning, device reboots.");
  d.setFont(&fonts::Font0);
  d.setCursor(10, 124);
  d.print("Use /clearwifi later to reset");
  d.setFont(&fonts::Font2);
}

void renderStatusBar() {
  auto& d = gCanvas;
  d.fillRect(0, 126, 240, 9, 0x18C3);
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_LIGHTGREY, 0x18C3);
  d.setCursor(4, 127);
  d.print(fitCurrentFontToWidth(gStatusText, 230));
  d.setFont(&fonts::Font2);
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
      case UiMode::Home:
        renderAssistantPulseUi();
        break;
      case UiMode::Face:
        renderFaceOnlyUi();
        break;
      case UiMode::Hero:
        renderHeroUi();
        break;
      case UiMode::Clock:
        renderClockUi();
        break;
      case UiMode::Battery:
        renderBatteryUi();
        break;
      case UiMode::Voice:
        renderVoicePlayerUi();
        break;
      case UiMode::Focus:
        renderFocusUi();
        break;
      case UiMode::Library:
        renderLibraryUi();
        break;
      case UiMode::AudioFolders:
        renderAudioFoldersUi();
        break;
      case UiMode::Assets:
        renderAssetsUi();
        break;
      case UiMode::Ota:
        renderOtaUi();
        break;
      case UiMode::Contexts:
        renderContextsUi();
        break;
      case UiMode::Settings:
        renderSettingsUi();
        break;
      case UiMode::Logs:
        renderLogsUi();
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

void renderBootSplash(const char* stage) {
  auto& d = M5Cardputer.Display;
  const uint16_t bg = rgb565_local(3, 8, 14);
  const uint16_t panel = rgb565_local(2, 6, 7);
  const uint16_t panelLine = rgb565_local(38, 53, 50);
  const uint16_t mint = rgb565_local(111, 255, 230);
  const uint16_t mintDim = rgb565_local(32, 94, 89);
  const uint16_t gold = rgb565_local(255, 211, 106);
  const uint16_t goldDim = rgb565_local(117, 82, 32);
  const uint16_t rungFront = rgb565_local(154, 169, 164);
  const uint16_t rungBack = rgb565_local(54, 70, 65);
  const uint16_t muted = rgb565_local(135, 154, 149);

  auto line = [&](int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, bool thick) {
    d.drawLine(x1, y1, x2, y2, color);
    if (thick) {
      d.drawLine(x1, y1 + 1, x2, y2 + 1, color);
    }
  };

  auto drawHelix = [&]() {
    constexpr int16_t axisY = 66;
    constexpr int16_t startX = 41;
    constexpr int16_t step = 14;
    constexpr int16_t count = 12;
    constexpr float amp = 24.0f;
    float phase = millis() * 0.0021f;

    for (int i = 0; i < count; ++i) {
      float theta = phase + i * 0.72f;
      float s = sinf(theta);
      float c = cosf(theta);
      int16_t x = startX + i * step;
      int16_t yA = axisY + static_cast<int16_t>(s * amp);
      int16_t yB = axisY - static_cast<int16_t>(s * amp);
      bool frontA = c > 0.0f;
      line(x, yA, x, yB, frontA ? rungFront : rungBack, frontA);
    }

    for (int pass = 0; pass < 2; ++pass) {
      for (int i = 0; i < count; ++i) {
        float theta = phase + i * 0.72f;
        float s = sinf(theta);
        float c = cosf(theta);
        int16_t x = startX + i * step;
        int16_t y = axisY + static_cast<int16_t>((pass == 0 ? s : -s) * amp);
        bool front = pass == 0 ? c > 0.0f : c < 0.0f;
        uint16_t color = pass == 0 ? (front ? mint : mintDim) : (front ? gold : goldDim);
        int16_t r = front ? 5 : 3;
        d.fillCircle(x, y, r, color);
        if (front) {
          d.fillCircle(x - 1, y - 1, 1, TFT_WHITE);
        }
        if (i < count - 1) {
          float nextTheta = phase + (i + 1) * 0.72f;
          float nextS = sinf(nextTheta);
          float nextC = cosf(nextTheta);
          int16_t nx = startX + (i + 1) * step;
          int16_t ny = axisY + static_cast<int16_t>((pass == 0 ? nextS : -nextS) * amp);
          bool nextMostlyFront = pass == 0 ? nextC > -0.25f : nextC < 0.25f;
          line(x, y, nx, ny, color, front && nextMostlyFront);
        }
      }
    }
  };

  d.fillScreen(bg);
  d.fillTriangle(0, 92, 240, 28, 240, 135, rgb565_local(6, 21, 20));
  d.fillRoundRect(7, 7, 226, 121, 9, panel);
  d.drawRoundRect(7, 7, 226, 121, 9, panelLine);

  drawHelix();
  d.drawEllipse(120, 66, 82, 42 + static_cast<int16_t>((millis() / 320) % 3), mintDim);

  d.setFont(&fonts::Font2);
  d.setTextSize(1);
  d.fillRoundRect(15, 14, 50, 17, 7, rgb565_local(6, 16, 13));
  d.drawRoundRect(15, 14, 50, 17, 7, mint);
  d.setTextColor(TFT_WHITE, rgb565_local(6, 16, 13));
  d.setCursor(32, 18);
  d.print("ADV");

  uint8_t pct = static_cast<uint8_t>(18 + ((millis() / 95) % 78));
  String pctText = String(pct) + "%";
  d.setTextColor(gold, panel);
  d.setCursor(220 - d.textWidth(pctText), 18);
  d.print(pctText);
  d.drawRoundRect(74, 17, 93, 4, 2, panelLine);
  d.fillRoundRect(77 + pct / 2, 18, 22 + ((millis() / 180) % 10), 2, 1, TFT_WHITE);

  const char* states[] = {"DNA", "LINK", "SYNC", "READY"};
  const char* label = states[(millis() / 900) % 4];
  d.setFont(&fonts::Font4);
  d.setTextColor(TFT_WHITE, panel);
  int16_t tw = d.textWidth(label);
  d.setCursor((240 - tw) / 2, 101);
  d.print(label);
  d.setFont(&fonts::Font2);
  d.setTextColor(muted, panel);
  String subtitle = stage && stage[0] ? String("molecular context / ") + stage : "molecular context";
  d.setCursor((240 - d.textWidth(subtitle)) / 2, 121);
  d.print(subtitle);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("[BOOT] serial ready");
  delay(800);
  ensureBangkokTimezone();

  auto cfg = M5.config();
  cfg.internal_imu = true;
  M5Cardputer.begin(cfg, false);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(180);
  M5Cardputer.Display.setFont(&fonts::Font2);
  M5Cardputer.Display.setTextSize(1);
  renderBootSplash("Display");
  gCanvas.createSprite(240, 135);
  gCanvas.setTextWrap(false);
  gCanvas.setFont(&fonts::Font2);
  gCanvas.setTextSize(1);
  configureEmojiRendering();

  renderBootSplash("Audio");
  initSpeaker();
  renderBootSplash("Storage");
  initStorage();
  renderBootSplash("Input");
  randomSeed(micros());
  scheduleBlink();
  gImuEnabled = M5.Imu.isEnabled();
  logf("[IMU] enabled=%d type=%d", gImuEnabled, static_cast<int>(M5.Imu.getType()));
  setupKeyboardInput();
  gTimeValid = hasValidSystemTime();

  renderBootSplash("Config");
  loadConfig();
  loadFocusSettings();
  loadDeviceSettings();
  pushSystem("Device ready: " + gDeviceId);
  pushSystem("Ctrl+Space = EN/RU");
  pushSystem("Voice: Ctrl x2 / Ctrl+V / hold G0");
  pushSystem("Tab screen, Tab+Down app, Ctrl+L menu");
  pushSystem("/hero = Klo screen");
  pushSystem(String("FW ") + kAppVersion + " " + String(kBuildGitSha).substring(0, 10));
  logf("[BOOT] device_id=%s", gDeviceId.c_str());
  logf("[BOOT] version=%s git=%s build=%s", kAppVersion, kBuildGitSha, kBuildTime);

  renderBootSplash("Wi-Fi");
  if (ensureWifi()) {
    render();
    startHubWebSocket();
    if (gDeviceSettings.checkUpdatesOnBoot) {
      setStatus("Checking updates...", 900);
      render();
      fetchRemoteOtaManifest();
      fetchRemoteAssetManifest();
    }
    if (gDeviceSettings.autoUpdateFirmware && gOtaManifest.parsed &&
        !gOtaManifest.url.isEmpty() && gOtaManifest.version != kAppVersion) {
      applyOtaFromManifest();
    }
    startTopicTask(true, false, false, "Topics preload...");
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
    updateBatterySnapshot();
    updateFocusTimer();
    serviceFocusMetronome();
    processCompletedTopicTask();
    processCompletedTextTurn();
    updateG0VoiceTrigger();
    updateVoiceTrigger();
    pollTyping();
    serviceRecordingToFile();
    servicePlaybackStream();
  }

  if (gPlaybackActive && !gPlaybackStreaming && !M5Cardputer.Speaker.isPlaying()) {
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
