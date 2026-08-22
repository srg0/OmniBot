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

IPAddress smartLampBroadcastAddress() {
  IPAddress ip = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  IPAddress broadcast;
  for (uint8_t i = 0; i < 4; ++i) {
    broadcast[i] = static_cast<uint8_t>((ip[i] & mask[i]) | static_cast<uint8_t>(~mask[i]));
  }
  return broadcast;
}

bool ensureSmartLampUdp() {
  if (WiFi.status() != WL_CONNECTED) {
    if (gSmartLamp.udpReady) {
      gSmartLampUdp.stop();
      gSmartLamp.udpReady = false;
    }
    gSmartLamp.online = false;
    gSmartLamp.status = "NO WIFI";
    return false;
  }
  if (gSmartLamp.udpReady) {
    return true;
  }
  gSmartLamp.udpReady = gSmartLampUdp.begin(kSmartLampControllerPort) == 1;
  if (!gSmartLamp.udpReady) {
    gSmartLamp.status = "UDP ERROR";
    logf("[LAMP] udp bind failed port=%u", static_cast<unsigned>(kSmartLampControllerPort));
    return false;
  }
  logf("[LAMP] udp ready port=%u", static_cast<unsigned>(kSmartLampControllerPort));
  return true;
}

bool smartLampSendPacket(const String& packet, bool forceBroadcast) {
  if (!ensureSmartLampUdp()) {
    setStatus("smartLamp: Wi-Fi unavailable", 1000);
    return false;
  }
  IPAddress target = (!forceBroadcast && gSmartLamp.found) ? gSmartLamp.ip : smartLampBroadcastAddress();
  if (!gSmartLampUdp.beginPacket(target, kSmartLampUdpPort)) {
    gSmartLamp.status = "SEND ERROR";
    return false;
  }
  gSmartLampUdp.write(reinterpret_cast<const uint8_t*>(packet.c_str()), packet.length());
  bool sent = gSmartLampUdp.endPacket() == 1;
  if (!sent) {
    gSmartLamp.status = "SEND ERROR";
  }
  return sent;
}

void smartLampRequestState(bool forceBroadcast = false) {
  if (smartLampSendPacket("GET", forceBroadcast)) {
    gSmartLamp.lastDiscoveryMs = millis();
    if (!gSmartLamp.online) {
      gSmartLamp.status = "SEARCHING";
    }
  }
}

void smartLampStartScan() {
  gSmartLamp.found = false;
  gSmartLamp.online = false;
  gSmartLamp.ip = IPAddress();
  gSmartLamp.status = "SEARCHING";
  gSmartLamp.lastDiscoveryMs = 0;
  gSmartLamp.refreshDueMs = 0;
  smartLampRequestState(true);
  setStatus("smartLamp scan", 900);
}

void smartLampProcessPacket(const String& packet, const IPAddress& remoteIp) {
  if (!packet.startsWith("CURR")) {
    return;
  }
  int effect = 0;
  int brightness = 0;
  int speed = 0;
  int scale = 0;
  int power = 0;
  if (sscanf(packet.c_str(), "CURR %d %d %d %d %d", &effect, &brightness, &speed, &scale, &power) != 5) {
    return;
  }
  bool wasOnline = gSmartLamp.online;
  gSmartLamp.ip = remoteIp;
  gSmartLamp.found = true;
  gSmartLamp.online = true;
  gSmartLamp.effect = static_cast<uint8_t>(clampValue<int>(effect, 0, kSmartLampEffectCount - 1));
  gSmartLamp.brightness = static_cast<uint8_t>(clampValue<int>(brightness, 0, 255));
  gSmartLamp.speed = static_cast<uint8_t>(clampValue<int>(speed, 0, 255));
  gSmartLamp.scale = static_cast<uint8_t>(clampValue<int>(scale, 0, 100));
  gSmartLamp.power = power != 0;
  gSmartLamp.lastSeenMs = millis();
  gSmartLamp.refreshDueMs = 0;
  gSmartLamp.status = "ONLINE";
  if (!wasOnline) {
    logf("[LAMP] found ip=%s effect=%u", remoteIp.toString().c_str(), static_cast<unsigned>(gSmartLamp.effect));
    setStatus("smartLamp online", 900);
  }
}

