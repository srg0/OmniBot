
bool connectHubClient(WiFiClient& client, const char* reason, uint32_t readTimeoutSec) {
  if (!ensureWifiForHttp(reason)) {
    setHttpDiag(String(reason ? reason : "?") + ": wifi unavailable");
    return false;
  }
  for (uint8_t attempt = 0; attempt < kHttpConnectRetries; ++attempt) {
    client.stop();
    delay(30);
    if (attempt > 0) {
      ensureWifiForHttp(reason);
      delay(150);
    }
    if (client.connect(gHubHost.c_str(), gHubPort)) {
      client.setTimeout(readTimeoutSec);
      if (!hubUsesTls()) {
        client.setNoDelay(true);
      }
      setHttpDiag(String(reason ? reason : "?") + ": connect ok");
      logf("[HTTP] connect ok reason=%s attempt=%u ip=%s",
           reason ? reason : "?",
           static_cast<unsigned>(attempt + 1),
           WiFi.localIP().toString().c_str());
      return true;
    }
    setHttpDiag(String(reason ? reason : "?") + ": connect fail");
    logf("[HTTP] connect fail reason=%s attempt=%u wifi=%d ip=%s",
         reason ? reason : "?",
         static_cast<unsigned>(attempt + 1),
         static_cast<int>(WiFi.status()),
         WiFi.localIP().toString().c_str());
  }
  return false;
}

String escapeJsonString(const String& raw) {
  String out;
  out.reserve(raw.length() + 16);
  for (size_t i = 0; i < raw.length(); ++i) {
    char c = raw[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<uint8_t>(c) >= 0x20) {
          out += c;
        }
        break;
    }
  }
  return out;
}

String authHeaderLine() {
  if (gDeviceToken.isEmpty()) {
    return "X-Device-Id: " + gDeviceId + "\r\n";
  }
  return "Authorization: Bearer " + gDeviceToken + "\r\n" +
         "X-Device-Token: " + gDeviceToken + "\r\n" +
         "X-Device-Id: " + gDeviceId + "\r\n";
}

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
    secureClient.setInsecure();
    secureClient.setHandshakeTimeout(12);
    secureClient.setTimeout((kTurnHttpTimeoutMs + 999) / 1000);
    client = &secureClient;
  } else {
    plainClient.setTimeout((kTurnHttpTimeoutMs + 999) / 1000);
    plainClient.setNoDelay(true);
    client = &plainClient;
  }

  setHttpDiag("voice_post: connect");
  if (!client->connect(gHubHost.c_str(), gHubPort)) {
    setHttpDiag("voice_post: connect failed");
    logf("[VOICE] raw post connect failed host=%s port=%u", gHubHost.c_str(), gHubPort);
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

