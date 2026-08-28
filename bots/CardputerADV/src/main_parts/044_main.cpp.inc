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

void smartLampUpdateFromPreset() {
  if (!gSmartLamp.presetsLearned || gSmartLamp.presets.count == 0) {
    return;
  }
  uint8_t selected = min<uint8_t>(gSmartLamp.presets.selected, gSmartLamp.presets.count - 1);
  gSmartLamp.effect = selected;
  gSmartLamp.speed = gSmartLamp.presets.values[selected][cardputer_smartlamp::kPresetSpeedIndex];
  gSmartLamp.scale = gSmartLamp.presets.values[selected][cardputer_smartlamp::kPresetScaleIndex];
  if (gSmartLamp.presets.values[selected][1] != 0) {
    gSmartLamp.brightness = gSmartLamp.presets.values[selected][2];
  }
}

void smartLampLoadPresetsOnce() {
  if (gSmartLamp.presetsLoaded) {
    return;
  }
  gSmartLamp.presetsLoaded = true;
  cardputer_smartlamp::PresetBank saved;
  gPrefs.begin(kSmartLampPrefsNs, true);
  size_t stored = gPrefs.getBytesLength("presets_v1");
  if (stored == sizeof(saved)) {
    gPrefs.getBytes("presets_v1", &saved, sizeof(saved));
  }
  gPrefs.end();
  if (stored != sizeof(saved) || saved.count == 0 ||
      saved.count > cardputer_smartlamp::kMaxPresets || saved.selected >= saved.count) {
    return;
  }
  gSmartLamp.presets = saved;
  gSmartLamp.presetsLearned = true;
  smartLampUpdateFromPreset();
  logf("[LAMP] restored %u GyverLamp2 presets", static_cast<unsigned>(saved.count));
}

void smartLampSavePresets() {
  if (!gSmartLamp.presetsLearned || gSmartLamp.presets.count == 0) {
    return;
  }
  gPrefs.begin(kSmartLampPrefsNs, false);
  gPrefs.putBytes("presets_v1", &gSmartLamp.presets, sizeof(gSmartLamp.presets));
  gPrefs.end();
}

bool ensureSmartLampUdp() {
  smartLampLoadPresetsOnce();
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
  gSmartLamp.udpReady = gSmartLampUdp.begin(kSmartLampUdpPort) == 1;
  if (!gSmartLamp.udpReady) {
    gSmartLamp.status = "UDP ERROR";
    logf("[LAMP] GyverLamp2 bind failed port=%u", static_cast<unsigned>(kSmartLampUdpPort));
    return false;
  }
  logf("[LAMP] GyverLamp2 listening port=%u", static_cast<unsigned>(kSmartLampUdpPort));
  return true;
}

