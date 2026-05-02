
void clearContextPreviewIfSelectionChanged() {
  String selectedKey = currentConversationKey();
  if (gContextHistoryKey != selectedKey) {
    gContextPreview.clear();
    gContextHistoryKey = "";
    gContextHistoryFetchedAtMs = 0;
  }
}

bool addContextItem(const String& label, const String& key, const String& shortName,
                    const String& topicKey, int32_t threadId, uint32_t headSeq,
                    uint16_t unread, bool remote) {
  if (gContexts.size() >= kMaxContexts) {
    return false;
  }
  ConversationContext ctx;
  ctx.label = label;
  ctx.emoji = fallbackTopicEmojiForTitle(label, threadId);
  ctx.key = key;
  ctx.shortName = shortName.isEmpty() ? makeTopicShortName(label, threadId) : shortName;
  ctx.topicKey = topicKey.isEmpty() ? key : topicKey;
  ctx.threadId = threadId;
  ctx.headSeq = headSeq;
  ctx.unread = unread;
  ctx.remote = remote;
  gContexts.push_back(ctx);
  return true;
}

bool addDefaultContext(const String& label, const String& key, const String& shortName) {
  return addContextItem(label, key, shortName, key, 0, 0, 0, false);
}

void loadDefaultConversationContexts() {
  gContexts.clear();
  addDefaultContext("Помощник", "tg:root-main:default:-1003665527854:54", "TG");
  addDefaultContext("Marathon", "tg:root-main:default:-1003665527854:741", "RUN");
  addDefaultContext("Cardputer", "tg:root-main:default:-1003665527854:323", "ADV");
}

void selectSavedContextOrDefault(const String& savedKey) {
  if (!savedKey.isEmpty()) {
    for (size_t i = 0; i < gContexts.size(); ++i) {
      if (gContexts[i].key == savedKey || gContexts[i].topicKey == savedKey) {
        gSelectedContext = static_cast<int>(i);
        return;
      }
    }
  }
  if (gSelectedContext >= static_cast<int>(gContexts.size())) {
    gSelectedContext = 0;
  }
}

void loadConversationContexts() {
  loadDefaultConversationContexts();

  fs::FS* fs = activeVoiceFs();
  if (fs && fsExists(*fs, kContextRegistryPath)) {
    File file = fs->open(kContextRegistryPath, "r");
    if (file) {
      DynamicJsonDocument doc(4096);
      auto err = deserializeJson(doc, file);
      file.close();
      if (!err && doc["contexts"].is<JsonArray>()) {
        std::vector<ConversationContext> loaded;
        for (JsonObject item : doc["contexts"].as<JsonArray>()) {
          if (loaded.size() >= kMaxContexts) {
            break;
          }
          String label = item["label"] | item["name"] | "";
          String topicKey = item["topic_key"] | "";
          String key = item["key"] | item["conversation_key"] | topicKey;
          String emoji = item["emoji"] | item["icon"] | "";
          int32_t threadId = item["thread_id"] | 0;
          String shortName = item["short"] | "";
          label = normalizeTopicTitleForDisplay(label, false);
          if (!label.isEmpty() && !key.isEmpty()) {
            ConversationContext ctx;
            ctx.label = label;
            ctx.emoji = emoji.isEmpty() ? fallbackTopicEmojiForTitle(label, threadId) : emoji;
            ctx.key = key;
            ctx.shortName = shortName.isEmpty() ? makeTopicShortName(label, threadId) : shortName;
            ctx.topicKey = topicKey.isEmpty() ? key : topicKey;
            ctx.threadId = threadId;
            ctx.headSeq = item["head_seq"] | 0;
            ctx.unread = item["unread"] | 0;
            ctx.remote = item["remote"] | false;
            loaded.push_back(ctx);
          }
        }
        if (!loaded.empty()) {
          gContexts = loaded;
        }
      }
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
