constexpr const char* kDeviceType = "cardputer_adv";
constexpr const char* kDefaultDisplayName = "ADV Cardputer";
#ifndef APP_VERSION
#define APP_VERSION "0.2.36-dev"
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
constexpr const char* kRealtimeWsPath = "/api/cardputer/v1/realtime";
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
constexpr const char* kRealtimeReplyPcmPath = "/voice/__realtime_reply.pcm";
constexpr uint8_t kSdSckPin = 40;
constexpr uint8_t kSdMisoPin = 39;
constexpr uint8_t kSdMosiPin = 14;
constexpr uint8_t kSdCsPin = 12;
constexpr uint32_t kSdFrequencyHz = 25000000;
constexpr size_t kRecordChunkSamples = 1024;
constexpr size_t kRecordChunkBytes = kRecordChunkSamples * sizeof(int16_t);
constexpr size_t kRecordChunkBufferCount = 3;
constexpr uint32_t kRealtimeInputSampleRate = kMicSampleRate;
constexpr uint32_t kRealtimeModelSampleRate = 24000;
constexpr size_t kRealtimeInputChunkSamples = 512;
constexpr size_t kRealtimeInputChunkBytes = kRealtimeInputChunkSamples * sizeof(int16_t);
constexpr size_t kRealtimeUpsampleRatio = kRealtimeModelSampleRate / kRealtimeInputSampleRate;
constexpr size_t kRealtimeOutputChunkSamples = kRealtimeInputChunkSamples * kRealtimeUpsampleRatio;
constexpr size_t kRealtimeOutputChunkBytes = kRealtimeOutputChunkSamples * sizeof(int16_t);
constexpr size_t kRealtimeChunkBufferCount = 3;
constexpr uint32_t kRecordDrainTimeoutMs = 900;
constexpr size_t kPlaybackChunkSamples = 2048;
constexpr size_t kPlaybackChunkBytes = kPlaybackChunkSamples * sizeof(int16_t);
constexpr size_t kPlaybackBufferCount = 2;
constexpr uint8_t kPlaybackSpectrumBands = 16;
constexpr uint8_t kPlaybackSpectrumMaxBar = 18;
constexpr size_t kOtaIoBufferBytes = 4096;
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
