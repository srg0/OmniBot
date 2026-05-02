
void servicePetRest() {
  if (gPet.activity != PetActivity::Sleeping || gPet.restPhase == PetRestPhase::None) {
    return;
  }
  uint32_t now = millis();
  if (gPet.restPhase == PetRestPhase::Enter && now - gPet.restPhaseStartMs > 1600) {
    gPet.restPhase = PetRestPhase::Deep;
    gPet.restPhaseStartMs = now;
    gPet.restStatsApplied = false;
    return;
  }
  if (gPet.restPhase == PetRestPhase::Deep) {
    if (!gPet.restStatsApplied && now - gPet.restPhaseStartMs > gPet.restDurationMs / 2) {
      gPet.happiness = petClampStat(gPet.happiness + 10);
      gPet.health = petClampStat(gPet.health + 15);
      gPet.energy = petClampStat(gPet.energy + 22);
      gPet.hunger = petClampStat(gPet.hunger - 3);
      gPet.restStatsApplied = true;
      savePetState(true);
    }
    if (now - gPet.restPhaseStartMs >= gPet.restDurationMs || gPet.energy >= 100) {
      gPet.restPhase = PetRestPhase::Wake;
      gPet.restPhaseStartMs = now;
      focusTone(659, 80);
    }
    return;
  }
  if (gPet.restPhase == PetRestPhase::Wake && now - gPet.restPhaseStartMs > 1600) {
    wakePet();
    savePetState(true);
  }
}

void decideNextPetActivity() {
  if (!gPet.alive || gPet.env.scanPending || gPet.activity != PetActivity::Idle ||
      gUiMode != UiMode::Hero || gRecording || gSubmitting || gThinking || gPlaybackActive) {
    return;
  }
  uint32_t now = millis();
  if (now - gPet.lastDecisionMs < gPet.nextDecisionMs) {
    return;
  }
  gPet.lastDecisionMs = now;
  gPet.nextDecisionMs = static_cast<uint32_t>(random(kPetDecisionMinMs, kPetDecisionMaxMs));

  int desireHunt = (100 - gPet.hunger) + gPet.traitCuriosity / 3;
  int desireExplore = gPet.traitCuriosity + gPet.env.hiddenCount * 7 + gPet.env.openCount * 5 + random(0, 18);
  int desireRest = (100 - gPet.energy) + (100 - gPet.health) + gPet.traitStress / 2;
  if (gPet.mood == PetMood::Hungry) {
    desireHunt += 24;
  } else if (gPet.mood == PetMood::Curious || gPet.mood == PetMood::Bored) {
    desireExplore += 20;
  } else if (gPet.mood == PetMood::Sick || gPet.mood == PetMood::Sleepy) {
    desireRest += 22;
  }
  if (gPet.hunger < 15) {
    desireRest -= 12;
  }

  int best = 20;
  PetActivity chosen = PetActivity::Idle;
  if (desireHunt > best) {
    best = desireHunt;
    chosen = PetActivity::Hunting;
  }
  if (desireExplore > best) {
    best = desireExplore;
    chosen = PetActivity::Exploring;
  }
  if (desireRest > best) {
    chosen = PetActivity::Sleeping;
  }

  if (chosen == PetActivity::Hunting) {
    startPetWifiScan(PetActivity::Hunting);
  } else if (chosen == PetActivity::Exploring) {
    startPetWifiScan(PetActivity::Exploring);
  } else if (chosen == PetActivity::Sleeping) {
    beginPetRest();
  }
}

void servicePet() {
  uint32_t now = millis();
  if (gPet.lastTickMs == 0) {
    gPet.lastTickMs = now;
    gPet.lastMinuteMs = now;
  }
  if (now - gPet.lastTickMs < kPetTickMs) {
    servicePetWifiScan();
    servicePetRest();
    return;
  }
  gPet.lastTickMs = now;

  servicePetWifiScan();
  servicePetRest();

  uint8_t applied = 0;
  while (now - gPet.lastMinuteMs >= kPetMinuteMs && applied < 5) {
    gPet.lastMinuteMs += kPetMinuteMs;
    applyPetElapsedMinute(false);
    ++applied;
  }

  if (gPet.actionUntilMs != 0 && now > gPet.actionUntilMs &&
      gPet.activity != PetActivity::Sleeping && !gPet.env.scanPending) {
    gPet.activity = gPet.alive ? PetActivity::Idle : PetActivity::Dead;
    gPet.actionUntilMs = 0;
  }
  updatePetMood();
  decideNextPetActivity();
  savePetState(false);
}

