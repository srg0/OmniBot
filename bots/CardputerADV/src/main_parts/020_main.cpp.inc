  DynamicJsonDocument doc(32768);
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
      JsonObject voice = content["voice"].as<JsonObject>();
      line.voiceUrl = voice["url"] | "";
      line.voiceSha256 = voice["sha256"] | "";
      line.voiceSize = voice["bytes"] | voice["size"] | 0;
      String mediaId = voice["media_id"] | "";
      line.voicePlayable = !line.voiceUrl.isEmpty();
      line.voiceTitle = mediaId.isEmpty() ? "history voice" : ("voice " + mediaId);
    } else {
      line.text = content["text"] | "";
    }
    if (!line.text.isEmpty()) {
      gContextPreview.push_back(line);
      historyChatLines.push_back({
          topicHistoryPrefix(line.role, line.voice),
          line.text,
          topicHistoryLineKind(line.role),
          line.voicePlayable,
          line.voiceUrl,
          line.voiceTitle,
          line.voiceSha256,
          line.voiceSize,
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

bool playLastInboxMessage() {
  if (gInboxLastKind == "voice" && !gInboxLastVoiceUrl.isEmpty()) {
    String url = gInboxLastVoiceUrl;
    if (url.startsWith("/")) {
      url = hubBaseUrl() + url;
    }
    return downloadAndPlayResponseAudio(url, "", gInboxLastVoiceSha256, gInboxLastVoiceSize,
                                        gInboxLastVoiceTitle, gInboxLastText);
  }
  if (!gInboxLastText.isEmpty()) {
    return requestAndPlayTts(gInboxLastText);
  }
  return false;
}

bool handleInboxHistoryItem(JsonObject item, bool forcePlay) {
  uint32_t seq = item["seq"] | 0;
  JsonObject sender = item["sender"].as<JsonObject>();
  JsonObject content = item["content"].as<JsonObject>();
  String senderKind = sender["kind"] | "";
  String senderName = sender["display_name"] | "";
  String kind = content["kind"] | "";
  String text = kind == "voice" ? String(content["transcript"] | content["text"] | "")
                                : String(content["text"] | "");
  if (senderKind == "device") {
    return false;
  }
  if (!forcePlay && seq != 0 && seq <= gInboxLastSeq) {
    return false;
  }

  gInboxLastSeq = seq;
  gInboxLastText = text;
  gInboxLastKind = kind;
  gInboxLastSender = senderName.isEmpty() ? senderKind : senderName;
  gInboxLastVoiceUrl = "";
  gInboxLastVoiceTitle = "";
  gInboxLastVoiceSha256 = "";
  gInboxLastVoiceSize = 0;

  String prefix = senderKind == "assistant" ? "AI" : "IN";
  LineKind lineKind = senderKind == "assistant" ? LineKind::Assistant : LineKind::User;
  if (kind == "voice") {
    JsonObject voice = content["voice"].as<JsonObject>();
    gInboxLastVoiceUrl = voice["url"] | "";
    gInboxLastVoiceSha256 = voice["sha256"] | "";
    gInboxLastVoiceSize = voice["bytes"] | voice["size"] | 0;
    String mediaId = voice["media_id"] | "";
    gInboxLastVoiceTitle = mediaId.isEmpty() ? "inbox voice" : ("inbox " + mediaId);
    if (text.isEmpty()) {
      text = "Voice message";
      gInboxLastText = text;
    }
    pushVoiceHistoryLine(prefix, text, lineKind, gInboxLastVoiceUrl, gInboxLastVoiceTitle,
                         gInboxLastVoiceSha256, gInboxLastVoiceSize);
  } else {
    if (text.isEmpty()) {
      return false;
    }
    pushLine(prefix, text, lineKind);
  }

  setStatus("Inbox message", 1200);
  appendRuntimeLog("INBOX", "seq=" + String(seq) + " kind=" + kind + " sender=" + gInboxLastSender, true);
  logf("[INBOX] received seq=%u kind=%s sender=%s text=%s",
       static_cast<unsigned>(seq), kind.c_str(), gInboxLastSender.c_str(), text.c_str());
  return true;
}

bool fetchLatestInboxMessage(bool forcePlay, bool speak) {
  if ((!gWifiReady && !ensureWifiForHttp("inbox", 3000)) || gHubHost.isEmpty()) {
    setStatus("Inbox offline", 1200);
    logf("[INBOX] offline");
    return false;
  }
  String topicKey = currentConversationKey();
  String url = hubBaseUrl() + "/api/cardputer/v1/topics/" + urlEncodePathSegment(topicKey) +
               "/inbox/latest?text_chars=4096";
  if (!forcePlay && gInboxLastSeq > 0) {
    url += "&after_seq=" + String(gInboxLastSeq);
  }
  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  if (!configureHttpClient(http, secureClient, url, 15000)) {
    setStatus("Inbox connect failed", 1200);
    logf("[INBOX] connect failed");
    return false;
  }
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  int code = http.GET();
  String body = code > 0 ? http.getString() : "";
  http.end();
  if (code < 200 || code >= 300) {
    setStatus("Inbox HTTP " + String(code), 1400);
    logf("[INBOX] http=%d body=%s", code, body.c_str());
    return false;
  }
  DynamicJsonDocument doc(8192);
  auto err = deserializeJson(doc, body);
  if (err) {
    setStatus("Inbox parse failed", 1400);
    logf("[INBOX] parse failed err=%s body_len=%u", err.c_str(), static_cast<unsigned>(body.length()));
    return false;
  }

  bool found = false;
  if (doc["item"].is<JsonObject>()) {
    JsonObject item = doc["item"].as<JsonObject>();
    if (handleInboxHistoryItem(item, forcePlay)) {
      found = true;
    }
  }
  doc.clear();
  body = "";
  if (found) {
    if (speak) {
      bool played = playLastInboxMessage();
      logf("[INBOX] playback ok=%d seq=%u", static_cast<int>(played), static_cast<unsigned>(gInboxLastSeq));
    }
    return true;
  }
  setStatus("No new inbox", 900);
  logf("[INBOX] no new message last_seq=%u", static_cast<unsigned>(gInboxLastSeq));
  return false;
}

bool injectInboxText(const String& text, bool speakAfter) {
  String bodyText = text;
  bodyText.trim();
  if (bodyText.isEmpty()) {
    logf("[INBOX] inject empty");
    return false;
  }
  if ((!gWifiReady && !ensureWifiForHttp("inbox-inject", 3000)) || gHubHost.isEmpty()) {
    setStatus("Inject offline", 1200);
    logf("[INBOX] inject offline");
    return false;
  }
  String topicKey = currentConversationKey();
  String url = hubBaseUrl() + "/api/cardputer/v1/topics/" + urlEncodePathSegment(topicKey) + "/inbox/inject";
  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> secureClient;
  if (!configureHttpClient(http, secureClient, url, 15000)) {
    setStatus("Inject connect failed", 1200);
    logf("[INBOX] inject connect failed");
    return false;
  }
  DynamicJsonDocument doc(1024);
  doc["text"] = bodyText;
  doc["sender_kind"] = "human";
  doc["sender_name"] = "Serial smoke";
  doc["source"] = "cardputer_serial_inject";
  String payload;
  serializeJson(doc, payload);
  http.addHeader("Content-Type", "application/json");
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }
  int code = http.POST(payload);
  String response = code > 0 ? http.getString() : "";
  http.end();
  if (code < 200 || code >= 300) {
    setStatus("Inject HTTP " + String(code), 1400);
    logf("[INBOX] inject http=%d body=%s", code, response.c_str());
    return false;
  }
  logf("[INBOX] inject ok chars=%d", bodyText.length());
  return fetchLatestInboxMessage(true, speakAfter);
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

bool persistWavFromPcmFile(const String& outPath, const String& pcmPath, uint32_t sampleRate, size_t audioLen) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return false;
  }
  File src = fs->open(pcmPath, "r");
  if (!src) {
    return false;
  }
  File dst = fs->open(outPath, "w");
  if (!dst) {
    src.close();
    return false;
  }

  uint8_t header[44];
  writeWavHeader(header, static_cast<uint32_t>(audioLen), sampleRate);
  bool ok = dst.write(header, sizeof(header)) == sizeof(header) && persistFileToFile(src, dst, audioLen);
  src.close();
  dst.close();
  if (!ok) {
    fsRemove(*fs, outPath);
  }
  return ok;
}

void rememberVoiceNote(const String& title, const String& preview, const uint8_t* header, size_t headerLen,
                       const uint8_t* audioBytes, size_t audioLen, uint32_t sampleRate, bool assistant) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage()) {
    return;
  }
  uint64_t totalBytes = activeStorageTotalBytes();
  uint64_t usedBytes = activeStorageUsedBytes();
  size_t requiredBytes = headerLen + audioLen + 2048;
  if (totalBytes > 0 && usedBytes + requiredBytes >= totalBytes) {
    logf("[VOICE] skip note storage used=%llu total=%llu required=%u",
         static_cast<unsigned long long>(usedBytes),
         static_cast<unsigned long long>(totalBytes),
         static_cast<unsigned>(requiredBytes));
    return;
  }
  VoiceNote note;
  note.id = makeVoiceNoteId(assistant);
  note.audioPath = voiceAudioPath(note.id);
  note.metaPath = voiceMetaPath(note.id);
  note.title = makeVoiceNoteTitle(assistant, audioLen, sampleRate);
  note.preview = trimPreview(preview);
  note.sampleRate = sampleRate;
  note.durationMs = sampleRate ? static_cast<uint32_t>((audioLen / 2U) * 1000ULL / sampleRate) : 0;
  note.assistant = assistant;

  if (!persistWavFile(note.audioPath, header, headerLen, audioBytes, audioLen)) {
    return;
  }
  if (!saveVoiceNoteMetadata(note)) {
    fsRemove(*fs, note.audioPath);
    return;
  }
  gVoiceNotes.push_back(note);
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
  gSelectedVoiceNote = 0;
}

