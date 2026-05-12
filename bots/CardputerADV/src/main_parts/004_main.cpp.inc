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

struct IrRemoteSlot {
  IRData receivedIRData = {};
  uint8_t rawCode[RAW_BUFFER_LENGTH] = {};
  uint16_t rawCodeLength = 0;
  bool learned = false;
  uint32_t learnedAtMs = 0;
};

struct IrRemotePersistSlot {
  uint32_t magic = kIrPersistMagic;
  uint8_t learned = 0;
  uint8_t reserved = 0;
  uint16_t rawCodeLength = 0;
  IRData receivedIRData = {};
  uint8_t rawCode[RAW_BUFFER_LENGTH] = {};
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
    {"Focus", "25", "Timer and focus setup", rgb565_local(246, 194, 74), 3,
     {{"Focus Space", UiMode::Focus}, {"Alarm", UiMode::Alarm}, {"Stopwatch", UiMode::Stopwatch}}},
    {"System", "SYS", "Bridge and firmware", rgb565_local(180, 150, 255), 2,
     {{"Logs", UiMode::Logs}, {"Firmware", UiMode::Ota}}},
    {"Device", "DEV", "Power and settings", rgb565_local(203, 216, 226), 2,
     {{"Battery", UiMode::Battery}, {"Settings", UiMode::Settings}}},
    {"Utilities", "UTIL", "Tools and games", rgb565_local(255, 150, 80), 4,
     {{"Calculator", UiMode::Calculator}, {"IR Remote", UiMode::IrRemote}, {"Tetris", UiMode::Tetris}, {"Calendar", UiMode::Calendar}}},
    {"Visuals", "C", "Ambient screen savers", rgb565_local(88, 240, 141), 1,
     {{"C-Matrix", UiMode::CMatrix}}},
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
String gRealtimeExtraHeaders;
uint16_t gHubPort = kDefaultHubPort;

std::deque<ChatLine> gChatLines;
String gInputBuffer;
String gStatusText = "Booting";
String gCalcExpression;
String gCalcResult = "0";
String gCalcStatus = "Ready";
String gIrRemoteStatus = "IR idle";
String gStopwatchStatus = "Ready";
String gTetrisStatus = "Ready";
String gAlarmStatus = "Ready";
String gAlarmEditBuffer;
String gDeferredDeviceActionsJson;
bool gPendingOtaCheckAction = false;
bool gPendingOtaApplyAction = false;
uint32_t gPendingOtaActionAfterMs = 0;
std::vector<VoiceNote> gVoiceNotes;
std::vector<NoteFile> gNotes;
std::vector<AudioAsset> gAudioAssets;
std::vector<AudioFolder> gAudioFolders;
std::vector<ConversationContext> gContexts;
std::deque<ContextPreviewLine> gContextPreview;
std::deque<String> gRuntimeLogs;
IrRemoteSlot gIrRemoteSlots[kIrRemoteSlotCount];

bool gWifiReady = false;
bool gWsReady = false;
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
bool gPostInstallStoryPending = false;
uint32_t gPostInstallStoryNextMs = 0;
uint8_t gPostInstallStoryAttempts = 0;
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
uint32_t gInboxLastSeq = 0;
uint32_t gLastInboxAutoPollMs = 0;
String gInboxLastTopicKey;
String gInboxLastText;
String gInboxLastKind;
String gInboxLastVoiceUrl;
String gInboxLastVoiceTitle;
String gInboxLastVoiceSha256;
uint32_t gInboxLastVoiceSize = 0;
String gInboxLastSender;
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
int gCalendarMonthOffset = 0;
int gSelectedIrRemoteSlot = 0;

uint32_t gLastRenderMs = 0;
uint32_t gLastUiSlowLogMs = 0;
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
bool gStopwatchRunning = false;
uint32_t gStopwatchElapsedMs = 0;
uint32_t gStopwatchStartedMs = 0;
uint32_t gStopwatchLapMs = 0;
uint8_t gTetrisBoard[16][10] = {};
uint8_t gTetrisPiece = 0;
uint8_t gTetrisRotation = 0;
uint8_t gTetrisNextPiece = 1;
int8_t gTetrisX = 3;
int8_t gTetrisY = 0;
uint32_t gTetrisScore = 0;
uint16_t gTetrisLines = 0;
uint8_t gTetrisLevel = 1;
uint32_t gTetrisLastDropMs = 0;
bool gTetrisStarted = false;
bool gTetrisPaused = false;
bool gTetrisGameOver = false;
bool gTetrisRightHandPortrait = true;
bool gTetrisHelpVisible = false;
bool gAlarmRinging = false;
bool gIrRemoteReady = false;
bool gIrRemoteLearning = false;
bool gIrRemoteAutoLearning = false;
uint32_t gAlarmTriggeredAtMs = 0;
uint32_t gAlarmNextBeepMs = 0;
uint32_t gIrRemoteLearnStartedMs = 0;
uint32_t gIrLastReceivePinProbeMs = 0;
uint8_t gIrReceivePinIndex = 0;
time_t gAlarmSnoozeEpoch = 0;
int16_t gAlarmLastTriggerYear = -1;
int16_t gAlarmLastTriggerYday = -1;
