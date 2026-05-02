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

  if (cmd == "/petrevive" || cmd == "/tamarevive") {
    revivePet("slash");
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
