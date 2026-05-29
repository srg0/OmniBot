  if (type == "focus.set_duration" || type == "focus.start") {
    if (!action["minutes"].isNull()) {
      uint32_t seconds = static_cast<uint32_t>(actionInt(action, "minutes", gFocusSettings.focusSec / 60, 1, 60)) * 60UL;
      gFocusSettings.focusSec = clampValue<uint32_t>(seconds, kFocusMinDurationSec, kFocusMaxDurationSec);
      if (gFocus.state == FocusState::Stopped) {
        gFocus.remainingSec = gFocusSettings.focusSec;
        gFocus.sessionTotalSec = gFocusSettings.focusSec;
      }
      saveFocusSettings();
    }
    setUiMode(UiMode::Focus);
    if (type == "focus.start" && !focusStateRuns(gFocus.state)) {
      focusStartOrResume();
    }
    setStatus("Action: focus " + String(gFocusSettings.focusSec / 60) + "m", 1200);
    return true;
  }
  if (type == "focus.pause") {
    setUiMode(UiMode::Focus);
    if (focusStateRuns(gFocus.state)) {
      focusStartOrResume();
    }
    return true;
  }
  if (type == "focus.resume") {
    setUiMode(UiMode::Focus);
    if (gFocus.state == FocusState::Paused || gFocus.state == FocusState::Stopped) {
      focusStartOrResume();
    }
    return true;
  }
  if (type == "focus.reset") {
    focusReset();
    setUiMode(UiMode::Focus);
    return true;
  }
  if (type == "focus.set_short_break") {
    gFocusSettings.shortBreakSec = static_cast<uint32_t>(actionInt(action, "minutes", gFocusSettings.shortBreakSec / 60, 1, 60)) * 60UL;
    saveFocusSettings();
    setStatus("Action: short break " + String(gFocusSettings.shortBreakSec / 60) + "m", 1200);
    return true;
  }
  if (type == "focus.set_long_break") {
    gFocusSettings.longBreakSec = static_cast<uint32_t>(actionInt(action, "minutes", gFocusSettings.longBreakSec / 60, 1, 60)) * 60UL;
    saveFocusSettings();
    setStatus("Action: long break " + String(gFocusSettings.longBreakSec / 60) + "m", 1200);
    return true;
  }
  if (type == "focus.set_cycles") {
    gFocusSettings.cyclesPerRound = static_cast<uint8_t>(actionInt(action, "cycles", gFocusSettings.cyclesPerRound, 1, 9));
    saveFocusSettings();
    setStatus("Action: cycles " + String(gFocusSettings.cyclesPerRound), 1200);
    return true;
  }
  if (type == "focus.set_autostart") {
    gFocusSettings.autoStart = actionBool(action["value"], gFocusSettings.autoStart);
    saveFocusSettings();
    setStatus(gFocusSettings.autoStart ? "Action: auto on" : "Action: auto off", 1200);
    return true;
  }
  if (type == "focus.set_metronome") {
    gFocusSettings.metronome = actionBool(action["value"], gFocusSettings.metronome);
    saveFocusSettings();
    setStatus(gFocusSettings.metronome ? "Action: metro on" : "Action: metro off", 1200);
    return true;
  }
  if (type == "focus.set_bpm") {
    gFocusSettings.bpm = static_cast<uint16_t>(actionInt(action, "bpm", gFocusSettings.bpm, kFocusMinBpm, kFocusMaxBpm));
    saveFocusSettings();
    setStatus("Action: bpm " + String(gFocusSettings.bpm), 1200);
    return true;
  }
  if (type == "ui.open") {
    UiMode mode;
    String modeName = action["mode"] | "";
    if (modeName.isEmpty()) {
      modeName = action["screen"] | "";
    }
    if (mapActionUiMode(modeName, mode)) {
      setUiMode(mode);
      return true;
    }
    return false;
  }
  if (type == "audio.play") {
    return playAudioAssetByAction(action);
  }
  if (type == "voice.play") {
    return playVoiceNoteByAction(action);
  }
  if (type == "settings.set_audio_language") {
    String language = action["value"] | "";
    language.trim();
    language.toLowerCase();
    if (language == "ru" || language == "en" || language == "es") {
      gDeviceSettings.audioLanguage = language;
      saveDeviceSettings();
      setStatus("Action: lang " + language, 1200);
      return true;
    }
    return false;
  }
  if (type == "settings.set_ota_auto") {
    gDeviceSettings.autoUpdateFirmware = actionBool(action["value"], gDeviceSettings.autoUpdateFirmware);
    saveDeviceSettings();
    setStatus(gDeviceSettings.autoUpdateFirmware ? "Action: OTA auto" : "Action: OTA manual", 1200);
    return true;
  }
  if (type == "settings.set_ota_boot_check") {
    gDeviceSettings.checkUpdatesOnBoot = actionBool(action["value"], gDeviceSettings.checkUpdatesOnBoot);
    saveDeviceSettings();
    setStatus(gDeviceSettings.checkUpdatesOnBoot ? "Action: boot check" : "Action: no boot check", 1200);
    return true;
  }
  if (type == "settings.set_ota_min_battery") {
    gDeviceSettings.minBatteryForUpdate = static_cast<uint8_t>(actionInt(action, "value", gDeviceSettings.minBatteryForUpdate, 5, 95));
    saveDeviceSettings();
    setStatus("Action: OTA battery " + String(gDeviceSettings.minBatteryForUpdate) + "%", 1200);
    return true;
  }
  if (type == "ota.check") {
    gPendingOtaCheckAction = true;
    gPendingOtaActionAfterMs = millis() + 500;
    logf("[ACT] ota.check queued");
    setUiMode(UiMode::Ota);
    setStatus("Action: OTA check", 700);
    return true;
  }
  if (type == "ota.apply") {
    gPendingOtaCheckAction = true;
    gPendingOtaApplyAction = true;
    gPendingOtaActionAfterMs = millis() + 500;
    logf("[ACT] ota.apply queued");
    setUiMode(UiMode::Ota);
    setStatus("Action: OTA apply", 700);
    appendRuntimeLog("OTA", "apply requested by device action", true);
    return true;
  }
  if (type == "topic.next") {
    return switchTopicRelative(1, true);
  }
  if (type == "topic.prev") {
    return switchTopicRelative(-1, true);
  }
  if (type == "topic.open") {
    return openTopicByAction(action);
  }
  return false;
}

