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
#include <esp_ota_ops.h>
#include <esp_partition.h>
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
#define APP_VERSION "0.2.20-dev"
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
constexpr const char* kPrefsOtaGuardNs = "ota_guard";
constexpr const char* kPrefsPetNs = "pet_v1";
constexpr const char* kLegacyBridgeHost = "109.199.103.176";
constexpr uint16_t kLegacyBridgePort = 31889;
constexpr const char* kProdBridgeHost = "bridge.ai.k-digital.pro";
constexpr uint16_t kProdBridgePort = 443;
constexpr uint32_t kOtaHealthProbeIntervalMs = 10000;
constexpr uint32_t kOtaCoreBootGraceMs = 90000;

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
constexpr uint16_t kTurnHttpTimeoutMs = 65000;
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
constexpr uint32_t kPetMinuteMs = 60UL * 1000UL;
constexpr uint32_t kPetTickMs = 1000;
constexpr uint32_t kPetAutoSaveMs = 45UL * 1000UL;
constexpr uint32_t kPetActionMs = 5UL * 1000UL;
constexpr uint32_t kPetRestMinMs = 90UL * 1000UL;
constexpr uint32_t kPetRestMaxMs = 180UL * 1000UL;
constexpr uint32_t kPetDecisionMinMs = 45UL * 1000UL;
constexpr uint32_t kPetDecisionMaxMs = 120UL * 1000UL;
constexpr uint32_t kPetOfflineMaxMinutes = 8UL * 60UL;
constexpr uint8_t kPetMaxPoop = 5;
constexpr uint32_t kTextTurnTaskStackSize = 8192;
constexpr uint32_t kTopicTaskStackSize = 8192;
constexpr size_t kMaxFlashVoiceNotes = 12;
constexpr size_t kMaxSdVoiceNotes = 256;
constexpr size_t kMaxStoredPreviewChars = 84;
constexpr size_t kMaxAudioAssets = 96;
constexpr size_t kMaxRuntimeLogLines = 36;
constexpr size_t kMaxContexts = 32;
constexpr size_t kMaxContextPreviewLines = 5;
constexpr size_t kMaxNotes = 128;
constexpr size_t kMaxNoteInputChars = 512;
constexpr size_t kVoicePreviewLineThreshold = 5;
constexpr const char* kVoiceDir = "/voice";
constexpr const char* kVoiceMetaExt = ".json";
constexpr const char* kVoiceAudioExt = ".wav";
constexpr const char* kNotesDir = "/notes";
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
constexpr uint8_t kPlaybackSpectrumBands = 16;
constexpr uint8_t kPlaybackSpectrumMaxBar = 18;
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
  Notes,
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

enum class PetStage : uint8_t {
  Egg,
  Baby,
  Teen,
  Adult,
  Elder,
};

enum class PetMood : uint8_t {
  Calm,
  Happy,
  Hungry,
  Bored,
  Curious,
  Sick,
  Excited,
  Sleepy,
  Dirty,
  Dead,
};

enum class PetActivity : uint8_t {
  Idle,
  Eating,
  Playing,
  Cleaning,
  Sleeping,
  Hunting,
  Exploring,
  Medicine,
  Discipline,
  Evolving,
  Dead,
};

enum class PetRestPhase : uint8_t {
  None,
  Enter,
  Deep,
  Wake,
};

enum class PetCareAction : uint8_t {
  Feed,
  Play,
  Clean,
  Sleep,
  Medicine,
  Discipline,
  Hunt,
  Explore,
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

enum class NotesMode : uint8_t {
  List,
  View,
  Edit,
};

struct DeviceActionCatalogEntry {
  const char* type;
  const char* label;
  const char* example;
};

constexpr const char* kDeviceActionCatalogVersion = "2026-05-01.2";
constexpr DeviceActionCatalogEntry kDeviceActionCatalog[] = {
    {"focus.start", "start focus", "set pomodoro to 45 minutes"},
    {"focus.pause", "pause focus", "pause focus"},
    {"focus.resume", "resume focus", "resume focus"},
    {"focus.reset", "reset focus", "reset pomodoro"},
    {"focus.set_duration", "focus length", "make focus 45 minutes"},
    {"focus.set_short_break", "short break", "short break 10 minutes"},
    {"focus.set_long_break", "long break", "long break 20 minutes"},
    {"focus.set_cycles", "cycles", "set cycles to 4"},
    {"focus.set_autostart", "auto start", "turn focus autostart on"},
    {"focus.set_metronome", "metronome", "turn metronome off"},
    {"focus.set_bpm", "bpm", "set bpm to 80"},
    {"ui.open", "open screen", "open battery"},
    {"audio.play", "play library", "play nutrition plan"},
    {"voice.play", "play voice note", "play last voice note"},
    {"settings.set_audio_language", "audio lang", "set language to russian"},
    {"settings.set_ota_auto", "OTA auto", "turn auto updates on"},
    {"settings.set_ota_boot_check", "OTA boot", "check updates on boot"},
    {"settings.set_ota_min_battery", "OTA battery", "update above 50 percent"},
    {"topic.next", "next topic", "next topic"},
    {"topic.prev", "prev topic", "previous topic"},
    {"topic.open", "open topic", "open project topic"},
    {"pet.feed", "pet food", "feed the OpenClaw pet"},
    {"pet.play", "pet play", "play with the pet"},
    {"pet.clean", "pet clean", "clean the pet"},
    {"pet.sleep", "pet sleep", "put the pet to sleep"},
    {"pet.medicine", "pet medicine", "heal the sick pet"},
    {"pet.discipline", "pet discipline", "discipline the pet"},
    {"pet.hunt", "pet hunt", "send the pet hunting Wi-Fi"},
    {"pet.explore", "pet explore", "let the pet explore"},
    {"pet.reset", "pet reset", "reset the pet"},
};
constexpr size_t kDeviceActionCatalogCount = sizeof(kDeviceActionCatalog) / sizeof(kDeviceActionCatalog[0]);

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

struct NoteFile {
  String name;
  String path;
  uint32_t size = 0;
  uint32_t lineCount = 0;
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
  String emoji;
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
  String clientMsgId;
  String turnId;
};

struct TextTurnTaskResult {
  int statusCode = 0;
  bool ok = false;
  bool parsed = false;
  bool hasReply = false;
  bool hasDeviceActions = false;
  bool hasAudio = false;
  bool telegramDelivered = false;
  String reply;
  String shortText;
  String deviceActionsJson;
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
  bool createTopic = false;
  String createTitle;
};

struct TopicTaskResult {
  bool catalogOk = false;
  bool selectOk = false;
  bool historyOk = false;
  bool createOk = false;
  String createdTopicKey;
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

struct PetEnvironment {
  int16_t netCount = 0;
  int16_t hiddenCount = 0;
  int16_t openCount = 0;
  int16_t strongCount = 0;
  int16_t avgRssi = -100;
  uint32_t lastScanMs = 0;
  bool scanPending = false;
  bool scanFresh = false;
};

struct PetRuntime {
  uint8_t hunger = 70;
  uint8_t happiness = 70;
  uint8_t health = 75;
  uint8_t energy = 80;
  uint8_t cleanliness = 80;
  uint8_t discipline = 45;
  uint8_t poop = 0;
  uint8_t careMistakes = 0;
  uint8_t traitCuriosity = 70;
  uint8_t traitActivity = 60;
  uint8_t traitStress = 40;
  uint32_t ageMinutes = 0;
  uint32_t bornEpoch = 0;
  uint32_t lastSavedEpoch = 0;
  uint32_t lastTickMs = 0;
  uint32_t lastMinuteMs = 0;
  uint32_t lastSaveMs = 0;
  uint32_t actionUntilMs = 0;
  uint32_t lastInteractionMs = 0;
  uint32_t lastDecisionMs = 0;
  uint32_t nextDecisionMs = kPetDecisionMinMs;
  uint32_t restPhaseStartMs = 0;
  uint32_t restDurationMs = kPetRestMinMs;
  bool alive = true;
  bool sick = false;
  bool restStatsApplied = false;
  PetStage stage = PetStage::Egg;
  PetMood mood = PetMood::Calm;
  PetActivity activity = PetActivity::Idle;
  PetRestPhase restPhase = PetRestPhase::None;
  PetEnvironment env;
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
    {"Assistant", "AI", "Voice, face, pet", rgb565_local(47, 227, 255), 4,
     {{"Pulse", UiMode::Home}, {"Big Eyes", UiMode::Face}, {"OpenClaw Pet", UiMode::Hero}, {"Chat Eyes", UiMode::ChatFace}}},
    {"Chat", "CH", "Chat, topics, notes", rgb565_local(88, 210, 255), 3,
     {{"Full Chat", UiMode::ChatFull}, {"Chat Eyes", UiMode::ChatFace}, {"Notes", UiMode::Notes}}},
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
std::vector<NoteFile> gNotes;
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
bool gNotesScanned = false;
int gChatScrollOffset = 0;
int gLauncherSelection = 0;
int gLauncherSubSelection = 0;
uint8_t gLauncherLastSub[kLauncherGroupCount] = {};
int gSelectedVoiceNote = 0;
int gSelectedNote = 0;
int gNoteScrollOffset = 0;
int gNoteListScrollOffset = 0;
int gSelectedAudioAsset = 0;
int gSelectedAudioFolder = 0;
String gAudioFolderFilter;
int gSelectedContext = 0;
String gSavedContextKey;
String gContextHistoryKey;
String gContextError;
bool gContextsRemoteLoaded = false;
bool gTopicCreateArmed = false;
uint32_t gContextsFetchedAtMs = 0;
uint32_t gContextHistoryFetchedAtMs = 0;
uint32_t gContextAnimStartMs = 0;
uint32_t gTopicOverlayUntilMs = 0;
uint32_t gTopicOverlayActionMs = 0;
uint32_t gTopicCreateArmedUntilMs = 0;
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
uint32_t gClientTurnSeq = 0;
uint32_t gLastBlockingRenderPumpMs = 0;
bool gBlockingRenderPumpEnabled = false;

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
String gNoteInputBuffer;
String gActiveNotePath;
String gActiveNoteName;
String gNotesStatus = "Ready";
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
size_t gPlaybackStreamTotalBytes = 0;
uint8_t gPlaybackStreamNextBuffer = 0;
bool gPlaybackStreamRawPcm = false;
uint8_t gPlaybackStreamBuffers[kPlaybackBufferCount][kPlaybackChunkBytes];
uint32_t gPlaybackStartedMs = 0;
uint32_t gPlaybackDurationMs = 0;
int16_t gVoiceTickerX = 90;
uint8_t gVoiceGraphSpeed = 0;
uint8_t gVoiceLevel8 = 28;
uint32_t gAssistantPulsePressUntilMs = 0;
uint8_t gVoiceEqBars[kPlaybackSpectrumBands] = {};
uint8_t gVoiceEqPeaks[kPlaybackSpectrumBands] = {};
BatterySnapshot gBatterySnapshot;
FocusSettings gFocusSettings;
DeviceSettings gDeviceSettings;
FocusRuntime gFocus;
PetRuntime gPet;
AssetManifestSummary gAssetManifest;
OtaManifestSummary gOtaManifest;
NotesMode gNotesMode = NotesMode::List;
bool gOtaPendingVerify = false;
bool gOtaConfirmedThisBoot = false;
uint32_t gOtaVerifyStartMs = 0;
uint32_t gOtaLastHealthProbeMs = 0;
String gOtaRunningSlot = "-";
String gOtaBootState = "unknown";
bool gTransferUiActive = false;
String gTransferTitle;
String gTransferDetail;
uint32_t gTransferDone = 0;
uint32_t gTransferTotal = 0;
uint32_t gTransferStartedMs = 0;
uint32_t gTransferLastRenderMs = 0;
uint16_t gTransferAccent = TFT_CYAN;
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
void setStatus(const String& text, uint32_t ttlMs);
void pushSystem(const String& text);
void beginTransferUi(const String& title, const String& detail, uint32_t total, uint16_t accent);
void updateTransferUi(uint32_t done, uint32_t total = 0, const String& detail = "");
void finishTransferUi(const String& statusText, bool ok);
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
void loadPetState();
void savePetState(bool force = false);
void resetPet(bool fullReset);
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

void clearOtaGuardPrefs() {
  gPrefs.begin(kPrefsOtaGuardNs, false);
  gPrefs.clear();
  gPrefs.end();
}

void recordOtaApplyIntent() {
  gPrefs.begin(kPrefsOtaGuardNs, false);
  gPrefs.putString("target", gOtaManifest.version);
  gPrefs.putString("sha", gOtaManifest.sha256);
  gPrefs.putString("from", kAppVersion);
  gPrefs.putString("git", kBuildGitSha);
  gPrefs.putUInt("started", millis());
  gPrefs.end();
}

void beginOtaBootGuard() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  gOtaRunningSlot = running ? String(running->label) : String("-");
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  esp_err_t err = running ? esp_ota_get_state_partition(running, &state) : ESP_ERR_NOT_FOUND;
  if (err == ESP_OK) {
    gOtaBootState = otaImageStateName(state);
  } else {
    gOtaBootState = "no-state";
  }

  gOtaPendingVerify = err == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
  gOtaConfirmedThisBoot = false;
  gOtaVerifyStartMs = millis();
  gOtaLastHealthProbeMs = 0;

