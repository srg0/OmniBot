void flashActivityLed(uint8_t r, uint8_t g, uint8_t b, uint32_t durationMs) {
  if (!M5.Led.isEnabled()) {
    return;
  }
  M5.Led.setAllColor(r, g, b);
  M5.Led.display();
  gLedFlashUntilMs = millis() + durationMs;
}

void updateStatusLed() {
  if (!M5.Led.isEnabled()) {
    return;
  }
  if (gLedFlashUntilMs != 0 && millis() >= gLedFlashUntilMs) {
    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();
    gLedFlashUntilMs = 0;
  }
}

uint16_t colorForLine(LineKind kind) {
  switch (kind) {
    case LineKind::User:
      return TFT_CYAN;
    case LineKind::Assistant:
      return TFT_GREENYELLOW;
    case LineKind::Error:
      return TFT_ORANGE;
    case LineKind::System:
    default:
      return TFT_LIGHTGREY;
  }
}

void pushLine(const String& prefix, const String& text, LineKind kind) {
  if (text.isEmpty()) {
    return;
  }
  gChatLines.push_back({prefix, text, kind});
  while (gChatLines.size() > kMaxChatLines) {
    gChatLines.pop_front();
  }
  gChatScrollOffset = 0;
}

void pushSystem(const String& text) { pushLine("SYS", text, LineKind::System); }
void pushError(const String& text) {
  pushLine("ERR", text, LineKind::Error);
  appendRuntimeLog("ERR", text, true);
}
void pushUser(const String& text) { pushLine("YOU", text, LineKind::User); }
void pushAssistant(const String& text) { pushLine("AI", text, LineKind::Assistant); }

LineKind topicHistoryLineKind(const String& role) {
  if (role == "assistant") {
    return LineKind::Assistant;
  }
  if (role == "device" || role == "human" || role == "user") {
    return LineKind::User;
  }
  return LineKind::System;
}

String topicHistoryPrefix(const String& role, bool voice) {
  if (voice) {
    return "VO";
  }
  if (role == "assistant") {
    return "AI";
  }
  if (role == "device") {
    return "ME";
  }
  if (role == "human" || role == "user") {
    return "TG";
  }
  return "MSG";
}

void replaceChatWithTopicHistory(const String& title, const std::vector<ChatLine>& lines) {
  gChatLines.clear();
  String header = "Topic: " + trimPreview(normalizeTopicTitleForDisplay(title, false), 72);
  pushLine("CTX", header, LineKind::System);
  if (lines.empty()) {
    pushLine("CTX", "No recent messages", LineKind::System);
    return;
  }
  for (const auto& line : lines) {
    pushLine(line.prefix, line.text, line.kind);
  }
  gChatScrollOffset = 0;
}

String fallbackTopicEmojiForTitle(const String& title, int32_t threadId) {
  String lower = title;
  lower.toLowerCase();
  if (lower.indexOf("card") >= 0 || lower.indexOf("adv") >= 0) {
    return "💾";
  }
  if (lower.indexOf("marathon") >= 0 || lower.indexOf("run") >= 0) {
    return "🏃";
  }
  if (lower.indexOf("body") >= 0 || lower.indexOf("health") >= 0) {
    return "💪";
  }
  if (lower.indexOf("focus") >= 0 || lower.indexOf("task") >= 0) {
    return "🎯";
  }
  if (lower.indexOf("video") >= 0) {
    return "🎬";
  }
  if (lower.indexOf("audio") >= 0 || lower.indexOf("music") >= 0) {
    return "🎧";
  }
  if (threadId == 1 || lower.indexOf("assistant") >= 0 || lower.indexOf("помощ") >= 0) {
    return "🤖";
  }
  return "💬";
}

String topicDisplayTitle(const ConversationContext& ctx, bool keepAvailableEmoji = false) {
  return normalizeTopicTitleForDisplay(ctx.label, keepAvailableEmoji);
}