bool executeDeviceActions(JsonVariantConst actions) {
  if (actions.isNull()) {
    return false;
  }
  bool executed = false;
  if (actions.is<JsonArrayConst>()) {
    uint8_t count = 0;
    for (JsonObjectConst action : actions.as<JsonArrayConst>()) {
      if (count++ >= 4) {
        break;
      }
      executed = executeDeviceAction(action) || executed;
    }
  } else if (actions.is<JsonObjectConst>()) {
    executed = executeDeviceAction(actions.as<JsonObjectConst>());
  }
  if (executed) {
    appendRuntimeLog("ACT", "device action executed", true);
  }
  return executed;
}

bool executeDeviceActionsJson(const String& json) {
  if (json.isEmpty()) {
    return false;
  }
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    appendRuntimeLog("ACT", "device action json rejected", true);
    return false;
  }
  return executeDeviceActions(doc.as<JsonVariantConst>());
}

bool executeOrDeferDeviceActionsJson(const String& json) {
  if (json.isEmpty()) {
    return false;
  }
  if (gPlaybackActive) {
    gDeferredDeviceActionsJson = json;
    appendRuntimeLog("ACT", "device action deferred until audio finishes", false);
    logf("[ACT] deferred while playback active bytes=%u", static_cast<unsigned>(json.length()));
    return true;
  }
  return executeDeviceActionsJson(json);
}

void serviceDeferredDeviceActions() {
  if (gDeferredDeviceActionsJson.isEmpty() || gPlaybackActive || gRecording ||
      gSubmitting || gThinking || gOtaApplyInProgress) {
    return;
  }
  String json = gDeferredDeviceActionsJson;
  gDeferredDeviceActionsJson = "";
  logf("[ACT] run deferred bytes=%u", static_cast<unsigned>(json.length()));
  executeDeviceActionsJson(json);
}

void servicePendingOtaAction() {
  if ((!gPendingOtaCheckAction && !gPendingOtaApplyAction) || millis() < gPendingOtaActionAfterMs ||
      gPlaybackActive || gRecording || gSubmitting || gThinking || gOtaApplyInProgress) {
    return;
  }
  bool shouldCheck = gPendingOtaCheckAction;
  bool shouldApply = gPendingOtaApplyAction;
  gPendingOtaCheckAction = false;
  gPendingOtaApplyAction = false;
  logf("[ACT] ota action run check=%d apply=%d", static_cast<int>(shouldCheck), static_cast<int>(shouldApply));
  appendRuntimeLog("OTA", String("action run check=") + String(static_cast<int>(shouldCheck)) +
                            " apply=" + String(static_cast<int>(shouldApply)), true);
  setUiMode(UiMode::Ota);
  bool checkOk = true;
  if (shouldCheck) {
    checkOk = fetchRemoteOtaManifest();
    logf("[ACT] ota.check result=%d", static_cast<int>(checkOk));
  }
  if (shouldApply && checkOk) {
    bool applyOk = applyOtaFromManifest();
    logf("[ACT] ota.apply result=%d", static_cast<int>(applyOk));
  }
}

