const char* uiModeName(UiMode mode) {
  switch (mode) {
    case UiMode::Home:
      return "Pulse";
    case UiMode::Face:
      return "Face";
    case UiMode::Hero:
      return "Pet";
    case UiMode::ChatFull:
      return "Chat";
    case UiMode::Clock:
      return "Pulse";
    case UiMode::Battery:
      return "Battery";
    case UiMode::Voice:
      return "Voice";
    case UiMode::Focus:
      return "Focus";
    case UiMode::Library:
      return "Library";
    case UiMode::AudioFolders:
      return "Folders";
    case UiMode::Notes:
      return "Notes";
    case UiMode::Assets:
      return "Assets";
    case UiMode::Ota:
      return "OTA";
    case UiMode::Contexts:
      return "Topics";
    case UiMode::Settings:
      return "Settings";
    case UiMode::Logs:
      return "Logs";
    case UiMode::ChatFace:
    default:
      return "Chat+Face";
  }
}

const char* eyeEmotionName(EyeEmotion emotion) {
  switch (emotion) {
    case EyeEmotion::Speaking:
      return "speaking";
    case EyeEmotion::Happy:
      return "happy";
    case EyeEmotion::Mad:
      return "mad";
    case EyeEmotion::Sad:
      return "sad";
    case EyeEmotion::Surprised:
      return "surprised";
    case EyeEmotion::Sleepy:
      return "sleepy";
    case EyeEmotion::Thinking:
      return "thinking";
    case EyeEmotion::Confused:
      return "confused";
    case EyeEmotion::Excited:
      return "excited";
    case EyeEmotion::Love:
      return "love";
    case EyeEmotion::Neutral:
    default:
      return "neutral";
  }
}

EyeEmotion currentEyeEmotion() {
  switch (gFaceMode) {
    case FaceMode::Listening:
      return EyeEmotion::Excited;
    case FaceMode::Thinking:
      return EyeEmotion::Thinking;
    case FaceMode::Speaking:
      return EyeEmotion::Speaking;
    case FaceMode::Error:
      return EyeEmotion::Mad;
    case FaceMode::Idle:
    default:
      break;
  }

  switch ((millis() / 3800) % 8) {
    case 1:
      return EyeEmotion::Happy;
    case 2:
      return EyeEmotion::Love;
    case 3:
      return EyeEmotion::Sleepy;
    case 4:
      return EyeEmotion::Confused;
    case 5:
      return EyeEmotion::Surprised;
    case 6:
      return EyeEmotion::Excited;
    default:
      return EyeEmotion::Neutral;
  }
}

const char* layoutName(KeyboardLayout layout) {
  return layout == KeyboardLayout::Ru ? "RU" : "EN";
}

String trimPreview(const String& text, size_t maxChars = kMaxStoredPreviewChars) {
  if (text.length() <= static_cast<int>(maxChars)) {
    return text;
  }
  return text.substring(0, maxChars) + "...";
}

size_t voiceNoteLimit() {
  return gSdReady ? kMaxSdVoiceNotes : kMaxFlashVoiceNotes;
}

String cleanReplyForDevice(const String& text) {
  String out = text;
  out.replace("[[audio_as_voice]]", "");
  out.replace("[[audio_as_text]]", "");
  out.replace("```", "");
  out.replace("**", "");
  out.replace("__", "");
  out.replace("~~", "");
  out.replace("`", "");
  out.replace("*", "");
  out.replace("_", "");
  out.replace("&nbsp;", " ");
  out.replace("&amp;", "&");
  out.replace("&lt;", "<");
  out.replace("&gt;", ">");
  out.replace("&quot;", "\"");
  out.replace("&#39;", "'");

  String linked;
  linked.reserve(out.length());
  for (int i = 0; i < out.length();) {
    bool image = out[i] == '!' && i + 1 < out.length() && out[i + 1] == '[';
    bool link = out[i] == '[' || image;
    if (link) {
      int textStart = i + (image ? 2 : 1);
      int textEnd = out.indexOf(']', textStart);
      int urlStart = textEnd >= 0 ? out.indexOf("](", textStart) : -1;
      int urlEnd = urlStart >= 0 ? out.indexOf(')', urlStart + 2) : -1;
      if (textEnd >= 0 && urlStart == textEnd && urlEnd >= 0) {
        linked += out.substring(textStart, textEnd);
        i = urlEnd + 1;
        continue;
      }
    }
    linked += out[i++];
  }
  out = linked;

  String lines;
  lines.reserve(out.length());
  int start = 0;
  while (start <= out.length()) {
    int end = out.indexOf('\n', start);
    if (end < 0) {
      end = out.length();
    }
    String line = out.substring(start, end);
    line.trim();
    while (line.startsWith("#")) {
      line = line.substring(1);
      line.trim();
    }
    while (line.startsWith(">")) {
      line = line.substring(1);
      line.trim();
    }
    if (line.startsWith("- ") || line.startsWith("+ ")) {
      line = "* " + line.substring(2);
    }
    if (!line.isEmpty()) {
      if (!lines.isEmpty()) {
        lines += " ";
      }
      lines += line;
    }
    if (end >= out.length()) {
      break;
    }
    start = end + 1;
  }
  out = lines;
  out.trim();
  return out;
}

void removeLastUtf8Char(String& value) {
  if (value.isEmpty()) {
    return;
  }
  int pos = value.length() - 1;
  while (pos > 0 && (static_cast<uint8_t>(value[pos]) & 0xC0) == 0x80) {
    --pos;
  }
  value.remove(pos);
}

void removeFirstUtf8Char(String& value) {
  if (value.isEmpty()) {
    return;
  }
  int pos = 1;
  while (pos < value.length() && (static_cast<uint8_t>(value[pos]) & 0xC0) == 0x80) {
    ++pos;
  }
  value.remove(0, pos);
}