String topicDisplayIcon(const ConversationContext& ctx) {
  if (!ctx.emoji.isEmpty()) {
    String emoji = normalizeEmojiDisplayText(ctx.emoji);
    uint32_t codepoint = 0;
    int consumed = 1;
    if (decodeUtf8At(emoji, 0, codepoint, consumed) && emojiAssetAvailable(codepoint)) {
      const EmojiCacheEntry* entry = emojiCacheLookup(codepoint);
      if (entry && entry->data && entry->width > 0 && entry->height > 0) {
        return emoji.substring(0, consumed);
      }
    }
  }
  return ctx.shortName.isEmpty() ? makeTopicShortName(ctx.label, ctx.threadId) : ctx.shortName;
}

bool decodeUtf8At(const String& value, int index, uint32_t& codepoint, int& consumed) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(value.c_str());
  int len = value.length();
  uint8_t b0 = bytes[index];
  if (b0 < 0x80) {
    codepoint = b0;
    consumed = 1;
    return true;
  }
  if ((b0 & 0xE0) == 0xC0 && index + 1 < len && (bytes[index + 1] & 0xC0) == 0x80) {
    codepoint = ((b0 & 0x1F) << 6) | (bytes[index + 1] & 0x3F);
    consumed = 2;
    return true;
  }
  if ((b0 & 0xF0) == 0xE0 && index + 2 < len &&
      (bytes[index + 1] & 0xC0) == 0x80 && (bytes[index + 2] & 0xC0) == 0x80) {
    codepoint = ((b0 & 0x0F) << 12) | ((bytes[index + 1] & 0x3F) << 6) |
                (bytes[index + 2] & 0x3F);
    consumed = 3;
    return true;
  }
  if ((b0 & 0xF8) == 0xF0 && index + 3 < len &&
      (bytes[index + 1] & 0xC0) == 0x80 && (bytes[index + 2] & 0xC0) == 0x80 &&
      (bytes[index + 3] & 0xC0) == 0x80) {
    codepoint = ((b0 & 0x07) << 18) | ((bytes[index + 1] & 0x3F) << 12) |
                ((bytes[index + 2] & 0x3F) << 6) | (bytes[index + 3] & 0x3F);
    consumed = 4;
    return true;
  }
  codepoint = b0;
  consumed = 1;
  return false;
}

bool shouldSuppressEmojiModifier(uint32_t codepoint) {
  return codepoint == 0x200D || codepoint == 0x20E3 || codepoint == 0xFE0E ||
         codepoint == 0xFE0F || (codepoint >= 0x1F3FB && codepoint <= 0x1F3FF) ||
         (codepoint >= 0xE0100 && codepoint <= 0xE01EF);
}

String normalizeEmojiDisplayText(const String& raw) {
  String out;
  out.reserve(raw.length());
  int i = 0;
  while (i < raw.length()) {
    uint32_t codepoint = 0;
    int consumed = 1;
    bool ok = decodeUtf8At(raw, i, codepoint, consumed);
    if (ok && shouldSuppressEmojiModifier(codepoint)) {
      i += consumed;
      continue;
    }
    for (int n = 0; n < consumed && i + n < raw.length(); ++n) {
      out += raw[i + n];
    }
    i += consumed;
  }
  return out;
}

bool emojiAssetAvailable(uint32_t codepoint) {
  if (!gSdReady || !isEmojiAssetCodepoint(codepoint)) {
    return false;
  }
  char path[40];
  snprintf(path, sizeof(path), "%s/u%lX.png", kEmojiDir, static_cast<unsigned long>(codepoint));
  if (SD.exists(path)) {
    return true;
  }
  snprintf(path, sizeof(path), "%s/u%lx.png", kEmojiDir, static_cast<unsigned long>(codepoint));
  return SD.exists(path);
}

String normalizeTopicTitleForDisplay(const String& raw, bool keepAvailableEmoji) {
  String cleaned = raw;
  cleaned.replace("to:root-main", "");
  cleaned.replace("root-main", "");
  int slashPos = cleaned.lastIndexOf(" / ");
  if (slashPos > 0) {
    bool numericSuffix = true;
    for (int i = slashPos + 3; i < cleaned.length(); ++i) {
      char c = cleaned[i];
      if (c < '0' || c > '9') {
        numericSuffix = false;
        break;
      }
    }
    if (numericSuffix) {
      cleaned = cleaned.substring(0, slashPos);
    }
  }
  cleaned.trim();

