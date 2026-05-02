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

