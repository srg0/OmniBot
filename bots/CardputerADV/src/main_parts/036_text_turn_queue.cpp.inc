
void maybeStartNextTextTurn() {
  if (gTextTurnTaskHandle != nullptr || gCompletedTextTurn != nullptr || !gActiveTextTurn.isEmpty()) {
    return;
  }
  if (gPendingTextTurns.empty()) {
    if (!gRecording) {
      gSubmitting = false;
      gThinking = false;
      gAssistantPendingVisible = false;
      if (!gPlaybackActive) {
        setFaceMode(FaceMode::Idle);
      }
    }
    return;
  }

  std::unique_ptr<TextTurnTaskPayload> payload(new TextTurnTaskPayload());
  if (!payload) {
    pushError("Send queue allocation failed.");
    gPendingTextTurns.pop_front();
    gSubmitting = false;
    gThinking = false;
    gAssistantPendingVisible = false;
    setFaceMode(FaceMode::Error);
    setStatus("Queue failed", 1500);
    return;
  }

  payload->text = gPendingTextTurns.front();
  gPendingTextTurns.pop_front();
  payload->url = hubBaseUrl() + kDeviceTextTurnPath;
  payload->deviceId = gDeviceId;
  payload->deviceToken = gDeviceToken;
  payload->conversationKey = currentConversationKey();
  payload->clientMsgId = makeClientTurnId("txt");
  payload->turnId = payload->clientMsgId;

  gActiveTextTurn = payload->text;
  gSubmitting = true;
  gThinking = true;
  gAssistantPendingVisible = true;
  setFaceMode(FaceMode::Thinking);
  setStatus(gPendingTextTurns.empty() ? "Sending..." : ("Sending... +" + String(gPendingTextTurns.size())));

  BaseType_t taskOk = xTaskCreate(
      textTurnTask,
      "text-turn",
      kTextTurnTaskStackSize,
      payload.release(),
      1,
      &gTextTurnTaskHandle);

  if (taskOk != pdPASS) {
    gActiveTextTurn = "";
    gTextTurnTaskHandle = nullptr;
    gSubmitting = false;
    gThinking = false;
    gAssistantPendingVisible = false;
    pushError("Failed to start send task.");
    setFaceMode(FaceMode::Error);
    setStatus("Send failed", 1500);
  }
}

void processCompletedTextTurn() {
  TextTurnTaskResult* result = nullptr;
  portENTER_CRITICAL(&gTextTurnMux);
  result = gCompletedTextTurn;
  gCompletedTextTurn = nullptr;
  portEXIT_CRITICAL(&gTextTurnMux);
  if (!result) {
    return;
  }

  std::unique_ptr<TextTurnTaskResult> done(result);
  gTextTurnTaskHandle = nullptr;
  gActiveTextTurn = "";

  if (!done->error.isEmpty()) {
    logf("[TEXT] async error code=%d body=%s", done->statusCode, done->response.c_str());
    pushError(done->error);
    if (done->statusCode <= 0) {
      setHttpDiag("text: " + done->error);
    } else {
      setHttpDiag("text: http " + String(done->statusCode));
    }
    if (!done->response.isEmpty()) {
      pushError(done->response);
    }
    setFaceMode(FaceMode::Error);
    setStatus("Text turn failed", 2000);
  } else if (done->ok && done->hasReply) {
    appendAssistantReply(done->reply);
    bool playedAudio = false;
    if (done->hasAudio) {
      playedAudio = downloadAndPlayResponseAudio(done->audioUrl, done->audioPath, done->audioSha256,
                                                 done->audioSize, done->audioTitle, done->reply);
    }
    if (!playedAudio && shouldSpeakAssistantTextReply(done->reply)) {
      requestAndPlayTts(done->reply);
    }
    logf("[TEXT] async reply=%s", done->reply.c_str());
    setStatus("Reply received", 1200);
    if (done->hasDeviceActions) {
      executeDeviceActionsJson(done->deviceActionsJson);
    }
  } else if (done->ok) {
    pushSystem("Hub accepted the turn.");
    logf("[TEXT] async accepted without reply");
    if (done->hasDeviceActions) {
      executeDeviceActionsJson(done->deviceActionsJson);
    }
    if (!gPlaybackActive) {
      setFaceMode(FaceMode::Idle);
    }
    setStatus("Turn accepted", 1200);
  }

  if (!gPendingTextTurns.empty()) {
    maybeStartNextTextTurn();
    return;
  }

  gSubmitting = false;
  gThinking = false;
  gAssistantPendingVisible = false;
  if (!gRecording && !gPlaybackActive) {
    setFaceMode(FaceMode::Idle);
  }
}

