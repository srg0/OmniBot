  if (statusCode == 204) {
    http.end();
    setVoiceDiag("tts: empty pcm");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  if (statusCode != 200) {
    String errorBody = http.getString();
    logf("[TTS] bad response status=%d body=%s", statusCode, errorBody.c_str());
    http.end();
    setVoiceDiag("tts: bad status " + String(statusCode));
    releaseTtsBuffer();
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage() || !fs) {
    http.end();
    setVoiceDiag("tts: fs unavailable");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  uint32_t sampleRate = kTtsDefaultSampleRate;
  String sampleRateHeader = http.header("X-Audio-Sample-Rate");
  if (!sampleRateHeader.isEmpty()) {
    uint32_t parsed = static_cast<uint32_t>(sampleRateHeader.toInt());
    if (parsed >= 8000 && parsed <= 48000) {
      sampleRate = parsed;
    }
  }

  fsRemove(*fs, kActiveTtsPcmPath);
  File pcmFile = fs->open(kActiveTtsPcmPath, "w");
  if (!pcmFile) {
    http.end();
    setVoiceDiag("tts: fs open failed");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  size_t bytesRead = 0;
  uint8_t ioBuffer[1024];
  int contentLength = http.getSize();
  WiFiClient* stream = http.getStreamPtr();
  unsigned long deadline = millis() + 90000;
  while (stream && http.connected() && millis() < deadline &&
         (contentLength < 0 || bytesRead < static_cast<size_t>(contentLength))) {
    int avail = stream->available();
    if (avail <= 0) {
      delay(1);
      yield();
      continue;
    }
    size_t chunk = static_cast<size_t>(avail);
    if (chunk > sizeof(ioBuffer)) {
      chunk = sizeof(ioBuffer);
    }
    if (contentLength > 0) {
      size_t left = static_cast<size_t>(contentLength) - bytesRead;
      if (chunk > left) {
        chunk = left;
      }
    }
    int got = stream->readBytes(ioBuffer, chunk);
    if (got > 0) {
      size_t written = pcmFile.write(ioBuffer, static_cast<size_t>(got));
      if (written != static_cast<size_t>(got)) {
        pcmFile.close();
        http.end();
        fsRemove(*fs, kActiveTtsPcmPath);
        setVoiceDiag("tts: fs write failed");
        if (pausedWsForTts) {
          resumeHubWebSocketAfterVoice();
        }
        return false;
      }
      bytesRead += static_cast<size_t>(got);
      if ((bytesRead % (16 * 1024)) == 0 || (contentLength > 0 && bytesRead == static_cast<size_t>(contentLength))) {
        setVoiceDiag("tts: rx " + String(static_cast<unsigned>(bytesRead / 1024)) + "k");
      }
    }
  }
  pcmFile.close();
  http.end();

  if (bytesRead == 0) {
    logf("[TTS] empty PCM response");
    setVoiceDiag("tts: empty pcm");
    fsRemove(*fs, kActiveTtsPcmPath);
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  if (contentLength > 0 && bytesRead < static_cast<size_t>(contentLength)) {
    logf("[TTS] short PCM response bytes=%u expected=%u",
         static_cast<unsigned>(bytesRead),
         static_cast<unsigned>(contentLength));
    fsRemove(*fs, kActiveTtsPcmPath);
    setVoiceDiag("tts: short pcm");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  rememberVoiceNoteFromPcmFile("Assistant voice", speakText, kActiveTtsPcmPath, bytesRead, sampleRate, true);
  bool started = startPlaybackStreamFromRawPcmFile(kActiveTtsPcmPath, sampleRate, bytesRead,
                                                   "tts: file " + String(static_cast<unsigned>(bytesRead / 1024)) + "k");
  if (!started) {
    fsRemove(*fs, kActiveTtsPcmPath);
  }
  if (pausedWsForTts) {
    resumeHubWebSocketAfterVoice();
  }
  return started;
}

bool sendTextTurn(const String& text) {
  if ((!gWifiReady && !ensureWifiForHttp("text-turn", 3000)) || gHubHost.isEmpty()) {
    pushError("Hub is not configured.");
    return false;
  }

  HTTPClient http;
  String url = hubBaseUrl() + kDeviceTextTurnPath;
  String turnId = makeClientTurnId("txt");
  http.setTimeout(kTurnHttpTimeoutMs);
  std::unique_ptr<WiFiClientSecure> secureClient;
  bool beginOk = false;
  if (hubUsesTls()) {
    secureClient.reset(new WiFiClientSecure());
    configureSecureClient(*secureClient);
    beginOk = http.begin(*secureClient, url);
  } else {
    beginOk = http.begin(url);
  }
  if (!beginOk) {
    pushError("Failed to open HTTP connection.");
    return false;
  }

  DynamicJsonDocument doc(768);
  doc["message"] = text;
  doc["device_id"] = gDeviceId;
  doc["firmware_version"] = kAppVersion;
  doc["conversation_key"] = currentConversationKey();
  doc["input_type"] = "text";
  doc["reply_audio"] = false;
  doc["save_reply_audio"] = false;
  doc["client_msg_id"] = turnId;
  doc["turn_id"] = turnId;

  String body;
  serializeJson(doc, body);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Client-Msg-Id", turnId);
  http.addHeader("X-Turn-Id", turnId);
  http.addHeader("X-Conversation-Key", currentConversationKey());
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }

  gSubmitting = true;
  gThinking = true;
  gAssistantPendingVisible = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Sending...");
  render();
  gLastRenderMs = millis();
  delay(10);

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  gSubmitting = false;
  gThinking = false;
  gAssistantPendingVisible = false;

  if (code < 200 || code >= 300) {
    logf("[TEXT] hub error code=%d body=%s", code, response.c_str());
    if (code <= 0) {
      String transportError = HTTPClient::errorToString(code);
      pushError("Hub transport failed: " + transportError);
      setHttpDiag("text: " + transportError);
    } else if (code == 504) {
      pushError("Assistant reply timeout.");
      setHttpDiag("text: assistant timeout");
    } else {
      pushError("Hub HTTP error: " + String(code));
      setHttpDiag("text: http " + String(code));
    }
    if (!response.isEmpty()) {
      pushError(response);
    }
    setFaceMode(FaceMode::Error);
    setStatus("Text turn failed", 2000);
    return false;
  }

  DynamicJsonDocument resp(2048);
  auto err = deserializeJson(resp, response);
  if (err) {
    pushError("Could not parse hub response.");
    setFaceMode(FaceMode::Error);
    setStatus("Bad response", 2000);
    return false;
  }
