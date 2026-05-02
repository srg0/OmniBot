
bool handleNotesKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (!gNotesScanned) {
    scanNotes();
  }

  if (keys.ctrl && lowerKey == 's') {
    if (gNotesMode == NotesMode::Edit && !gNoteInputBuffer.isEmpty()) {
      appendLineToActiveNote(gNoteInputBuffer);
      gNoteInputBuffer = "";
    }
    gNotesMode = NotesMode::View;
    gNotesStatus = "Saved";
    return true;
  }

  if (gNotesMode == NotesMode::Edit) {
    if (keys.enter || key == '\n' || key == '\r') {
      appendLineToActiveNote(gNoteInputBuffer);
      gNoteInputBuffer = "";
      return true;
    }
    if (keys.del || key == '\b') {
      if (!gNoteInputBuffer.isEmpty()) {
        removeLastUtf8Char(gNoteInputBuffer);
      }
      return true;
    }
    if (keys.ctrl && (lowerKey == 'x' || lowerKey == 'q')) {
      gNotesMode = NotesMode::View;
      gNotesStatus = "View mode";
      return true;
    }
    if (key >= 32 && key < 127 && gNoteInputBuffer.length() < static_cast<int>(kMaxNoteInputChars)) {
      gNoteInputBuffer += translateInputChar(key);
      return true;
    }
    return false;
  }

  if (lowerKey == 'r') {
    scanNotes();
    setStatus("Notes rescanned", 700);
    return true;
  }
  if (lowerKey == 'n') {
    return createNewNote();
  }
  if (lowerKey == 'e') {
    return openSelectedNote(NotesMode::Edit);
  }
  if (lowerKey == 'l' || keys.del || key == '\b') {
    gNotesMode = NotesMode::List;
    gNoteInputBuffer = "";
    gNotesStatus = "List";
    return true;
  }

  if (gNotesMode == NotesMode::List) {
    if (gNotes.empty()) {
      return lowerKey == 'r' || lowerKey == 'n';
    }
    if (key == ',' || key == ';') {
      gSelectedNote = max<int>(0, gSelectedNote - 1);
      gNoteListScrollOffset = max<int>(0, gSelectedNote - 2);
      setStatus(gNotes[gSelectedNote].name, 600);
      return true;
    }
    if (key == '.' || key == '/') {
      gSelectedNote = min<int>(static_cast<int>(gNotes.size()) - 1, gSelectedNote + 1);
      gNoteListScrollOffset = max<int>(0, gSelectedNote - 2);
      setStatus(gNotes[gSelectedNote].name, 600);
      return true;
    }
    if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
      return openSelectedNote(NotesMode::View);
    }
    return false;
  }

  if (key == ',' || key == ';') {
    gNoteScrollOffset = max<int>(0, gNoteScrollOffset - 1);
    return true;
  }
  if (key == '.' || key == '/') {
    uint32_t lineCount = 0;
    if (!gActiveNotePath.isEmpty()) {
      fs::FS* fs = activeNotesFs();
      if (fs) {
        lineCount = countNoteLines(*fs, gActiveNotePath);
      }
    }
    int maxScroll = max<int>(0, static_cast<int>(lineCount) - 7);
    gNoteScrollOffset = min<int>(maxScroll, gNoteScrollOffset + 1);
    return true;
  }
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    return openSelectedNote(NotesMode::Edit);
  }
  return false;
}

bool handleAssetsKey(char key) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'f') {
    fetchRemoteAssetManifest();
    setStatus("Asset manifest fetched", 900);
    return true;
  }
  if (lowerKey == 's') {
    syncAssetsFromManifest();
    return true;
  }
  if (lowerKey == 'r') {
    loadAssetManifestSummary();
    scanAudioAssets();
    setStatus("Assets reloaded", 900);
    return true;
  }
  return false;
}

bool handleOtaKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'f') {
    fetchRemoteOtaManifest();
    return true;
  }
  if (lowerKey == 'r') {
    loadOtaManifestSummary();
    setStatus("OTA manifest reloaded", 900);
    return true;
  }
  if (lowerKey == 'c') {
    confirmOtaBootIfPending("manual confirm");
    return true;
  }
  if (lowerKey == 'x' || lowerKey == 'b') {
    rollbackPendingOta("manual key");
    return true;
  }
  if (lowerKey == 'a' || keys.enter || key == '\n' || key == '\r') {
    setStatus("OTA apply requested", 700);
    appendRuntimeLog("OTA", "apply requested by keyboard", true);
    render();
    applyOtaFromManifest();
    return true;
  }
  return false;
}

