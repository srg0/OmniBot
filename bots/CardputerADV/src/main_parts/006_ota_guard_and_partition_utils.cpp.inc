
String describeOtaPartition(const esp_partition_t* partition) {
  if (!partition) {
    return "none";
  }
  return String(partition->label) + "@0x" + String(static_cast<uint32_t>(partition->address), HEX) +
         "/" + String(static_cast<unsigned>(partition->size));
}

void otaLog(const String& message, bool persist = true) {
  appendRuntimeLog("OTA", message, persist);
  logf("[OTA] %s", message.c_str());
}

void clearOtaGuardPrefs() {
  gPrefs.begin(kPrefsOtaGuardNs, false);
  gPrefs.clear();
  gPrefs.end();
}

void recordOtaApplyIntent() {
  gPrefs.begin(kPrefsOtaGuardNs, false);
  gPrefs.putString("target", gOtaManifest.version);
  gPrefs.putString("sha", gOtaManifest.sha256);
  gPrefs.putString("from", kAppVersion);
  gPrefs.putString("git", kBuildGitSha);
  gPrefs.putUInt("started", millis());
  gPrefs.end();
}

void beginOtaBootGuard() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  gOtaRunningSlot = running ? String(running->label) : String("-");
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  esp_err_t err = running ? esp_ota_get_state_partition(running, &state) : ESP_ERR_NOT_FOUND;
  if (err == ESP_OK) {
    gOtaBootState = otaImageStateName(state);
  } else {
    gOtaBootState = "no-state";
  }

  gOtaPendingVerify = err == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
  gOtaConfirmedThisBoot = false;
  gOtaVerifyStartMs = millis();
  gOtaLastHealthProbeMs = 0;

  gPrefs.begin(kPrefsOtaGuardNs, false);
  String intendedTarget = gPrefs.getString("target", "");
  String intendedFrom = gPrefs.getString("from", "");
  gPrefs.end();
  if (!intendedTarget.isEmpty() && intendedTarget != kAppVersion && !gOtaPendingVerify) {
    appendRuntimeLog("OTA", "boot returned to " + String(kAppVersion) +
                              " after target=" + intendedTarget +
                              " from=" + intendedFrom +
                              " slot=" + gOtaRunningSlot +
                              " state=" + gOtaBootState, true);
    pushError("OTA did not switch to " + intendedTarget);
    setStatus("OTA stayed on " + String(kAppVersion), 1800);
    clearOtaGuardPrefs();
  }

  if (gOtaPendingVerify) {
    gPrefs.begin(kPrefsOtaGuardNs, false);
    uint32_t boots = gPrefs.getUInt("boots", 0) + 1;
    gPrefs.putUInt("boots", boots);
    gPrefs.putString("slot", gOtaRunningSlot);
    gPrefs.putString("boot_version", kAppVersion);
    gPrefs.end();
    appendRuntimeLog("OTA", "pending verify slot=" + gOtaRunningSlot +
                              " boot=" + String(boots), true);
    pushSystem("OTA verify: " + gOtaRunningSlot);
    setStatus("OTA verifying...", 1200);
  } else if (err == ESP_OK && state == ESP_OTA_IMG_VALID) {
    clearOtaGuardPrefs();
  }
  logf("[OTA] running=%s state=%s pending=%d", gOtaRunningSlot.c_str(),
       gOtaBootState.c_str(), static_cast<int>(gOtaPendingVerify));
}

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
