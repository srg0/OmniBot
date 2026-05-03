  String turnId = makeClientTurnId("voice");
  setStatus("Uploading voice...");
  render();
  gLastRenderMs = millis();
  appendRuntimeLog("VOICE", "upload start bytes=" + String(static_cast<unsigned>(dataSize)) +
                            " ctx=" + currentConversationKey() +
                            " turn=" + turnId, true);
  int statusCode = -1;
  String response;
  bool transportOk = false;
  bool prevRenderPump = gBlockingRenderPumpEnabled;
  gBlockingRenderPumpEnabled = true;
  gLastBlockingRenderPumpMs = 0;
  pumpBlockingNetworkUi();
  if (gRecordingToFile) {
    setHttpDiag("voice_pcm: raw file " + String(static_cast<unsigned>(dataSize / 1024)) + "k");
    transportOk = postRawVoiceTurn(nullptr, pcmUploadPath, dataSize, turnId, statusCode, response);
  } else {
    setHttpDiag("voice_pcm: raw ram " + String(static_cast<unsigned>(dataSize / 1024)) + "k");
    transportOk = postRawVoiceTurn(reinterpret_cast<const uint8_t*>(gRecordBuffer), "", dataSize,
                                   turnId, statusCode, response);
  }
  gBlockingRenderPumpEnabled = prevRenderPump;
  finishVoiceTurn();
  if (gRecordingToFile) {
    fs::FS* fs = activeVoiceFs();
    if (fs) {
      fsRemove(*fs, pcmUploadPath);
    }
    gRecordingToFile = false;
    gActiveRecordPath = "";
    gActiveRecordBytes = 0;
    gActiveRecordCommittedBytes = 0;
    gActiveRecordExpectedBytes = 0;
  } else {
    releaseRecordBuffer();
  }

  if (!transportOk || statusCode <= 0) {
    String transportError = gLastHttpDiag;
    if (transportError.isEmpty()) {
      transportError = statusCode <= 0 ? HTTPClient::errorToString(statusCode) : String("unknown");
    }
    pushError("Voice transport failed: " + transportError);
    setFaceMode(FaceMode::Error);
    setStatus("Voice transport failed", 2200);
    setVoiceDiag("voice: transport " + String(statusCode));
    logf("[VOICE] raw upload failed code=%d err=%s bytes=%u",
         statusCode,
         transportError.c_str(),
         static_cast<unsigned>(dataSize));
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    logf("[VOICE] HTTP error status=%d body=%s", statusCode, response.c_str());
    if (statusCode == 504) {
      pushError("Assistant reply timeout.");
      setStatus("Assistant timeout", 2200);
      setVoiceDiag("voice: assistant timeout");
    } else {
      pushError("Voice HTTP error: " + String(statusCode));
      setStatus("Voice turn failed", 2000);
      setVoiceDiag("voice: http " + String(statusCode));
    }
    if (!response.isEmpty()) {
      pushError(response);
    }
    setFaceMode(FaceMode::Error);
    setHttpDiag("voice: http " + String(statusCode));
    return false;
  }

  DynamicJsonDocument resp(4096);
  auto err = deserializeJson(resp, response);
  if (err) {
    pushError("Could not parse voice reply.");
    setFaceMode(FaceMode::Error);
    setStatus("Voice parse failed", 2000);
    setVoiceDiag("voice: json parse failed");
    logf("[VOICE] JSON parse failed body=%s", response.c_str());
    return false;
  }

  String transcript = resp["transcript"] | resp["text"] | "";
  String reply = resp["reply"] | resp["full_text"] | "";
  String shortText = resp["short_text"] | "";
  String deviceActionsJson;
  if (!resp["device_actions"].isNull()) {
    serializeJson(resp["device_actions"], deviceActionsJson);
  }
  if (reply.isEmpty()) {
    reply = shortText;
  }
  String audioUrl;
  String audioPath;
  String audioTitle;
  String audioSha;
  uint32_t audioSize = 0;
  if (resp["audio"].is<JsonObject>()) {
    JsonObject audio = resp["audio"].as<JsonObject>();
    audioUrl = audio["url"] | "";
    audioPath = audio["path"] | "";
    audioTitle = audio["title"] | "";
    audioSha = audio["sha256"] | "";
    audioSize = audio["size"] | 0;
  }
  logf("[VOICE] transcript=%s", transcript.c_str());
  logf("[VOICE] reply=%s", reply.c_str());
  if (!transcript.isEmpty()) {
    pushUser(transcript);
    setVoiceDiag("voice: transcript ok");
  }
  if (!reply.isEmpty()) {
    appendAssistantReply(reply);
    if (!downloadAndPlayResponseAudio(audioUrl, audioPath, audioSha, audioSize, audioTitle, reply) &&
        !requestAndPlayTts(reply)) {
      logf("[VOICE] TTS request failed");
      setFaceMode(FaceMode::Idle);
      setStatus("Reply text only", 1200);
    }
  } else {
    setFaceMode(FaceMode::Idle);
    setStatus("No reply", 1200);
  }
  if (!deviceActionsJson.isEmpty()) {
    executeOrDeferDeviceActionsJson(deviceActionsJson);
  }
  return true;
}

void syncClockFromNtp(bool force) {
  ensureBangkokTimezone();
  if (!gWifiReady) {
    return;
  }
  uint32_t now = millis();
  if (gLastTimeSyncAttemptMs != 0 && !gTimeValid) {
    if (hasValidSystemTime()) {
      gTimeValid = true;
      gLastTimeSyncMs = now;
      persistRtcFromSystemTime();
      setStatus("Clock synced", 1000);
      logf("[TIME] synced epoch=%lu", static_cast<unsigned long>(time(nullptr)));
      return;
    }
    if (now - gLastTimeSyncAttemptMs < 8000) {
      return;
    }
    logf("[TIME] sync timed out");
  }
  if (!force) {
    if (gTimeValid && gLastTimeSyncMs != 0 && millis() - gLastTimeSyncMs < kTimeResyncMs) {
      return;
    }
    if (gLastTimeSyncAttemptMs != 0 && millis() - gLastTimeSyncAttemptMs < kTimeSyncRetryMs) {
      return;
    }
  }

  gLastTimeSyncAttemptMs = now;
  gTimeValid = hasValidSystemTime();
  if (gTimeValid) {
    gLastTimeSyncMs = now;
    return;
  }
  configTzTime(kBangkokTz, "time.google.com", "pool.ntp.org", "time.cloudflare.com");
  setStatus("Clock sync...", 700);
}
