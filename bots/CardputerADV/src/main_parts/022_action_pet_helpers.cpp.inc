
bool actionLocalPathAllowed(const String& path) {
  return path.startsWith("/audio/") || path.startsWith("/pomodoro/audio/") ||
         path.startsWith("/voice/") || path.startsWith("/reply/");
}

bool textMatchesQuery(const String& value, const String& query) {
  if (query.isEmpty()) {
    return false;
  }
  String hay = value;
  String needle = query;
  hay.toLowerCase();
  needle.toLowerCase();
  return hay.indexOf(needle) >= 0;
}

bool playAudioAssetByAction(JsonObjectConst action) {
  if (gAudioAssets.empty()) {
    scanAudioAssets();
  }
  if (gAudioAssets.empty()) {
    setStatus("No audio assets", 1200);
    return false;
  }

  int index = -1;
  if (!action["index"].isNull()) {
    int requested = actionInt(action, "index", 0, 0, static_cast<int>(gAudioAssets.size()));
    index = requested > 0 ? requested - 1 : 0;
  }
  String path = action["path"] | "";
  if (index < 0 && !path.isEmpty() && actionLocalPathAllowed(path)) {
    for (int i = 0; i < static_cast<int>(gAudioAssets.size()); ++i) {
      if (gAudioAssets[i].path == path) {
        index = i;
        break;
      }
    }
  }
  String query = action["query"] | "";
  if (index < 0 && !query.isEmpty()) {
    for (int i = 0; i < static_cast<int>(gAudioAssets.size()); ++i) {
      const auto& asset = gAudioAssets[i];
      if (textMatchesQuery(asset.title, query) || textMatchesQuery(asset.path, query) ||
          textMatchesQuery(asset.category, query)) {
        index = i;
        break;
      }
    }
  }
  if (index < 0 || index >= static_cast<int>(gAudioAssets.size())) {
    setStatus("Audio not found", 1200);
    return false;
  }
  gSelectedAudioAsset = index;
  setUiMode(UiMode::Library);
  return startPlaybackStreamFromWavFile(gAudioAssets[index].path, String("asset: ") + gAudioAssets[index].title);
}

bool playVoiceNoteByAction(JsonObjectConst action) {
  if (gVoiceNotes.empty()) {
    loadVoiceNotes();
  }
  if (gVoiceNotes.empty()) {
    setStatus("No voice notes", 1200);
    return false;
  }
  int index = -1;
  if (!action["index"].isNull()) {
    int requested = actionInt(action, "index", 0, 0, static_cast<int>(gVoiceNotes.size()));
    index = requested > 0 ? requested - 1 : 0;
  }
  String query = action["query"] | "";
  if (index < 0 && !query.isEmpty()) {
    for (int i = 0; i < static_cast<int>(gVoiceNotes.size()); ++i) {
      const auto& note = gVoiceNotes[i];
      if (textMatchesQuery(note.title, query) || textMatchesQuery(note.preview, query) ||
          textMatchesQuery(note.audioPath, query)) {
        index = i;
        break;
      }
    }
  }
  if (index < 0 || index >= static_cast<int>(gVoiceNotes.size())) {
    setStatus("Voice note not found", 1200);
    return false;
  }
  gSelectedVoiceNote = index;
  setUiMode(UiMode::Voice);
  return playVoiceNoteAt(index);
}

bool openTopicByAction(JsonObjectConst action) {
  if (gContexts.empty()) {
    startTopicTask(true, false, true, "Syncing topics...");
    setStatus("Topics syncing", 1200);
    return false;
  }
  int index = -1;
  if (!action["index"].isNull()) {
    int requested = actionInt(action, "index", 0, 0, static_cast<int>(gContexts.size()));
    index = requested > 0 ? requested - 1 : 0;
  }
  String key = action["key"] | "";
  String query = action["query"] | "";
  if (index < 0 && !key.isEmpty()) {
    for (int i = 0; i < static_cast<int>(gContexts.size()); ++i) {
      if (gContexts[i].key == key || gContexts[i].topicKey == key) {
        index = i;
        break;
      }
    }
  }
  if (index < 0 && !query.isEmpty()) {
    for (int i = 0; i < static_cast<int>(gContexts.size()); ++i) {
      if (textMatchesQuery(gContexts[i].label, query) || textMatchesQuery(gContexts[i].shortName, query)) {
        index = i;
        break;
      }
    }
  }
  if (index < 0 || index >= static_cast<int>(gContexts.size())) {
    setStatus("Topic not found", 1200);
    return false;
  }
  gSelectedContext = index;
  saveCurrentContext();
  showTopicOverlay(0, 1800);
  setUiMode(UiMode::ChatFace);
  startTopicTask(false, true, true, "Topic loading...");
  return true;
}

String normalizePetActionText(String value) {
  value.trim();
  value.toLowerCase();
  value.replace("-", ".");
  value.replace("_", ".");
  if (value.startsWith("pet.")) {
    value = value.substring(4);
  } else if (value.startsWith("tama.")) {
    value = value.substring(5);
  } else if (value.startsWith("tamagotchi.")) {
    value = value.substring(11);
  }
  return value;
}

bool executePetActionByName(String actionName) {
  String op = normalizePetActionText(actionName);
  bool executed = false;
  if (op == "open" || op == "status" || op == "show") {
    executed = true;
    setStatus("Pet screen", 900);
  } else if (op == "feed" || op == "food" || op == "eat") {
    executed = applyPetCare(PetCareAction::Feed);
  } else if (op == "play" || op == "fun") {
    executed = applyPetCare(PetCareAction::Play);
  } else if (op == "clean" || op == "wash" || op == "bath") {
    executed = applyPetCare(PetCareAction::Clean);
  } else if (op == "sleep" || op == "rest") {
    executed = applyPetCare(PetCareAction::Sleep);
  } else if (op == "medicine" || op == "heal" || op == "doctor") {
    executed = applyPetCare(PetCareAction::Medicine);
  } else if (op == "discipline" || op == "train" || op == "scold") {
    executed = applyPetCare(PetCareAction::Discipline);
  } else if (op == "hunt" || op == "wifi" || op == "forage") {
    executed = applyPetCare(PetCareAction::Hunt);
  } else if (op == "explore" || op == "scan") {
    executed = applyPetCare(PetCareAction::Explore);
  } else if (op == "revive" || op == "restore" || op == "resurrect") {
    revivePet("action");
    executed = true;
  } else if (op == "reset" || op == "new") {
    resetPet(true);
    executed = true;
  }
  if (executed) {
    setUiMode(UiMode::Hero);
  }
  return executed;
}

