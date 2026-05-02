
String defaultNewTopicTitle() {
  String title = gInputBuffer;
  title.trim();
  if (!title.isEmpty()) {
    title = normalizeEmojiDisplayText(title);
    if (title.length() > 42) {
      title = title.substring(0, 42);
      title.trim();
    }
    return title;
  }
  ensureBangkokTimezone();
  time_t now = time(nullptr);
  if (now > 100000) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "Cardputer %d %b %H:%M", &timeinfo);
    return String(buf);
  }
  return "Cardputer topic";
}

bool createRemoteTopic(const String& title, String& createdTopicKey) {
  if ((!gWifiReady && !ensureWifiForHttp("topic-create", 3000)) || gHubHost.isEmpty()) {
    gContextError = "Bridge offline";
    setStatus("Create offline", 1200);
    return false;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + "/api/cardputer/v1/topics/create";
  if (!configureHttpClient(http, secureClient, url, 15000)) {
    gContextError = "Create connect";
    setStatus("Create connect failed", 1200);
    return false;
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  http.addHeader("Content-Type", "application/json");
  DynamicJsonDocument req(512);
  req["title"] = title;
  String payload;
  serializeJson(req, payload);
  int code = http.POST(payload);
  String body = code > 0 ? http.getString() : "";
  http.end();
  if (code < 200 || code >= 300) {
    gContextError = "Create HTTP " + String(code);
    setStatus(gContextError, 1400);
    appendRuntimeLog("CTX", "create http " + String(code), true);
    return false;
  }
  DynamicJsonDocument doc(4096);
  auto err = deserializeJson(doc, body);
  if (err) {
    gContextError = "Create parse";
    setStatus("Create parse failed", 1400);
    return false;
  }
  createdTopicKey = doc["topic"]["topic_key"] | doc["topic_key"] | "";
  if (createdTopicKey.isEmpty()) {
    gContextError = "Create bad response";
    setStatus(gContextError, 1400);
    return false;
  }
  appendRuntimeLog("CTX", "created topic", true);
  return true;
}

bool fetchSelectedTopicHistory() {
  if (gContexts.empty()) {
    return false;
  }
  if ((!gWifiReady && !ensureWifiForHttp("topic-history", 3000)) || gHubHost.isEmpty()) {
    gContextError = "Bridge offline";
    setStatus("History offline", 1200);
    return false;
  }
  gSelectedContext = clampValue<int>(gSelectedContext, 0, static_cast<int>(gContexts.size()) - 1);
  String topicKey = gContexts[gSelectedContext].topicKey.isEmpty() ? gContexts[gSelectedContext].key : gContexts[gSelectedContext].topicKey;
  String url = hubBaseUrl() + "/api/cardputer/v1/topics/" + urlEncodePathSegment(topicKey) +
               "/history?limit=5&compact=1&text_chars=150";

  auto getHistoryBody = [&](String& outBody) -> int {
    HTTPClient http;
    std::unique_ptr<WiFiClientSecure> secureClient;
    if (!configureHttpClient(http, secureClient, url, 15000)) {
      return -1000;
    }
    if (!gDeviceToken.isEmpty()) {
      http.addHeader("Authorization", "Bearer " + gDeviceToken);
    }
    int httpCode = http.GET();
    outBody = httpCode > 0 ? http.getString() : "";
    if (httpCode <= 0) {
      appendRuntimeLog("CTX", "history transport " + String(httpCode), true);
      setHttpDiag("history transport " + String(httpCode));
    }
    http.end();
    return httpCode;
  };

  String body;
  int code = getHistoryBody(body);
  if (code <= 0) {
    ensureWifiForHttp("topic-history-retry", 3000);
    delay(80);
    code = getHistoryBody(body);
  }
  if (code < 200 || code >= 300) {
    gContextError = "History HTTP " + String(code);
    setStatus("History HTTP " + String(code), 1400);
    appendRuntimeLog("CTX", "history http " + String(code), true);
    return false;
  }

  DynamicJsonDocument doc(8192);
  auto err = deserializeJson(doc, body);
  if (err || !doc["items"].is<JsonArray>()) {
    gContextError = err ? String(err.c_str()) : "Bad history";
    setStatus("History parse failed", 1400);
    return false;
  }

  String responseTitle = doc["topic"]["display_title"] | doc["topic"]["title"] | doc["title"] | "";
  String responseEmoji = doc["topic"]["emoji"] | doc["topic"]["icon"] | "";
  if (!responseEmoji.isEmpty() && responseTitle.indexOf(responseEmoji) < 0) {
    responseTitle = responseEmoji + " " + responseTitle;
  }
  if (!responseTitle.isEmpty()) {
    responseTitle = normalizeTopicTitleForDisplay(responseTitle, false);
    if (!responseTitle.isEmpty() && responseTitle != "Topic") {
      gContexts[gSelectedContext].label = responseTitle;
      gContexts[gSelectedContext].emoji = responseEmoji.isEmpty()
                                              ? fallbackTopicEmojiForTitle(responseTitle, gContexts[gSelectedContext].threadId)
                                              : responseEmoji;
      gContexts[gSelectedContext].shortName = makeTopicShortName(responseTitle, gContexts[gSelectedContext].threadId);
    }
  }

  std::vector<ChatLine> historyChatLines;
  historyChatLines.reserve(kMaxContextPreviewLines);
  gContextPreview.clear();
  for (JsonObject item : doc["items"].as<JsonArray>()) {
    if (gContextPreview.size() >= kMaxContextPreviewLines) {
      break;
    }
    JsonObject sender = item["sender"].as<JsonObject>();
    JsonObject content = item["content"].as<JsonObject>();
    String kind = content["kind"] | "";
    ContextPreviewLine line;
    line.role = sender["kind"] | "";
    line.voice = kind == "voice";
    if (line.role.isEmpty()) {
      line.role = "msg";
    }
    if (kind == "voice") {
      line.text = content["transcript"] | "";
      if (line.text.isEmpty()) {
        uint32_t durationMs = content["voice"]["duration_ms"] | 0;
        line.text = "Voice " + String(durationMs / 1000) + "s";
      }
    } else {
      line.text = content["text"] | "";
    }
    line.text = trimPreview(line.text, 92);
    if (!line.text.isEmpty()) {
      gContextPreview.push_back(line);
      historyChatLines.push_back({
          topicHistoryPrefix(line.role, line.voice),
          trimPreview(line.text, 180),
          topicHistoryLineKind(line.role),
      });
    }
  }
  gContextHistoryKey = topicKey;
  gContextHistoryFetchedAtMs = millis();
  gContextError = "";
  replaceChatWithTopicHistory(gContexts[gSelectedContext].label, historyChatLines);
  appendRuntimeLog("CTX", "history " + gContexts[gSelectedContext].shortName +
                            " lines=" + String(gContextPreview.size()), true);
  setStatus("History loaded", 800);
  return true;
}

bool saveVoiceNoteMetadata(const VoiceNote& note) {
  fs::FS* fs = activeVoiceFs();
  if (!fs) {
    return false;
  }
  DynamicJsonDocument doc(768);
  doc["id"] = note.id;
  doc["audio_path"] = note.audioPath;
  doc["title"] = note.title;
  doc["preview"] = note.preview;
  doc["sample_rate"] = note.sampleRate;
  doc["duration_ms"] = note.durationMs;
  doc["assistant"] = note.assistant;

  File meta = fs->open(note.metaPath, "w");
  if (!meta) {
    return false;
  }
  serializeJson(doc, meta);
  meta.close();
  return true;
}

bool persistWavFile(const String& path, const uint8_t* header, size_t headerLen, const uint8_t* audioBytes,
                    size_t audioLen) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return false;
  }
  File file = fs->open(path, "w");
  if (!file) {
    return false;
  }
  size_t writtenHeader = file.write(header, headerLen);
  size_t writtenBody = file.write(audioBytes, audioLen);
  file.close();
  return writtenHeader == headerLen && writtenBody == audioLen;
}

bool persistFileToFile(File& src, File& dst, size_t bytesToCopy) {
  uint8_t buffer[1024];
  size_t copied = 0;
  while (copied < bytesToCopy) {
    size_t chunk = bytesToCopy - copied;
    if (chunk > sizeof(buffer)) {
      chunk = sizeof(buffer);
    }
    size_t readBytes = src.read(buffer, chunk);
    if (readBytes == 0) {
      return false;
    }
    size_t written = dst.write(buffer, readBytes);
    if (written != readBytes) {
      return false;
    }
    copied += readBytes;
  }
  return copied == bytesToCopy;
}