bool findLauncherEntryForMode(UiMode mode, int& groupIndex, int& subIndex) {
  for (uint8_t g = 0; g < kLauncherGroupCount; ++g) {
    for (uint8_t s = 0; s < kLauncherGroups[g].count; ++s) {
      if (kLauncherGroups[g].items[s].mode == mode) {
        groupIndex = g;
        subIndex = s;
        return true;
      }
    }
  }
  return false;
}

void syncLauncherToMode(UiMode mode) {
  int groupIndex = 0;
  int subIndex = 0;
  if (!findLauncherEntryForMode(mode, groupIndex, subIndex)) {
    return;
  }
  gLauncherSelection = groupIndex;
  gLauncherSubSelection = subIndex;
  gLauncherLastSub[groupIndex] = subIndex;
}

bool uiModeInterruptsRealtime(UiMode mode) {
  switch (mode) {
    case UiMode::Home:
    case UiMode::ChatFace:
    case UiMode::Face:
    case UiMode::Hero:
    case UiMode::ChatFull:
    case UiMode::Contexts:
      return false;
    default:
      return true;
  }
}

void setUiMode(UiMode mode) {
  bool modeChanged = mode != gUiMode;
  if (modeChanged && uiModeInterruptsRealtime(mode) &&
      (gRealtimeActive || gRealtimeConnected || gRealtimeRecording || gRealtimeAwaiting)) {
    closeRealtimeSession("mode change");
  }
  gUiMode = mode;
  if (mode != UiMode::CMatrix && (gScreensaverActive || gScreensaverDimmed || gScreenOffActive)) {
    gScreensaverActive = false;
    gScreensaverDimmed = false;
    gScreenOffActive = false;
    applyDisplayBrightness();
  }
  gLauncherVisible = false;
  if (mode != UiMode::Tetris) {
    gTetrisHelpVisible = false;
  }
  if (mode == UiMode::CMatrix && modeChanged) {
    gCMatrixInitialized = false;
  }
  if (modeChanged && (mode == UiMode::Settings || mode == UiMode::FocusSettings || mode == UiMode::ScreenSettings)) {
    gSelectedSetting = 0;
  }
  if (mode == UiMode::RfidLab) {
    initRfidLab(false);
  }
  if (gLauncherSelection >= 0 && gLauncherSelection < kLauncherGroupCount &&
      gLauncherSubSelection >= 0 && gLauncherSubSelection < kLauncherGroups[gLauncherSelection].count &&
      kLauncherGroups[gLauncherSelection].items[gLauncherSubSelection].mode == mode) {
    gLauncherLastSub[gLauncherSelection] = gLauncherSubSelection;
  } else {
    syncLauncherToMode(mode);
  }
  setStatus(String("Mode: ") + uiModeName(mode), 900);
  if (mode == UiMode::Contexts) {
    if (kSingleCardputerTopicMode) {
      loadDefaultConversationContexts();
      return;
    }
    bool needCatalog = !gContextsRemoteLoaded;
    bool needHistory = !gContexts.empty() && gContextHistoryKey != currentConversationKey();
    if (needCatalog || needHistory) {
      startTopicTask(needCatalog, false, true, needCatalog ? "Syncing topics..." : "Loading history...");
    }
  } else if (mode == UiMode::Notes && !gNotesScanned) {
    scanNotes();
  }
}

void moveLauncherSelection(int delta) {
  gLauncherVisible = true;
  gLauncherSelection = (gLauncherSelection + delta + kLauncherGroupCount) % kLauncherGroupCount;
  uint8_t count = kLauncherGroups[gLauncherSelection].count;
  gLauncherSubSelection = clampValue<int>(gLauncherLastSub[gLauncherSelection], 0, count - 1);
}

void moveLauncherSubSelection(int delta) {
  gLauncherVisible = true;
  uint8_t count = kLauncherGroups[gLauncherSelection].count;
  if (count <= 1) {
    moveLauncherSelection(delta);
    return;
  }
  gLauncherSubSelection = (gLauncherSubSelection + delta + count) % count;
  gLauncherLastSub[gLauncherSelection] = gLauncherSubSelection;
}

void applyLauncherSelection() {
  gLauncherSelection = clampValue<int>(gLauncherSelection, 0, kLauncherGroupCount - 1);
  uint8_t count = kLauncherGroups[gLauncherSelection].count;
  gLauncherSubSelection = clampValue<int>(gLauncherSubSelection, 0, count - 1);
  gLauncherLastSub[gLauncherSelection] = gLauncherSubSelection;
  UiMode target = kLauncherGroups[gLauncherSelection].items[gLauncherSubSelection].mode;
  setUiMode(target);
}
