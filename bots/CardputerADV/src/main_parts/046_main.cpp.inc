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

bool startTopicTask(bool syncCatalog, bool selectCurrent, bool loadHistory, const char* statusText,
                    bool createTopic, const String& createTitle) {
  if (gTopicTaskHandle != nullptr || gCompletedTopicTask != nullptr) {
    setStatus("Topic busy", 500);
    return false;
  }
  std::unique_ptr<TopicTaskPayload> payload(new TopicTaskPayload());
  if (!payload) {
    setStatus("Topic alloc failed", 900);
    return false;
  }
  payload->syncCatalog = syncCatalog;
  payload->selectCurrent = selectCurrent;
  payload->loadHistory = loadHistory;
  payload->createTopic = createTopic;
  payload->createTitle = createTitle;
  setStatus(statusText ? statusText : "Topic sync...");
  BaseType_t ok = xTaskCreate(topicTask, "topic-sync", kTopicTaskStackSize, payload.release(), 1, &gTopicTaskHandle);
  if (ok != pdPASS) {
    gTopicTaskHandle = nullptr;
    setStatus("Topic task failed", 1200);
    return false;
  }
  return true;
}

void processCompletedTopicTask() {
  TopicTaskResult* result = nullptr;
  portENTER_CRITICAL(&gTopicTaskMux);
  result = gCompletedTopicTask;
  gCompletedTopicTask = nullptr;
  portEXIT_CRITICAL(&gTopicTaskMux);
  if (!result) {
    return;
  }
  std::unique_ptr<TopicTaskResult> done(result);
  gTopicTaskHandle = nullptr;
  if (done->createOk) {
    setStatus("Topic created", 1000);
    showTopicOverlay(0, 2200);
  } else if (done->historyOk) {
    setStatus(done->label.isEmpty() ? "History loaded" : ("Topic " + done->label), 900);
  } else if (done->catalogOk) {
    setStatus("Topics synced", 900);
  } else if (!gContextError.isEmpty()) {
    setStatus(gContextError, 1200);
  }
}

bool handleLogsKey(char key) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (key == ',' || key == ';') {
    gLogScrollOffset = min<int>(gLogScrollOffset + 1, max<int>(0, static_cast<int>(gRuntimeLogs.size()) - 9));
    return true;
  }
  if (key == '.' || key == '/') {
    gLogScrollOffset = max<int>(0, gLogScrollOffset - 1);
    return true;
  }
  if (lowerKey == 'r') {
    scanAudioAssets();
    loadAssetManifestSummary();
    loadOtaManifestSummary();
    loadConversationContexts();
    appendRuntimeLog("LOG", "manual refresh", true);
    setStatus("Diagnostics refreshed", 900);
    return true;
  }
  if (lowerKey == 'u') {
    uploadRuntimeLogs();
    return true;
  }
  return false;
}