void rememberVoiceNoteFromPcmFile(const String& title, const String& preview, const String& pcmPath,
                                  size_t audioLen, uint32_t sampleRate, bool assistant) {
  fs::FS* fs = activeVoiceFs();
  if (!gStorageReady || !ensureVoiceStorage() || !fs || !fsExists(*fs, pcmPath)) {
    return;
  }
  uint64_t totalBytes = activeStorageTotalBytes();
  uint64_t usedBytes = activeStorageUsedBytes();
  size_t requiredBytes = 44 + audioLen + 2048;
  if (totalBytes > 0 && usedBytes + requiredBytes >= totalBytes) {
    logf("[VOICE] skip note storage used=%llu total=%llu required=%u",
         static_cast<unsigned long long>(usedBytes),
         static_cast<unsigned long long>(totalBytes),
         static_cast<unsigned>(requiredBytes));
    return;
  }

  VoiceNote note;
  note.id = makeVoiceNoteId(assistant);
  note.audioPath = voiceAudioPath(note.id);
  note.metaPath = voiceMetaPath(note.id);
  note.title = makeVoiceNoteTitle(assistant, audioLen, sampleRate);
  note.preview = trimPreview(preview);
  note.sampleRate = sampleRate;
  note.durationMs = sampleRate ? static_cast<uint32_t>((audioLen / 2U) * 1000ULL / sampleRate) : 0;
  note.assistant = assistant;

  if (!persistWavFromPcmFile(note.audioPath, pcmPath, sampleRate, audioLen)) {
    return;
  }
  if (!saveVoiceNoteMetadata(note)) {
    fsRemove(*fs, note.audioPath);
    return;
  }
  gVoiceNotes.push_back(note);
  sortVoiceNotesNewestFirst();
  pruneVoiceNotes();
  gSelectedVoiceNote = 0;
}

uint16_t readLe16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
