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
  pushAssistant(displayReply);
}

bool setupKeyboardInput() {
  // M5Cardputer 1.1.1 owns the ADV TCA8418 reader and its IRQ/state machine.
  // It is initialized by M5Cardputer.begin(..., true).
  gKeyboardState = M5Cardputer.Keyboard.keysState();
  logf("[KEY] using M5Cardputer ADV keyboard reader");
  setKeyDiag("kbd:m5-adv");
  return true;
}

void refreshKeyboardState() {
  // M5Cardputer.update() has already advanced the built-in keyboard reader.
  gKeyboardState = M5Cardputer.Keyboard.keysState();
  String sig = keyboardSignatureFromState();
  if (sig != gLastKeySignature) {
    gLastKeySignature = sig;
    setKeyDiag(sig.isEmpty() ? "-" : sig);
    if (!sig.isEmpty()) {
      logf("[KEY] %s", sig.c_str());
    }
  }
}