bool handleContextsKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'r') {
    startTopicTask(true, false, true, "Syncing topics...");
    return true;
  }
  if (lowerKey == 'h') {
    startTopicTask(false, false, true, "Loading history...");
    return true;
  }
  if (gContexts.empty()) {
    return lowerKey == 'r';
  }
  if (key == ',' || key == ';') {
    return switchTopicRelative(-1, false);
  }
  if (key == '.' || key == '/') {
    return switchTopicRelative(1, false);
  }
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    saveCurrentContext();
    startTopicTask(false, true, true, "Topic loading...");
    return true;
  }
  return false;
}

bool switchTopicRelative(int delta, bool selectAndLoad) {
  if (gContexts.empty()) {
    startTopicTask(true, false, true, "Syncing topics...");
    setStatus("No topics", 900);
    return false;
  }
  uint32_t now = millis();
  if (gTopicCreateArmed && now > gTopicCreateArmedUntilMs) {
    gTopicCreateArmed = false;
  }
  if (delta < 0 && gSelectedContext <= 0) {
    if (!gTopicCreateArmed) {
      armTopicCreate();
      return true;
    }
    String title = defaultNewTopicTitle();
    gTopicCreateArmed = false;
    return startTopicTask(false, false, false, "Creating topic...", true, title);
  }
  if (delta > 0 && gTopicCreateArmed) {
    gTopicCreateArmed = false;
  }
  int next = gSelectedContext + delta;
  if (next < 0 || next >= static_cast<int>(gContexts.size())) {
    showTopicOverlay(delta < 0 ? -1 : 1, 900);
    setStatus(delta < 0 ? "New topic: Alt+Left" : "Last topic", 900);
    return false;
  }
  gSelectedContext = next;
  showTopicOverlay(delta < 0 ? -1 : 1, 1800);
  clearContextPreviewIfSelectionChanged();
  if (selectAndLoad) {
    saveCurrentContext();
    startTopicTask(!gContextsRemoteLoaded, true, true, "Topic loading...");
    setStatus("Topic " + normalizeTopicTitleForDisplay(gContexts[gSelectedContext].label), 900);
  } else {
    setStatus(normalizeTopicTitleForDisplay(gContexts[gSelectedContext].label), 700);
  }
  return true;
}

void armTopicCreate() {
  uint32_t now = millis();
  gTopicCreateArmed = true;
  gTopicCreateArmedUntilMs = now + 3200;
  gContextAnimDir = -1;
  gContextAnimStartMs = now;
  showTopicOverlay(-1, 3200);
  setStatus("Alt+Left again creates topic", 1600);
}

void topicTask(void* arg) {
  std::unique_ptr<TopicTaskPayload> payload(static_cast<TopicTaskPayload*>(arg));
  std::unique_ptr<TopicTaskResult> result(new TopicTaskResult());
  if (!result) {
    gTopicTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }
  if (payload->createTopic) {
    result->createOk = createRemoteTopic(payload->createTitle, result->createdTopicKey);
    if (result->createOk && !result->createdTopicKey.isEmpty()) {
      gSavedContextKey = result->createdTopicKey;
      result->catalogOk = fetchRemoteTopicCatalog();
      selectSavedContextOrDefault(result->createdTopicKey);
      saveCurrentContext();
      result->selectOk = selectRemoteCurrentTopic();
      result->historyOk = fetchSelectedTopicHistory();
    }
  } else if (payload->syncCatalog) {
    result->catalogOk = fetchRemoteTopicCatalog();
  }
  if (payload->selectCurrent) {
    result->selectOk = selectRemoteCurrentTopic();
  }
  if (payload->loadHistory) {
    result->historyOk = fetchSelectedTopicHistory();
  }
  if (!gContexts.empty()) {
    result->label = gContexts[gSelectedContext].shortName;
  }

  portENTER_CRITICAL(&gTopicTaskMux);
  if (gCompletedTopicTask == nullptr) {
    gCompletedTopicTask = result.release();
  } else {
    delete gCompletedTopicTask;
    gCompletedTopicTask = result.release();
  }
  portEXIT_CRITICAL(&gTopicTaskMux);
  vTaskDelete(nullptr);
}
