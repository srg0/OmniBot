  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, 0);
  WiFiClient* stream = http.getStreamPtr();
  uint32_t written = 0;
  uint32_t lastLogged = 0;
  uint32_t lastByteMs = millis();
  while (written < expectedSize) {
    size_t available = stream ? stream->available() : 0;
    if (!available) {
      if (!http.connected()) {
        break;
      }
      if (millis() - lastByteMs > 60000) {
        esp_ota_abort(otaHandle);
        mbedtls_sha256_free(&shaCtx);
        http.end();
        finishTransferUi("download stalled", false);
        setHttpDiag("ota: stalled");
        return fail("stream stalled " + String(static_cast<unsigned>(written)));
      }
      updateTransferUi(written, expectedSize, "receiving");
      delay(2);
      continue;
    }

    size_t toRead = min(available, kOtaIoBufferBytes);
    toRead = min<uint32_t>(static_cast<uint32_t>(toRead), expectedSize - written);
    int readBytes = stream->readBytes(gOtaIoBuffer, toRead);
    if (readBytes <= 0) {
      delay(1);
      continue;
    }

    otaErr = esp_ota_write(otaHandle, gOtaIoBuffer, static_cast<size_t>(readBytes));
    if (otaErr != ESP_OK) {
      esp_ota_abort(otaHandle);
      mbedtls_sha256_free(&shaCtx);
      http.end();
      finishTransferUi("flash write failed", false);
      return fail("write err " + String(static_cast<int>(otaErr)) +
                  " at " + String(static_cast<unsigned>(written)));
    }
    mbedtls_sha256_update(&shaCtx, gOtaIoBuffer, static_cast<size_t>(readBytes));
    written += static_cast<uint32_t>(readBytes);
    lastByteMs = millis();
    updateTransferUi(written, expectedSize, "receiving + flashing");
    if (written == expectedSize || written - lastLogged >= 256UL * 1024UL) {
      lastLogged = written;
      otaLog("direct flash " + String(static_cast<unsigned>(written / 1024)) + "k/" +
             String(static_cast<unsigned>(expectedSize / 1024)) + "k", false);
    }
    delay(1);
  }

  uint8_t hash[32];
  mbedtls_sha256_finish(&shaCtx, hash);
  mbedtls_sha256_free(&shaCtx);
  http.end();

  if (written != expectedSize) {
    esp_ota_abort(otaHandle);
    finishTransferUi("flash short", false);
    return fail("short write " + String(static_cast<unsigned>(written)) +
                "/" + String(static_cast<unsigned>(expectedSize)));
  }

  String actualSha = bytesToHex(hash, sizeof(hash));
  actualSha.toLowerCase();
  String expectedSha = gOtaManifest.sha256;
  expectedSha.toLowerCase();
  if (actualSha != expectedSha) {
    esp_ota_abort(otaHandle);
    finishTransferUi("SHA mismatch", false);
    return fail("sha mismatch expected=" + expectedSha.substring(0, 12) +
                " actual=" + actualSha.substring(0, 12));
  }

  bool ok = finishOtaApply(otaHandle, next, written, expectedSize);
  if (!ok) {
    return fail("direct flash failed");
  }
  return true;
}

String urlEncodePathSegment(const String& raw) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  out.reserve(raw.length() + 8);
  for (size_t i = 0; i < raw.length(); ++i) {
    uint8_t c = static_cast<uint8_t>(raw[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

String makeTopicShortName(const String& title, int32_t threadId) {
  (void)threadId;
  String displayTitle = normalizeTopicTitleForDisplay(title, false);
  String out;
  for (size_t i = 0; i < displayTitle.length() && out.length() < 4; ++i) {
    char c = displayTitle[i];
    if (isalnum(static_cast<unsigned char>(c))) {
      out += static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
  }
  if (!out.isEmpty()) {
    return out;
  }
  return "TG";
}

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
