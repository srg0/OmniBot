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
bool gPetHelpVisible = false;
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
bool gRealtimeActive = false;
bool gRealtimeConnected = false;
bool gRealtimeStartAfterReady = false;
bool gRealtimeRecording = false;
bool gRealtimeAwaiting = false;
bool gRealtimeGotAudio = false;
File gActiveRecordFile;
String gActiveRecordPath;
size_t gActiveRecordBytes = 0;
size_t gActiveRecordCommittedBytes = 0;
size_t gActiveRecordExpectedBytes = 0;
std::deque<uint8_t> gRecordInflightChunks;
uint8_t gRecordNextChunkIndex = 0;
int16_t gRecordChunkBuffers[kRecordChunkBufferCount][kRecordChunkSamples];
String gRealtimeSessionId;
String gRealtimeText;
String gRealtimeReplyPath = kRealtimeReplyPcmPath;
File gRealtimeReplyFile;
size_t gRealtimeReplyBytes = 0;
uint32_t gRealtimeStartedMs = 0;
std::deque<uint8_t> gRealtimeInflightChunks;
uint8_t gRealtimeNextChunkIndex = 0;
int16_t gRealtimeInputBuffers[kRealtimeChunkBufferCount][kRealtimeInputChunkSamples];
int16_t gRealtimeUpsampleBuffer[kRealtimeOutputChunkSamples];
File gPlaybackStreamFile;
String gPlaybackStreamPath;
uint32_t gPlaybackStreamSampleRate = 0;
size_t gPlaybackStreamRemainingBytes = 0;
size_t gPlaybackStreamTotalBytes = 0;
uint8_t gPlaybackStreamNextBuffer = 0;
bool gPlaybackStreamRawPcm = false;
uint8_t gPlaybackStreamBuffers[kPlaybackBufferCount][kPlaybackChunkBytes];
// OTA apply runs on Arduino's loopTask; keep the large flash scratch buffer
// out of that task stack or the device reboots before selecting the new slot.
uint8_t gOtaIoBuffer[kOtaIoBufferBytes];
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
bool gOtaApplyInProgress = false;
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
void pushError(const String& text);
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
void serviceRealtimeVoice();
void toggleRealtimeVoice();
