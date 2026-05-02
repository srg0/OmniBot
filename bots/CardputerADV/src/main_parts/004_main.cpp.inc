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
WebSocketsClient gRealtimeWs;
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

