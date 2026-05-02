bool readHttpResponseBody(WiFiClient& client, int contentLength, String& response,
                          uint32_t timeoutMs, size_t maxBytes = 16384) {
  response = "";
  if (contentLength > 0) {
    response.reserve(min<size_t>(static_cast<size_t>(contentLength), maxBytes));
  } else {
    response.reserve(1024);
  }
  size_t total = 0;
  unsigned long deadline = millis() + timeoutMs;
  uint8_t buffer[384];
  while (millis() < deadline && (contentLength < 0 || total < static_cast<size_t>(contentLength))) {
    int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) {
        break;
      }
      pumpBlockingNetworkUi();
      continue;
    }
    size_t chunk = static_cast<size_t>(avail);
    if (chunk > sizeof(buffer)) {
      chunk = sizeof(buffer);
    }
    if (contentLength > 0) {
      size_t left = static_cast<size_t>(contentLength) - total;
      if (chunk > left) {
        chunk = left;
      }
    }
    int got = client.read(buffer, chunk);
    if (got <= 0) {
      pumpBlockingNetworkUi();
      continue;
    }
    size_t n = static_cast<size_t>(got);
    if (response.length() < static_cast<int>(maxBytes)) {
      size_t room = maxBytes - static_cast<size_t>(response.length());
      size_t copyLen = min(room, n);
      for (size_t i = 0; i < copyLen; ++i) {
        response += static_cast<char>(buffer[i]);
      }
    }
    total += n;
    pumpBlockingNetworkUi();
  }
  if (contentLength > 0 && total < static_cast<size_t>(contentLength)) {
    setHttpDiag("response short " + String(static_cast<unsigned>(total)) + "/" +
                String(static_cast<unsigned>(contentLength)));
    return false;
  }
  return true;
}

bool postRawVoiceTurn(const uint8_t* ramPayload, const String& filePath, size_t dataSize,
                      const String& turnId, int& statusCode, String& response) {
  statusCode = -1;
  response = "";
  if (dataSize == 0 || (!ramPayload && filePath.isEmpty())) {
    setHttpDiag("voice_post: empty payload");
    return false;
  }
  if (!ensureWifiForHttp("voice-post", 3000) || gHubHost.isEmpty()) {
    setHttpDiag("voice_post: wifi/hub unavailable");
    return false;
  }

  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  WiFiClient* client = nullptr;
  if (hubUsesTls()) {
    configureSecureClient(secureClient);
    secureClient.setTimeout((kTurnHttpTimeoutMs + 999) / 1000);
    client = &secureClient;
  } else {
    plainClient.setTimeout((kTurnHttpTimeoutMs + 999) / 1000);
    plainClient.setNoDelay(true);
    client = &plainClient;
  }

  logf("[VOICE] raw post preconnect free=%u largest=%u host=%s port=%u",
       static_cast<unsigned>(ESP.getFreeHeap()),
       static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
       gHubHost.c_str(),
       gHubPort);
  if (hubUsesTls()) {
    logf("[VOICE] raw post tls_verify=%s", tlsVerifyMode());
  }
  setHttpDiag("voice_post: connect");
  if (!client->connect(gHubHost.c_str(), gHubPort)) {
    setHttpDiag("voice_post: connect failed largest=" +
                String(static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT))));
    logf("[VOICE] raw post connect failed host=%s port=%u free=%u largest=%u",
         gHubHost.c_str(),
         gHubPort,
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    return false;
  }

  String hostHeader = gHubHost;
  if ((!hubUsesTls() && gHubPort != 80) || (hubUsesTls() && gHubPort != 443)) {
    hostHeader += ":" + String(gHubPort);
  }
  String headers;
  headers.reserve(768);
  headers += "POST ";
  headers += kDeviceAudioTurnRawPath;
  headers += " HTTP/1.1\r\nHost: ";
  headers += hostHeader;
  headers += "\r\nUser-Agent: OpenClaw-Cardputer/";
  headers += kAppVersion;
  headers += "\r\nConnection: close\r\nContent-Type: application/octet-stream\r\nContent-Length: ";
  headers += String(static_cast<unsigned>(dataSize));
  headers += "\r\nX-Audio-Sample-Rate: ";
  headers += String(kMicSampleRate);
  headers += "\r\nX-Device-Id: ";
  headers += gDeviceId;
  headers += "\r\nX-Firmware-Version: ";
  headers += kAppVersion;
  headers += "\r\nX-Conversation-Key: ";
  headers += currentConversationKey();
  headers += "\r\nX-Client-Msg-Id: ";
  headers += turnId;
  headers += "\r\nX-Turn-Id: ";
  headers += turnId;
  headers += "\r\nX-Reply-Audio: true\r\nX-Save-Reply-Audio: true\r\n";
  if (!gDeviceToken.isEmpty()) {
    headers += "Authorization: Bearer ";
    headers += gDeviceToken;
    headers += "\r\nX-Device-Token: ";
    headers += gDeviceToken;
    headers += "\r\n";
  }
  headers += "\r\n";

  if (!writeClientAll(*client, headers, "voice_headers")) {
    client->stop();
    return false;
  }

  bool payloadOk = false;
  if (!filePath.isEmpty()) {
    fs::FS* fs = activeVoiceFs();
    File pcmFile = fs ? fs->open(filePath, "r") : File();
    if (!pcmFile) {
      setHttpDiag("voice_body: file open failed");
      client->stop();
      return false;
    }
    payloadOk = writeClientAllFileStaged(*client, pcmFile, dataSize, "voice_body");
    pcmFile.close();
  } else {
    payloadOk = writeClientAllStaged(*client, ramPayload, dataSize, "voice_body");
  }
  if (!payloadOk) {
    client->stop();
    return false;
  }

  setHttpDiag("voice_post: wait reply");
  int contentLength = -1;
  uint32_t ignoredSampleRate = kTtsDefaultSampleRate;
  if (!parseHttpStatusAndHeaders(*client, statusCode, contentLength, ignoredSampleRate, kTurnHttpTimeoutMs)) {
    setHttpDiag("voice_post: response header failed");
    client->stop();
    return false;
  }
  if (!readHttpResponseBody(*client, contentLength, response, 15000)) {
    logf("[VOICE] raw post response body short status=%d body=%s", statusCode, response.c_str());
  }
  client->stop();
  setHttpDiag("voice_post: http " + String(statusCode));
  logf("[VOICE] raw post status=%d body=%u", statusCode, static_cast<unsigned>(response.length()));
  return true;
}

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
    configureSecureClient(*secureClient);
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
