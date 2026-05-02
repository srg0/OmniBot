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

