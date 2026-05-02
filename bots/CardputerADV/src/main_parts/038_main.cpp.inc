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

bool stopRecordingAndSend() {
  if (!gRecording) {
    return false;
  }
  gRecording = false;
  pulseButtonPress();

  float elapsed = (millis() - gRecordStartMs) / 1000.0f;
  size_t dataSize = 0;
  String pcmUploadPath;

  setStatus("Finalizing...");
  render();
  gLastRenderMs = millis();

  if (gRecordingToFile) {
    drainRecordingToFile();
    size_t queuedBeforeStop = M5Cardputer.Mic.isRecording();
    if (queuedBeforeStop > 0 || !gRecordInflightChunks.empty()) {
      logf("[VOICE] dropping tail queued=%u inflight=%u",
           static_cast<unsigned>(queuedBeforeStop),
           static_cast<unsigned>(gRecordInflightChunks.size()));
    }
    stopMic();
    gRecordInflightChunks.clear();
    if (gActiveRecordFile) {
      gActiveRecordFile.flush();
      gActiveRecordFile.close();
    }
    fs::FS* fs = activeVoiceFs();
    if (fs && !gActiveRecordPath.isEmpty() && fsExists(*fs, gActiveRecordPath)) {
      File verifyFile = fs->open(gActiveRecordPath, "r");
      if (verifyFile) {
        size_t actualBytes = verifyFile.size();
        verifyFile.close();
        if (actualBytes != gActiveRecordBytes) {
          appendRuntimeLog("VOICE", "record size adjusted counter=" +
                                    String(static_cast<unsigned>(gActiveRecordBytes)) +
                                    " file=" + String(static_cast<unsigned>(actualBytes)), true);
          logf("[VOICE] record size adjusted counter=%u file=%u",
               static_cast<unsigned>(gActiveRecordBytes),
               static_cast<unsigned>(actualBytes));
          gActiveRecordBytes = actualBytes;
        }
      }
    }
    gRecordedSamples = gActiveRecordBytes / sizeof(int16_t);
    dataSize = static_cast<uint32_t>(gActiveRecordBytes);
    pcmUploadPath = gActiveRecordPath;
  } else {
    size_t expectedSamples = static_cast<size_t>(elapsed * kMicSampleRate);
    gRecordedSamples = expectedSamples;
    if (gRecordedSamples > gRecordCapacitySamples) {
      gRecordedSamples = gRecordCapacitySamples;
    }
    stopMic();
    dataSize = static_cast<uint32_t>(gRecordedSamples * sizeof(int16_t));
  }

  logf("[VOICE] recording stopped elapsed=%.2f samples=%u mode=%s", elapsed,
       static_cast<unsigned>(gRecordedSamples),
       gRecordingToFile ? "file" : "ram");

  if (elapsed < kMinRecordSeconds || gRecordedSamples == 0) {
    setFaceMode(FaceMode::Idle);
    setStatus("Voice turn canceled", 1200);
    logf("[VOICE] canceled: too short");
    if (gRecordingToFile) {
      fs::FS* fs = activeVoiceFs();
      if (!pcmUploadPath.isEmpty()) {
        if (fs) {
          fsRemove(*fs, pcmUploadPath);
        }
      }
      gRecordingToFile = false;
      gActiveRecordPath = "";
      gActiveRecordBytes = 0;
      gActiveRecordCommittedBytes = 0;
      gActiveRecordExpectedBytes = 0;
    } else {
      releaseRecordBuffer();
    }
    return false;
  }

  gWifiReady = WiFi.status() == WL_CONNECTED;
  if (!gWifiReady) {
    pushError("Wi-Fi dropped during recording.");
    setFaceMode(FaceMode::Error);
    setStatus("Wi-Fi lost", 2000);
    setVoiceDiag("voice: wifi lost");
    logf("[VOICE] wifi lost before upload");
    if (gRecordingToFile) {
      fs::FS* fs = activeVoiceFs();
      if (!pcmUploadPath.isEmpty()) {
        rememberVoiceNoteFromPcmFile("My voice", "Outbound voice note", pcmUploadPath, dataSize, kMicSampleRate, false);
        if (fs) {
          fsRemove(*fs, pcmUploadPath);
        }
      }
      gRecordingToFile = false;
      gActiveRecordPath = "";
      gActiveRecordBytes = 0;
      gActiveRecordCommittedBytes = 0;
      gActiveRecordExpectedBytes = 0;
    } else {
      releaseRecordBuffer();
    }
    return false;
  }

  gSubmitting = true;
  gThinking = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Uploading...");
  render();
  gLastRenderMs = millis();
  delay(10);
  pauseHubWebSocketForVoice();
  auto finishVoiceTurn = [&]() {
    gSubmitting = false;
    gThinking = false;
    resumeHubWebSocketAfterVoice();
  };

  if (gRecordingToFile) {
    rememberVoiceNoteFromPcmFile("My voice", "Outbound voice note", pcmUploadPath, dataSize, kMicSampleRate, false);
  } else {
    uint8_t wavHeader[44];
    writeWavHeader(wavHeader, dataSize, kMicSampleRate);
    rememberVoiceNote("My voice", "Outbound voice note", wavHeader, sizeof(wavHeader),
                      reinterpret_cast<const uint8_t*>(gRecordBuffer), dataSize, kMicSampleRate, false);
  }
  logHeapStats("voice-upload");