  if (gOtaPendingVerify) {
    gPrefs.begin(kPrefsOtaGuardNs, false);
    uint32_t boots = gPrefs.getUInt("boots", 0) + 1;
    gPrefs.putUInt("boots", boots);
    gPrefs.putString("slot", gOtaRunningSlot);
    gPrefs.putString("boot_version", kAppVersion);
    gPrefs.end();
    appendRuntimeLog("OTA", "pending verify slot=" + gOtaRunningSlot +
                              " boot=" + String(boots), true);
    pushSystem("OTA verify: " + gOtaRunningSlot);
    setStatus("OTA verifying...", 1200);
  } else if (err == ESP_OK && state == ESP_OTA_IMG_VALID) {
    clearOtaGuardPrefs();
  }
  logf("[OTA] running=%s state=%s pending=%d", gOtaRunningSlot.c_str(),
       gOtaBootState.c_str(), static_cast<int>(gOtaPendingVerify));
}

const char* uiModeName(UiMode mode) {
  switch (mode) {
    case UiMode::Home:
      return "Pulse";
    case UiMode::Face:
      return "Face";
    case UiMode::Hero:
      return "Pet";
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
    case UiMode::Notes:
      return "Notes";
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
  out.replace("```", "");
  out.replace("**", "");
  out.replace("__", "");
  out.replace("~~", "");
  out.replace("`", "");
  out.replace("*", "");
  out.replace("_", "");
  out.replace("&nbsp;", " ");
  out.replace("&amp;", "&");
  out.replace("&lt;", "<");
  out.replace("&gt;", ">");
  out.replace("&quot;", "\"");
  out.replace("&#39;", "'");

  String linked;
  linked.reserve(out.length());
  for (int i = 0; i < out.length();) {
    bool image = out[i] == '!' && i + 1 < out.length() && out[i + 1] == '[';
    bool link = out[i] == '[' || image;
    if (link) {
      int textStart = i + (image ? 2 : 1);
      int textEnd = out.indexOf(']', textStart);
      int urlStart = textEnd >= 0 ? out.indexOf("](", textStart) : -1;
      int urlEnd = urlStart >= 0 ? out.indexOf(')', urlStart + 2) : -1;
      if (textEnd >= 0 && urlStart == textEnd && urlEnd >= 0) {
        linked += out.substring(textStart, textEnd);
        i = urlEnd + 1;
        continue;
      }
    }
    linked += out[i++];
  }
  out = linked;

  String lines;
  lines.reserve(out.length());
  int start = 0;
  while (start <= out.length()) {
    int end = out.indexOf('\n', start);
    if (end < 0) {
      end = out.length();
    }
    String line = out.substring(start, end);
    line.trim();
    while (line.startsWith("#")) {
      line = line.substring(1);
      line.trim();
    }
    while (line.startsWith(">")) {
      line = line.substring(1);
      line.trim();
    }
    if (line.startsWith("- ") || line.startsWith("+ ")) {
      line = "* " + line.substring(2);
    }
    if (!line.isEmpty()) {
      if (!lines.isEmpty()) {
        lines += " ";
      }
      lines += line;
    }
    if (end >= out.length()) {
      break;
    }
    start = end + 1;
  }
  out = lines;
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

void renderTransferUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 6, 12);
  const uint16_t panel = rgb565_local(7, 13, 22);
  const uint16_t ink = rgb565_local(224, 239, 246);
  const uint16_t muted = rgb565_local(117, 136, 150);
  const uint16_t track = rgb565_local(22, 35, 47);
  uint32_t now = millis();
  uint32_t elapsed = gTransferStartedMs ? (now - gTransferStartedMs) : 0;
  int pct = gTransferTotal ? clampValue<int>((gTransferDone * 100ULL) / gTransferTotal, 0, 100) : -1;
  float pulse = (sinf(now * 0.008f) + 1.0f) * 0.5f;
  int sweep = static_cast<int>((now / 18) % 172);

  d.fillScreen(bg);
  d.fillRoundRect(8, 8, 224, 119, 14, panel);
  d.drawRoundRect(8, 8, 224, 119, 14, gTransferAccent);
  d.drawRoundRect(12, 12, 216, 111, 11, rgb565_local(18, 33, 45));

  d.setFont(&fonts::Font2);
  d.setTextSize(1);
  d.setTextColor(gTransferAccent, panel);
  d.setCursor(18, 18);
  d.print(trimPreview(gTransferTitle, 22));
  d.setTextColor(muted, panel);
  String version = String("v") + kAppVersion;
  d.setCursor(222 - d.textWidth(version), 18);
  d.print(version);

  int16_t cx = 54;
  int16_t cy = 64;
  for (int i = 0; i < 8; ++i) {
    float a = now * 0.006f + i * 0.785f;
    int16_t x = cx + static_cast<int16_t>(cosf(a) * (18 + i % 2));
    int16_t y = cy + static_cast<int16_t>(sinf(a) * 18);
    uint16_t c = (i % 2) ? rgb565_local(255, 205, 96) : gTransferAccent;
    d.fillCircle(x, y, 2 + ((i + now / 180) % 2), c);
  }
  d.fillCircle(cx, cy, 10 + static_cast<int>(pulse * 2.0f), rgb565_local(10, 27, 35));
  d.drawCircle(cx, cy, 13, gTransferAccent);

  d.setFont(&fonts::Font4);
  d.setTextColor(ink, panel);
  String pctText = pct >= 0 ? String(pct) + "%" : String("...");
  d.setCursor(92, 43);
  d.print(pctText);

  d.setFont(&fonts::Font2);
  d.setTextColor(muted, panel);
  String detail = gTransferDetail.isEmpty() ? "please wait" : gTransferDetail;
  d.setCursor(92, 70);
  d.print(trimPreview(detail, 24));

  if (gTransferTotal > 0) {
    d.setCursor(92, 84);
    d.print(compactBytes(gTransferDone));
    d.print("/");
    d.print(compactBytes(gTransferTotal));
  } else {
    d.setCursor(92, 84);
    d.print(String(elapsed / 1000) + "s");
  }

  int barX = 18;
  int barY = 104;
  int barW = 204;
  d.fillRoundRect(barX, barY, barW, 8, 4, track);
  if (pct >= 0) {
    int fillW = max<int>(4, (barW * pct) / 100);
    d.fillRoundRect(barX, barY, fillW, 8, 4, gTransferAccent);
    d.fillRoundRect(barX + max<int>(0, fillW - 18), barY + 2, min<int>(18, fillW), 4, 2, TFT_WHITE);
  } else {
    d.fillRoundRect(barX + sweep, barY, 32, 8, 4, gTransferAccent);
  }

  d.setTextColor(muted, panel);
  d.setCursor(18, 116);
  d.print("do not power off");
  gCanvas.pushSprite(0, 0);
}

void beginTransferUi(const String& title, const String& detail, uint32_t total, uint16_t accent) {
  gTransferUiActive = true;
  gTransferTitle = title;
  gTransferDetail = detail;
  gTransferDone = 0;
  gTransferTotal = total;
  gTransferStartedMs = millis();
  gTransferLastRenderMs = 0;
  gTransferAccent = accent;
  renderTransferUi();
}

void updateTransferUi(uint32_t done, uint32_t total, const String& detail) {
  if (!gTransferUiActive) {
    return;
  }
  gTransferDone = done;
  if (total > 0) {
    gTransferTotal = total;
  }
  if (!detail.isEmpty()) {
    gTransferDetail = detail;
  }
  uint32_t now = millis();
  if (now - gTransferLastRenderMs >= 220 || done >= gTransferTotal) {
    gTransferLastRenderMs = now;
    renderTransferUi();
  }
}

void finishTransferUi(const String& statusText, bool ok) {
  if (!gTransferUiActive) {
    setStatus(statusText, ok ? 900 : 1600);
    return;
  }
  if (!statusText.isEmpty()) {
    gTransferDetail = statusText;
  }
  gTransferDone = gTransferTotal;
  renderTransferUi();
  gTransferUiActive = false;
  setStatus(statusText, ok ? 900 : 1600);
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

uint32_t playbackDurationMsForBytes(size_t bytes, uint32_t sampleRate) {
  if (bytes == 0 || sampleRate == 0) {
    return 0;
  }
  return static_cast<uint32_t>((static_cast<uint64_t>(bytes / sizeof(int16_t)) * 1000ULL) / sampleRate);
}

uint32_t playbackElapsedMs() {
  if (!gPlaybackActive || gPlaybackStartedMs == 0) {
    return 0;
  }
  return min<uint32_t>(millis() - gPlaybackStartedMs, gPlaybackDurationMs);
}

uint32_t playbackRemainingMs() {
  if (!gPlaybackActive || gPlaybackDurationMs == 0) {
    return 0;
  }
  uint32_t elapsed = playbackElapsedMs();
  return elapsed >= gPlaybackDurationMs ? 0 : gPlaybackDurationMs - elapsed;
}

uint8_t playbackProgressPct() {
  if (!gPlaybackActive || gPlaybackDurationMs == 0) {
    return 0;
  }
  return static_cast<uint8_t>(clampValue<uint32_t>((playbackElapsedMs() * 100ULL) / gPlaybackDurationMs, 0, 100));
}

String formatPlaybackMs(uint32_t ms) {
  char buf[8];
  uint32_t sec = ms / 1000U;
  snprintf(buf, sizeof(buf), "%02u:%02u", static_cast<unsigned>(sec / 60U),
           static_cast<unsigned>(sec % 60U));
  return String(buf);
}

void resetPlaybackAnalyzer() {
  gVoiceLevel8 = 28;
  for (uint8_t i = 0; i < kPlaybackSpectrumBands; ++i) {
    gVoiceEqBars[i] = 1;
    gVoiceEqPeaks[i] = 1;
  }
}

void updatePlaybackAnalyzerFromSamples(const int16_t* samples, size_t sampleCount, uint32_t sampleRate) {
  if (!samples || sampleCount < 32 || sampleRate < 4000) {
    return;
  }
  updateVoiceLevelFromSamples(samples, sampleCount);

  // Lightweight real spectrum: Goertzel bands over the PCM chunk. It avoids a
  // full FFT dependency while still reflecting the actual audio being played.
  const size_t n = min<size_t>(sampleCount, 384);
  int64_t dcAcc = 0;
  for (size_t i = 0; i < n; ++i) {
    dcAcc += samples[i];
  }
  const float dc = static_cast<float>(dcAcc) / static_cast<float>(n);
  const float maxFreq = max<float>(420.0f, min<float>(static_cast<float>(sampleRate) * 0.46f, 7600.0f));
  const float minFreq = 120.0f;
  const float ratio = maxFreq / minFreq;

  for (uint8_t band = 0; band < kPlaybackSpectrumBands; ++band) {
    float norm = (static_cast<float>(band) + 0.5f) / static_cast<float>(kPlaybackSpectrumBands);
    float freq = minFreq * powf(ratio, norm);
    float omega = 2.0f * kPi * freq / static_cast<float>(sampleRate);
    float coeff = 2.0f * cosf(omega);
    float q0 = 0.0f;
    float q1 = 0.0f;
    float q2 = 0.0f;
    for (size_t i = 0; i < n; i += 2) {
      float sample = static_cast<float>(samples[i]) - dc;
      q0 = coeff * q1 - q2 + sample;
      q2 = q1;
      q1 = q0;
    }
    float power = max<float>(0.0f, q1 * q1 + q2 * q2 - coeff * q1 * q2);
    float magnitude = sqrtf(power) / static_cast<float>(n);
    uint8_t target = static_cast<uint8_t>(clampValue<int>(
        1 + static_cast<int>(sqrtf(magnitude) * 0.24f),
        1,
        kPlaybackSpectrumMaxBar));

    uint8_t current = gVoiceEqBars[band];
    if (target >= current) {
      current = static_cast<uint8_t>((current + target * 3U) / 4U);
    } else {
      current = static_cast<uint8_t>(max<int>(1, static_cast<int>(current) - 1));
    }
    gVoiceEqBars[band] = current;
    if (current >= gVoiceEqPeaks[band]) {
      gVoiceEqPeaks[band] = current;
    } else if ((millis() / 90U + band) % 2U == 0U) {
      gVoiceEqPeaks[band] = static_cast<uint8_t>(max<int>(1, static_cast<int>(gVoiceEqPeaks[band]) - 1));
    }
  }
}

bool adjustSpeakerVolume(int delta, uint32_t ttlMs = 700) {
  int next = clampValue<int>(static_cast<int>(gSpeakerVolume) + delta, 0, 255);
  if (next == static_cast<int>(gSpeakerVolume)) {
    setStatus(delta > 0 ? "Vol max" : "Vol min", ttlMs);
    return false;
  }
  gSpeakerVolume = static_cast<uint8_t>(next);
  M5Cardputer.Speaker.setVolume(gSpeakerVolume);
  setStatus("Vol " + String(map(gSpeakerVolume, 0, 255, 0, 100)) + "%", ttlMs);
  return true;
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

String makeClientTurnId(const char* prefix) {
  uint32_t seq = ++gClientTurnSeq;
  char buf[48];
  snprintf(buf, sizeof(buf), "%s-%08lx-%04lx",
           prefix && prefix[0] ? prefix : "turn",
           static_cast<unsigned long>(millis()),
           static_cast<unsigned long>(seq & 0xFFFF));
  return String(buf);
}

void pumpBlockingNetworkUi() {
  uint32_t now = millis();
  if (gBlockingRenderPumpEnabled && now - gLastBlockingRenderPumpMs >= kBusyRenderIntervalMs) {
    gLastBlockingRenderPumpMs = now;
    if (gTransferUiActive) {
      renderTransferUi();
    } else {
      render();
      gLastRenderMs = now;
    }
  }
  yield();
  delay(1);
}

bool shouldSpeakAssistantTextReply(const String& reply) {
  String text = cleanReplyForDevice(reply);
  if (text.length() > 90) {
    return true;
  }
  uint8_t words = 0;
  bool inWord = false;
  for (size_t i = 0; i < text.length(); ++i) {
    bool space = isspace(static_cast<unsigned char>(text[i]));
    if (!space && !inWord) {
      ++words;
      inWord = true;
      if (words > 5) {
        return true;
      }
    } else if (space) {
      inWord = false;
    }
  }
  return false;
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

LineKind topicHistoryLineKind(const String& role) {
  if (role == "assistant") {
    return LineKind::Assistant;
  }
  if (role == "device" || role == "human" || role == "user") {
    return LineKind::User;
  }
  return LineKind::System;
}

String topicHistoryPrefix(const String& role, bool voice) {
  if (voice) {
    return "VO";
  }
  if (role == "assistant") {
    return "AI";
  }
  if (role == "device") {
    return "ME";
  }
  if (role == "human" || role == "user") {
    return "TG";
  }
  return "MSG";
}

void replaceChatWithTopicHistory(const String& title, const std::vector<ChatLine>& lines) {
  gChatLines.clear();
  String header = "Topic: " + trimPreview(normalizeTopicTitleForDisplay(title, false), 72);
  pushLine("CTX", header, LineKind::System);
  if (lines.empty()) {
    pushLine("CTX", "No recent messages", LineKind::System);
    return;
  }
  for (const auto& line : lines) {
    pushLine(line.prefix, line.text, line.kind);
  }
  gChatScrollOffset = 0;
}

String fallbackTopicEmojiForTitle(const String& title, int32_t threadId) {
  String lower = title;
  lower.toLowerCase();
  if (lower.indexOf("card") >= 0 || lower.indexOf("adv") >= 0) {
    return "💾";
  }
  if (lower.indexOf("marathon") >= 0 || lower.indexOf("run") >= 0) {
    return "🏃";
  }
  if (lower.indexOf("body") >= 0 || lower.indexOf("health") >= 0) {
    return "💪";
  }
  if (lower.indexOf("focus") >= 0 || lower.indexOf("task") >= 0) {
    return "🎯";
  }
  if (lower.indexOf("video") >= 0) {
    return "🎬";
  }
  if (lower.indexOf("audio") >= 0 || lower.indexOf("music") >= 0) {
    return "🎧";
  }
  if (threadId == 1 || lower.indexOf("assistant") >= 0 || lower.indexOf("помощ") >= 0) {
    return "🤖";
  }
  return "💬";
}

String topicDisplayTitle(const ConversationContext& ctx, bool keepAvailableEmoji = false) {
  return normalizeTopicTitleForDisplay(ctx.label, keepAvailableEmoji);
}

String topicDisplayIcon(const ConversationContext& ctx) {
  if (!ctx.emoji.isEmpty()) {
    String emoji = normalizeEmojiDisplayText(ctx.emoji);
    uint32_t codepoint = 0;
    int consumed = 1;
    if (decodeUtf8At(emoji, 0, codepoint, consumed) && emojiAssetAvailable(codepoint)) {
      const EmojiCacheEntry* entry = emojiCacheLookup(codepoint);
      if (entry && entry->data && entry->width > 0 && entry->height > 0) {
        return emoji.substring(0, consumed);
      }
    }
  }
  return ctx.shortName.isEmpty() ? makeTopicShortName(ctx.label, ctx.threadId) : ctx.shortName;
}

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

String normalizeTopicTitleForDisplay(const String& raw, bool keepAvailableEmoji) {
  String cleaned = raw;
  cleaned.replace("to:root-main", "");
  cleaned.replace("root-main", "");
  int slashPos = cleaned.lastIndexOf(" / ");
  if (slashPos > 0) {
    bool numericSuffix = true;
    for (int i = slashPos + 3; i < cleaned.length(); ++i) {
      char c = cleaned[i];
      if (c < '0' || c > '9') {
        numericSuffix = false;
        break;
      }
    }
    if (numericSuffix) {
      cleaned = cleaned.substring(0, slashPos);
    }
  }
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
    if (ok && isEmojiAssetCodepoint(codepoint) && (!keepAvailableEmoji || !emojiAssetAvailable(codepoint))) {
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

fs::FS* activeNotesFs() {
  if (gSdReady) {
    return &SD;
  }
  if (gLittleFsReady) {
    return &LittleFS;
  }
  return nullptr;
}

String activeNotesStorageLabel() {
  if (gSdReady) {
    return "SD";
  }
  if (gLittleFsReady) {
    return "FLASH";
  }
  return "OFF";
}

bool ensureNotesStorageOn(fs::FS& fs) {
  return ensureDirRecursive(fs, kNotesDir);
}

bool ensureNotesStorage() {
  fs::FS* fs = activeNotesFs();
  if (!fs) {
    return false;
  }
  return ensureNotesStorageOn(*fs);
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

bool isNoteFileName(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".txt") || lower.endsWith(".md") || lower.endsWith(".note");
}

String basenameOfPath(const String& path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

String sanitizeNoteTitle(String value) {
  value.trim();
  value.toLowerCase();
  String out;
  out.reserve(32);
  for (int i = 0; i < value.length() && out.length() < 28; ++i) {
    char c = value[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      out += c;
    } else if ((c == ' ' || c == '-' || c == '_' || c == '.') && !out.endsWith("-")) {
      out += '-';
    }
  }
  while (out.endsWith("-")) {
    out.remove(out.length() - 1);
  }
  return out.isEmpty() ? "note" : out;
}

String makeNoteFileName(const String& titleHint = "") {
  String stem = sanitizeNoteTitle(titleHint);
  char stamp[24];
  if (hasValidSystemTime()) {
    time_t now = time(nullptr);
    struct tm localNow;
    localtime_r(&now, &localNow);
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &localNow);
  } else {
    snprintf(stamp, sizeof(stamp), "%lu", static_cast<unsigned long>(millis() / 1000UL));
  }
  return stem + "-" + String(stamp) + ".md";
}

uint32_t countNoteLines(fs::FS& fs, const String& path) {
  File file = fs.open(path, "r");
  if (!file) {
    return 0;
  }
  uint32_t lines = 0;
  bool hasChars = false;
  while (file.available()) {
    char c = static_cast<char>(file.read());
    hasChars = true;
    if (c == '\n') {
      ++lines;
    }
  }
  file.close();
  return lines + (hasChars ? 1 : 0);
}

void clampNoteSelection() {
  if (gNotes.empty()) {
    gSelectedNote = 0;
    gNoteListScrollOffset = 0;
    return;
  }
  gSelectedNote = clampValue<int>(gSelectedNote, 0, static_cast<int>(gNotes.size()) - 1);
}

void scanNotes() {
  gNotes.clear();
  fs::FS* fs = activeNotesFs();
  if (!fs || !ensureNotesStorage()) {
    gNotesStatus = "Storage unavailable";
    gNotesScanned = true;
    return;
  }
  File dir = fs->open(kNotesDir);
  if (!dir || !dir.isDirectory()) {
    gNotesStatus = "Notes dir unavailable";
    gNotesScanned = true;
    return;
  }
  while (gNotes.size() < kMaxNotes) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }
    if (!file.isDirectory()) {
      String path = file.name();
      if (!path.startsWith("/")) {
        path = String(kNotesDir) + "/" + path;
      }
      String name = basenameOfPath(path);
      if (isNoteFileName(name)) {
        NoteFile note;
        note.name = name;
        note.path = path;
        note.size = static_cast<uint32_t>(file.size());
        note.lineCount = countNoteLines(*fs, path);
        gNotes.push_back(note);
      }
    }
    file.close();
  }
  dir.close();
  std::sort(gNotes.begin(), gNotes.end(), [](const NoteFile& a, const NoteFile& b) {
    return a.name > b.name;
  });
  clampNoteSelection();
  gNotesStatus = String(gNotes.size()) + " notes on " + activeNotesStorageLabel();
  gNotesScanned = true;
}

bool createNewNote(const String& titleHint = "") {
  fs::FS* fs = activeNotesFs();
  if (!fs || !ensureNotesStorage()) {
    gNotesStatus = "Storage unavailable";
    setStatus("Notes storage unavailable", 1200);
    return false;
  }
  String name = makeNoteFileName(titleHint);
  String path = String(kNotesDir) + "/" + name;
  int suffix = 2;
  while (fsExists(*fs, path) && suffix < 50) {
    name = makeNoteFileName(titleHint);
    name.replace(".md", "-" + String(suffix++) + ".md");
    path = String(kNotesDir) + "/" + name;
  }
  File file = fs->open(path, "w");
  if (!file) {
    gNotesStatus = "Create failed";
    setStatus("Note create failed", 1200);
    return false;
  }
  file.println(String("# ") + name);
  file.println();
  file.close();
  gActiveNotePath = path;
  gActiveNoteName = name;
  gNoteInputBuffer = "";
  gNoteScrollOffset = 0;
  gNotesMode = NotesMode::Edit;
  scanNotes();
  for (int i = 0; i < static_cast<int>(gNotes.size()); ++i) {
    if (gNotes[i].path == path) {
      gSelectedNote = i;
      break;
    }
  }
  gNotesStatus = "Editing new note";
  setStatus("New note", 900);
  return true;
}

bool openSelectedNote(NotesMode mode) {
  if (gNotes.empty()) {
    return createNewNote();
  }
  clampNoteSelection();
  gActiveNotePath = gNotes[gSelectedNote].path;
  gActiveNoteName = gNotes[gSelectedNote].name;
  gNoteScrollOffset = 0;
  gNoteInputBuffer = "";
  gNotesMode = mode;
  gNotesStatus = mode == NotesMode::Edit ? "Append mode" : "Viewing";
  return true;
}

bool appendLineToActiveNote(const String& line) {
  fs::FS* fs = activeNotesFs();
  if (!fs || gActiveNotePath.isEmpty()) {
    gNotesStatus = "No note open";
    return false;
  }
  File file = fs->open(gActiveNotePath, "a");
  if (!file) {
    gNotesStatus = "Save failed";
    setStatus("Note save failed", 1200);
    return false;
  }
  file.println(line);
  file.close();
  gNotesStatus = "Saved line";
  setStatus("Note saved", 700);
  scanNotes();
  for (int i = 0; i < static_cast<int>(gNotes.size()); ++i) {
    if (gNotes[i].path == gActiveNotePath) {
      gSelectedNote = i;
      break;
    }
  }
  return true;
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

bool downloadUrlToFile(const String& url, const String& path, const String& expectedSha = "",
                       uint32_t expectedSize = 0, const String& progressTitle = "Downloading",
                       uint16_t progressAccent = TFT_CYAN) {
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

  beginTransferUi(progressTitle, "connecting", expectedSize, progressAccent);
  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  if (!configureHttpClient(http, secureClient, url, 90000)) {
    out.close();
    fsRemove(*fs, tempPath);
    finishTransferUi("connect failed", false);
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
    finishTransferUi("HTTP " + String(code), false);
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[1024];
  uint32_t written = 0;
  int contentLength = http.getSize();
  uint32_t progressTotal = contentLength > 0 ? static_cast<uint32_t>(contentLength) : expectedSize;
  uint32_t lastProgress = millis();
  while (http.connected() && (contentLength < 0 || written < static_cast<uint32_t>(contentLength))) {
    size_t available = stream->available();
    if (!available) {
      if (millis() - lastProgress > 15000) {
        break;
      }
      updateTransferUi(written, progressTotal, "waiting");
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
      finishTransferUi("write failed", false);
      return false;
    }
    written += readBytes;
    lastProgress = millis();
    updateTransferUi(written, progressTotal, "receiving");
  }
  http.end();
  out.close();

  if ((expectedSize > 0 && written != expectedSize) ||
      (contentLength > 0 && written != static_cast<uint32_t>(contentLength))) {
    fsRemove(*fs, tempPath);
    finishTransferUi("download short", false);
    return false;
  }
  if (!expectedSha.isEmpty()) {
    updateTransferUi(written, progressTotal, "verifying SHA");
    String actualSha = sha256File(*fs, tempPath);
    actualSha.toLowerCase();
    String expected = expectedSha;
    expected.toLowerCase();
    if (actualSha != expected) {
      fsRemove(*fs, tempPath);
      finishTransferUi("SHA mismatch", false);
      appendRuntimeLog("SHA", path + " mismatch", true);
      return false;
    }
  }
  fsRemove(*fs, path);
  if (!fsRename(*fs, tempPath, path)) {
    fsRemove(*fs, tempPath);
    finishTransferUi("rename failed", false);
    return false;
  }
  finishTransferUi("saved", true);
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
  String title = label == "OTA" ? "OTA Manifest" : (label == "ASSET" ? "Asset Manifest" : "Download");
  uint16_t accent = label == "OTA" ? rgb565_local(255, 93, 88) : rgb565_local(88, 240, 141);
  bool ok = downloadUrlToFile(url, localPath, "", 0, title, accent);
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
    if (alreadyOk || downloadUrlToFile(url, path, sha, size,
                                       "Asset " + String(okCount + failCount + 1) + "/" +
                                           String(doc["files"].as<JsonArray>().size()),
                                       rgb565_local(88, 240, 141))) {
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

bool probeOtaBridgeHealth(uint32_t timeoutMs) {
  if ((!gWifiReady && !ensureWifiForHttp("ota-health", 3000)) || gHubHost.isEmpty()) {
    appendRuntimeLog("OTA", "health skipped wifi/hub", false);
    return false;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + "/healthz";
  if (!configureHttpClient(http, secureClient, url, timeoutMs)) {
    appendRuntimeLog("OTA", "health connect failed", false);
    return false;
  }
  int code = http.GET();
  String body = code > 0 ? http.getString() : "";
  http.end();
  if (code < 200 || code >= 300) {
    appendRuntimeLog("OTA", "health http " + String(code), false);
    return false;
  }

  DynamicJsonDocument doc(768);
  auto err = deserializeJson(doc, body);
  if (!err && doc["upstream_connected"].is<bool>() && !doc["upstream_connected"].as<bool>()) {
    appendRuntimeLog("OTA", "health upstream false", false);
    return false;
  }
  appendRuntimeLog("OTA", "health ok", false);
  return true;
}

bool confirmOtaBootIfPending(const char* reason) {
  if (!gOtaPendingVerify) {
    return true;
  }
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err != ESP_OK) {
    appendRuntimeLog("OTA", "confirm failed err=" + String(static_cast<int>(err)), true);
    setStatus("OTA confirm failed", 1600);
    return false;
  }
  gOtaPendingVerify = false;
  gOtaConfirmedThisBoot = true;
  gOtaBootState = "valid";
  clearOtaGuardPrefs();
  String why = reason && reason[0] ? String(reason) : String("boot ok");
  appendRuntimeLog("OTA", "confirmed " + why + " slot=" + gOtaRunningSlot, true);
  pushSystem("OTA confirmed: " + gOtaRunningSlot);
  setStatus("OTA confirmed", 1200);
  return true;
}

void rollbackPendingOta(const char* reason) {
  if (!gOtaPendingVerify) {
    setStatus("No pending OTA", 1000);
    return;
  }
  String why = reason && reason[0] ? String(reason) : String("manual");
  appendRuntimeLog("OTA", "rollback requested: " + why, true);
  setStatus("Rolling back OTA...", 0);
  render();
  delay(400);
  esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
  appendRuntimeLog("OTA", "rollback failed err=" + String(static_cast<int>(err)), true);
  setStatus("Rollback failed", 1800);
}

void serviceOtaBootGuard() {
  if (!gOtaPendingVerify) {
    return;
  }
  uint32_t now = millis();
  if (now - gOtaLastHealthProbeMs >= kOtaHealthProbeIntervalMs) {
    gOtaLastHealthProbeMs = now;
    if (probeOtaBridgeHealth(6000)) {
      confirmOtaBootIfPending("bridge health");
      return;
    }
  }
  if (now - gOtaVerifyStartMs >= kOtaCoreBootGraceMs) {
    // If the app survived the boot window, commit it to avoid false rollbacks
    // caused by external Wi-Fi/bridge outages. Crashes before this point roll
    // back automatically because the slot remains PENDING_VERIFY.
    confirmOtaBootIfPending("core boot grace");
  }
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
  if (!downloadUrlToFile(gOtaManifest.url, kOtaTempPath, gOtaManifest.sha256, gOtaManifest.size,
                         "Firmware OTA", rgb565_local(255, 93, 88))) {
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
  auto describePartition = [](const esp_partition_t* partition) -> String {
    if (!partition) {
      return "none";
    }
    return String(partition->label) + "@0x" + String(static_cast<uint32_t>(partition->address), HEX) +
           "/" + String(static_cast<unsigned>(partition->size));
  };
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  appendRuntimeLog("OTA", "partition running=" + describePartition(running) +
                            " boot=" + describePartition(boot) +
                            " next=" + describePartition(next), true);
  if (!next) {
    firmware.close();
    setStatus("OTA slot missing", 2200);
    appendRuntimeLog("OTA", "no inactive OTA partition", true);
    return false;
  }
  if (size == 0 || size > next->size) {
    firmware.close();
    setStatus("OTA image too large", 2200);
    appendRuntimeLog("OTA", "image too large size=" + String(static_cast<unsigned>(size)) +
                              " slot=" + String(static_cast<unsigned>(next->size)), true);
    return false;
  }
  if (!Update.begin(size, U_FLASH)) {
    uint8_t updateError = Update.getError();
    firmware.close();
    setStatus("OTA begin err " + String(updateError), 1800);
    appendRuntimeLog("OTA", "begin failed err=" + String(updateError) +
                              " size=" + String(static_cast<unsigned>(size)), true);
    return false;
  }
  beginTransferUi("Flashing OTA", "writing flash", static_cast<uint32_t>(size), rgb565_local(255, 93, 88));
  uint8_t otaBuffer[4096];
  size_t written = 0;
  while (firmware.available() && written < size) {
    size_t toRead = min(sizeof(otaBuffer), size - written);
    int readBytes = firmware.read(otaBuffer, toRead);
    if (readBytes <= 0) {
      break;
    }
    size_t updateWritten = Update.write(otaBuffer, static_cast<size_t>(readBytes));
    if (updateWritten != static_cast<size_t>(readBytes)) {
      uint8_t updateError = Update.getError();
      Update.abort();
      firmware.close();
      finishTransferUi("flash write failed", false);
      setStatus("OTA write err " + String(updateError), 1800);
      appendRuntimeLog("OTA", "flash write failed err=" + String(updateError) +
                                " written=" + String(static_cast<unsigned>(written)), true);
      return false;
    }
    written += updateWritten;
    updateTransferUi(static_cast<uint32_t>(written), static_cast<uint32_t>(size), "writing flash");
    delay(1);
  }
  firmware.close();
  if (written != size) {
    uint8_t updateError = Update.getError();
    Update.abort();
    finishTransferUi("flash short", false);
    setStatus("OTA short err " + String(updateError), 1800);
    appendRuntimeLog("OTA", "short write err=" + String(updateError) +
                              " written=" + String(static_cast<unsigned>(written)) +
                              " size=" + String(static_cast<unsigned>(size)), true);
    return false;
  }
  if (!Update.isFinished()) {
    uint8_t updateError = Update.getError();
    Update.abort();
    finishTransferUi("flash incomplete", false);
    setStatus("OTA incomplete " + String(updateError), 1800);
    appendRuntimeLog("OTA", "not finished err=" + String(updateError), true);
    return false;
  }
  if (!Update.end(false)) {
    uint8_t updateError = Update.getError();
    finishTransferUi("finalize failed", false);
    setStatus("OTA end err " + String(updateError), 1800);
    appendRuntimeLog("OTA", "end failed err=" + String(updateError), true);
    return false;
  }
  const esp_partition_t* newBoot = esp_ota_get_boot_partition();
  if (!newBoot || (next && strcmp(newBoot->label, next->label) != 0)) {
    finishTransferUi("boot slot not set", false);
    setStatus("OTA boot slot failed", 2200);
    appendRuntimeLog("OTA", "boot slot mismatch expected=" + describePartition(next) +
                              " actual=" + describePartition(newBoot), true);
    return false;
  }
  recordOtaApplyIntent();
  appendRuntimeLog("OTA", "apply ok boot=" + describePartition(newBoot) + " reboot pending verify", true);
  finishTransferUi("rebooting", true);
  setStatus("OTA staged; rebooting", 900);
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
  ctx.emoji = fallbackTopicEmojiForTitle(label, threadId);
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
          String emoji = item["emoji"] | item["icon"] | "";
          int32_t threadId = item["thread_id"] | 0;
          String shortName = item["short"] | "";
          label = normalizeTopicTitleForDisplay(label, false);
          if (!label.isEmpty() && !key.isEmpty()) {
            ConversationContext ctx;
            ctx.label = label;
            ctx.emoji = emoji.isEmpty() ? fallbackTopicEmojiForTitle(label, threadId) : emoji;
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
  String url = hubBaseUrl() + "/api/cardputer/v1/topics?recent_days=7&limit=32";
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
    String title = item["display_title"] | item["title"] | "";
    String emoji = item["emoji"] | item["icon"] | item["emoji_name"] | "";
    int32_t threadId = item["thread_id"] | 0;
    if (topicKey.isEmpty()) {
      continue;
    }
    if (title.isEmpty()) {
      title = "Topic " + String(threadId > 0 ? threadId : static_cast<int>(loaded.size() + 1));
    }
    if (!emoji.isEmpty() && title.indexOf(emoji) < 0) {
      title = emoji + " " + title;
    }
    title = normalizeTopicTitleForDisplay(title, false);
    ConversationContext ctx;
    ctx.label = title;
    ctx.emoji = emoji.isEmpty() ? fallbackTopicEmojiForTitle(title, threadId) : emoji;
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

String defaultNewTopicTitle() {
  String title = gInputBuffer;
  title.trim();
  if (!title.isEmpty()) {
    title = normalizeEmojiDisplayText(title);
    if (title.length() > 42) {
      title = title.substring(0, 42);
      title.trim();
    }
    return title;
  }
  ensureBangkokTimezone();
  time_t now = time(nullptr);
  if (now > 100000) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "Cardputer %d %b %H:%M", &timeinfo);
    return String(buf);
  }
  return "Cardputer topic";
}

bool createRemoteTopic(const String& title, String& createdTopicKey) {
  if ((!gWifiReady && !ensureWifiForHttp("topic-create", 3000)) || gHubHost.isEmpty()) {
    gContextError = "Bridge offline";
    setStatus("Create offline", 1200);
    return false;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + "/api/cardputer/v1/topics/create";
  if (!configureHttpClient(http, secureClient, url, 15000)) {
    gContextError = "Create connect";
    setStatus("Create connect failed", 1200);
    return false;
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  http.addHeader("Content-Type", "application/json");
  DynamicJsonDocument req(512);
  req["title"] = title;
  String payload;
  serializeJson(req, payload);
  int code = http.POST(payload);
  String body = code > 0 ? http.getString() : "";
  http.end();
  if (code < 200 || code >= 300) {
    gContextError = "Create HTTP " + String(code);
    setStatus(gContextError, 1400);
    appendRuntimeLog("CTX", "create http " + String(code), true);
    return false;
  }
  DynamicJsonDocument doc(4096);
  auto err = deserializeJson(doc, body);
  if (err) {
    gContextError = "Create parse";
    setStatus("Create parse failed", 1400);
    return false;
  }
  createdTopicKey = doc["topic"]["topic_key"] | doc["topic_key"] | "";
  if (createdTopicKey.isEmpty()) {
    gContextError = "Create bad response";
    setStatus(gContextError, 1400);
    return false;
  }
  appendRuntimeLog("CTX", "created topic", true);
  return true;
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
  String url = hubBaseUrl() + "/api/cardputer/v1/topics/" + urlEncodePathSegment(topicKey) +
               "/history?limit=5&compact=1&text_chars=150";

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

  String responseTitle = doc["topic"]["display_title"] | doc["topic"]["title"] | doc["title"] | "";
  String responseEmoji = doc["topic"]["emoji"] | doc["topic"]["icon"] | "";
  if (!responseEmoji.isEmpty() && responseTitle.indexOf(responseEmoji) < 0) {
    responseTitle = responseEmoji + " " + responseTitle;
  }
  if (!responseTitle.isEmpty()) {
    responseTitle = normalizeTopicTitleForDisplay(responseTitle, false);
    if (!responseTitle.isEmpty() && responseTitle != "Topic") {
      gContexts[gSelectedContext].label = responseTitle;
      gContexts[gSelectedContext].emoji = responseEmoji.isEmpty()
                                              ? fallbackTopicEmojiForTitle(responseTitle, gContexts[gSelectedContext].threadId)
                                              : responseEmoji;
      gContexts[gSelectedContext].shortName = makeTopicShortName(responseTitle, gContexts[gSelectedContext].threadId);
    }
  }

  std::vector<ChatLine> historyChatLines;
  historyChatLines.reserve(kMaxContextPreviewLines);
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
      historyChatLines.push_back({
          topicHistoryPrefix(line.role, line.voice),
          trimPreview(line.text, 180),
          topicHistoryLineKind(line.role),
      });
    }
  }
  gContextHistoryKey = topicKey;
  gContextHistoryFetchedAtMs = millis();
  gContextError = "";
  replaceChatWithTopicHistory(gContexts[gSelectedContext].label, historyChatLines);
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

uint16_t readLe16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t readLe32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

struct WavInfo {
  uint32_t dataOffset = 0;
  uint32_t dataBytes = 0;
  uint32_t sampleRate = 0;
  uint16_t channels = 0;
  uint16_t bitsPerSample = 0;
  uint16_t blockAlign = 0;
  uint16_t audioFormat = 0;
  String error;
};

bool readWavBytes(File& file, uint8_t* out, size_t len) {
  return static_cast<size_t>(file.read(out, len)) == len;
}

bool parseWavInfo(File& file, WavInfo& info) {
  info = WavInfo();
  if (!file || file.size() < 12) {
    info.error = "small file";
    return false;
  }
  if (!file.seek(0)) {
    info.error = "seek failed";
    return false;
  }
  uint8_t riff[12];
  if (!readWavBytes(file, riff, sizeof(riff)) ||
      memcmp(riff, "RIFF", 4) != 0 ||
      memcmp(riff + 8, "WAVE", 4) != 0) {
    info.error = "not RIFF/WAVE";
    return false;
  }

  bool haveFmt = false;
  const uint32_t fileSize = static_cast<uint32_t>(file.size());
  while (file.position() + 8 <= fileSize) {
    uint8_t chunkHeader[8];
    if (!readWavBytes(file, chunkHeader, sizeof(chunkHeader))) {
      info.error = "chunk read";
      return false;
    }
    char id[5] = {
        static_cast<char>(chunkHeader[0]),
        static_cast<char>(chunkHeader[1]),
        static_cast<char>(chunkHeader[2]),
        static_cast<char>(chunkHeader[3]),
        0};
    uint32_t chunkSize = readLe32(chunkHeader + 4);
    uint32_t chunkStart = static_cast<uint32_t>(file.position());
    uint32_t nextChunk = chunkStart + chunkSize + (chunkSize & 1U);
    if (chunkStart > fileSize || chunkSize > fileSize - chunkStart) {
      info.error = String(id) + " size";
      return false;
    }

    if (memcmp(chunkHeader, "fmt ", 4) == 0) {
      if (chunkSize < 16) {
        info.error = "fmt small";
        return false;
      }
      uint8_t fmt[16];
      if (!readWavBytes(file, fmt, sizeof(fmt))) {
        info.error = "fmt read";
        return false;
      }
      info.audioFormat = readLe16(fmt);
      info.channels = readLe16(fmt + 2);
      info.sampleRate = readLe32(fmt + 4);
      info.blockAlign = readLe16(fmt + 12);
      info.bitsPerSample = readLe16(fmt + 14);
      haveFmt = true;
    } else if (memcmp(chunkHeader, "data", 4) == 0) {
      info.dataOffset = chunkStart;
      info.dataBytes = chunkSize;
      break;
    }

    if (!file.seek(nextChunk)) {
      info.error = String(id) + " seek";
      return false;
    }
  }

  if (!haveFmt) {
    info.error = "fmt missing";
    return false;
  }
  if (info.dataBytes == 0) {
    info.error = "data missing";
    return false;
  }
  if (info.audioFormat != 1 || info.bitsPerSample != 16 || info.channels != 1 || info.sampleRate < 8000) {
    info.error = "unsupported wav";
    return false;
  }
  if (info.blockAlign == 0) {
    info.blockAlign = static_cast<uint16_t>((info.bitsPerSample / 8U) * info.channels);
  }
  return true;
}

uint32_t wavDurationMs(const String& path, uint32_t* sampleRateOut = nullptr) {
  fs::FS* fs = activeVoiceFs();
  if (!fs || !fsExists(*fs, path)) {
    return 0;
  }
  File file = fs->open(path, "r");
  WavInfo info;
  bool ok = parseWavInfo(file, info);
  file.close();
  if (!ok) {
    appendRuntimeLog("AUDIO", "wav duration failed " + path + ": " + info.error, false);
    return 0;
  }
  if (sampleRateOut) {
    *sampleRateOut = info.sampleRate;
  }
  uint32_t bytesPerSampleFrame = max<uint32_t>(1, info.blockAlign);
  return static_cast<uint32_t>((static_cast<uint64_t>(info.dataBytes) * 1000ULL) /
                               (static_cast<uint64_t>(info.sampleRate) * bytesPerSampleFrame));
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
  gPlaybackStreamTotalBytes = 0;
  gPlaybackStreamSampleRate = 0;
  gPlaybackStreamNextBuffer = 0;
  gPlaybackStreamRawPcm = false;
  gPlaybackStartedMs = 0;
  gPlaybackDurationMs = 0;
  resetPlaybackAnalyzer();
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
  gPlaybackStreamTotalBytes = audioLen;
  gPlaybackStreamSampleRate = sampleRate;
  gPlaybackStreamNextBuffer = 0;
  gPlaybackStreamRawPcm = true;
  gPlaybackStartedMs = 0;
  gPlaybackDurationMs = playbackDurationMsForBytes(audioLen, sampleRate);
  resetPlaybackAnalyzer();
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
  WavInfo info;
  if (!parseWavInfo(file, info)) {
    String err = info.error.isEmpty() ? String("invalid wav") : info.error;
    file.close();
    setStatus("WAV " + err, 1600);
    setVoiceDiag("playback: " + err);
    appendRuntimeLog("AUDIO", "wav parse failed " + path + ": " + err, true);
    return false;
  }
  if (!file.seek(info.dataOffset)) {
    file.close();
    setStatus("WAV seek failed", 1400);
    setVoiceDiag("playback: seek failed");
    appendRuntimeLog("AUDIO", "wav data seek failed " + path, true);
    return false;
  }
  gPlaybackStreamFile = file;
  gPlaybackStreamPath = path;
  gPlaybackStreamRemainingBytes = info.dataBytes;
  gPlaybackStreamTotalBytes = info.dataBytes;
  gPlaybackStreamSampleRate = info.sampleRate;
  gPlaybackStreamNextBuffer = 0;
  gPlaybackStreamRawPcm = false;
  gPlaybackStartedMs = 0;
  gPlaybackDurationMs = playbackDurationMsForBytes(info.dataBytes, info.sampleRate);
  resetPlaybackAnalyzer();
  gPlaybackStreaming = true;
  gPlaybackActive = true;
  pulseButtonPress();
  setFaceMode(FaceMode::Speaking);
  setStatus("Playing voice...", 1200);
  setVoiceDiag(diag);
  appendRuntimeLog("AUDIO", "play " + path + " " + String(info.sampleRate) + "Hz " +
                            String(static_cast<unsigned>(info.dataBytes / 1024)) + "k", false);
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
    if (chunk > 1 && (chunk & 1U)) {
      --chunk;
    }
    if (chunk < sizeof(int16_t)) {
      gPlaybackStreamRemainingBytes = 0;
      break;
    }
    uint8_t* buffer = gPlaybackStreamBuffers[gPlaybackStreamNextBuffer];
    size_t readBytes = gPlaybackStreamFile.read(buffer, chunk);
    if (readBytes == 0) {
      gPlaybackStreamRemainingBytes = 0;
      break;
    }
    updatePlaybackAnalyzerFromSamples(reinterpret_cast<const int16_t*>(buffer),
                                      readBytes / sizeof(int16_t),
                                      gPlaybackStreamSampleRate);
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
    if (gPlaybackStartedMs == 0) {
      gPlaybackStartedMs = millis();
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

String normalizeActionText(String value) {
  value.trim();
  value.toLowerCase();
  value.replace("_", ".");
  value.replace("-", "_");
  return value;
}

bool actionBool(JsonVariantConst value, bool fallback = false) {
  if (value.is<bool>()) {
    return value.as<bool>();
  }
  if (value.is<int>()) {
    return value.as<int>() != 0;
  }
  String text = value.as<String>();
  text.trim();
  text.toLowerCase();
  if (text == "true" || text == "on" || text == "yes" || text == "1") {
    return true;
  }
  if (text == "false" || text == "off" || text == "no" || text == "0") {
    return false;
  }
  return fallback;
}

int actionInt(JsonObjectConst action, const char* key, int fallback, int minValue, int maxValue) {
  if (action[key].isNull()) {
    return fallback;
  }
  return clampValue<int>(action[key] | fallback, minValue, maxValue);
}

bool mapActionUiMode(String mode, UiMode& out) {
  mode.trim();
  mode.toLowerCase();
  mode.replace("-", "_");
  mode.replace(" ", "_");
  if (mode == "home" || mode == "pulse") {
    out = UiMode::Home;
  } else if (mode == "chat" || mode == "chat_eyes") {
    out = UiMode::ChatFace;
  } else if (mode == "chat_full" || mode == "full_chat") {
    out = UiMode::ChatFull;
  } else if (mode == "face" || mode == "eyes") {
    out = UiMode::Face;
  } else if (mode == "hero" || mode == "pet" || mode == "tama" ||
             mode == "tamagotchi" || mode == "companion") {
    out = UiMode::Hero;
  } else if (mode == "voice") {
    out = UiMode::Voice;
  } else if (mode == "library") {
    out = UiMode::Library;
  } else if (mode == "folders") {
    out = UiMode::AudioFolders;
  } else if (mode == "notes" || mode == "note" || mode == "notepad") {
    out = UiMode::Notes;
  } else if (mode == "assets") {
    out = UiMode::Assets;
  } else if (mode == "focus") {
    out = UiMode::Focus;
  } else if (mode == "topics" || mode == "contexts") {
    out = UiMode::Contexts;
  } else if (mode == "logs") {
    out = UiMode::Logs;
  } else if (mode == "settings") {
    out = UiMode::Settings;
  } else if (mode == "battery") {
    out = UiMode::Battery;
  } else if (mode == "ota" || mode == "firmware") {
    out = UiMode::Ota;
  } else {
    return false;
  }
  return true;
}

bool actionLocalPathAllowed(const String& path) {
  return path.startsWith("/audio/") || path.startsWith("/pomodoro/audio/") ||
         path.startsWith("/voice/") || path.startsWith("/reply/");
}

bool textMatchesQuery(const String& value, const String& query) {
  if (query.isEmpty()) {
    return false;
  }
  String hay = value;
  String needle = query;
  hay.toLowerCase();
  needle.toLowerCase();
  return hay.indexOf(needle) >= 0;
}

bool playAudioAssetByAction(JsonObjectConst action) {
  if (gAudioAssets.empty()) {
    scanAudioAssets();
  }
  if (gAudioAssets.empty()) {
    setStatus("No audio assets", 1200);
    return false;
  }

  int index = -1;
  if (!action["index"].isNull()) {
    int requested = actionInt(action, "index", 0, 0, static_cast<int>(gAudioAssets.size()));
    index = requested > 0 ? requested - 1 : 0;
  }
  String path = action["path"] | "";
  if (index < 0 && !path.isEmpty() && actionLocalPathAllowed(path)) {
    for (int i = 0; i < static_cast<int>(gAudioAssets.size()); ++i) {
      if (gAudioAssets[i].path == path) {
        index = i;
        break;
      }
    }
  }
  String query = action["query"] | "";
  if (index < 0 && !query.isEmpty()) {
    for (int i = 0; i < static_cast<int>(gAudioAssets.size()); ++i) {
      const auto& asset = gAudioAssets[i];
      if (textMatchesQuery(asset.title, query) || textMatchesQuery(asset.path, query) ||
          textMatchesQuery(asset.category, query)) {
        index = i;
        break;
      }
    }
  }
  if (index < 0 || index >= static_cast<int>(gAudioAssets.size())) {
    setStatus("Audio not found", 1200);
    return false;
  }
  gSelectedAudioAsset = index;
  setUiMode(UiMode::Library);
  return startPlaybackStreamFromWavFile(gAudioAssets[index].path, String("asset: ") + gAudioAssets[index].title);
}

bool playVoiceNoteByAction(JsonObjectConst action) {
  if (gVoiceNotes.empty()) {
    loadVoiceNotes();
  }
  if (gVoiceNotes.empty()) {
    setStatus("No voice notes", 1200);
    return false;
  }
  int index = -1;
  if (!action["index"].isNull()) {
    int requested = actionInt(action, "index", 0, 0, static_cast<int>(gVoiceNotes.size()));
    index = requested > 0 ? requested - 1 : 0;
  }
  String query = action["query"] | "";
  if (index < 0 && !query.isEmpty()) {
    for (int i = 0; i < static_cast<int>(gVoiceNotes.size()); ++i) {
      const auto& note = gVoiceNotes[i];
      if (textMatchesQuery(note.title, query) || textMatchesQuery(note.preview, query) ||
          textMatchesQuery(note.audioPath, query)) {
        index = i;
        break;
      }
    }
  }
  if (index < 0 || index >= static_cast<int>(gVoiceNotes.size())) {
    setStatus("Voice note not found", 1200);
    return false;
  }
  gSelectedVoiceNote = index;
  setUiMode(UiMode::Voice);
  return playVoiceNoteAt(index);
}

bool openTopicByAction(JsonObjectConst action) {
  if (gContexts.empty()) {
    startTopicTask(true, false, true, "Syncing topics...");
    setStatus("Topics syncing", 1200);
    return false;
  }
  int index = -1;
  if (!action["index"].isNull()) {
    int requested = actionInt(action, "index", 0, 0, static_cast<int>(gContexts.size()));
    index = requested > 0 ? requested - 1 : 0;
  }
  String key = action["key"] | "";
  String query = action["query"] | "";
  if (index < 0 && !key.isEmpty()) {
    for (int i = 0; i < static_cast<int>(gContexts.size()); ++i) {
      if (gContexts[i].key == key || gContexts[i].topicKey == key) {
        index = i;
        break;
      }
    }
  }
  if (index < 0 && !query.isEmpty()) {
    for (int i = 0; i < static_cast<int>(gContexts.size()); ++i) {
      if (textMatchesQuery(gContexts[i].label, query) || textMatchesQuery(gContexts[i].shortName, query)) {
        index = i;
        break;
      }
    }
  }
  if (index < 0 || index >= static_cast<int>(gContexts.size())) {
    setStatus("Topic not found", 1200);
    return false;
  }
  gSelectedContext = index;
  saveCurrentContext();
  showTopicOverlay(0, 1800);
  setUiMode(UiMode::ChatFace);
  startTopicTask(false, true, true, "Topic loading...");
  return true;
}

String normalizePetActionText(String value) {
  value.trim();
  value.toLowerCase();
  value.replace("-", ".");
  value.replace("_", ".");
  if (value.startsWith("pet.")) {
    value = value.substring(4);
  } else if (value.startsWith("tama.")) {
    value = value.substring(5);
  } else if (value.startsWith("tamagotchi.")) {
    value = value.substring(11);
  }
  return value;
}

bool executePetActionByName(String actionName) {
  String op = normalizePetActionText(actionName);
  bool executed = false;
  if (op == "open" || op == "status" || op == "show") {
    executed = true;
    setStatus("Pet screen", 900);
  } else if (op == "feed" || op == "food" || op == "eat") {
    executed = applyPetCare(PetCareAction::Feed);
  } else if (op == "play" || op == "fun") {
    executed = applyPetCare(PetCareAction::Play);
  } else if (op == "clean" || op == "wash" || op == "bath") {
    executed = applyPetCare(PetCareAction::Clean);
  } else if (op == "sleep" || op == "rest") {
    executed = applyPetCare(PetCareAction::Sleep);
  } else if (op == "medicine" || op == "heal" || op == "doctor") {
    executed = applyPetCare(PetCareAction::Medicine);
  } else if (op == "discipline" || op == "train" || op == "scold") {
    executed = applyPetCare(PetCareAction::Discipline);
  } else if (op == "hunt" || op == "wifi" || op == "forage") {
    executed = applyPetCare(PetCareAction::Hunt);
  } else if (op == "explore" || op == "scan") {
    executed = applyPetCare(PetCareAction::Explore);
  } else if (op == "reset" || op == "new") {
    resetPet(true);
    executed = true;
  }
  if (executed) {
    setUiMode(UiMode::Hero);
  }
  return executed;
}

bool executeDeviceAction(JsonObjectConst action) {
  String type = normalizeActionText(action["type"] | "");
  if (type.isEmpty()) {
    return false;
  }

  String petType = type;
  petType.replace("_", ".");
  if (petType == "pet.care" || petType == "tama.care" || petType == "tamagotchi.care") {
    String care = action["action"] | "";
    if (care.isEmpty()) {
      care = action["value"] | "";
    }
    return executePetActionByName(care);
  }
  if (petType == "pet" || petType == "tama" || petType == "tamagotchi" ||
      petType.startsWith("pet.") || petType.startsWith("tama.") || petType.startsWith("tamagotchi.")) {
    return executePetActionByName(petType);
  }

  if (type == "focus.set_duration" || type == "focus.start") {
    if (!action["minutes"].isNull()) {
      uint32_t seconds = static_cast<uint32_t>(actionInt(action, "minutes", gFocusSettings.focusSec / 60, 1, 60)) * 60UL;
      gFocusSettings.focusSec = clampValue<uint32_t>(seconds, kFocusMinDurationSec, kFocusMaxDurationSec);
      if (gFocus.state == FocusState::Stopped) {
        gFocus.remainingSec = gFocusSettings.focusSec;
        gFocus.sessionTotalSec = gFocusSettings.focusSec;
      }
      saveFocusSettings();
    }
    setUiMode(UiMode::Focus);
    if (type == "focus.start" && !focusStateRuns(gFocus.state)) {
      focusStartOrResume();
    }
    setStatus("Action: focus " + String(gFocusSettings.focusSec / 60) + "m", 1200);
    return true;
  }
  if (type == "focus.pause") {
    setUiMode(UiMode::Focus);
    if (focusStateRuns(gFocus.state)) {
      focusStartOrResume();
    }
    return true;
  }
  if (type == "focus.resume") {
    setUiMode(UiMode::Focus);
    if (gFocus.state == FocusState::Paused || gFocus.state == FocusState::Stopped) {
      focusStartOrResume();
    }
    return true;
  }
  if (type == "focus.reset") {
    focusReset();
    setUiMode(UiMode::Focus);
    return true;
  }
  if (type == "focus.set_short_break") {
    gFocusSettings.shortBreakSec = static_cast<uint32_t>(actionInt(action, "minutes", gFocusSettings.shortBreakSec / 60, 1, 60)) * 60UL;
    saveFocusSettings();
    setStatus("Action: short break " + String(gFocusSettings.shortBreakSec / 60) + "m", 1200);
    return true;
  }
  if (type == "focus.set_long_break") {
    gFocusSettings.longBreakSec = static_cast<uint32_t>(actionInt(action, "minutes", gFocusSettings.longBreakSec / 60, 1, 60)) * 60UL;
    saveFocusSettings();
    setStatus("Action: long break " + String(gFocusSettings.longBreakSec / 60) + "m", 1200);
    return true;
  }
  if (type == "focus.set_cycles") {
    gFocusSettings.cyclesPerRound = static_cast<uint8_t>(actionInt(action, "cycles", gFocusSettings.cyclesPerRound, 1, 9));
    saveFocusSettings();
    setStatus("Action: cycles " + String(gFocusSettings.cyclesPerRound), 1200);
    return true;
  }
  if (type == "focus.set_autostart") {
    gFocusSettings.autoStart = actionBool(action["value"], gFocusSettings.autoStart);
    saveFocusSettings();
    setStatus(gFocusSettings.autoStart ? "Action: auto on" : "Action: auto off", 1200);
    return true;
  }
  if (type == "focus.set_metronome") {
    gFocusSettings.metronome = actionBool(action["value"], gFocusSettings.metronome);
    saveFocusSettings();
    setStatus(gFocusSettings.metronome ? "Action: metro on" : "Action: metro off", 1200);
    return true;
  }
  if (type == "focus.set_bpm") {
    gFocusSettings.bpm = static_cast<uint16_t>(actionInt(action, "bpm", gFocusSettings.bpm, kFocusMinBpm, kFocusMaxBpm));
    saveFocusSettings();
    setStatus("Action: bpm " + String(gFocusSettings.bpm), 1200);
    return true;
  }
  if (type == "ui.open") {
    UiMode mode;
    if (mapActionUiMode(action["mode"] | "", mode)) {
      setUiMode(mode);
      return true;
    }
    return false;
  }
  if (type == "audio.play") {
    return playAudioAssetByAction(action);
  }
  if (type == "voice.play") {
    return playVoiceNoteByAction(action);
  }
  if (type == "settings.set_audio_language") {
    String language = action["value"] | "";
    language.trim();
    language.toLowerCase();
    if (language == "ru" || language == "en" || language == "es") {
      gDeviceSettings.audioLanguage = language;
      saveDeviceSettings();
      setStatus("Action: lang " + language, 1200);
      return true;
    }
    return false;
  }
  if (type == "settings.set_ota_auto") {
    gDeviceSettings.autoUpdateFirmware = actionBool(action["value"], gDeviceSettings.autoUpdateFirmware);
    saveDeviceSettings();
    setStatus(gDeviceSettings.autoUpdateFirmware ? "Action: OTA auto" : "Action: OTA manual", 1200);
    return true;
  }
  if (type == "settings.set_ota_boot_check") {
    gDeviceSettings.checkUpdatesOnBoot = actionBool(action["value"], gDeviceSettings.checkUpdatesOnBoot);
    saveDeviceSettings();
    setStatus(gDeviceSettings.checkUpdatesOnBoot ? "Action: boot check" : "Action: no boot check", 1200);
    return true;
  }
  if (type == "settings.set_ota_min_battery") {
    gDeviceSettings.minBatteryForUpdate = static_cast<uint8_t>(actionInt(action, "value", gDeviceSettings.minBatteryForUpdate, 5, 95));
    saveDeviceSettings();
    setStatus("Action: OTA battery " + String(gDeviceSettings.minBatteryForUpdate) + "%", 1200);
    return true;
  }
  if (type == "topic.next") {
    return switchTopicRelative(1, true);
  }
  if (type == "topic.prev") {
    return switchTopicRelative(-1, true);
  }
  if (type == "topic.open") {
    return openTopicByAction(action);
  }
  return false;
}

bool executeDeviceActions(JsonVariantConst actions) {
  if (actions.isNull()) {
    return false;
  }
  bool executed = false;
  if (actions.is<JsonArrayConst>()) {
    uint8_t count = 0;
    for (JsonObjectConst action : actions.as<JsonArrayConst>()) {
      if (count++ >= 4) {
        break;
      }
      executed = executeDeviceAction(action) || executed;
    }
  } else if (actions.is<JsonObjectConst>()) {
    executed = executeDeviceAction(actions.as<JsonObjectConst>());
  }
  if (executed) {
    appendRuntimeLog("ACT", "device action executed", true);
  }
  return executed;
}

bool executeDeviceActionsJson(const String& json) {
  if (json.isEmpty()) {
    return false;
  }
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    appendRuntimeLog("ACT", "device action json rejected", true);
    return false;
  }
  return executeDeviceActions(doc.as<JsonVariantConst>());
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
  } else if (mode == UiMode::Notes && !gNotesScanned) {
    scanNotes();
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

void togglePulseChatDisplay() {
  gLauncherVisible = false;
  if (gUiMode == UiMode::Home) {
    setUiMode(UiMode::ChatFull);
    setStatus("View: full chat", 900);
  } else if (gUiMode == UiMode::ChatFull || gUiMode == UiMode::ChatFace) {
    setUiMode(UiMode::Home);
    setStatus("View: pulse", 900);
  } else {
    setUiMode(UiMode::Home);
    setStatus("View: pulse", 900);
  }
  showTopicOverlay(0, 1200);
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

uint8_t petClampStat(int value) {
  return static_cast<uint8_t>(clampValue<int>(value, 0, 100));
}

uint32_t petEpochNow() {
  if (!hasValidSystemTime()) {
    return 0;
  }
  time_t now = time(nullptr);
  return now > 0 ? static_cast<uint32_t>(now) : 0;
}

const char* petStageName(PetStage stage) {
  switch (stage) {
    case PetStage::Egg:
      return "EGG";
    case PetStage::Baby:
      return "BABY";
    case PetStage::Teen:
      return "TEEN";
    case PetStage::Adult:
      return "ADULT";
    case PetStage::Elder:
    default:
      return "ELDER";
  }
}

const char* petMoodName(PetMood mood) {
  switch (mood) {
    case PetMood::Happy:
      return "HAPPY";
    case PetMood::Hungry:
      return "HUNGRY";
    case PetMood::Bored:
      return "BORED";
    case PetMood::Curious:
      return "CURIOUS";
    case PetMood::Sick:
      return "SICK";
    case PetMood::Excited:
      return "EXCITED";
    case PetMood::Sleepy:
      return "SLEEPY";
    case PetMood::Dirty:
      return "DIRTY";
    case PetMood::Dead:
      return "DEAD";
    case PetMood::Calm:
    default:
      return "CALM";
  }
}

const char* petActivityName(PetActivity activity) {
  switch (activity) {
    case PetActivity::Eating:
      return "EATING";
    case PetActivity::Playing:
      return "PLAY";
    case PetActivity::Cleaning:
      return "CLEAN";
    case PetActivity::Sleeping:
      return "SLEEP";
    case PetActivity::Hunting:
      return "HUNT";
    case PetActivity::Exploring:
      return "EXPLORE";
    case PetActivity::Medicine:
      return "MED";
    case PetActivity::Discipline:
      return "DISC";
    case PetActivity::Evolving:
      return "EVOLVE";
    case PetActivity::Dead:
      return "DEAD";
    case PetActivity::Idle:
    default:
      return "IDLE";
  }
}

String petAgeText() {
  uint32_t days = gPet.ageMinutes / 1440UL;
  uint32_t hours = (gPet.ageMinutes / 60UL) % 24UL;
  uint32_t minutes = gPet.ageMinutes % 60UL;
  if (days > 0) {
    return String(days) + "d " + String(hours) + "h";
  }
  if (hours > 0) {
    return String(hours) + "h " + String(minutes) + "m";
  }
  return String(minutes) + "m";
}

uint8_t petCareAverage() {
  return static_cast<uint8_t>((gPet.hunger + gPet.happiness + gPet.health +
                               gPet.energy + gPet.cleanliness + gPet.discipline) / 6);
}

void petSetDefaults(bool fullReset) {
  uint32_t keepAge = fullReset ? 0 : gPet.ageMinutes;
  uint32_t keepBorn = fullReset ? petEpochNow() : gPet.bornEpoch;
  gPet = PetRuntime();
  gPet.hunger = 70;
  gPet.happiness = 70;
  gPet.health = 75;
  gPet.energy = 80;
  gPet.cleanliness = 80;
  gPet.discipline = 45;
  gPet.traitCuriosity = static_cast<uint8_t>(random(45, 91));
  gPet.traitActivity = static_cast<uint8_t>(random(35, 86));
  gPet.traitStress = static_cast<uint8_t>(random(20, 76));
  gPet.ageMinutes = keepAge;
  gPet.bornEpoch = keepBorn;
  gPet.lastSavedEpoch = petEpochNow();
  gPet.lastTickMs = millis();
  gPet.lastMinuteMs = millis();
  gPet.lastSaveMs = 0;
  gPet.nextDecisionMs = static_cast<uint32_t>(random(kPetDecisionMinMs, kPetDecisionMaxMs));
  gPet.stage = gPet.ageMinutes < 3 ? PetStage::Egg : PetStage::Baby;
  gPet.mood = PetMood::Calm;
  gPet.activity = PetActivity::Idle;
}

void savePetState(bool force) {
  uint32_t nowMs = millis();
  if (!force && nowMs - gPet.lastSaveMs < kPetAutoSaveMs) {
    return;
  }
  uint32_t nowEpoch = petEpochNow();
  gPrefs.begin(kPrefsPetNs, false);
  gPrefs.putUChar("magic", 0xC1);
  gPrefs.putUChar("hun", gPet.hunger);
  gPrefs.putUChar("hap", gPet.happiness);
  gPrefs.putUChar("hea", gPet.health);
  gPrefs.putUChar("ene", gPet.energy);
  gPrefs.putUChar("cln", gPet.cleanliness);
  gPrefs.putUChar("disc", gPet.discipline);
  gPrefs.putUChar("poop", gPet.poop);
  gPrefs.putUChar("mist", gPet.careMistakes);
  gPrefs.putUChar("cur", gPet.traitCuriosity);
  gPrefs.putUChar("act", gPet.traitActivity);
  gPrefs.putUChar("str", gPet.traitStress);
  gPrefs.putUInt("age", gPet.ageMinutes);
  gPrefs.putUInt("born", gPet.bornEpoch);
  gPrefs.putUInt("epoch", nowEpoch);
  gPrefs.putBool("alive", gPet.alive);
  gPrefs.putBool("sick", gPet.sick);
  gPrefs.putUChar("stage", static_cast<uint8_t>(gPet.stage));
  gPrefs.end();
  gPet.lastSavedEpoch = nowEpoch;
  gPet.lastSaveMs = nowMs;
}

void updatePetMood() {
  if (!gPet.alive) {
    gPet.mood = PetMood::Dead;
    gPet.activity = PetActivity::Dead;
    return;
  }
  if (gPet.activity == PetActivity::Sleeping) {
    gPet.mood = PetMood::Sleepy;
    return;
  }
  if (gPet.sick || gPet.health < 28) {
    gPet.mood = PetMood::Sick;
    return;
  }
  if (gPet.hunger < 25) {
    gPet.mood = PetMood::Hungry;
    return;
  }
  if (gPet.cleanliness < 25 || gPet.poop >= 4) {
    gPet.mood = PetMood::Dirty;
    return;
  }
  if (gPet.energy < 18) {
    gPet.mood = PetMood::Sleepy;
    return;
  }
  if (gPet.happiness < 30) {
    gPet.mood = PetMood::Bored;
    return;
  }
  if (gPet.env.hiddenCount > 0 || gPet.env.openCount > 0) {
    gPet.mood = PetMood::Curious;
    return;
  }
  uint8_t avg = petCareAverage();
  if (avg > 82 && gPet.env.netCount > 3) {
    gPet.mood = PetMood::Excited;
  } else if (avg > 64) {
    gPet.mood = PetMood::Happy;
  } else {
    gPet.mood = PetMood::Calm;
  }
}

void maybePetDeath() {
  if (!gPet.alive) {
    return;
  }
  bool collapse = gPet.health == 0 ||
                  (gPet.hunger == 0 && gPet.energy == 0 && gPet.cleanliness < 15) ||
                  gPet.careMistakes >= 20;
  if (!collapse) {
    return;
  }
  gPet.alive = false;
  gPet.sick = false;
  gPet.activity = PetActivity::Dead;
  gPet.mood = PetMood::Dead;
  gPet.restPhase = PetRestPhase::None;
  setStatus("Pet lost. /petreset", 2500);
  appendRuntimeLog("PET", "dead age=" + String(gPet.ageMinutes) +
                          " mistakes=" + String(gPet.careMistakes), true);
  savePetState(true);
}

void updatePetEvolution() {
  if (!gPet.alive || gPet.activity == PetActivity::Dead) {
    return;
  }
  PetStage target = gPet.stage;
  uint8_t avg = petCareAverage();
  if (gPet.ageMinutes >= 360 && avg >= 42) {
    target = PetStage::Elder;
  } else if (gPet.ageMinutes >= 120 && avg >= 40) {
    target = PetStage::Adult;
  } else if (gPet.ageMinutes >= 30 && avg >= 35) {
    target = PetStage::Teen;
  } else if (gPet.ageMinutes >= 3) {
    target = PetStage::Baby;
  } else {
    target = PetStage::Egg;
  }
  if (static_cast<uint8_t>(target) <= static_cast<uint8_t>(gPet.stage)) {
    return;
  }
  gPet.stage = target;
  gPet.activity = PetActivity::Evolving;
  gPet.actionUntilMs = millis() + 4500;
  setStatus(String("Pet evolved: ") + petStageName(target), 1600);
  focusTone(1568, 120);
  appendRuntimeLog("PET", String("evolved ") + petStageName(target), true);
  savePetState(true);
}

void applyPetElapsedMinute(bool offline) {
  if (!gPet.alive) {
    return;
  }
  ++gPet.ageMinutes;

  bool sleeping = gPet.activity == PetActivity::Sleeping;
  if (sleeping) {
    gPet.hunger = petClampStat(gPet.hunger - 1);
    gPet.happiness = petClampStat(gPet.happiness - 1);
    gPet.energy = petClampStat(gPet.energy + 8);
    gPet.cleanliness = petClampStat(gPet.cleanliness - 1);
    if (gPet.hunger > 20 && gPet.cleanliness > 25) {
      gPet.health = petClampStat(gPet.health + 2);
    }
  } else {
    uint8_t happyDrain = (gPet.env.lastScanMs == 0 || millis() - gPet.env.lastScanMs > 180000) ? 2 : 1;
    gPet.hunger = petClampStat(gPet.hunger - 2);
    gPet.happiness = petClampStat(gPet.happiness - happyDrain);
    gPet.energy = petClampStat(gPet.energy - 1);
    gPet.cleanliness = petClampStat(gPet.cleanliness - 1);
  }

  if ((gPet.ageMinutes % 9UL) == 0 && gPet.poop < kPetMaxPoop) {
    uint8_t chance = offline ? 25 : static_cast<uint8_t>(12 + max<int>(0, gPet.hunger - 65) / 3);
    if (random(0, 100) < chance) {
      ++gPet.poop;
      gPet.cleanliness = petClampStat(gPet.cleanliness - 10);
    }
  }
  if ((gPet.ageMinutes % 10UL) == 0 && gPet.discipline > 0) {
    gPet.discipline = petClampStat(gPet.discipline - 1);
  }

  if (gPet.hunger < 18 || gPet.cleanliness < 18 || gPet.poop >= 4) {
    gPet.health = petClampStat(gPet.health - 3);
  } else if (gPet.happiness < 18 || gPet.energy < 10) {
    gPet.health = petClampStat(gPet.health - 1);
  } else if (!sleeping && gPet.health < 92 && gPet.hunger > 55 && gPet.cleanliness > 55) {
    gPet.health = petClampStat(gPet.health + 1);
  }

  if (gPet.health < 25) {
    gPet.sick = true;
  }
  if (gPet.hunger == 0 || gPet.cleanliness == 0 || gPet.poop >= kPetMaxPoop) {
    gPet.careMistakes = petClampStat(gPet.careMistakes + 1);
  }
  if (gPet.energy >= 100 && sleeping && gPet.restPhase == PetRestPhase::Deep) {
    gPet.restPhase = PetRestPhase::Wake;
    gPet.restPhaseStartMs = millis();
  }
  updatePetMood();
  updatePetEvolution();
  maybePetDeath();
}

void applyPetElapsedMinutes(uint32_t minutes, bool offline) {
  minutes = min<uint32_t>(minutes, kPetOfflineMaxMinutes);
  for (uint32_t i = 0; i < minutes; ++i) {
    applyPetElapsedMinute(offline);
    if (!gPet.alive) {
      break;
    }
  }
}

void loadPetState() {
  gPrefs.begin(kPrefsPetNs, true);
  uint8_t magic = gPrefs.getUChar("magic", 0);
  if (magic != 0xC1) {
    gPrefs.end();
    petSetDefaults(true);
    savePetState(true);
    appendRuntimeLog("PET", "new pet created", true);
    return;
  }
  gPet.hunger = gPrefs.getUChar("hun", 70);
  gPet.happiness = gPrefs.getUChar("hap", 70);
  gPet.health = gPrefs.getUChar("hea", 75);
  gPet.energy = gPrefs.getUChar("ene", 80);
  gPet.cleanliness = gPrefs.getUChar("cln", 80);
  gPet.discipline = gPrefs.getUChar("disc", 45);
  gPet.poop = min<uint8_t>(gPrefs.getUChar("poop", 0), kPetMaxPoop);
  gPet.careMistakes = gPrefs.getUChar("mist", 0);
  gPet.traitCuriosity = gPrefs.getUChar("cur", 70);
  gPet.traitActivity = gPrefs.getUChar("act", 60);
  gPet.traitStress = gPrefs.getUChar("str", 40);
  gPet.ageMinutes = gPrefs.getUInt("age", 0);
  gPet.bornEpoch = gPrefs.getUInt("born", 0);
  uint32_t savedEpoch = gPrefs.getUInt("epoch", 0);
  gPet.lastSavedEpoch = savedEpoch;
  gPet.alive = gPrefs.getBool("alive", true);
  gPet.sick = gPrefs.getBool("sick", false);
  uint8_t stageRaw = clampValue<uint8_t>(gPrefs.getUChar("stage", 0), 0, 4);
  gPet.stage = static_cast<PetStage>(stageRaw);
  gPrefs.end();

  gPet.lastTickMs = millis();
  gPet.lastMinuteMs = millis();
  gPet.lastSaveMs = millis();
  gPet.activity = gPet.alive ? PetActivity::Idle : PetActivity::Dead;
  gPet.restPhase = PetRestPhase::None;
  gPet.nextDecisionMs = static_cast<uint32_t>(random(kPetDecisionMinMs, kPetDecisionMaxMs));

  uint32_t nowEpoch = petEpochNow();
  if (nowEpoch > savedEpoch + 90 && savedEpoch > 0) {
    uint32_t offlineMinutes = (nowEpoch - savedEpoch) / 60UL;
    applyPetElapsedMinutes(offlineMinutes, true);
    appendRuntimeLog("PET", "offline minutes=" + String(min<uint32_t>(offlineMinutes, kPetOfflineMaxMinutes)), true);
    savePetState(true);
  } else {
    updatePetMood();
    updatePetEvolution();
  }
}

void resetPet(bool fullReset) {
  petSetDefaults(fullReset);
  updatePetMood();
  setStatus(fullReset ? "Pet reset" : "Pet restored", 1200);
  appendRuntimeLog("PET", fullReset ? "reset full" : "reset stats", true);
  savePetState(true);
}

void setPetActivity(PetActivity activity, uint32_t ttlMs = kPetActionMs) {
  gPet.activity = activity;
  gPet.actionUntilMs = ttlMs > 0 ? millis() + ttlMs : 0;
  if (activity != PetActivity::Sleeping) {
    gPet.restPhase = PetRestPhase::None;
  }
}

void beginPetRest() {
  gPet.activity = PetActivity::Sleeping;
  gPet.restPhase = PetRestPhase::Enter;
  gPet.restPhaseStartMs = millis();
  gPet.restDurationMs = static_cast<uint32_t>(random(kPetRestMinMs, kPetRestMaxMs));
  gPet.restStatsApplied = false;
  gPet.actionUntilMs = 0;
  setStatus("Pet sleeping", 1000);
}

void wakePet() {
  gPet.activity = PetActivity::Idle;
  gPet.restPhase = PetRestPhase::None;
  gPet.energy = petClampStat(gPet.energy + 4);
  setStatus("Pet awake", 900);
}

void useConnectedWifiAsPetEnv() {
  gPet.env.netCount = WiFi.status() == WL_CONNECTED ? 1 : 0;
  gPet.env.hiddenCount = 0;
  gPet.env.openCount = 0;
  int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -100;
  gPet.env.strongCount = rssi > -67 ? 1 : 0;
  gPet.env.avgRssi = static_cast<int16_t>(rssi);
  gPet.env.lastScanMs = millis();
  gPet.env.scanFresh = false;
}

void resolvePetHunt() {
  if (gPet.env.netCount <= 0) {
    gPet.hunger = petClampStat(gPet.hunger - 10);
    gPet.happiness = petClampStat(gPet.happiness - 6);
    gPet.health = petClampStat(gPet.health - 4);
    setStatus("Hunt found nothing", 1200);
    focusTone(220, 120);
  } else {
    int hungerDelta = min<int>(34, gPet.env.netCount * 2 + gPet.env.strongCount * 4);
    int variety = gPet.env.hiddenCount * 2 + gPet.env.openCount;
    int happyDelta = min<int>(26, variety * 3 + max<int>(0, gPet.env.avgRssi + 95) / 3);
    int healthDelta = (gPet.env.avgRssi > -75 ? 4 : 0) + (gPet.env.avgRssi > -63 ? 4 : 0);
    gPet.hunger = petClampStat(gPet.hunger + hungerDelta);
    gPet.happiness = petClampStat(gPet.happiness + happyDelta);
    gPet.health = petClampStat(gPet.health + healthDelta);
    gPet.energy = petClampStat(gPet.energy - 4);
    setStatus("Hunt +" + String(hungerDelta) + " food", 1200);
    focusTone(1047, 90);
  }
  setPetActivity(PetActivity::Eating, 3800);
  updatePetMood();
  savePetState(true);
}

void resolvePetExplore() {
  if (gPet.env.netCount <= 0) {
    gPet.happiness = petClampStat(gPet.happiness - 5);
    gPet.hunger = petClampStat(gPet.hunger - 3);
    setStatus("Explore quiet", 1000);
    focusTone(330, 90);
  } else {
    int curiosity = gPet.env.hiddenCount * 4 + gPet.env.openCount * 3 + gPet.env.netCount;
    int happyDelta = min<int>(32, curiosity / 2);
    gPet.happiness = petClampStat(gPet.happiness + happyDelta);
    gPet.hunger = petClampStat(gPet.hunger - 5);
    gPet.energy = petClampStat(gPet.energy - 5);
    setStatus("Explore +" + String(happyDelta) + " fun", 1000);
    focusTone(1319, 80);
  }
  setPetActivity(PetActivity::Exploring, 3600);
  updatePetMood();
  savePetState(true);
}

bool startPetWifiScan(PetActivity activity) {
  if (gPet.env.scanPending) {
    setStatus("Pet scan running", 700);
    return false;
  }
  if (!gPet.alive) {
    setStatus("Pet is gone", 1200);
    return false;
  }
  gPet.activity = activity;
  gPet.actionUntilMs = 0;
  gPet.env.scanPending = true;
  gPet.env.lastScanMs = millis();
  WiFi.scanDelete();
  int started = WiFi.scanNetworks(true, true);
  if (started == -2) {
    gPet.env.scanPending = false;
    useConnectedWifiAsPetEnv();
    if (activity == PetActivity::Hunting) {
      resolvePetHunt();
    } else {
      resolvePetExplore();
    }
    return true;
  }
  setStatus(activity == PetActivity::Hunting ? "Pet hunting Wi-Fi" : "Pet exploring", 1000);
  return true;
}

void completePetWifiScan(int count) {
  if (count < 0) {
    useConnectedWifiAsPetEnv();
  } else {
    int totalRssi = 0;
    int seen = min<int>(count, 48);
    gPet.env.netCount = seen;
    gPet.env.hiddenCount = 0;
    gPet.env.openCount = 0;
    gPet.env.strongCount = 0;
    for (int i = 0; i < seen; ++i) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      totalRssi += rssi;
      if (ssid.isEmpty()) {
        ++gPet.env.hiddenCount;
      }
      if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
        ++gPet.env.openCount;
      }
      if (rssi > -67) {
        ++gPet.env.strongCount;
      }
    }
    gPet.env.avgRssi = seen > 0 ? static_cast<int16_t>(totalRssi / seen) : -100;
    gPet.env.lastScanMs = millis();
    gPet.env.scanFresh = true;
    WiFi.scanDelete();
  }
  gPet.env.scanPending = false;
  if (gPet.activity == PetActivity::Hunting) {
    resolvePetHunt();
  } else if (gPet.activity == PetActivity::Exploring) {
    resolvePetExplore();
  }
}

void servicePetWifiScan() {
  if (!gPet.env.scanPending) {
    return;
  }
  int count = WiFi.scanComplete();
  if (count == -1 && millis() - gPet.env.lastScanMs < 16000) {
    return;
  }
  completePetWifiScan(count >= 0 ? count : -1);
}

bool applyPetCare(PetCareAction action) {
  if (!gPet.alive) {
    setStatus("Pet is gone. /petreset", 1600);
    return false;
  }
  gPet.lastInteractionMs = millis();
  switch (action) {
    case PetCareAction::Feed: {
      int before = gPet.hunger;
      gPet.hunger = petClampStat(gPet.hunger + 22);
      gPet.energy = petClampStat(gPet.energy - 4);
      gPet.happiness = petClampStat(gPet.happiness + 4);
      gPet.cleanliness = petClampStat(gPet.cleanliness - 2);
      if (before > 88) {
        gPet.health = petClampStat(gPet.health - 4);
        gPet.poop = min<uint8_t>(kPetMaxPoop, gPet.poop + 1);
        setStatus("Pet overfed", 1100);
      } else {
        if (before < 25) {
          gPet.health = petClampStat(gPet.health + 4);
        }
        if (random(0, 100) < 35 && gPet.poop < kPetMaxPoop) {
          ++gPet.poop;
        }
        setStatus("Pet fed", 900);
      }
      setPetActivity(PetActivity::Eating);
      focusTone(988, 70);
      break;
    }
    case PetCareAction::Play:
      if (gPet.energy < 10 || gPet.health < 15) {
        setStatus("Too tired to play", 1100);
        return false;
      }
      gPet.happiness = petClampStat(gPet.happiness + 18);
      gPet.energy = petClampStat(gPet.energy - 14);
      gPet.hunger = petClampStat(gPet.hunger - 8);
      gPet.cleanliness = petClampStat(gPet.cleanliness - 4);
      gPet.discipline = petClampStat(gPet.discipline + 2);
      setPetActivity(PetActivity::Playing);
      setStatus("Pet played", 900);
      focusTone(1175, 80);
      break;
    case PetCareAction::Clean:
      gPet.cleanliness = 100;
      gPet.poop = 0;
      gPet.health = petClampStat(gPet.health + 6);
      gPet.happiness = petClampStat(gPet.happiness + 2);
      setPetActivity(PetActivity::Cleaning);
      setStatus("Pet clean", 900);
      focusTone(1397, 70);
      break;
    case PetCareAction::Sleep:
      if (gPet.activity == PetActivity::Sleeping) {
        wakePet();
      } else {
        beginPetRest();
      }
      break;
    case PetCareAction::Medicine:
      if (!gPet.sick && gPet.health > 64) {
        setStatus("No medicine needed", 1000);
        return false;
      }
      gPet.health = petClampStat(gPet.health + 28);
      gPet.energy = petClampStat(gPet.energy - 3);
      gPet.happiness = petClampStat(gPet.happiness - 4);
      if (gPet.health > 42) {
        gPet.sick = false;
      }
      setPetActivity(PetActivity::Medicine);
      setStatus("Medicine given", 1000);
      focusTone(784, 100);
      break;
    case PetCareAction::Discipline: {
      bool fair = gPet.poop >= 3 || gPet.cleanliness < 30 || gPet.discipline < 20 || gPet.hunger > 92;
      gPet.discipline = petClampStat(gPet.discipline + (fair ? 16 : 5));
      gPet.happiness = petClampStat(gPet.happiness - (fair ? 5 : 12));
      if (!fair) {
        gPet.careMistakes = petClampStat(gPet.careMistakes + 1);
      }
      setPetActivity(PetActivity::Discipline);
      setStatus(fair ? "Discipline understood" : "Unfair discipline", 1200);
      focusTone(fair ? 880 : 262, 90);
      break;
    }
    case PetCareAction::Hunt:
      return startPetWifiScan(PetActivity::Hunting);
    case PetCareAction::Explore:
      return startPetWifiScan(PetActivity::Exploring);
  }
  updatePetMood();
  updatePetEvolution();
  maybePetDeath();
  savePetState(true);
  return true;
}

void servicePetRest() {
  if (gPet.activity != PetActivity::Sleeping || gPet.restPhase == PetRestPhase::None) {
    return;
  }
  uint32_t now = millis();
  if (gPet.restPhase == PetRestPhase::Enter && now - gPet.restPhaseStartMs > 1600) {
    gPet.restPhase = PetRestPhase::Deep;
    gPet.restPhaseStartMs = now;
    gPet.restStatsApplied = false;
    return;
  }
  if (gPet.restPhase == PetRestPhase::Deep) {
    if (!gPet.restStatsApplied && now - gPet.restPhaseStartMs > gPet.restDurationMs / 2) {
      gPet.happiness = petClampStat(gPet.happiness + 10);
      gPet.health = petClampStat(gPet.health + 15);
      gPet.energy = petClampStat(gPet.energy + 22);
      gPet.hunger = petClampStat(gPet.hunger - 3);
      gPet.restStatsApplied = true;
      savePetState(true);
    }
    if (now - gPet.restPhaseStartMs >= gPet.restDurationMs || gPet.energy >= 100) {
      gPet.restPhase = PetRestPhase::Wake;
      gPet.restPhaseStartMs = now;
      focusTone(659, 80);
    }
    return;
  }
  if (gPet.restPhase == PetRestPhase::Wake && now - gPet.restPhaseStartMs > 1600) {
    wakePet();
    savePetState(true);
  }
}

void decideNextPetActivity() {
  if (!gPet.alive || gPet.env.scanPending || gPet.activity != PetActivity::Idle ||
      gUiMode != UiMode::Hero || gRecording || gSubmitting || gThinking || gPlaybackActive) {
    return;
  }
  uint32_t now = millis();
  if (now - gPet.lastDecisionMs < gPet.nextDecisionMs) {
    return;
  }
  gPet.lastDecisionMs = now;
  gPet.nextDecisionMs = static_cast<uint32_t>(random(kPetDecisionMinMs, kPetDecisionMaxMs));

  int desireHunt = (100 - gPet.hunger) + gPet.traitCuriosity / 3;
  int desireExplore = gPet.traitCuriosity + gPet.env.hiddenCount * 7 + gPet.env.openCount * 5 + random(0, 18);
  int desireRest = (100 - gPet.energy) + (100 - gPet.health) + gPet.traitStress / 2;
  if (gPet.mood == PetMood::Hungry) {
    desireHunt += 24;
  } else if (gPet.mood == PetMood::Curious || gPet.mood == PetMood::Bored) {
    desireExplore += 20;
  } else if (gPet.mood == PetMood::Sick || gPet.mood == PetMood::Sleepy) {
    desireRest += 22;
  }
  if (gPet.hunger < 15) {
    desireRest -= 12;
  }

  int best = 20;
  PetActivity chosen = PetActivity::Idle;
  if (desireHunt > best) {
    best = desireHunt;
    chosen = PetActivity::Hunting;
  }
  if (desireExplore > best) {
    best = desireExplore;
    chosen = PetActivity::Exploring;
  }
  if (desireRest > best) {
    chosen = PetActivity::Sleeping;
  }

  if (chosen == PetActivity::Hunting) {
    startPetWifiScan(PetActivity::Hunting);
  } else if (chosen == PetActivity::Exploring) {
    startPetWifiScan(PetActivity::Exploring);
  } else if (chosen == PetActivity::Sleeping) {
    beginPetRest();
  }
}

void servicePet() {
  uint32_t now = millis();
  if (gPet.lastTickMs == 0) {
    gPet.lastTickMs = now;
    gPet.lastMinuteMs = now;
  }
  if (now - gPet.lastTickMs < kPetTickMs) {
    servicePetWifiScan();
    servicePetRest();
    return;
  }
  gPet.lastTickMs = now;

  servicePetWifiScan();
  servicePetRest();

  uint8_t applied = 0;
  while (now - gPet.lastMinuteMs >= kPetMinuteMs && applied < 5) {
    gPet.lastMinuteMs += kPetMinuteMs;
    applyPetElapsedMinute(false);
    ++applied;
  }

  if (gPet.actionUntilMs != 0 && now > gPet.actionUntilMs &&
      gPet.activity != PetActivity::Sleeping && !gPet.env.scanPending) {
    gPet.activity = gPet.alive ? PetActivity::Idle : PetActivity::Dead;
    gPet.actionUntilMs = 0;
  }
  updatePetMood();
  decideNextPetActivity();
  savePetState(false);
}

PetCareAction suggestedPetAction() {
  if (gPet.sick || gPet.health < 35) {
    return PetCareAction::Medicine;
  }
  if (gPet.hunger < 35) {
    return PetCareAction::Feed;
  }
  if (gPet.cleanliness < 45 || gPet.poop >= 3) {
    return PetCareAction::Clean;
  }
  if (gPet.energy < 28) {
    return PetCareAction::Sleep;
  }
  if (gPet.happiness < 55) {
    return PetCareAction::Play;
  }
  return PetCareAction::Explore;
}

bool handlePetKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    return applyPetCare(suggestedPetAction());
  }
  if (lowerKey == 'f') {
    return applyPetCare(PetCareAction::Feed);
  }
  if (lowerKey == 'p') {
    return applyPetCare(PetCareAction::Play);
  }
  if (lowerKey == 'c') {
    return applyPetCare(PetCareAction::Clean);
  }
  if (lowerKey == 's') {
    return applyPetCare(PetCareAction::Sleep);
  }
  if (lowerKey == 'm') {
    return applyPetCare(PetCareAction::Medicine);
  }
  if (lowerKey == 'd') {
    return applyPetCare(PetCareAction::Discipline);
  }
  if (lowerKey == 'h') {
    return applyPetCare(PetCareAction::Hunt);
  }
  if (lowerKey == 'e') {
    return applyPetCare(PetCareAction::Explore);
  }
  if (lowerKey == 'r') {
    updatePetMood();
    setStatus(String(petMoodName(gPet.mood)) + " " + petAgeText(), 1000);
    return true;
  }
  return false;
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
      size_t written = gActiveRecordFile.write(reinterpret_cast<const uint8_t*>(gRecordChunkBuffers[chunkIndex]),
                                               kRecordChunkBytes);
      gActiveRecordBytes += written;
      gActiveRecordCommittedBytes += written;
      if (written != kRecordChunkBytes) {
        setHttpDiag("record fs short " + String(static_cast<unsigned>(written)) + "/" +
                    String(static_cast<unsigned>(kRecordChunkBytes)));
        logf("[VOICE] record fs short write=%u expected=%u",
             static_cast<unsigned>(written),
             static_cast<unsigned>(kRecordChunkBytes));
      }
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
    out = "";
    while (millis() < deadline) {
      while (client.available() > 0) {
        int raw = client.read();
        if (raw < 0) {
          break;
        }
        char c = static_cast<char>(raw);
        if (c == '\n') {
          return true;
        }
        if (out.length() < 512) {
          out += c;
        }
      }
      if (!client.connected()) {
        return out.length() > 0;
      }
      pumpBlockingNetworkUi();
    }
    return out.length() > 0;
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
      pumpBlockingNetworkUi();
      continue;
    }
    written += sent;
    lastProgress = millis();
    pumpBlockingNetworkUi();
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
      pumpBlockingNetworkUi();
      continue;
    }
    written += sent;
    lastProgress = millis();
    if ((written % (8 * 1024)) == 0 || written == length) {
      setHttpDiag(String(label) + ": " + String(static_cast<unsigned>(written / 1024)) + "k/" +
                  String(static_cast<unsigned>(length / 1024)) + "k");
    }
    pumpBlockingNetworkUi();
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
        pumpBlockingNetworkUi();
        continue;
      }
      sentFromStage += sent;
      written += sent;
      lastProgress = millis();
      if ((written % (8 * 1024)) == 0 || written == length) {
        setHttpDiag(String(label) + ": " + String(static_cast<unsigned>(written / 1024)) + "k/" +
                    String(static_cast<unsigned>(length / 1024)) + "k");
      }
      pumpBlockingNetworkUi();
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

bool readHttpResponseBody(WiFiClient& client, int contentLength, String& response,
                          uint32_t timeoutMs, size_t maxBytes = 16384) {
  response = "";
  if (contentLength > 0) {
    response.reserve(min<size_t>(static_cast<size_t>(contentLength), maxBytes));
  } else {
    response.reserve(1024);
  }
  size_t total = 0;
  unsigned long deadline = millis() + timeoutMs;
  uint8_t buffer[384];
  while (millis() < deadline && (contentLength < 0 || total < static_cast<size_t>(contentLength))) {
    int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) {
        break;
      }
      pumpBlockingNetworkUi();
      continue;
    }
    size_t chunk = static_cast<size_t>(avail);
    if (chunk > sizeof(buffer)) {
      chunk = sizeof(buffer);
    }
    if (contentLength > 0) {
      size_t left = static_cast<size_t>(contentLength) - total;
      if (chunk > left) {
        chunk = left;
      }
    }
    int got = client.read(buffer, chunk);
    if (got <= 0) {
      pumpBlockingNetworkUi();
      continue;
    }
    size_t n = static_cast<size_t>(got);
    if (response.length() < static_cast<int>(maxBytes)) {
      size_t room = maxBytes - static_cast<size_t>(response.length());
      size_t copyLen = min(room, n);
      for (size_t i = 0; i < copyLen; ++i) {
        response += static_cast<char>(buffer[i]);
      }
    }
    total += n;
    pumpBlockingNetworkUi();
  }
  if (contentLength > 0 && total < static_cast<size_t>(contentLength)) {
    setHttpDiag("response short " + String(static_cast<unsigned>(total)) + "/" +
                String(static_cast<unsigned>(contentLength)));
    return false;
  }
  return true;
}

bool postRawVoiceTurn(const uint8_t* ramPayload, const String& filePath, size_t dataSize,
                      const String& turnId, int& statusCode, String& response) {
  statusCode = -1;
  response = "";
  if (dataSize == 0 || (!ramPayload && filePath.isEmpty())) {
    setHttpDiag("voice_post: empty payload");
    return false;
  }
  if (!ensureWifiForHttp("voice-post", 3000) || gHubHost.isEmpty()) {
    setHttpDiag("voice_post: wifi/hub unavailable");
    return false;
  }

  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  WiFiClient* client = nullptr;
  if (hubUsesTls()) {
    secureClient.setInsecure();
    secureClient.setHandshakeTimeout(12);
    secureClient.setTimeout((kTurnHttpTimeoutMs + 999) / 1000);
    client = &secureClient;
  } else {
    plainClient.setTimeout((kTurnHttpTimeoutMs + 999) / 1000);
    plainClient.setNoDelay(true);
    client = &plainClient;
  }

  setHttpDiag("voice_post: connect");
  if (!client->connect(gHubHost.c_str(), gHubPort)) {
    setHttpDiag("voice_post: connect failed");
    logf("[VOICE] raw post connect failed host=%s port=%u", gHubHost.c_str(), gHubPort);
    return false;
  }

  String hostHeader = gHubHost;
  if ((!hubUsesTls() && gHubPort != 80) || (hubUsesTls() && gHubPort != 443)) {
    hostHeader += ":" + String(gHubPort);
  }
  String headers;
  headers.reserve(768);
  headers += "POST ";
  headers += kDeviceAudioTurnRawPath;
  headers += " HTTP/1.1\r\nHost: ";
  headers += hostHeader;
  headers += "\r\nUser-Agent: OpenClaw-Cardputer/";
  headers += kAppVersion;
  headers += "\r\nConnection: close\r\nContent-Type: application/octet-stream\r\nContent-Length: ";
  headers += String(static_cast<unsigned>(dataSize));
  headers += "\r\nX-Audio-Sample-Rate: ";
  headers += String(kMicSampleRate);
  headers += "\r\nX-Device-Id: ";
  headers += gDeviceId;
  headers += "\r\nX-Firmware-Version: ";
  headers += kAppVersion;
  headers += "\r\nX-Conversation-Key: ";
  headers += currentConversationKey();
  headers += "\r\nX-Client-Msg-Id: ";
  headers += turnId;
  headers += "\r\nX-Turn-Id: ";
  headers += turnId;
  headers += "\r\nX-Reply-Audio: true\r\nX-Save-Reply-Audio: true\r\n";
  if (!gDeviceToken.isEmpty()) {
    headers += "Authorization: Bearer ";
    headers += gDeviceToken;
    headers += "\r\nX-Device-Token: ";
    headers += gDeviceToken;
    headers += "\r\n";
  }
  headers += "\r\n";

  if (!writeClientAll(*client, headers, "voice_headers")) {
    client->stop();
    return false;
  }

  bool payloadOk = false;
  if (!filePath.isEmpty()) {
    fs::FS* fs = activeVoiceFs();
    File pcmFile = fs ? fs->open(filePath, "r") : File();
    if (!pcmFile) {
      setHttpDiag("voice_body: file open failed");
      client->stop();
      return false;
    }
    payloadOk = writeClientAllFileStaged(*client, pcmFile, dataSize, "voice_body");
    pcmFile.close();
  } else {
    payloadOk = writeClientAllStaged(*client, ramPayload, dataSize, "voice_body");
  }
  if (!payloadOk) {
    client->stop();
    return false;
  }

  setHttpDiag("voice_post: wait reply");
  int contentLength = -1;
  uint32_t ignoredSampleRate = kTtsDefaultSampleRate;
  if (!parseHttpStatusAndHeaders(*client, statusCode, contentLength, ignoredSampleRate, kTurnHttpTimeoutMs)) {
    setHttpDiag("voice_post: response header failed");
    client->stop();
    return false;
  }
  if (!readHttpResponseBody(*client, contentLength, response, 15000)) {
    logf("[VOICE] raw post response body short status=%d body=%s", statusCode, response.c_str());
  }
  client->stop();
  setHttpDiag("voice_post: http " + String(statusCode));
  logf("[VOICE] raw post status=%d body=%u", statusCode, static_cast<unsigned>(response.length()));
  return true;
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
  String turnId = makeClientTurnId("txt");
  http.setTimeout(kTurnHttpTimeoutMs);
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
  doc["reply_audio"] = false;
  doc["save_reply_audio"] = false;
  doc["client_msg_id"] = turnId;
  doc["turn_id"] = turnId;

  String body;
  serializeJson(doc, body);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Client-Msg-Id", turnId);
  http.addHeader("X-Turn-Id", turnId);
  http.addHeader("X-Conversation-Key", currentConversationKey());
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
    if (code <= 0) {
      String transportError = HTTPClient::errorToString(code);
      pushError("Hub transport failed: " + transportError);
      setHttpDiag("text: " + transportError);
    } else if (code == 504) {
      pushError("Assistant reply timeout.");
      setHttpDiag("text: assistant timeout");
    } else {
      pushError("Hub HTTP error: " + String(code));
      setHttpDiag("text: http " + String(code));
    }
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
  String deviceActionsJson;
  if (!resp["device_actions"].isNull()) {
    serializeJson(resp["device_actions"], deviceActionsJson);
  }
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
    bool hasAudio = resp["audio"].is<JsonObject>();
    if (hasAudio) {
      JsonObject audio = resp["audio"].as<JsonObject>();
      audioUrl = audio["url"] | "";
      audioPath = audio["path"] | "";
      audioTitle = audio["title"] | "";
      audioSha = audio["sha256"] | "";
      audioSize = audio["size"] | 0;
    }
    if (hasAudio) {
      downloadAndPlayResponseAudio(audioUrl, audioPath, audioSha, audioSize, audioTitle, reply);
    } else if (shouldSpeakAssistantTextReply(reply)) {
      requestAndPlayTts(reply);
    }
    logf("[TEXT] reply=%s", reply.c_str());
  } else {
    pushSystem("Hub accepted the turn.");
    logf("[TEXT] accepted without reply");
  }
  if (!deviceActionsJson.isEmpty()) {
    executeDeviceActionsJson(deviceActionsJson);
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
    http.setTimeout(kTurnHttpTimeoutMs);
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
      doc["reply_audio"] = false;
      doc["save_reply_audio"] = false;
      doc["client_msg_id"] = payload->clientMsgId;
      doc["turn_id"] = payload->turnId;

      String body;
      serializeJson(doc, body);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("X-Conversation-Key", payload->conversationKey);
      http.addHeader("X-Client-Msg-Id", payload->clientMsgId);
      http.addHeader("X-Turn-Id", payload->turnId);
      if (!payload->deviceToken.isEmpty()) {
        http.addHeader("Authorization", "Bearer " + payload->deviceToken);
      }
      int code = http.POST(body);
      result->statusCode = code;
      result->response = http.getString();
      http.end();

      if (code < 200 || code >= 300) {
        if (code <= 0) {
          result->error = "Hub transport failed: " + HTTPClient::errorToString(code);
        } else if (code == 504) {
          result->error = "Assistant reply timeout.";
        } else {
          result->error = "Hub HTTP error: " + String(code);
        }
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
          if (!resp["device_actions"].isNull()) {
            serializeJson(resp["device_actions"], result->deviceActionsJson);
            result->hasDeviceActions = !result->deviceActionsJson.isEmpty();
          }
          if (resp["audio"].is<JsonObject>()) {
            result->hasAudio = true;
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
  payload->clientMsgId = makeClientTurnId("txt");
  payload->turnId = payload->clientMsgId;

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
    if (done->statusCode <= 0) {
      setHttpDiag("text: " + done->error);
    } else {
      setHttpDiag("text: http " + String(done->statusCode));
    }
    if (!done->response.isEmpty()) {
      pushError(done->response);
    }
    setFaceMode(FaceMode::Error);
    setStatus("Text turn failed", 2000);
  } else if (done->ok && done->hasReply) {
    appendAssistantReply(done->reply);
    bool playedAudio = false;
    if (done->hasAudio) {
      playedAudio = downloadAndPlayResponseAudio(done->audioUrl, done->audioPath, done->audioSha256,
                                                 done->audioSize, done->audioTitle, done->reply);
    }
    if (!playedAudio && shouldSpeakAssistantTextReply(done->reply)) {
      requestAndPlayTts(done->reply);
    }
    logf("[TEXT] async reply=%s", done->reply.c_str());
    setStatus("Reply received", 1200);
    if (done->hasDeviceActions) {
      executeDeviceActionsJson(done->deviceActionsJson);
    }
  } else if (done->ok) {
    pushSystem("Hub accepted the turn.");
    logf("[TEXT] async accepted without reply");
    if (done->hasDeviceActions) {
      executeDeviceActionsJson(done->deviceActionsJson);
    }
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
    fs::FS* fs = activeVoiceFs();
    if (fs && !gActiveRecordPath.isEmpty() && fsExists(*fs, gActiveRecordPath)) {
      File verifyFile = fs->open(gActiveRecordPath, "r");
      if (verifyFile) {
        size_t actualBytes = verifyFile.size();
        verifyFile.close();
        if (actualBytes != gActiveRecordBytes) {
          appendRuntimeLog("VOICE", "record size adjusted counter=" +
                                    String(static_cast<unsigned>(gActiveRecordBytes)) +
                                    " file=" + String(static_cast<unsigned>(actualBytes)), true);
          logf("[VOICE] record size adjusted counter=%u file=%u",
               static_cast<unsigned>(gActiveRecordBytes),
               static_cast<unsigned>(actualBytes));
          gActiveRecordBytes = actualBytes;
        }
      }
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

  String turnId = makeClientTurnId("voice");
  setStatus("Uploading voice...");
  render();
  gLastRenderMs = millis();
  appendRuntimeLog("VOICE", "upload start bytes=" + String(static_cast<unsigned>(dataSize)) +
                            " ctx=" + currentConversationKey() +
                            " turn=" + turnId, true);
  int statusCode = -1;
  String response;
  bool transportOk = false;
  bool prevRenderPump = gBlockingRenderPumpEnabled;
  gBlockingRenderPumpEnabled = true;
  gLastBlockingRenderPumpMs = 0;
  pumpBlockingNetworkUi();
  if (gRecordingToFile) {
    setHttpDiag("voice_pcm: raw file " + String(static_cast<unsigned>(dataSize / 1024)) + "k");
    transportOk = postRawVoiceTurn(nullptr, pcmUploadPath, dataSize, turnId, statusCode, response);
  } else {
    setHttpDiag("voice_pcm: raw ram " + String(static_cast<unsigned>(dataSize / 1024)) + "k");
    transportOk = postRawVoiceTurn(reinterpret_cast<const uint8_t*>(gRecordBuffer), "", dataSize,
                                   turnId, statusCode, response);
  }
  gBlockingRenderPumpEnabled = prevRenderPump;
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

  if (!transportOk || statusCode <= 0) {
    String transportError = gLastHttpDiag;
    if (transportError.isEmpty()) {
      transportError = statusCode <= 0 ? HTTPClient::errorToString(statusCode) : String("unknown");
    }
    pushError("Voice transport failed: " + transportError);
    setFaceMode(FaceMode::Error);
    setStatus("Voice transport failed", 2200);
    setVoiceDiag("voice: transport " + String(statusCode));
    logf("[VOICE] raw upload failed code=%d err=%s bytes=%u",
         statusCode,
         transportError.c_str(),
         static_cast<unsigned>(dataSize));
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    logf("[VOICE] HTTP error status=%d body=%s", statusCode, response.c_str());
    if (statusCode == 504) {
      pushError("Assistant reply timeout.");
      setStatus("Assistant timeout", 2200);
      setVoiceDiag("voice: assistant timeout");
    } else {
      pushError("Voice HTTP error: " + String(statusCode));
      setStatus("Voice turn failed", 2000);
      setVoiceDiag("voice: http " + String(statusCode));
    }
    if (!response.isEmpty()) {
      pushError(response);
    }
    setFaceMode(FaceMode::Error);
    setHttpDiag("voice: http " + String(statusCode));
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
  String deviceActionsJson;
  if (!resp["device_actions"].isNull()) {
    serializeJson(resp["device_actions"], deviceActionsJson);
  }
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
  if (!deviceActionsJson.isEmpty()) {
    executeDeviceActionsJson(deviceActionsJson);
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
  caps["device_actions"] = true;
  caps["device_actions_version"] = kDeviceActionCatalogVersion;
  caps["device_actions_count"] = static_cast<uint32_t>(kDeviceActionCatalogCount);

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
    if (!doc["device_actions"].isNull()) {
      executeDeviceActions(doc["device_actions"]);
    }
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

  if (cmd == "/help" || cmd == "help" || cmd == "commands" || cmd == "actions" ||
      cmd == "команды" || cmd == "помощь") {
    pushSystem("Ctrl+Space = EN/RU");
    pushSystem("Voice: Ctrl x2, Ctrl+V, or hold G0");
    pushSystem("Tab = next screen, Tab+Down/Right = next app");
    pushSystem("Ctrl+L = launcher map, arrows = group/item");
    pushSystem("Ctrl+D = debug; type: voice focus topics pet notes");
    pushSystem("Pet: F food P play C clean S sleep H hunt");
    pushSystem("Agent actions: focus, ui.open, audio/voice, topics, pet, settings");
    pushSystem("Examples: start pomodoro 25; open library; next topic");
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

  if (cmd == "/hero" || cmd == "/pet" || cmd == "/tama") {
    setUiMode(UiMode::Hero);
    return true;
  }

  if (cmd == "/petreset" || cmd == "/tamareset") {
    resetPet(true);
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
    adjustSpeakerVolume(gSpeakerVolume >= 224 ? -160 : 32);
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
    adjustSpeakerVolume(-32);
    return;
  }
  if ((key == '=' || key == '+') && gSpeakerVolume <= 223) {
    adjustSpeakerVolume(32);
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
    adjustSpeakerVolume(-32);
    return true;
  }
  if ((key == '=' || key == '+') && gSpeakerVolume <= 223) {
    adjustSpeakerVolume(32);
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

bool handleNotesKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (!gNotesScanned) {
    scanNotes();
  }

  if (keys.ctrl && lowerKey == 's') {
    if (gNotesMode == NotesMode::Edit && !gNoteInputBuffer.isEmpty()) {
      appendLineToActiveNote(gNoteInputBuffer);
      gNoteInputBuffer = "";
    }
    gNotesMode = NotesMode::View;
    gNotesStatus = "Saved";
    return true;
  }

  if (gNotesMode == NotesMode::Edit) {
    if (keys.enter || key == '\n' || key == '\r') {
      appendLineToActiveNote(gNoteInputBuffer);
      gNoteInputBuffer = "";
      return true;
    }
    if (keys.del || key == '\b') {
      if (!gNoteInputBuffer.isEmpty()) {
        removeLastUtf8Char(gNoteInputBuffer);
      }
      return true;
    }
    if (keys.ctrl && (lowerKey == 'x' || lowerKey == 'q')) {
      gNotesMode = NotesMode::View;
      gNotesStatus = "View mode";
      return true;
    }
    if (key >= 32 && key < 127 && gNoteInputBuffer.length() < static_cast<int>(kMaxNoteInputChars)) {
      gNoteInputBuffer += translateInputChar(key);
      return true;
    }
    return false;
  }

  if (lowerKey == 'r') {
    scanNotes();
    setStatus("Notes rescanned", 700);
    return true;
  }
  if (lowerKey == 'n') {
    return createNewNote();
  }
  if (lowerKey == 'e') {
    return openSelectedNote(NotesMode::Edit);
  }
  if (lowerKey == 'l' || keys.del || key == '\b') {
    gNotesMode = NotesMode::List;
    gNoteInputBuffer = "";
    gNotesStatus = "List";
    return true;
  }

  if (gNotesMode == NotesMode::List) {
    if (gNotes.empty()) {
      return lowerKey == 'r' || lowerKey == 'n';
    }
    if (key == ',' || key == ';') {
      gSelectedNote = max<int>(0, gSelectedNote - 1);
      gNoteListScrollOffset = max<int>(0, gSelectedNote - 2);
      setStatus(gNotes[gSelectedNote].name, 600);
      return true;
    }
    if (key == '.' || key == '/') {
      gSelectedNote = min<int>(static_cast<int>(gNotes.size()) - 1, gSelectedNote + 1);
      gNoteListScrollOffset = max<int>(0, gSelectedNote - 2);
      setStatus(gNotes[gSelectedNote].name, 600);
      return true;
    }
    if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
      return openSelectedNote(NotesMode::View);
    }
    return false;
  }

  if (key == ',' || key == ';') {
    gNoteScrollOffset = max<int>(0, gNoteScrollOffset - 1);
    return true;
  }
  if (key == '.' || key == '/') {
    uint32_t lineCount = 0;
    if (!gActiveNotePath.isEmpty()) {
      fs::FS* fs = activeNotesFs();
      if (fs) {
        lineCount = countNoteLines(*fs, gActiveNotePath);
      }
    }
    int maxScroll = max<int>(0, static_cast<int>(lineCount) - 7);
    gNoteScrollOffset = min<int>(maxScroll, gNoteScrollOffset + 1);
    return true;
  }
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    return openSelectedNote(NotesMode::Edit);
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
  if (lowerKey == 'c') {
    confirmOtaBootIfPending("manual confirm");
    return true;
  }
  if (lowerKey == 'x' || lowerKey == 'b') {
    rollbackPendingOta("manual key");
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
  uint32_t now = millis();
  if (gTopicCreateArmed && now > gTopicCreateArmedUntilMs) {
    gTopicCreateArmed = false;
  }
  if (delta < 0 && gSelectedContext <= 0) {
    if (!gTopicCreateArmed) {
      armTopicCreate();
      return true;
    }
    String title = defaultNewTopicTitle();
    gTopicCreateArmed = false;
    return startTopicTask(false, false, false, "Creating topic...", true, title);
  }
  if (delta > 0 && gTopicCreateArmed) {
    gTopicCreateArmed = false;
  }
  int next = gSelectedContext + delta;
  if (next < 0 || next >= static_cast<int>(gContexts.size())) {
    showTopicOverlay(delta < 0 ? -1 : 1, 900);
    setStatus(delta < 0 ? "New topic: Alt+Left" : "Last topic", 900);
    return false;
  }
  gSelectedContext = next;
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

void armTopicCreate() {
  uint32_t now = millis();
  gTopicCreateArmed = true;
  gTopicCreateArmedUntilMs = now + 3200;
  gContextAnimDir = -1;
  gContextAnimStartMs = now;
  showTopicOverlay(-1, 3200);
  setStatus("Alt+Left again creates topic", 1600);
}

void topicTask(void* arg) {
  std::unique_ptr<TopicTaskPayload> payload(static_cast<TopicTaskPayload*>(arg));
  std::unique_ptr<TopicTaskResult> result(new TopicTaskResult());
  if (!result) {
    gTopicTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }
  if (payload->createTopic) {
    result->createOk = createRemoteTopic(payload->createTitle, result->createdTopicKey);
    if (result->createOk && !result->createdTopicKey.isEmpty()) {
      gSavedContextKey = result->createdTopicKey;
      result->catalogOk = fetchRemoteTopicCatalog();
      selectSavedContextOrDefault(result->createdTopicKey);
      saveCurrentContext();
      result->selectOk = selectRemoteCurrentTopic();
      result->historyOk = fetchSelectedTopicHistory();
    }
  } else if (payload->syncCatalog) {
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

bool startTopicTask(bool syncCatalog, bool selectCurrent, bool loadHistory, const char* statusText,
                    bool createTopic, const String& createTitle) {
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
  payload->createTopic = createTopic;
  payload->createTitle = createTitle;
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
  if (done->createOk) {
    setStatus("Topic created", 1000);
    showTopicOverlay(0, 2200);
  } else if (done->historyOk) {
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
    gLogScrollOffset = min<int>(gLogScrollOffset + 1, max<int>(0, static_cast<int>(gRuntimeLogs.size()) - 9));
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
  if (gPlaybackActive && (key == '-' || key == '_')) {
    adjustSpeakerVolume(-24);
    return;
  }
  if (gPlaybackActive && (key == '=' || key == '+')) {
    adjustSpeakerVolume(24);
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
  if (gUiMode == UiMode::Hero && handlePetKey(key, keys)) {
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
  if (gUiMode == UiMode::Notes && handleNotesKey(key, keys)) {
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
  static bool pOpt = false;
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
  bool optDown = keys.opt && !pOpt;
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
  bool optionViewToggle = optDown && !keys.alt && !keys.ctrl && !keys.fn && !keys.shift &&
                          !keys.tab && !keys.enter && !keys.del && !keys.space &&
                          !upHeld && !downHeld && !leftHeld && !rightHeld &&
                          wordSignature.isEmpty();

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
  } else if (optionViewToggle) {
    togglePulseChatDisplay();
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
  } else if (!gLauncherVisible && gPlaybackActive && upDown) {
    adjustSpeakerVolume(24);
    setKeyDiag("vol:up");
  } else if (!gLauncherVisible && gPlaybackActive && downDown) {
    adjustSpeakerVolume(-24);
    setKeyDiag("vol:down");
  } else if (gUiMode == UiMode::Contexts && (leftDown || upDown)) {
    handleTypingKey(',', keys);
  } else if (gUiMode == UiMode::Contexts && (rightDown || downDown)) {
    handleTypingKey('.', keys);
  } else if (gUiMode == UiMode::Notes && (leftDown || upDown)) {
    handleTypingKey(',', keys);
  } else if (gUiMode == UiMode::Notes && (rightDown || downDown)) {
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
  pOpt = keys.opt;
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

uint16_t petMoodAccent() {
  switch (gPet.mood) {
    case PetMood::Happy:
      return rgb565_local(88, 240, 141);
    case PetMood::Hungry:
      return rgb565_local(255, 194, 74);
    case PetMood::Bored:
      return rgb565_local(126, 143, 157);
    case PetMood::Curious:
      return rgb565_local(47, 227, 255);
    case PetMood::Sick:
      return rgb565_local(128, 230, 104);
    case PetMood::Excited:
      return rgb565_local(255, 105, 135);
    case PetMood::Sleepy:
      return rgb565_local(143, 126, 255);
    case PetMood::Dirty:
      return rgb565_local(181, 142, 86);
    case PetMood::Dead:
      return rgb565_local(120, 128, 136);
    case PetMood::Calm:
    default:
      return rgb565_local(34, 209, 189);
  }
}

uint16_t petBodyColor() {
  if (!gPet.alive) {
    return rgb565_local(84, 90, 96);
  }
  if (gPet.mood == PetMood::Sick) {
    return rgb565_local(119, 176, 95);
  }
  if (gPet.mood == PetMood::Sleepy) {
    return rgb565_local(130, 86, 198);
  }
  switch (gPet.stage) {
    case PetStage::Egg:
      return rgb565_local(236, 218, 172);
    case PetStage::Baby:
      return rgb565_local(228, 74, 62);
    case PetStage::Teen:
      return rgb565_local(222, 58, 64);
    case PetStage::Adult:
      return rgb565_local(203, 52, 58);
    case PetStage::Elder:
    default:
      return rgb565_local(186, 74, 88);
  }
}

void drawPetSpark(int16_t x, int16_t y, int16_t r, uint16_t color) {
  auto& d = gCanvas;
  d.drawFastHLine(x - r, y, r * 2 + 1, color);
  d.drawFastVLine(x, y - r, r * 2 + 1, color);
  d.drawLine(x - r + 1, y - r + 1, x + r - 1, y + r - 1, color);
  d.drawLine(x - r + 1, y + r - 1, x + r - 1, y - r + 1, color);
}

void drawProceduralOpenClawPet(int16_t cx, int16_t cy, int16_t size, bool mini) {
  auto& d = gCanvas;
  uint32_t now = millis();
  uint16_t body = petBodyColor();
  uint16_t accent = petMoodAccent();
  uint16_t line = rgb565_local(13, 18, 22);
  uint16_t highlight = rgb565_local(255, 134, 108);
  int16_t r = max<int16_t>(8, size / 2);
  int16_t bob = mini ? 0 : static_cast<int16_t>(abs(static_cast<int>((now / 170) % 10) - 5) / 2);
  cy += bob;

  d.fillRoundRect(cx - r + 4, cy + r - 3, (r - 4) * 2, max<int16_t>(4, r / 4), max<int16_t>(2, r / 8), rgb565_local(0, 0, 0));

  if (gPet.stage == PetStage::Egg && gPet.alive) {
    uint16_t shell = rgb565_local(238, 224, 184);
    uint16_t shell2 = rgb565_local(255, 242, 202);
    d.fillCircle(cx, cy + r / 8, r, shell);
    d.fillRoundRect(cx - r, cy - r / 2, r * 2, r + r / 2, r / 2, shell);
    d.fillCircle(cx - r / 4, cy - r / 5, r / 3, shell2);
    d.drawCircle(cx, cy + r / 8, r, accent);
    d.drawLine(cx - r / 4, cy - r / 5, cx, cy, line);
    d.drawLine(cx, cy, cx - r / 7, cy + r / 4, line);
    d.drawLine(cx - r / 7, cy + r / 4, cx + r / 5, cy + r / 2, line);
    if (!mini) {
      d.setFont(&fonts::Font0);
      d.setTextColor(accent, rgb565_local(8, 18, 26));
      d.setCursor(cx - 18, cy + r + 4);
      d.print("hatching");
    }
    return;
  }

  int16_t bodyW = r * 2;
  int16_t bodyH = r * 2 + r / 4;
  d.fillTriangle(cx - r + 3, cy - r + 4, cx - r / 2, cy - r - r / 2, cx - r / 6, cy - r + 8, line);
  d.fillTriangle(cx + r - 3, cy - r + 4, cx + r / 2, cy - r - r / 2, cx + r / 6, cy - r + 8, line);
  d.fillRoundRect(cx - r, cy - r, bodyW, bodyH, r / 2, line);
  d.fillRoundRect(cx - r + 3, cy - r + 3, bodyW - 6, bodyH - 6, r / 2, body);
  d.fillCircle(cx - r / 3, cy - r / 3, max<int16_t>(3, r / 5), highlight);
  d.fillCircle(cx + r / 3, cy - r / 3, max<int16_t>(3, r / 5), highlight);

  int16_t clawY = cy + r / 2;
  d.fillCircle(cx - r - 2, clawY, max<int16_t>(3, r / 5), line);
  d.fillCircle(cx + r + 2, clawY, max<int16_t>(3, r / 5), line);
  d.fillCircle(cx - r - 2, clawY, max<int16_t>(2, r / 5 - 2), rgb565_local(196, 45, 48));
  d.fillCircle(cx + r + 2, clawY, max<int16_t>(2, r / 5 - 2), rgb565_local(196, 45, 48));

  int16_t eyeY = cy - r / 5;
  int16_t leftEye = cx - r / 3;
  int16_t rightEye = cx + r / 3;
  if (!gPet.alive) {
    int16_t e = max<int16_t>(3, r / 7);
    d.drawLine(leftEye - e, eyeY - e, leftEye + e, eyeY + e, line);
    d.drawLine(leftEye - e, eyeY + e, leftEye + e, eyeY - e, line);
    d.drawLine(rightEye - e, eyeY - e, rightEye + e, eyeY + e, line);
    d.drawLine(rightEye - e, eyeY + e, rightEye + e, eyeY - e, line);
  } else if (gPet.mood == PetMood::Sleepy || gPet.activity == PetActivity::Sleeping) {
    d.drawFastHLine(leftEye - r / 7, eyeY, max<int16_t>(4, r / 4), line);
    d.drawFastHLine(rightEye - r / 7, eyeY, max<int16_t>(4, r / 4), line);
  } else if (gPet.mood == PetMood::Happy || gPet.mood == PetMood::Excited) {
    d.drawArc(leftEye, eyeY + 2, max<int16_t>(5, r / 5), max<int16_t>(3, r / 5 - 2), 205, 335, line);
    d.drawArc(rightEye, eyeY + 2, max<int16_t>(5, r / 5), max<int16_t>(3, r / 5 - 2), 205, 335, line);
  } else {
    d.fillCircle(leftEye, eyeY, max<int16_t>(2, r / 8), line);
    d.fillCircle(rightEye, eyeY, max<int16_t>(2, r / 8), line);
  }

  int16_t mouthY = cy + r / 5;
  if (gPet.mood == PetMood::Hungry || gPet.mood == PetMood::Bored || gPet.mood == PetMood::Sick) {
    d.drawArc(cx, mouthY + r / 8, max<int16_t>(6, r / 4), max<int16_t>(4, r / 4 - 2), 205, 335, line);
  } else if (gPet.activity == PetActivity::Eating) {
    d.fillRoundRect(cx - r / 5, mouthY - 1, max<int16_t>(4, r / 3), max<int16_t>(3, r / 7), 2, line);
  } else {
    d.drawArc(cx, mouthY - r / 8, max<int16_t>(7, r / 4), max<int16_t>(5, r / 4 - 2), 25, 155, line);
  }

  if (mini) {
    return;
  }

  if (gPet.activity == PetActivity::Eating || gPet.activity == PetActivity::Hunting) {
    for (int i = 0; i < 5; ++i) {
      int16_t fx = cx - r - 16 + i * 7;
      int16_t fy = cy - 18 + static_cast<int16_t>((now / 120 + i * 3) % 18);
      d.fillCircle(fx, fy, 2, rgb565_local(255, 194, 74));
    }
  }
  if (gPet.activity == PetActivity::Playing || gPet.mood == PetMood::Excited) {
    drawPetSpark(cx - r - 11, cy - r + 10, 4, accent);
    drawPetSpark(cx + r + 12, cy - r / 3, 3, rgb565_local(255, 194, 74));
  }
  if (gPet.activity == PetActivity::Cleaning || gPet.mood == PetMood::Dirty) {
    for (int i = 0; i < 4; ++i) {
      int16_t bx = cx + r + 9 + (i % 2) * 8;
      int16_t by = cy - r / 2 + i * 10 - static_cast<int16_t>((now / 150) % 5);
      d.drawCircle(bx, by, 3 + (i % 2), rgb565_local(47, 227, 255));
    }
  }
  if (gPet.activity == PetActivity::Sleeping) {
    d.setFont(&fonts::Font2);
    d.setTextColor(accent, rgb565_local(8, 18, 26));
    d.setCursor(cx + r + 9, cy - r - 8);
    d.print("Z");
    d.setCursor(cx + r + 17, cy - r - 16);
    d.print("z");
  }
  if (gPet.activity == PetActivity::Exploring) {
    int sweep = static_cast<int>((now / 10) % 360);
    d.drawArc(cx, cy, r + 13, r + 10, sweep, sweep + 70, accent);
    d.drawArc(cx, cy, r + 23, r + 20, 220 - sweep, 290 - sweep, rgb565_local(255, 194, 74));
  }
  if (gPet.activity == PetActivity::Medicine || gPet.mood == PetMood::Sick) {
    d.drawFastHLine(cx + r + 8, cy + r / 2, 13, rgb565_local(128, 230, 104));
    d.drawFastVLine(cx + r + 14, cy + r / 2 - 6, 13, rgb565_local(128, 230, 104));
  }
  if (gPet.activity == PetActivity::Discipline) {
    d.setFont(&fonts::Font4);
    d.setTextColor(rgb565_local(255, 194, 74), rgb565_local(8, 18, 26));
    d.setCursor(cx + r + 8, cy - r / 2);
    d.print("!");
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
  d.drawRoundRect(x, y, w, h, 10, petMoodAccent());
  d.setFont(&fonts::Font2);
  d.setTextColor(petMoodAccent(), 0x0841);
  d.setCursor(x + 8, y + 8);
  d.print("Pet");

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

  drawProceduralOpenClawPet(heroX + cellW / 2, cellY + cellH / 2, 25, true);

  d.setTextColor(TFT_LIGHTGREY, 0x0841);
  d.setCursor(x + 8, y + h - 18);
  d.print(String(petStageName(gPet.stage)) + " " + petAgeText());
  d.setTextColor(petMoodAccent(), 0x0841);
  d.setCursor(x + 8, y + h - 8);
  d.print(String(petMoodName(gPet.mood)) + " " + petActivityName(gPet.activity));
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
  const uint16_t bg = rgb565_local(0, 3, 8);
  const uint16_t line = rgb565_local(53, 68, 82);
  d.fillRoundRect(8, 8, 224, 104, 10, bg);
  d.drawRoundRect(8, 8, 224, 104, 10, line);
  d.setFont(&fonts::Font0);
  d.setTextColor(TFT_WHITE, bg);
  d.setCursor(14, 15);
  d.print("DBG");
  d.setCursor(48, 15);
  d.print(gWifiReady ? "wifi=ok" : "wifi=off");
  d.setCursor(118, 15);
  d.print(gWsReady ? "ws=ok" : "ws=off");
  d.setCursor(14, 28);
  d.print("face=");
  d.print(static_cast<int>(gFaceMode));
  d.setCursor(86, 28);
  d.print("scroll=");
  d.print(gChatScrollOffset);
  d.setCursor(150, 28);
  d.print(layoutName(gKeyboardLayout));
  d.setCursor(14, 41);
  d.print("rec=");
  d.print(static_cast<unsigned>(gRecordCapacityBytes / 1024));
  d.print("k");
  d.setCursor(86, 41);
  d.print("tts=");
  d.print(static_cast<unsigned>(gTtsCapacityBytes / 1024));
  d.print("k");
  d.setCursor(150, 41);
  d.print(gStorageReady ? gStorageLabel : "fs=off");
  d.setCursor(14, 55);
  String diag = gLastVoiceDiag;
  if (diag.length() > 28) {
    diag = diag.substring(diag.length() - 28);
  }
  d.print(diag);
  d.setCursor(14, 69);
  String httpDiag = gLastHttpDiag;
  if (httpDiag.length() > 28) {
    httpDiag = httpDiag.substring(httpDiag.length() - 28);
  }
  d.print(httpDiag);
  d.setCursor(14, 83);
  String keyDiag = gLastKeyDiag;
  if (keyDiag.length() > 28) {
    keyDiag = keyDiag.substring(keyDiag.length() - 28);
  }
  d.print(keyDiag);
  d.setCursor(150, 83);
  d.print(String(uiModeName(gUiMode)).substring(0, 6));
  d.setFont(&fonts::Font2);
}

void drawLauncherIcon(uint8_t group, int16_t x, int16_t y, int16_t s, uint16_t accent, uint16_t bg) {
  auto& d = gCanvas;
  int16_t cx = x + s / 2;
  int16_t cy = y + s / 2;
  int16_t r = max<int16_t>(5, s / 4);
  d.fillRoundRect(x, y, s, s, max<int16_t>(5, s / 5), bg);
  d.drawRoundRect(x, y, s, s, max<int16_t>(5, s / 5), accent);
  d.fillCircle(cx, cy, max<int16_t>(6, s / 3), rgb565_local(10, 14, 26));
  d.drawCircle(cx, cy, max<int16_t>(8, s / 3 + 3), accent);
  switch (group) {
    case 0:
      d.drawRoundRect(cx - r, cy - r / 2, r * 2, r, 4, accent);
      d.drawFastHLine(cx - r / 2, cy - 1, r, accent);
      d.fillCircle(cx - r / 2, cy - r / 2, 2, accent);
      d.fillCircle(cx + r / 2, cy - r / 2, 2, accent);
      break;
    case 1:
      d.drawRoundRect(cx - r - 3, cy - r / 2, r, r, 4, accent);
      d.drawRoundRect(cx + 3, cy - r / 2, r, r, 4, accent);
      d.fillCircle(cx - r / 2 - 3, cy, 2, accent);
      d.fillCircle(cx + r / 2 + 3, cy, 2, accent);
      break;
    case 2:
      for (int i = 0; i < 6; ++i) {
        int h = max<int>(5, (s / 5) + ((i * 7 + millis() / 90) % max<int>(6, s / 3)));
        d.fillRoundRect(x + 7 + i * max<int>(3, s / 8), y + s - 7 - h, max<int>(2, s / 10), h, 2, accent);
      }
      break;
    case 3:
      d.setFont(&fonts::Font2);
      d.setTextColor(accent, bg);
      d.setCursor(cx - 10, cy - 6);
      d.print("25");
      d.drawArc(cx, cy, max<int16_t>(10, s / 2 - 4), max<int16_t>(7, s / 2 - 8), 35, 310, accent);
      break;
    case 4:
      d.drawRoundRect(cx - r, cy - r, r * 2, r * 2, 4, accent);
      d.drawLine(cx - r + 3, cy, cx - 1, cy + r - 3, accent);
      d.drawLine(cx - 1, cy + r - 3, cx + r - 3, cy - r + 3, accent);
      d.fillCircle(cx + r - 3, cy - r + 3, 2, accent);
      break;
    case 5:
      d.drawCircle(cx, cy, r + 2, accent);
      d.drawFastHLine(cx - r - 2, cy, (r + 2) * 2, accent);
      d.drawFastVLine(cx, cy - r - 2, (r + 2) * 2, accent);
      d.fillCircle(cx, cy, 5, accent);
      break;
    case 6:
    default:
      d.drawRoundRect(cx - r, cy - r / 2, r * 2, r, 4, accent);
      d.drawFastHLine(cx - r / 2, cy - 2, r, accent);
      d.drawFastHLine(cx - r / 2, cy + 3, r - 3, accent);
      d.fillCircle(cx + r / 2, cy, 2, accent);
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
  const int16_t railW = 82;
  const int16_t railH = 10;
  const int16_t railGap = 2;

  for (int idx = 0; idx < kLauncherGroupCount; ++idx) {
    const LauncherGroup& item = kLauncherGroups[idx];
    const bool isSelected = idx == gLauncherSelection;
    const int16_t y = railY + idx * (railH + railGap);
    const uint16_t fill = isSelected ? selectedBg : rowBg;
    d.fillRoundRect(railX, y, railW, railH, 4, fill);
    d.drawRoundRect(railX, y, railW, railH, 4, isSelected ? item.accent : line);

    d.setFont(&fonts::Font0);
    d.setTextColor(isSelected ? TFT_WHITE : muted, fill);
    d.setCursor(railX + 5, y + 1);
    d.print(String(idx + 1));

    d.setTextColor(isSelected ? item.accent : TFT_LIGHTGREY, fill);
    d.setCursor(railX + 17, y + 1);
    d.print(fitCurrentFontToWidth(item.title, railW - 28));

    if (isSelected) {
      d.fillCircle(railX + railW - 7, y + railH / 2, 3, item.accent);
    }
  }
  d.setFont(&fonts::Font2);

  d.fillRoundRect(94, 33, 136, 78, 7, panel);
  d.drawRoundRect(94, 33, 136, 78, 7, group.accent);

  drawLauncherIcon(gLauncherSelection, 102, 41, 32, group.accent, rgb565_local(4, 9, 14));
  d.setFont(&fonts::Font2);
  d.setTextColor(group.accent, panel);
  d.setCursor(142, 41);
  d.print(fitCurrentFontToWidth(group.glyph, 76));
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(142, 56);
  d.print(fitCurrentFontToWidth(group.hint, 78));

  for (int i = 0; i < group.count; ++i) {
    const int16_t y = 76 + i * 8;
    const bool selected = i == gLauncherSubSelection;
    const uint16_t fill = selected ? rgb565_local(18, 39, 48) : panel;
    if (selected) {
      d.fillRoundRect(100, y - 1, 124, 8, 3, fill);
    }
    d.setFont(&fonts::Font0);
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, fill);
    d.setCursor(104, y);
    d.print(selected ? "> " : "  ");
    d.print(fitCurrentFontToWidth(group.items[i].label, 106));
  }
  d.setFont(&fonts::Font2);

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
  const int16_t textY = min<int16_t>(y, 118);
  const int16_t clearX = max<int16_t>(0, x - 2);
  const int16_t clearY = max<int16_t>(0, textY - 2);
  const int16_t clearW = min<int16_t>(240 - clearX, w + 4);
  d.fillRect(clearX, clearY, clearW, 14, bg);
  d.setFont(&fonts::Font0);
  d.setTextColor(fg, bg);
  d.setCursor(x, textY);
  d.print(fitCurrentFontToWidth(text, w));
  d.setFont(&fonts::Font2);
}

void renderFocusUi() {
  auto& d = gCanvas;
  const bool isBreak = gFocus.state == FocusState::ShortBreak || gFocus.state == FocusState::LongBreak;
  const bool isReflect = gFocus.state == FocusState::Reflect;
  const bool isPaused = gFocus.state == FocusState::Paused || gFocus.state == FocusState::Stopped;
  uint16_t bg = isBreak ? rgb565_local(1, 10, 8) : isReflect ? rgb565_local(10, 4, 16) : rgb565_local(2, 6, 12);
  uint16_t panel = isBreak ? rgb565_local(5, 23, 19) : isReflect ? rgb565_local(20, 11, 31) : rgb565_local(7, 14, 24);
  uint16_t panel2 = isBreak ? rgb565_local(10, 40, 31) : isReflect ? rgb565_local(34, 18, 52) : rgb565_local(10, 23, 36);
  uint16_t accent = isBreak ? rgb565_local(105, 255, 156)
                    : isReflect ? rgb565_local(214, 133, 255)
                    : isPaused ? rgb565_local(154, 143, 255)
                               : rgb565_local(47, 227, 255);
  uint16_t secondary = isBreak ? rgb565_local(47, 227, 255)
                       : isReflect ? rgb565_local(255, 194, 74)
                                   : rgb565_local(246, 194, 74);
  uint16_t muted = rgb565_local(126, 143, 157);
  uint32_t now = millis();
  float t = now / 1000.0f;

  String timeText = focusTimeString(gFocus.remainingSec);
  uint32_t elapsed = gFocus.sessionTotalSec > gFocus.remainingSec ? gFocus.sessionTotalSec - gFocus.remainingSec : 0;
  int progressDeg = gFocus.sessionTotalSec ? static_cast<int>((elapsed * 300ULL) / gFocus.sessionTotalSec) : 0;
  progressDeg = clampValue<int>(progressDeg, 0, 300);

  d.fillScreen(bg);
  for (int x = -20; x < 260; x += 20) {
    int offset = static_cast<int>(sinf(t * 0.45f + x * 0.045f) * 5.0f);
    d.drawFastVLine(x + offset, 0, 135, rgb565_local(5, 18, 27));
  }
  d.fillRoundRect(6, 5, 228, 124, 14, panel);
  d.drawRoundRect(6, 5, 228, 124, 14, rgb565_local(28, 50, 70));

  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(14, 12);
  d.print(fitCurrentFontToWidth(focusStateName(gFocus.state), 74));
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(101, 15);
  d.print("cycle ");
  d.print(gFocus.cycle);
  d.print("/");
  d.print(gFocusSettings.cyclesPerRound);
  String done = String(gFocus.completedToday) + " done";
  d.setCursor(222 - d.textWidth(done), 15);
  d.print(done);

  if (isBreak) {
    float breath = (sinf(t * 1.3f) + 1.0f) * 0.5f;
    int r = 20 + static_cast<int>(breath * 15.0f);
    int cx = 70;
    int cy = 67;
    d.fillCircle(cx, cy, r + 13, rgb565_local(4, 18, 20));
    d.drawCircle(cx, cy, r + 7, rgb565_local(28, 84, 74));
    d.drawCircle(cx, cy, r, accent);
    d.fillCircle(cx, cy, max<int>(8, r - 13), panel2);
    d.setFont(&fonts::Font0);
    d.setTextColor(accent, panel2);
    d.setCursor(cx - 22, cy - 3);
    d.print(breath > 0.55f ? "inhale" : "exhale");
    d.setFont(&fonts::Font7);
    d.setTextColor(TFT_WHITE, panel);
    int timeWidth = d.textWidth(timeText);
    d.setCursor(clampValue<int>(226 - timeWidth, 98, 128), 45);
    d.print(timeText);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(130, 86);
    d.print("follow the ball");
  } else if (isReflect) {
    d.fillRoundRect(18, 34, 204, 62, 10, panel2);
    d.drawRoundRect(18, 34, 204, 62, 10, accent);
    d.setFont(&fonts::Font2);
    d.setTextColor(secondary, panel2);
    d.setCursor(31, 43);
    d.print("REFLECT");
    d.setTextColor(TFT_WHITE, panel2);
    d.setFont(&fonts::Font7);
    d.setCursor(31, 58);
    d.print(timeText);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel2);
    d.setCursor(132, 46);
    d.print("write 1 line");
    d.setCursor(132, 58);
    d.print("then return");
    for (int i = 0; i < 9; ++i) {
      int px = 156 + i * 6;
      int py = 83 + static_cast<int>(sinf(t * 1.2f + i) * 3.0f);
      d.fillCircle(px, py, 1, (i % 2) ? accent : secondary);
    }
  } else {
    int cx = 72;
    int cy = 70;
    d.fillCircle(cx, cy, 47, rgb565_local(4, 16, 24));
    d.drawCircle(cx, cy, 44, rgb565_local(22, 47, 64));
    d.drawArc(cx, cy, 40, 33, -150, -150 + progressDeg, accent);
    d.drawArc(cx, cy, 28, 23, 40 + static_cast<int>(t * 18) % 360,
              280 + static_cast<int>(t * 18) % 360, secondary);
    d.fillCircle(cx, cy, isPaused ? 16 : 19 + static_cast<int>(sinf(t * 2.0f) * 2.0f), panel2);
    d.setFont(&fonts::Font0);
    d.setTextColor(accent, panel2);
    d.setCursor(cx - 15, cy - 3);
    d.print(isPaused ? "idle" : "deep");
    d.setFont(&fonts::Font7);
    d.setTextColor(TFT_WHITE, panel);
    int timeWidth = d.textWidth(timeText);
    d.setCursor(clampValue<int>(226 - timeWidth, 92, 124), 44);
    d.print(timeText);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(132, 86);
    d.print(gFocusSettings.metronome ? "metro pulse" : "quiet focus");
  }

  if (gFocusSettings.metronome && !isBreak && !isReflect) {
    int pivotX = 204;
    int pivotY = 39;
    float swing = sinf(t * max<float>(0.6f, gFocusSettings.bpm / 60.0f) * kPi);
    int tipX = pivotX + static_cast<int>(swing * 18.0f);
    int tipY = pivotY + 42;
    d.drawLine(pivotX, pivotY, tipX, tipY, secondary);
    d.fillCircle(pivotX, pivotY, 3, secondary);
    d.fillCircle(tipX, tipY, 5, accent);
  }

  int progressW = gFocus.sessionTotalSec ? static_cast<int>((elapsed * 204ULL) / gFocus.sessionTotalSec) : 0;
  progressW = clampValue<int>(progressW, 0, 204);
  d.fillRoundRect(18, 102, 204, 8, 4, rgb565_local(18, 33, 45));
  d.fillRoundRect(19, 103, max<int>(1, progressW), 6, 3, accent);

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
    d.setFont(&fonts::Font2);
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
    int progress = 18;
    if (gPlaybackActive && gPlaybackStreamPath == asset.path && gPlaybackStreamTotalBytes > 0) {
      size_t played = gPlaybackStreamTotalBytes > gPlaybackStreamRemainingBytes
                          ? gPlaybackStreamTotalBytes - gPlaybackStreamRemainingBytes
                          : 0;
      progress = static_cast<int>((played * 126ULL) / gPlaybackStreamTotalBytes);
      progress = max<int>(4, min<int>(126, progress));
    } else if (gPlaybackActive) {
      progress = 10 + ((millis() / 180) % 28);
    }
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

String readNoteLineAt(fs::FS& fs, const String& path, uint32_t targetLine) {
  File file = fs.open(path, "r");
  if (!file) {
    return "";
  }
  uint32_t line = 0;
  String value;
  while (file.available()) {
    value = file.readStringUntil('\n');
    value.trim();
    if (line == targetLine) {
      file.close();
      return value;
    }
    ++line;
  }
  file.close();
  return "";
}

void drawNotesHeader(const String& title, uint16_t accent, uint16_t bg, uint16_t panel) {
  auto& d = gCanvas;
  d.fillScreen(bg);
  d.fillRoundRect(5, 5, 230, 125, 8, panel);
  d.drawRoundRect(5, 5, 230, 125, 8, rgb565_local(42, 55, 63));
  d.fillRoundRect(10, 10, 220, 17, 5, rgb565_local(3, 11, 17));
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, rgb565_local(3, 11, 17));
  d.setCursor(16, 14);
  d.print(fitCurrentFontToWidth(title, 148));
  String store = activeNotesStorageLabel();
  d.setTextColor(TFT_LIGHTGREY, rgb565_local(3, 11, 17));
  d.setCursor(224 - d.textWidth(store), 14);
  d.print(store);
}

void renderNotesUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(5, 13, 19);
  const uint16_t rowBg = rgb565_local(7, 18, 25);
  const uint16_t selected = rgb565_local(14, 40, 48);
  const uint16_t line = rgb565_local(35, 52, 62);
  const uint16_t accent = rgb565_local(255, 211, 106);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(100, 255, 170);
  const uint16_t muted = rgb565_local(132, 148, 160);

  if (!gNotesScanned) {
    scanNotes();
  }
  if (!activeNotesFs()) {
    renderSdRequiredBanner("Notes");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }

  if (gNotesMode == NotesMode::List) {
    drawNotesHeader("Notes", accent, bg, panel);
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(16, 31);
    d.print(fitCurrentFontToWidth(gNotesStatus, 208));

    if (gNotes.empty()) {
      d.fillRoundRect(20, 48, 200, 42, 9, rowBg);
      d.drawRoundRect(20, 48, 200, 42, 9, accent);
      d.setFont(&fonts::Font2);
      d.setTextColor(accent, rowBg);
      d.setCursor(54, 61);
      d.print("No notes yet");
      d.setFont(&fonts::Font0);
      d.setTextColor(muted, rowBg);
      d.setCursor(44, 77);
      d.print("Press N to create first note");
      drawTinyFooter(panel, muted, "N new  R rescan  Tab menu", 16, 118, 208);
      renderLauncherOverlay();
      renderDebugOverlay();
      return;
    }

    clampNoteSelection();
    constexpr int visible = 6;
    int maxStart = max<int>(0, static_cast<int>(gNotes.size()) - visible);
    int start = clampValue<int>(min<int>(gNoteListScrollOffset, gSelectedNote), 0, maxStart);
    if (gSelectedNote < start) {
      start = gSelectedNote;
    } else if (gSelectedNote >= start + visible) {
      start = gSelectedNote - visible + 1;
    }
    gNoteListScrollOffset = start;
    for (int i = 0; i < visible; ++i) {
      int idx = start + i;
      if (idx >= static_cast<int>(gNotes.size())) {
        break;
      }
      const NoteFile& note = gNotes[idx];
      bool isSelected = idx == gSelectedNote;
      int y = 43 + i * 12;
      uint16_t fill = isSelected ? selected : rowBg;
      d.fillRoundRect(14, y, 212, 10, 4, fill);
      d.drawRoundRect(14, y, 212, 10, 4, isSelected ? accent : line);
      d.setTextColor(isSelected ? TFT_WHITE : TFT_LIGHTGREY, fill);
      d.setCursor(20, y + 2);
      d.print(fitCurrentFontToWidth(note.name, 145));
      String meta = String(note.lineCount) + "l " + compactBytes(note.size);
      d.setTextColor(isSelected ? accent : muted, fill);
      d.setCursor(222 - d.textWidth(meta), y + 2);
      d.print(meta);
    }
    drawTinyFooter(panel, muted, "< > pick  Ent open  N new  E edit", 13, 118, 214);
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }

  const bool edit = gNotesMode == NotesMode::Edit;
  drawNotesHeader(edit ? "Edit Note" : "Read Note", edit ? green : cyan, bg, panel);
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  d.setCursor(16, 31);
  d.print(fitCurrentFontToWidth(gActiveNoteName.isEmpty() ? "untitled" : gActiveNoteName, 150));
  d.setTextColor(edit ? green : cyan, panel);
  String mode = edit ? "APPEND" : "VIEW";
  d.setCursor(224 - d.textWidth(mode), 31);
  d.print(mode);

  fs::FS* fs = activeNotesFs();
  uint32_t lines = 0;
  if (fs && !gActiveNotePath.isEmpty()) {
    lines = countNoteLines(*fs, gActiveNotePath);
  }
  int previewRows = edit ? 3 : 5;
  int maxScroll = max<int>(0, static_cast<int>(lines) - previewRows);
  gNoteScrollOffset = clampValue<int>(gNoteScrollOffset, 0, maxScroll);
  int startLine = edit ? max<int>(0, static_cast<int>(lines) - previewRows) : gNoteScrollOffset;

  d.fillRoundRect(12, 43, 216, edit ? 49 : 71, 7, rgb565_local(3, 10, 15));
  d.drawRoundRect(12, 43, 216, edit ? 49 : 71, 7, rgb565_local(22, 40, 52));
  d.setFont(&fonts::efontCN_12);
  int y = 47;
  for (int i = 0; i < previewRows; ++i) {
    uint32_t lineNo = static_cast<uint32_t>(startLine + i);
    if (lineNo >= lines) {
      break;
    }
    String lineText = fs ? readNoteLineAt(*fs, gActiveNotePath, lineNo) : "";
    d.setTextColor(i % 2 == 0 ? TFT_LIGHTGREY : muted, rgb565_local(3, 10, 15));
    d.setCursor(18, y);
    d.print(fitCurrentFontToWidth(lineText, 202));
    y += 13;
  }
  d.setFont(&fonts::Font0);
  if (!edit && maxScroll > 0) {
    int railH = 64;
    int knobH = max<int>(8, railH / (maxScroll + 1));
    int knobY = 47 + ((railH - knobH) * gNoteScrollOffset) / max<int>(1, maxScroll);
    d.fillRoundRect(224, 48, 2, railH, 1, rgb565_local(23, 32, 42));
    d.fillRoundRect(224, knobY, 2, knobH, 1, cyan);
  }

  if (edit) {
    d.fillRoundRect(12, 98, 216, 18, 7, rgb565_local(2, 17, 22));
    d.drawRoundRect(12, 98, 216, 18, 7, gNoteInputBuffer.isEmpty() ? line : green);
    d.setTextColor(gNoteInputBuffer.isEmpty() ? muted : TFT_WHITE, rgb565_local(2, 17, 22));
    d.setFont(&fonts::efontCN_12);
    String input = gNoteInputBuffer.isEmpty() ? "type note line..." : tailUtf8ToFit(gNoteInputBuffer, 190);
    d.setCursor(20, 101);
    d.print(input);
    d.setFont(&fonts::Font0);
    d.setTextColor(green, panel);
    d.setCursor(16, 119);
    d.print("Enter save line  Ctrl+S done");
  } else {
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(16, 119);
    d.print("< > scroll  E append  L list");
  }
  renderLauncherOverlay();
  renderDebugOverlay();
}

void drawServiceOrb(int16_t cx, int16_t cy, uint16_t accent, uint16_t secondary, const String& label, bool active) {
  auto& d = gCanvas;
  uint32_t now = millis();
  float t = now / 1000.0f;
  int pulse = active ? static_cast<int>((sinf(t * 4.0f) + 1.0f) * 2.0f) : 0;
  d.fillCircle(cx, cy, 37, rgb565_local(3, 12, 18));
  d.drawCircle(cx, cy, 34 + pulse, rgb565_local(18, 45, 58));
  d.drawArc(cx, cy, 31, 25, static_cast<int>(t * 42.0f) % 360,
            (static_cast<int>(t * 42.0f) + 250) % 360, accent);
  d.drawArc(cx, cy, 20, 15, (220 - static_cast<int>(t * 64.0f)) % 360,
            (40 - static_cast<int>(t * 64.0f)) % 360, secondary);
  d.fillCircle(cx, cy, 12, active ? accent : rgb565_local(13, 31, 40));
  d.setFont(&fonts::Font0);
  d.setTextColor(active ? rgb565_local(2, 6, 12) : accent, active ? accent : rgb565_local(13, 31, 40));
  d.setCursor(cx - d.textWidth(label) / 2, cy - 3);
  d.print(label);
  d.setFont(&fonts::Font2);
}

void drawInfoRow(int16_t x, int16_t y, int16_t w, const String& label, const String& value,
                 uint16_t bg, uint16_t labelColor, uint16_t valueColor) {
  auto& d = gCanvas;
  d.fillRoundRect(x, y, w, 12, 4, bg);
  d.setFont(&fonts::Font0);
  d.setTextColor(labelColor, bg);
  d.setCursor(x + 6, y + 3);
  d.print(fitCurrentFontToWidth(label, 52));
  String fitted = fitCurrentFontToWidth(value, w - 68);
  d.setTextColor(valueColor, bg);
  d.setCursor(x + w - 7 - d.textWidth(fitted), y + 3);
  d.print(fitted);
  d.setFont(&fonts::Font2);
}

void drawActionChip(int16_t x, int16_t y, int16_t w, const String& key, const String& label,
                    uint16_t accent, uint16_t bg) {
  auto& d = gCanvas;
  d.fillRoundRect(x, y, w, 14, 5, bg);
  d.drawRoundRect(x, y, w, 14, 5, accent);
  d.setFont(&fonts::Font0);
  d.setTextColor(accent, bg);
  d.setCursor(x + 5, y + 4);
  d.print(key);
  d.setTextColor(TFT_LIGHTGREY, bg);
  d.setCursor(x + 18, y + 4);
  d.print(fitCurrentFontToWidth(label, w - 23));
  d.setFont(&fonts::Font2);
}

void renderAssetsUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Asset Sync");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  auto& d = gCanvas;
  fs::FS* fs = activeVoiceFs();
  const bool indexReady = fs && fsExists(*fs, kAudioIndexPath);
  const bool ready = gAssetManifest.parsed && gAssetManifest.fileCount > 0;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(7, 14, 24);
  const uint16_t panel2 = rgb565_local(10, 24, 34);
  const uint16_t row = rgb565_local(13, 28, 40);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(126, 143, 157);

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 14, panel);
  d.drawRoundRect(6, 5, 228, 124, 14, ready ? green : amber);
  d.drawRoundRect(10, 9, 220, 116, 11, rgb565_local(18, 34, 48));

  d.setFont(&fonts::Font2);
  d.setTextColor(green, panel);
  d.setCursor(16, 14);
  d.print("ASSET SYNC");
  d.setFont(&fonts::Font0);
  String state = ready ? "READY" : (gAssetManifest.present ? "CHECK" : "FETCH");
  d.setTextColor(ready ? green : amber, panel);
  d.setCursor(224 - d.textWidth(state), 17);
  d.print(state);

  drawServiceOrb(48, 66, green, cyan, "AS", ready);

  int x = 91;
  drawInfoRow(x, 34, 128, "manifest", gAssetManifest.present ? (gAssetManifest.parsed ? "ok" : "bad") : "missing",
              row, muted, ready ? green : amber);
  drawInfoRow(x, 49, 128, "version", gAssetManifest.version.isEmpty() ? "-" : gAssetManifest.version,
              row, muted, TFT_WHITE);
  drawInfoRow(x, 64, 128, "files", String(gAssetManifest.fileCount) + " / " + compactBytes(gAssetManifest.totalBytes),
              row, muted, cyan);
  drawInfoRow(x, 79, 128, "storage", String(gStorageReady ? gStorageLabel : "off") + (indexReady ? " index" : ""),
              row, muted, gStorageReady ? green : amber);

  if (!gAssetManifest.error.isEmpty()) {
    d.setFont(&fonts::Font0);
    d.setTextColor(amber, panel);
    d.setCursor(18, 99);
    d.print(fitCurrentFontToWidth(gAssetManifest.error, 202));
  } else {
    int fillW = ready ? min<int>(128, max<int>(12, gAssetManifest.fileCount * 128 / max<int>(1, gAssetManifest.fileCount))) : 24 + ((millis() / 90) % 82);
    d.fillRoundRect(91, 100, 128, 7, 4, rgb565_local(19, 35, 46));
    d.fillRoundRect(92, 101, fillW, 5, 3, ready ? green : amber);
  }

  drawActionChip(17, 112, 58, "F", "fetch", green, panel2);
  drawActionChip(81, 112, 58, "S", "sync", cyan, panel2);
  drawActionChip(145, 112, 58, "R", "load", amber, panel2);
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
  auto& d = gCanvas;
  const bool hasCandidate = gOtaManifest.parsed && !gOtaManifest.version.isEmpty();
  const bool updateReady = hasCandidate && gOtaManifest.version != kAppVersion;
  const uint16_t bg = rgb565_local(3, 5, 10);
  const uint16_t panel = rgb565_local(12, 12, 20);
  const uint16_t panel2 = rgb565_local(25, 16, 24);
  const uint16_t row = rgb565_local(18, 24, 35);
  const uint16_t red = rgb565_local(255, 83, 92);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t muted = rgb565_local(126, 143, 157);
  const uint16_t accent = gOtaPendingVerify ? amber : (updateReady ? red : green);

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 14, panel);
  d.drawRoundRect(6, 5, 228, 124, 14, accent);
  d.drawRoundRect(10, 9, 220, 116, 11, rgb565_local(42, 36, 48));

  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(16, 14);
  d.print("FIRMWARE OTA");
  d.setFont(&fonts::Font0);
  String state = gOtaPendingVerify ? "VERIFY" : updateReady ? "UPDATE" : hasCandidate ? "CURRENT" : "FETCH";
  d.setTextColor(accent, panel);
  d.setCursor(224 - d.textWidth(state), 17);
  d.print(state);

  drawServiceOrb(48, 66, accent, cyan, "OTA", updateReady || gOtaPendingVerify);

  int x = 91;
  drawInfoRow(x, 34, 128, "current", String(kAppVersion), row, muted, TFT_WHITE);
  drawInfoRow(x, 49, 128, "candidate", gOtaManifest.version.isEmpty() ? "-" : gOtaManifest.version,
              row, muted, updateReady ? red : green);
  drawInfoRow(x, 64, 128, "size", gOtaManifest.size ? compactBytes(gOtaManifest.size) : "-",
              row, muted, cyan);
  drawInfoRow(x, 79, 128, "slot", gOtaRunningSlot + " " + (gOtaPendingVerify ? "pending" : gOtaBootState),
              row, muted, gOtaPendingVerify ? amber : muted);

  d.setFont(&fonts::Font0);
  d.setTextColor(gOtaConfirmedThisBoot ? green : (gOtaPendingVerify ? amber : muted), panel);
  d.setCursor(18, 99);
  if (!gOtaManifest.error.isEmpty()) {
    d.setTextColor(amber, panel);
    d.print(fitCurrentFontToWidth(gOtaManifest.error, 202));
  } else if (gOtaPendingVerify) {
    d.print("new app is pending verification");
  } else if (updateReady) {
    d.print("ready to download and flash");
  } else if (hasCandidate) {
    d.print("installed version is current");
  } else {
    d.print("fetch manifest from bridge");
  }

  if (gOtaPendingVerify) {
    drawActionChip(17, 112, 58, "C", "confirm", green, panel2);
    drawActionChip(81, 112, 58, "X", "rollback", red, panel2);
    drawActionChip(145, 112, 58, "F", "fetch", cyan, panel2);
  } else {
    drawActionChip(17, 112, 58, "F", "fetch", cyan, panel2);
    drawActionChip(81, 112, 58, "A", "apply", red, panel2);
    drawActionChip(145, 112, 58, "R", "check", amber, panel2);
  }
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
  d.print(fitCurrentFontToWidth(bottom, 130));
  d.setCursor(152, 114);
  String meta = String("poop ") + String(gPet.poop) + " wifi " + String(gPet.env.netCount);
  d.print(fitCurrentFontToWidth(meta, 72));

  drawTinyFooter(panel, muted, "F food P play C clean S sleep H hunt", 14, 123, 212);

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
  d.drawCircle(cx, cy, 51 + static_cast<int16_t>(breathe), rgb565_local(8, 34, 43));
  d.drawCircle(cx, cy, 49 + static_cast<int16_t>(breathe), c3);
  drawAssistantPulseArc(cx, cy, 45, 36, -2.25f, 1.00f, rgb565_local(10, 38, 48));
  drawAssistantPulseArc(cx, cy, 31, 23, 0.58f, 4.54f, rgb565_local(11, 35, 48));
  drawAssistantPulseArc(cx, cy, 45, 36, -2.25f + spin, 1.00f + spin, c1);
  drawAssistantPulseArc(cx, cy, 31, 23, 0.58f - spin * 1.08f, 4.54f - spin * 1.08f, c2);

  if (state == AssistantPulseState::Recording) {
    int16_t pulseR = 18 + static_cast<int16_t>(level * 14.0f + sinf(t * 12.0f));
    d.fillCircle(cx, cy, pulseR, c1);
    d.fillCircle(cx, cy, 9, fg);
  } else if (state == AssistantPulseState::Thinking) {
    for (int i = 0; i < 4; ++i) {
      float a = spin * 1.9f + i * 1.55f;
      drawAssistantPulseArc(cx, cy, 22, 16, a, a + 0.72f, (i % 2) ? c2 : c1);
    }
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

  const uint16_t clockPanel = state == AssistantPulseState::Recording
                                  ? rgb565_local(32, 13, 19)
                                  : state == AssistantPulseState::Thinking
                                        ? rgb565_local(28, 22, 11)
                                        : state == AssistantPulseState::Playback
                                              ? rgb565_local(5, 27, 24)
                                              : rgb565_local(3, 21, 38);
  d.fillRoundRect(119, 13, 103, 57, 14, clockPanel);
  d.drawRoundRect(119, 13, 103, 57, 14, rgb565_local(14, 58, 82));
  for (int i = 0; i < 5; ++i) {
    int16_t yy = 21 + i * 9 + static_cast<int16_t>(sinf(t * 0.9f + i * 0.8f) * 2.0f);
    d.drawFastHLine(126, yy, 89, rgb565_local(5, 35, 58));
  }
  d.fillRoundRect(128, 61, 82, 2, 1, rgb565_local(16, 83, 112));

	  d.setFont(&fonts::Font7);
  d.setTextColor(fg, clockPanel);
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
    if (state == AssistantPulseState::Playback) {
      uint8_t band = static_cast<uint8_t>(clampValue<int>(
          (i * kPlaybackSpectrumBands) / max<int>(1, bars), 0, kPlaybackSpectrumBands - 1));
      int16_t h = static_cast<int16_t>(4 + gVoiceEqBars[band] * 2);
      h = clampValue<int16_t>(h, 5, 42);
      uint16_t barColor = (i % 3 == 0) ? c2 : c1;
      d.fillRoundRect(bx - 2, waveY - h / 2, 4, h, 2, barColor);
      uint8_t peak = gVoiceEqPeaks[band];
      int16_t py = waveY - static_cast<int16_t>(min<int>(42, 4 + peak * 2)) / 2 - 2;
      d.drawFastHLine(bx - 2, py, 4, fg);
      continue;
    }
    float amp = state == AssistantPulseState::Recording ? 1.05f :
                0.68f;
    int16_t h = static_cast<int16_t>((8.0f + fabsf(sinf(t * 3.6f + i * 0.74f)) * 22.0f +
                                      fabsf(cosf(t * 1.7f + i * 0.31f)) * 7.0f) *
                                     amp * (0.65f + level * 0.75f));
    h = clampValue<int16_t>(h, 3, 42);
    d.fillRoundRect(bx - 1, waveY - h / 2, 3, h, 2, (i % 2) ? c1 : c2);
  }

  if (state == AssistantPulseState::Playback) {
    d.fillRoundRect(117, 110, 88, 2, 1, rgb565_local(23, 61, 69));
    int16_t progress = static_cast<int16_t>((playbackProgressPct() * 88U) / 100U);
    d.fillRoundRect(117, 110, max<int16_t>(1, progress), 3, 1, c1);
    d.setFont(&fonts::Font0);
    d.setTextColor(rgb565_local(150, 244, 211), bg);
    d.setCursor(206, 107);
    d.print(formatPlaybackMs(playbackRemainingMs()));
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
  const uint16_t bg = rgb565_local(2, 6, 12);
  const uint16_t panel = rgb565_local(7, 14, 24);
  const uint16_t panel2 = rgb565_local(10, 24, 36);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(105, 255, 156);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(126, 143, 157);
  d.fillScreen(bg);
  d.fillRoundRect(6, 6, 228, 123, 14, panel);
  d.drawRoundRect(6, 6, 228, 123, 14, rgb565_local(25, 48, 68));

  d.setFont(&fonts::Font2);
  d.setTextColor(cyan, panel);
  d.setCursor(16, 14);
  d.print("VOICE NOTES");
  String count = String(max<int>(0, static_cast<int>(gVoiceNotes.size()))) + " wav";
  d.setTextColor(muted, panel);
  d.setCursor(224 - d.textWidth(count), 14);
  d.print(count);

  uint32_t now = millis();
  const int spectrumBaseY = 112;
  const int spectrumX = 16;
  const int spectrumGap = 13;
  for (uint8_t i = 0; i < kPlaybackSpectrumBands; ++i) {
    int h = gPlaybackActive ? (6 + gVoiceEqBars[i] * 3) : (7 + ((i * 3 + now / 180) % 12));
    h = clampValue<int>(h, 5, 56);
    int x = spectrumX + i * spectrumGap;
    uint16_t fill = gPlaybackActive
                        ? (i < 5 ? rgb565_local(68, 255, 178)
                                 : (i < 11 ? rgb565_local(47, 227, 255) : rgb565_local(255, 194, 74)))
                        : rgb565_local(17, 57, 72);
    d.fillRoundRect(x, spectrumBaseY - h, 7, h, 3, rgb565_local(4, 20, 28));
    d.fillRoundRect(x + 1, spectrumBaseY - h + 1, 5, h - 1, 2, fill);
    if (gPlaybackActive) {
      int peakY = spectrumBaseY - clampValue<int>(6 + gVoiceEqPeaks[i] * 3, 5, 56) - 3;
      d.drawFastHLine(x + 1, peakY, 5, rgb565_local(236, 255, 246));
    }
  }

  int cx = 52;
  int cy = 68;
  for (int i = 0; i < 18; ++i) {
    float a = i * 0.349f + now * 0.004f;
    int r = 18 + (i % 3) * 4;
    int x = cx + static_cast<int>(cosf(a) * r);
    int y = cy + static_cast<int>(sinf(a) * r);
    d.fillCircle(x, y, 1 + (i % 2), gPlaybackActive ? green : cyan);
  }
  d.fillCircle(cx, cy, 18, panel2);
  d.drawCircle(cx, cy, 19, gPlaybackActive ? green : cyan);
  d.setTextColor(gPlaybackActive ? green : cyan, panel2);
  d.setTextDatum(middle_center);
  d.setFont(&fonts::Font4);
  d.drawString(gPlaybackActive ? "PLAY" : "WAV", cx, cy - 4);
  d.setTextDatum(top_left);
  d.setFont(&fonts::Font2);

  if (gVoiceNotes.empty()) {
    d.setTextColor(amber, panel);
    d.setCursor(92, 54);
    d.print("No saved notes");
    d.setTextColor(muted, panel);
    d.setCursor(92, 70);
    d.print("Record voice first");
  } else {
    gSelectedVoiceNote = clampValue<int>(gSelectedVoiceNote, 0, static_cast<int>(gVoiceNotes.size()) - 1);
    const auto& note = gVoiceNotes[gSelectedVoiceNote];
    String who = note.assistant ? "AI" : "ME";
    d.fillRoundRect(88, 38, 134, 54, 10, panel2);
    d.drawRoundRect(88, 38, 134, 54, 10, note.assistant ? green : cyan);
    d.setTextColor(note.assistant ? green : cyan, panel2);
    d.setCursor(98, 46);
    d.print(who);
    d.setTextColor(TFT_WHITE, panel2);
    d.setCursor(120, 46);
    d.print(marqueeText(note.title, 90, 260));
    d.setTextColor(muted, panel2);
    d.setCursor(98, 61);
    d.print(trimPreview(note.preview, 22));
    d.setCursor(98, 76);
    d.print(String(gSelectedVoiceNote + 1) + "/" + String(gVoiceNotes.size()));

    String timeText = "--:--";
    if (gPlaybackActive) {
      timeText = formatPlaybackMs(playbackRemainingMs());
    }
    int progressW = gPlaybackActive ? static_cast<int>((playbackProgressPct() * 112U) / 100U) : 0;
    d.fillRoundRect(98, 87, 114, 3, 2, rgb565_local(26, 43, 55));
    if (progressW > 0) {
      d.fillRoundRect(98, 87, progressW, 3, 2, green);
    }
    d.setTextColor(gPlaybackActive ? green : amber, panel);
    d.setCursor(184, 96);
    d.print(timeText);
  }

  gVoiceGraphSpeed++;

  int volW = map(gSpeakerVolume, 0, 255, 0, 58);
  d.fillRoundRect(88, 99, 64, 7, 4, rgb565_local(21, 35, 46));
  d.fillRoundRect(91, 101, volW, 3, 2, amber);
  drawTinyFooter(panel, muted, "< > note  Ent play  +/- vol", 15, 120, 210);

  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderChatUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(3, 7, 13);
  const uint16_t panel = rgb565_local(6, 13, 23);
  const uint16_t userBg = rgb565_local(8, 43, 55);
  const uint16_t aiBg = rgb565_local(11, 42, 31);
  const uint16_t sysBg = rgb565_local(20, 25, 35);
  const uint16_t accent = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(100, 255, 170);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(126, 143, 157);
  d.fillScreen(bg);
  d.setTextSize(1);
  d.setFont(&fonts::Font2);

  bool showFace = gUiMode == UiMode::ChatFace;
  if (showFace) {
    renderFacePanel(false);
  }

  bool showStatus = !gStatusText.isEmpty() && gStatusText != "Ready";
  int inputLines = 1;
  if (gInputBuffer.length() > 36) {
    inputLines = 2;
  }
  if (gInputBuffer.length() > 78) {
    inputLines = 3;
  }
  int composerH = 17 + (inputLines - 1) * 11;
  int composerY = 135 - composerH - 3;
  int transcriptTop = showFace ? 78 : 8;
  int transcriptBottom = composerY - (showStatus ? 14 : 4);

  d.setFont(&fonts::efontCN_12);
  int lineHeight = d.fontHeight() + 1;
  int visibleRows = max<int>(1, (transcriptBottom - transcriptTop) / max<int>(1, lineHeight + 2));
  int y = transcriptTop;
  auto lines = buildVisualLines(showFace ? 216 : 228);
  int maxOffset = max<int>(0, static_cast<int>(lines.size()) - visibleRows);
  if (gChatScrollOffset > maxOffset) {
    gChatScrollOffset = maxOffset;
  }
  int startIndex = max<int>(0, static_cast<int>(lines.size()) - visibleRows - gChatScrollOffset);
  int endIndex = min<int>(static_cast<int>(lines.size()), startIndex + visibleRows);

  if (!showFace) {
    d.fillRoundRect(4, 4, 232, transcriptBottom - 2, 10, panel);
    d.drawRoundRect(4, 4, 232, transcriptBottom - 2, 10, rgb565_local(18, 35, 50));
  }

  for (int i = startIndex; i < endIndex; ++i) {
    LineKind kind = lines[i].kind;
    uint16_t bubble = kind == LineKind::User ? userBg : (kind == LineKind::Assistant ? aiBg : sysBg);
    uint16_t ink = kind == LineKind::User ? accent : (kind == LineKind::Assistant ? green : muted);
    int bx = kind == LineKind::User ? 22 : 8;
    int bw = showFace ? 212 : 224;
    if (kind == LineKind::User && !showFace) {
      bw = 210;
    }
    d.fillRoundRect(bx, y - 1, bw, lineHeight + 1, 4, bubble);
    d.setTextColor(ink, bubble);
    d.setCursor(bx + 5, y);
    d.print(fitCurrentFontToWidth(lines[i].text, bw - 10));
    y += lineHeight + 2;
  }
  if (maxOffset > 0 && !showFace) {
    int railX = 232;
    int railH = max<int>(12, transcriptBottom - transcriptTop - 8);
    int knobH = max<int>(8, railH / (maxOffset + 1));
    int knobY = transcriptTop + 4 + ((railH - knobH) * (maxOffset - gChatScrollOffset)) / max<int>(1, maxOffset);
    d.fillRoundRect(railX, transcriptTop + 4, 3, railH, 2, rgb565_local(24, 35, 48));
    d.fillRoundRect(railX, knobY, 3, knobH, 2, accent);
  } else if (showFace) {
    drawScrollIndicators(gChatScrollOffset < maxOffset, gChatScrollOffset > 0);
  }

  if (showStatus) {
    d.setFont(&fonts::Font0);
    d.setTextColor(amber, bg);
    d.setCursor(8, composerY - 11);
    d.print(fitCurrentFontToWidth(gStatusText, 220));
  }

  d.setFont(&fonts::efontCN_12);
  d.fillRoundRect(6, composerY, 228, composerH, 7, rgb565_local(2, 17, 29));
  d.drawRoundRect(6, composerY, 228, composerH, 7, gInputBuffer.isEmpty() ? rgb565_local(28, 51, 65) : accent);
  d.fillRoundRect(10, composerY + 4, 18, composerH - 8, 5, rgb565_local(4, 32, 45));
  d.setTextColor(gInputBuffer.isEmpty() ? muted : amber, rgb565_local(4, 32, 45));
  d.setCursor(15, composerY + 5);
  d.print(">");
  d.setTextColor(gInputBuffer.isEmpty() ? muted : TFT_WHITE, rgb565_local(2, 17, 29));
  String inputLine = gInputBuffer.isEmpty() ? "type to chat" : tailUtf8ToFit(gInputBuffer, 174);
  d.setCursor(34, composerY + 4);
  d.print(inputLine);
  d.setFont(&fonts::Font0);
  d.setTextColor(accent, rgb565_local(2, 17, 29));
  d.setCursor(204, composerY + composerH - 9);
  d.print("ENT");
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
      case UiMode::Notes:
        renderNotesUi();
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
  uint8_t stageIndex = 0;
  String stageName = stage && stage[0] ? String(stage) : String("");
  if (stageName == "Display") {
    stageIndex = 1;
  } else if (stageName == "Audio") {
    stageIndex = 2;
  } else if (stageName == "Storage") {
    stageIndex = 3;
  } else if (stageName == "Input") {
    stageIndex = 4;
  } else if (stageName == "Config") {
    stageIndex = 5;
  } else if (stageName == "Wi-Fi") {
    stageIndex = 6;
  }

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
    float phase = stageIndex * 0.36f;

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
  d.drawEllipse(120, 66, 82, 42 + static_cast<int16_t>(stageIndex % 2), mintDim);

  d.setFont(&fonts::Font2);
  d.setTextSize(1);
  d.fillRoundRect(15, 14, 86, 17, 7, rgb565_local(6, 16, 13));
  d.drawRoundRect(15, 14, 86, 17, 7, mint);
  d.setTextColor(TFT_WHITE, rgb565_local(6, 16, 13));
  d.setCursor(23, 18);
  d.print("ADV ");
  d.setTextColor(mint, rgb565_local(6, 16, 13));
  d.print(kAppVersion);

  uint8_t pct = static_cast<uint8_t>(clampValue<int>(12 + stageIndex * 14, 12, 96));
  String pctText = String(pct) + "%";
  d.setTextColor(gold, panel);
  d.setCursor(220 - d.textWidth(pctText), 18);
  d.print(pctText);
  d.drawRoundRect(108, 17, 59, 4, 2, panelLine);
  d.fillRoundRect(111 + pct / 4, 18, 18, 2, 1, TFT_WHITE);

  const char* states[] = {"DNA", "LINK", "SYNC", "READY"};
  const char* label = states[clampValue<int>((stageIndex + 1) / 2, 0, 3)];
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
  loadPetState();
  beginOtaBootGuard();
  pushSystem("Device ready: " + gDeviceId);
  pushSystem("Ctrl+Space = EN/RU");
  pushSystem("Voice: Ctrl x2 / Ctrl+V / hold G0");
  pushSystem("Tab screen, Tab+Down app, Ctrl+L menu");
  pushSystem("/pet = OpenClaw pet, /petreset resets");
  pushSystem(String("FW ") + kAppVersion + " " + String(kBuildGitSha).substring(0, 10));
  logf("[BOOT] device_id=%s", gDeviceId.c_str());
  logf("[BOOT] version=%s git=%s build=%s", kAppVersion, kBuildGitSha, kBuildTime);

  renderBootSplash("Wi-Fi");
  if (ensureWifi()) {
    render();
    startHubWebSocket();
    if (gOtaPendingVerify && probeOtaBridgeHealth(8000)) {
      confirmOtaBootIfPending("bridge health");
    }
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
    serviceOtaBootGuard();
    syncClockFromNtp(false);
    updateBatterySnapshot();
    updateFocusTimer();
    serviceFocusMetronome();
    servicePet();
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