void smartLampSendControl(const String& command, const String& label) {
  if (!smartLampSendPacket(command, !gSmartLamp.found)) {
    setStatus("smartLamp send failed", 900);
    return;
  }
  gSmartLamp.status = "SENT";
  gSmartLamp.refreshDueMs = millis() + kSmartLampRefreshDelayMs;
  setStatus(label, 650);
}

void smartLampSelectControl(int delta) {
  int selected = static_cast<int>(gSmartLamp.selected) + delta;
  selected = (selected + kSmartLampControlCount) % kSmartLampControlCount;
  gSmartLamp.selected = static_cast<uint8_t>(selected);
}

void smartLampAdjustSelected(int delta) {
  switch (gSmartLamp.selected) {
    case 0: {
      int effect = (static_cast<int>(gSmartLamp.effect) + delta + kSmartLampEffectCount) % kSmartLampEffectCount;
      gSmartLamp.effect = static_cast<uint8_t>(effect);
      smartLampSendControl("EFF" + String(effect), "Effect " + String(effect));
      break;
    }
    case 1: {
      int brightness = clampValue<int>(static_cast<int>(gSmartLamp.brightness) + delta * 5, 0, 255);
      gSmartLamp.brightness = static_cast<uint8_t>(brightness);
      smartLampSendControl("BRI" + String(brightness), "Brightness " + String(brightness));
      break;
    }
    case 2: {
      int speed = clampValue<int>(static_cast<int>(gSmartLamp.speed) + delta * 5, 0, 255);
      gSmartLamp.speed = static_cast<uint8_t>(speed);
      smartLampSendControl("SPD" + String(speed), "Speed " + String(speed));
      break;
    }
    case 3:
    default: {
      int scale = clampValue<int>(static_cast<int>(gSmartLamp.scale) + delta * 2, 0, 100);
      gSmartLamp.scale = static_cast<uint8_t>(scale);
      smartLampSendControl("SCA" + String(scale), "Scale " + String(scale));
      break;
    }
  }
}

void smartLampTogglePower() {
  gSmartLamp.power = !gSmartLamp.power;
  smartLampSendControl(gSmartLamp.power ? "P_ON" : "P_OFF", gSmartLamp.power ? "Lamp on" : "Lamp off");
}

void serviceSmartLamp() {
  if (WiFi.status() != WL_CONNECTED) {
    ensureSmartLampUdp();
    return;
  }
  if (!ensureSmartLampUdp()) {
    return;
  }

  for (uint8_t handled = 0; handled < 4; ++handled) {
    int packetSize = gSmartLampUdp.parsePacket();
    if (packetSize <= 0) {
      break;
    }
    char packet[96] = {};
    int read = gSmartLampUdp.read(reinterpret_cast<uint8_t*>(packet), sizeof(packet) - 1);
    if (read <= 0) {
      continue;
    }
    packet[read] = '\0';
    String response(packet);
    response.trim();
    smartLampProcessPacket(response, gSmartLampUdp.remoteIP());
  }

  uint32_t now = millis();
  if (gSmartLamp.online && now - gSmartLamp.lastSeenMs > kSmartLampOnlineTimeoutMs) {
    gSmartLamp.online = false;
    gSmartLamp.status = "NO REPLY";
  }
  if (gSmartLamp.refreshDueMs != 0 && static_cast<int32_t>(now - gSmartLamp.refreshDueMs) >= 0) {
    gSmartLamp.refreshDueMs = 0;
    smartLampRequestState(false);
  }
  if (gUiMode == UiMode::SmartLamp && now - gSmartLamp.lastDiscoveryMs >= kSmartLampDiscoveryIntervalMs) {
    smartLampRequestState(!gSmartLamp.found || !gSmartLamp.online);
  }
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