PetCareAction suggestedPetAction() {
  if (gPet.sick || gPet.health < 35) {
    return PetCareAction::Medicine;
  }
  if (gPet.hunger < 35) {
    return PetCareAction::Feed;
  }
  if (gPet.cleanliness < 45 || gPet.poop >= 3) {
    return PetCareAction::Clean;
  }
  if (gPet.energy < 28) {
    return PetCareAction::Sleep;
  }
  if (gPet.happiness < 55) {
    return PetCareAction::Play;
  }
  return PetCareAction::Explore;
}

bool handlePetKey(char key, const Keyboard_Class::KeysState& keys) {
  char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));
  if (lowerKey == 'q') {
    gPetHelpVisible = !gPetHelpVisible;
    setStatus(gPetHelpVisible ? "Pet controls" : "Pet clean", 700);
    return true;
  }
  if (gPetHelpVisible) {
    return true;
  }
  if (keys.enter || keys.space || key == '\n' || key == '\r' || key == ' ') {
    if (!gPet.alive) {
      revivePet("enter");
      return true;
    }
    return applyPetCare(suggestedPetAction());
  }
  if (lowerKey == 'f') {
    return applyPetCare(PetCareAction::Feed);
  }
  if (lowerKey == 'p') {
    return applyPetCare(PetCareAction::Play);
  }
  if (lowerKey == 'c') {
    return applyPetCare(PetCareAction::Clean);
  }
  if (lowerKey == 's') {
    return applyPetCare(PetCareAction::Sleep);
  }
  if (lowerKey == 'm') {
    return applyPetCare(PetCareAction::Medicine);
  }
  if (lowerKey == 'd') {
    return applyPetCare(PetCareAction::Discipline);
  }
  if (lowerKey == 'h') {
    return applyPetCare(PetCareAction::Hunt);
  }
  if (lowerKey == 'e') {
    return applyPetCare(PetCareAction::Explore);
  }
  if (lowerKey == 'r') {
    updatePetMood();
    setStatus(String(petMoodName(gPet.mood)) + " " + petAgeText(), 1000);
    return true;
  }
  return false;
}

bool copyFileBetweenFs(fs::FS& fromFs, fs::FS& toFs, const String& path) {
  if (!fsExists(fromFs, path)) {
    return false;
  }
  if (fsExists(toFs, path)) {
    return true;
  }
  File src = fromFs.open(path, "r");
  if (!src) {
    return false;
  }
  File dst = toFs.open(path, "w");
  if (!dst) {
    src.close();
    return false;
  }
  uint8_t buffer[1024];
  bool ok = true;
  while (src.available()) {
    size_t readBytes = src.read(buffer, sizeof(buffer));
    if (readBytes == 0) {
      break;
    }
    if (dst.write(buffer, readBytes) != readBytes) {
      ok = false;
      break;
    }
  }
  src.close();
  dst.close();
  if (!ok) {
    fsRemove(toFs, path);
  }
  return ok;
}

void migrateLittleFsVoiceNotesToSd() {
  if (!gSdReady || !gLittleFsReady || !ensureVoiceStorageOn(SD) || !LittleFS.exists(kVoiceDir)) {
    return;
  }
  File dir = LittleFS.open(kVoiceDir);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  uint16_t copied = 0;
  while (true) {
    File file = dir.openNextFile();
    if (!file) {
      break;
    }
    String path = file.path();
    bool regular = !file.isDirectory();
    file.close();
    if (!regular || path.indexOf("__active_") >= 0 ||
        (!path.endsWith(kVoiceMetaExt) && !path.endsWith(kVoiceAudioExt))) {
      continue;
    }
    if (copyFileBetweenFs(LittleFS, SD, path)) {
      ++copied;
    }
  }
  if (copied > 0) {
    logf("[SD] migrated %u voice files from LittleFS", static_cast<unsigned>(copied));
  }
}
