    }
  }

  gPrefs.begin(kPrefsContextNs, true);
  String savedKey = gPrefs.getString("key", "");
  gPrefs.end();
  gSavedContextKey = savedKey;
  selectSavedContextOrDefault(savedKey);
  if (gSavedContextKey.isEmpty() && !gContexts.empty()) {
    gSavedContextKey = currentConversationKey();
  }
  clearContextPreviewIfSelectionChanged();
  appendRuntimeLog("CTX", "loaded count=" + String(gContexts.size()), true);
}

void saveCurrentContext() {
  if (gContexts.empty()) {
    return;
  }
  gPrefs.begin(kPrefsContextNs, false);
  gPrefs.putString("key", currentConversationKey());
  gPrefs.end();
  gSavedContextKey = currentConversationKey();
  appendRuntimeLog("CTX", "selected " + gContexts[gSelectedContext].shortName, true);
}

String currentConversationKey() {
  if (gContexts.empty()) {
    return "tg:root-main:default:-1003665527854:54";
  }
  gSelectedContext = clampValue<int>(gSelectedContext, 0, static_cast<int>(gContexts.size()) - 1);
  return gContexts[gSelectedContext].key;
}

bool fetchRemoteTopicCatalog() {
  if ((!gWifiReady && !ensureWifiForHttp("topics", 3000)) || gHubHost.isEmpty()) {
    gContextError = "Bridge offline";
    setStatus("Topics offline", 1200);
    return false;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + "/api/cardputer/v1/topics?recent_days=7&limit=32";
  if (!configureHttpClient(http, secureClient, url, 15000)) {
    gContextError = "Connect failed";
    setStatus("Topics connect failed", 1200);
    return false;
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  int code = http.GET();
  String body = code > 0 ? http.getString() : "";
  http.end();
  if (code < 200 || code >= 300) {
    gContextError = "HTTP " + String(code);
    setStatus("Topics HTTP " + String(code), 1400);
    appendRuntimeLog("CTX", "topics http " + String(code), true);
    return false;
  }

  DynamicJsonDocument doc(16384);
  auto err = deserializeJson(doc, body);
  if (err || !doc["items"].is<JsonArray>()) {
    gContextError = err ? String(err.c_str()) : "Bad topics";
    setStatus("Topics parse failed", 1400);
    appendRuntimeLog("CTX", "topics parse failed", true);
    return false;
  }

  std::vector<ConversationContext> loaded;
  for (JsonObject item : doc["items"].as<JsonArray>()) {
    if (loaded.size() >= kMaxContexts) {
      break;
    }
    String topicKey = item["topic_key"] | "";
    String title = item["display_title"] | item["title"] | "";
    String emoji = item["emoji"] | item["icon"] | item["emoji_name"] | "";
    int32_t threadId = item["thread_id"] | 0;
    if (topicKey.isEmpty()) {
      continue;
    }
    if (title.isEmpty()) {
      title = "Topic " + String(threadId > 0 ? threadId : static_cast<int>(loaded.size() + 1));
    }
    if (!emoji.isEmpty() && title.indexOf(emoji) < 0) {
      title = emoji + " " + title;
    }
    title = normalizeTopicTitleForDisplay(title, false);
    ConversationContext ctx;
    ctx.label = title;
    ctx.emoji = emoji.isEmpty() ? fallbackTopicEmojiForTitle(title, threadId) : emoji;
    ctx.key = topicKey;
    ctx.shortName = makeTopicShortName(title, threadId);
    ctx.topicKey = topicKey;
    ctx.threadId = threadId;
    ctx.headSeq = item["head_seq"] | 0;
    ctx.unread = item["unread_count"] | 0;
    ctx.remote = true;
    loaded.push_back(ctx);
  }
  if (loaded.empty()) {
    gContextError = "No topics";
    setStatus("No topics", 1200);
    return false;
  }

  gContexts = loaded;
  gContextsRemoteLoaded = true;
  gContextsFetchedAtMs = millis();
  gContextError = "";
  selectSavedContextOrDefault(gSavedContextKey);
  clearContextPreviewIfSelectionChanged();
  appendRuntimeLog("CTX", "remote topics=" + String(gContexts.size()), true);
  setStatus("Topics synced", 900);
  return true;
}

bool selectRemoteCurrentTopic() {
  if (gContexts.empty() || (!gWifiReady && !ensureWifiForHttp("topic-select", 2000)) || gHubHost.isEmpty()) {
    return false;
  }
  String topicKey = gContexts[gSelectedContext].topicKey.isEmpty() ? gContexts[gSelectedContext].key : gContexts[gSelectedContext].topicKey;
  if (!topicKey.startsWith("tg:")) {
    return false;
  }
  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  String url = hubBaseUrl() + "/api/cardputer/v1/topics/" + urlEncodePathSegment(topicKey) + "/select";
  if (!configureHttpClient(http, secureClient, url, 8000)) {
    appendRuntimeLog("CTX", "select connect failed", true);
    return false;
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{}");
  http.end();
  bool ok = code >= 200 && code < 300;
	  appendRuntimeLog("CTX", ok ? "remote selected" : ("select http " + String(code)), true);
	  return ok;
	}

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
