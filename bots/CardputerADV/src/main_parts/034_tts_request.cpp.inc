bool requestAndPlayTts(const String& text) {
  String speakText = cleanReplyForDevice(text);
  if (speakText.isEmpty()) {
    setVoiceDiag("tts: empty/buffer");
    return false;
  }
  if (!gWifiReady || gHubHost.isEmpty()) {
    setVoiceDiag("tts: wifi unavailable");
    return false;
  }

  if (!gRecording && gRecordBuffer) {
    releaseRecordBuffer();
  }
  if (gTtsBuffer) {
    releaseTtsBuffer();
  }

  bool pausedWsForTts = false;
  if (gWsReady) {
    pauseHubWebSocketForVoice();
    pausedWsForTts = true;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + kDeviceTtsPath;
  http.setTimeout(60000);
  http.setReuse(false);
  const char* responseHeaders[] = {"X-Audio-Sample-Rate"};
  http.collectHeaders(responseHeaders, 1);

  bool beginOk = false;
  if (hubUsesTls()) {
    secureClient.reset(new WiFiClientSecure());
    secureClient->setInsecure();
    secureClient->setHandshakeTimeout(12);
    beginOk = http.begin(*secureClient, url);
  } else {
    beginOk = http.begin(url);
  }
  if (!beginOk) {
    logf("[TTS] begin failed url=%s", url.c_str());
    setVoiceDiag("tts: connect failed");
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

  String body =
      "{\"device_id\":\"" + escapeJsonString(gDeviceId) + "\",\"text\":\"" +
      escapeJsonString(speakText) + "\"}";
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Id", gDeviceId);
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
    http.addHeader("X-Device-Token", gDeviceToken);
  }

  int statusCode = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body.c_str())), body.length());
  if (statusCode <= 0) {
    logf("[TTS] HTTPClient failed code=%d err=%s",
         statusCode,
         HTTPClient::errorToString(statusCode).c_str());
    http.end();
    setVoiceDiag("tts: httpclient " + String(statusCode));
    releaseTtsBuffer();
    if (pausedWsForTts) {
      resumeHubWebSocketAfterVoice();
    }
    return false;
  }

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