bool smartLampSendPacket(const String& packet) {
  if (!ensureSmartLampUdp()) {
    setStatus("smartLamp: Wi-Fi unavailable", 1000);
    return false;
  }
  if (!gSmartLampUdp.beginPacket(smartLampBroadcastAddress(), kSmartLampUdpPort)) {
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

void smartLampClockFields(uint8_t& weekday, uint8_t& hour, uint8_t& minute, uint8_t& second) {
  weekday = 0;
  hour = 0;
  minute = 0;
  second = 0;
  time_t now = time(nullptr);
  if (now < 1700000000) {
    return;
  }
  struct tm localNow = {};
  localtime_r(&now, &localNow);
  weekday = static_cast<uint8_t>(localNow.tm_wday);
  hour = static_cast<uint8_t>(localNow.tm_hour);
  minute = static_cast<uint8_t>(localNow.tm_min);
  second = static_cast<uint8_t>(localNow.tm_sec);
}

String smartLampControlPacket(uint8_t command) {
  uint8_t weekday = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  smartLampClockFields(weekday, hour, minute, second);
  return "GL,0," + String(command) + "," + String(weekday) + "," +
         String(hour) + "," + String(minute) + "," + String(second);
}

void smartLampRequestState(bool forceBroadcast = false) {
  (void)forceBroadcast;
  ensureSmartLampUdp();
  if (!gSmartLamp.online && WiFi.status() == WL_CONNECTED) {
    gSmartLamp.status = "SEARCHING";
  }
}

void smartLampStartScan() {
  gSmartLamp.found = false;
  gSmartLamp.online = false;
  gSmartLamp.ip = IPAddress();
  gSmartLamp.status = "SEARCHING";
  gSmartLamp.lastSeenMs = 0;
  if (gSmartLamp.udpReady) {
    gSmartLampUdp.stop();
    gSmartLamp.udpReady = false;
  }
  ensureSmartLampUdp();
  setStatus("smartLamp: listening GL2", 900);
}

bool smartLampLearnPresetPacket(const String& packet) {
  cardputer_smartlamp::PresetBank parsed;
  if (!cardputer_smartlamp::parsePresetPacket(packet.c_str(), parsed)) {
    return false;
  }
  bool changed = !gSmartLamp.presetsLearned ||
                 memcmp(&parsed, &gSmartLamp.presets, sizeof(parsed)) != 0;
  gSmartLamp.presets = parsed;
  gSmartLamp.presetsLearned = true;
  smartLampUpdateFromPreset();
  if (changed) {
    smartLampSavePresets();
    logf("[LAMP] learned %u presets current=%u", static_cast<unsigned>(parsed.count),
         static_cast<unsigned>(parsed.selected));
    setStatus("smartLamp presets synced", 1100);
  }
  return true;
}

void smartLampProcessPacket(const String& packet, const IPAddress& remoteIp) {
  if (packet.startsWith("GL,2,")) {
    smartLampLearnPresetPacket(packet);
    return;
  }
  if (!packet.startsWith("GL_ONL")) {
    return;
  }
  int preset = packet.substring(7).toInt();
  if (preset < 0 || preset >= static_cast<int>(cardputer_smartlamp::kMaxPresets)) {
    return;
  }
  bool wasOnline = gSmartLamp.online;
  gSmartLamp.ip = remoteIp;
  gSmartLamp.found = true;
  gSmartLamp.online = true;
  gSmartLamp.effect = static_cast<uint8_t>(preset);
  if (gSmartLamp.presetsLearned && gSmartLamp.effect < gSmartLamp.presets.count) {
    gSmartLamp.presets.selected = gSmartLamp.effect;
    smartLampUpdateFromPreset();
  }
  gSmartLamp.lastSeenMs = millis();
  gSmartLamp.status = "ONLINE";
  if (!wasOnline) {
    logf("[LAMP] GyverLamp2 ip=%s port=%u preset=%u", remoteIp.toString().c_str(),
         static_cast<unsigned>(kSmartLampUdpPort), static_cast<unsigned>(gSmartLamp.effect));
    setStatus("smartLamp online", 900);
  }
}

void smartLampSendControl(const String& command, const String& label) {
  if (!smartLampSendPacket(command)) {
    setStatus("smartLamp send failed", 900);
    return;
  }
  gSmartLamp.status = "SENT";
  gSmartLamp.lastCommandMs = millis();
  setStatus(label, 650);
}

bool smartLampSendPresetBank() {
  if (!gSmartLamp.presetsLearned || gSmartLamp.presets.count == 0 ||
      gSmartLamp.effect >= gSmartLamp.presets.count) {
    return false;
  }
  gSmartLamp.presets.selected = gSmartLamp.effect;
  uint8_t weekday = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  smartLampClockFields(weekday, hour, minute, second);
  static char packet[cardputer_smartlamp::kMaxPacketBytes] = {};
  size_t length = cardputer_smartlamp::buildPresetPacket(
      gSmartLamp.presets, weekday, hour, minute, second, packet, sizeof(packet));
  if (length == 0 || !smartLampSendPacket(String(packet, length))) {
    gSmartLamp.status = "SEND ERROR";
    return false;
  }
  gSmartLamp.lastCommandMs = millis();
  gSmartLamp.status = "SENT";
  return true;
}

void smartLampSelectControl(int delta) {
  int selected = static_cast<int>(gSmartLamp.selected) + delta;
  selected = (selected + kSmartLampControlCount) % kSmartLampControlCount;
  gSmartLamp.selected = static_cast<uint8_t>(selected);
}

void smartLampAdjustSelected(int delta) {
  switch (gSmartLamp.selected) {
    case 0: {
      uint8_t command = delta < 0 ? 4 : 5;
      smartLampSendControl(smartLampControlPacket(command), delta < 0 ? "Previous preset" : "Next preset");
      break;
    }
    case 1: {
      int brightness = clampValue<int>(static_cast<int>(gSmartLamp.brightness) + delta * 5, 0, 255);
      gSmartLamp.brightness = static_cast<uint8_t>(brightness);
      smartLampSendControl("GL,4,2," + String(brightness), "Brightness " + String(brightness));
      break;
    }
    case 2: {
      if (!gSmartLamp.presetsLearned || gSmartLamp.effect >= gSmartLamp.presets.count) {
        gSmartLamp.status = "SYNC";
        setStatus("Move a phone lamp slider once", 1400);
        return;
      }
      int speed = clampValue<int>(static_cast<int>(gSmartLamp.speed) + delta * 5, 0, 255);
      gSmartLamp.speed = static_cast<uint8_t>(speed);
      gSmartLamp.presets.selected = gSmartLamp.effect;
      gSmartLamp.presets.values[gSmartLamp.effect][cardputer_smartlamp::kPresetSpeedIndex] = gSmartLamp.speed;
      gSmartLamp.presetSendDueMs = millis() + kSmartLampPresetSendDelayMs;
      setStatus("Speed " + String(speed), 650);
      break;
    }
    case 3:
    default: {
      if (!gSmartLamp.presetsLearned || gSmartLamp.effect >= gSmartLamp.presets.count) {
        gSmartLamp.status = "SYNC";
        setStatus("Move a phone lamp slider once", 1400);
        return;
      }
      int scale = clampValue<int>(static_cast<int>(gSmartLamp.scale) + delta * 2, 0, 255);
      gSmartLamp.scale = static_cast<uint8_t>(scale);
      gSmartLamp.presets.selected = gSmartLamp.effect;
      gSmartLamp.presets.values[gSmartLamp.effect][cardputer_smartlamp::kPresetScaleIndex] = gSmartLamp.scale;
      gSmartLamp.presetSendDueMs = millis() + kSmartLampPresetSendDelayMs;
      setStatus("Scale " + String(scale), 650);
      break;
    }
  }
}

void smartLampTogglePower() {
  gSmartLamp.power = !gSmartLamp.power;
  smartLampSendControl(smartLampControlPacket(gSmartLamp.power ? 1 : 0),
                       gSmartLamp.power ? "Lamp on" : "Lamp off");
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
    static char packet[cardputer_smartlamp::kMaxPacketBytes] = {};
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
  if (gSmartLamp.presetSendDueMs != 0 &&
      static_cast<int32_t>(now - gSmartLamp.presetSendDueMs) >= 0 &&
      now - gSmartLamp.lastCommandMs >= kSmartLampMinCommandGapMs) {
    gSmartLamp.presetSendDueMs = 0;
    if (smartLampSendPresetBank()) {
      smartLampSavePresets();
    }
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
  const uint8_t submenuCount = kLauncherGroups[gLauncherSelection].count;
  const auto shortcut = launcher_shortcuts::resolve(
      key, keys.ctrl, kLauncherGroupCount, submenuCount);
  if (shortcut.kind == launcher_shortcuts::Kind::Submenu) {
    gLauncherSubSelection = shortcut.index;
    gLauncherLastSub[gLauncherSelection] = shortcut.index;
    applyLauncherSelection();
    return;
  }
  if (shortcut.kind == launcher_shortcuts::Kind::InvalidSubmenu) {
    setStatus("No item " + String(shortcut.index + 1), 700);
    return;
  }
  if (shortcut.kind == launcher_shortcuts::Kind::Group) {
    gLauncherSelection = shortcut.index;
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
