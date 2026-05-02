
void handleTypingKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  setKeyDiag(String("key=") + String(static_cast<int>(key)));
  if (key == '`' || key == '~' || key == 27 || hasHidKey(keys, kHidKeyEscape)) {
    handleGlobalEscape();
    return;
  }
  if (gRealtimeRecording && !(keys.ctrl && lowerKey == 'r')) {
    setStatus("Ctrl+R stop realtime", 600);
    return;
  }
  if (keys.ctrl && keys.space) {
    gKeyboardLayout = (gKeyboardLayout == KeyboardLayout::En) ? KeyboardLayout::Ru : KeyboardLayout::En;
    setStatus(String("Layout ") + layoutName(gKeyboardLayout), 900);
    return;
  }
  if (keys.ctrl && keys.shift && lowerKey == 'r') {
    revivePet("hotkey");
    setUiMode(UiMode::Hero);
    return;
  }
  if (keys.ctrl && !keys.shift && !keys.alt && !keys.opt && !keys.fn && lowerKey == 'r') {
    toggleRealtimeVoice();
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
