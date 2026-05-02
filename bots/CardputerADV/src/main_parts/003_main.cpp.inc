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
  Realtime,
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
    {"ui.open", "open screen", "open stopwatch"},
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
  bool voicePlayable = false;
  String voiceUrl;
  String voiceTitle;
  String voiceSha256;
  uint32_t voiceSize = 0;

  ChatLine() = default;
  ChatLine(const String& linePrefix, const String& lineText, LineKind lineKind)
      : prefix(linePrefix), text(lineText), kind(lineKind) {}
  ChatLine(const String& linePrefix, const String& lineText, LineKind lineKind, bool playable,
           const String& url, const String& title, const String& sha256, uint32_t size)
      : prefix(linePrefix),
        text(lineText),
        kind(lineKind),
        voicePlayable(playable),
        voiceUrl(url),
        voiceTitle(title),
        voiceSha256(sha256),
        voiceSize(size) {}
};

struct VisualLine {
  String text;
  LineKind kind;
  int16_t chatIndex = -1;

  VisualLine() = default;
  VisualLine(const String& lineText, LineKind lineKind, int16_t lineChatIndex)
      : text(lineText), kind(lineKind), chatIndex(lineChatIndex) {}
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
  bool voicePlayable = false;
  String voiceUrl;
  String voiceTitle;
  String voiceSha256;
  uint32_t voiceSize = 0;
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
