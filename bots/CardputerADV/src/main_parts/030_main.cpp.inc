  if (gPet.health < 25) {
    gPet.sick = true;
  }
  if (gPet.hunger == 0 || gPet.cleanliness == 0 || gPet.poop >= kPetMaxPoop) {
    gPet.careMistakes = petClampStat(gPet.careMistakes + 1);
  }
  if (gPet.energy >= 100 && sleeping && gPet.restPhase == PetRestPhase::Deep) {
    gPet.restPhase = PetRestPhase::Wake;
    gPet.restPhaseStartMs = millis();
  }
  updatePetMood();
  updatePetEvolution();
  maybePetDeath();
}

void applyPetElapsedMinutes(uint32_t minutes, bool offline) {
  minutes = min<uint32_t>(minutes, kPetOfflineMaxMinutes);
  for (uint32_t i = 0; i < minutes; ++i) {
    applyPetElapsedMinute(offline);
    if (!gPet.alive) {
      break;
    }
  }
}

void loadPetState() {
  gPrefs.begin(kPrefsPetNs, true);
  uint8_t magic = gPrefs.getUChar("magic", 0);
  if (magic != 0xC1) {
    gPrefs.end();
    petSetDefaults(true);
    savePetState(true);
    appendRuntimeLog("PET", "new pet created", true);
    return;
  }
  gPet.hunger = gPrefs.getUChar("hun", 70);
  gPet.happiness = gPrefs.getUChar("hap", 70);
  gPet.health = gPrefs.getUChar("hea", 75);
  gPet.energy = gPrefs.getUChar("ene", 80);
  gPet.cleanliness = gPrefs.getUChar("cln", 80);
  gPet.discipline = gPrefs.getUChar("disc", 45);
  gPet.poop = min<uint8_t>(gPrefs.getUChar("poop", 0), kPetMaxPoop);
  gPet.careMistakes = gPrefs.getUChar("mist", 0);
  gPet.traitCuriosity = gPrefs.getUChar("cur", 70);
  gPet.traitActivity = gPrefs.getUChar("act", 60);
  gPet.traitStress = gPrefs.getUChar("str", 40);
  gPet.ageMinutes = gPrefs.getUInt("age", 0);
  gPet.bornEpoch = gPrefs.getUInt("born", 0);
  uint32_t savedEpoch = gPrefs.getUInt("epoch", 0);
  gPet.lastSavedEpoch = savedEpoch;
  gPet.alive = gPrefs.getBool("alive", true);
  gPet.sick = gPrefs.getBool("sick", false);
  uint8_t stageRaw = clampValue<uint8_t>(gPrefs.getUChar("stage", 0), 0, 4);
  gPet.stage = static_cast<PetStage>(stageRaw);
  gPrefs.end();

  gPet.lastTickMs = millis();
  gPet.lastMinuteMs = millis();
  gPet.lastSaveMs = millis();
  gPet.activity = gPet.alive ? PetActivity::Idle : PetActivity::Dead;
  gPet.restPhase = PetRestPhase::None;
  gPet.nextDecisionMs = static_cast<uint32_t>(random(kPetDecisionMinMs, kPetDecisionMaxMs));

  uint32_t nowEpoch = petEpochNow();
  if (nowEpoch > savedEpoch + 90 && savedEpoch > 0) {
    uint32_t offlineMinutes = (nowEpoch - savedEpoch) / 60UL;
    applyPetElapsedMinutes(offlineMinutes, true);
    appendRuntimeLog("PET", "offline minutes=" + String(min<uint32_t>(offlineMinutes, kPetOfflineMaxMinutes)), true);
    savePetState(true);
  } else {
    updatePetMood();
    updatePetEvolution();
  }
}

void resetPet(bool fullReset) {
  petSetDefaults(fullReset);
  updatePetMood();
  setStatus(fullReset ? "Pet reset" : "Pet restored", 1200);
  appendRuntimeLog("PET", fullReset ? "reset full" : "reset stats", true);
  savePetState(true);
}

void setPetActivity(PetActivity activity, uint32_t ttlMs = kPetActionMs) {
  gPet.activity = activity;
  gPet.actionUntilMs = ttlMs > 0 ? millis() + ttlMs : 0;
  if (activity != PetActivity::Sleeping) {
    gPet.restPhase = PetRestPhase::None;
  }
}

void beginPetRest() {
  gPet.activity = PetActivity::Sleeping;
  gPet.restPhase = PetRestPhase::Enter;
  gPet.restPhaseStartMs = millis();
  gPet.restDurationMs = static_cast<uint32_t>(random(kPetRestMinMs, kPetRestMaxMs));
  gPet.restStatsApplied = false;
  gPet.actionUntilMs = 0;
  setStatus("Pet sleeping", 1000);
}

void wakePet() {
  gPet.activity = PetActivity::Idle;
  gPet.restPhase = PetRestPhase::None;
  gPet.energy = petClampStat(gPet.energy + 4);
  setStatus("Pet awake", 900);
}

void useConnectedWifiAsPetEnv() {
  gPet.env.netCount = WiFi.status() == WL_CONNECTED ? 1 : 0;
  gPet.env.hiddenCount = 0;
  gPet.env.openCount = 0;
  int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -100;
  gPet.env.strongCount = rssi > -67 ? 1 : 0;
  gPet.env.avgRssi = static_cast<int16_t>(rssi);
  gPet.env.lastScanMs = millis();
  gPet.env.scanFresh = false;
}

void resolvePetHunt() {
  if (gPet.env.netCount <= 0) {
    gPet.hunger = petClampStat(gPet.hunger - 10);
    gPet.happiness = petClampStat(gPet.happiness - 6);
    gPet.health = petClampStat(gPet.health - 4);
    setStatus("Hunt found nothing", 1200);
    focusTone(220, 120);
  } else {
    int hungerDelta = min<int>(34, gPet.env.netCount * 2 + gPet.env.strongCount * 4);
    int variety = gPet.env.hiddenCount * 2 + gPet.env.openCount;
    int happyDelta = min<int>(26, variety * 3 + max<int>(0, gPet.env.avgRssi + 95) / 3);
    int healthDelta = (gPet.env.avgRssi > -75 ? 4 : 0) + (gPet.env.avgRssi > -63 ? 4 : 0);
    gPet.hunger = petClampStat(gPet.hunger + hungerDelta);
    gPet.happiness = petClampStat(gPet.happiness + happyDelta);
    gPet.health = petClampStat(gPet.health + healthDelta);
    gPet.energy = petClampStat(gPet.energy - 4);
    setStatus("Hunt +" + String(hungerDelta) + " food", 1200);
    focusTone(1047, 90);
  }
  setPetActivity(PetActivity::Eating, 3800);
  updatePetMood();
  savePetState(true);
}

void resolvePetExplore() {
  if (gPet.env.netCount <= 0) {
    gPet.happiness = petClampStat(gPet.happiness - 5);
    gPet.hunger = petClampStat(gPet.hunger - 3);
    setStatus("Explore quiet", 1000);
    focusTone(330, 90);
  } else {
    int curiosity = gPet.env.hiddenCount * 4 + gPet.env.openCount * 3 + gPet.env.netCount;
    int happyDelta = min<int>(32, curiosity / 2);
    gPet.happiness = petClampStat(gPet.happiness + happyDelta);
    gPet.hunger = petClampStat(gPet.hunger - 5);
    gPet.energy = petClampStat(gPet.energy - 5);
    setStatus("Explore +" + String(happyDelta) + " fun", 1000);
    focusTone(1319, 80);
  }
  setPetActivity(PetActivity::Exploring, 3600);
  updatePetMood();
  savePetState(true);
}

bool startPetWifiScan(PetActivity activity) {
  if (gPet.env.scanPending) {
    setStatus("Pet scan running", 700);
    return false;
  }
  if (!gPet.alive) {
    setStatus("Pet is gone", 1200);
    return false;
  }
  gPet.activity = activity;
  gPet.actionUntilMs = 0;
  gPet.env.scanPending = true;
  gPet.env.lastScanMs = millis();
  WiFi.scanDelete();
  int started = WiFi.scanNetworks(true, true);
  if (started == -2) {
    gPet.env.scanPending = false;
    useConnectedWifiAsPetEnv();
    if (activity == PetActivity::Hunting) {
      resolvePetHunt();
    } else {
      resolvePetExplore();
    }
    return true;
  }
  setStatus(activity == PetActivity::Hunting ? "Pet hunting Wi-Fi" : "Pet exploring", 1000);
  return true;
}

void completePetWifiScan(int count) {
  if (count < 0) {
    useConnectedWifiAsPetEnv();
  } else {
    int totalRssi = 0;
    int seen = min<int>(count, 48);
    gPet.env.netCount = seen;
    gPet.env.hiddenCount = 0;
    gPet.env.openCount = 0;
    gPet.env.strongCount = 0;
    for (int i = 0; i < seen; ++i) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      totalRssi += rssi;
      if (ssid.isEmpty()) {
        ++gPet.env.hiddenCount;
      }
      if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
        ++gPet.env.openCount;
      }
      if (rssi > -67) {
        ++gPet.env.strongCount;
      }
    }
    gPet.env.avgRssi = seen > 0 ? static_cast<int16_t>(totalRssi / seen) : -100;
    gPet.env.lastScanMs = millis();
    gPet.env.scanFresh = true;
    WiFi.scanDelete();
  }
  gPet.env.scanPending = false;
  if (gPet.activity == PetActivity::Hunting) {
    resolvePetHunt();
  } else if (gPet.activity == PetActivity::Exploring) {
    resolvePetExplore();
  }
}

void servicePetWifiScan() {
  if (!gPet.env.scanPending) {
    return;
  }
  int count = WiFi.scanComplete();
  if (count == -1 && millis() - gPet.env.lastScanMs < 16000) {
    return;
  }
  completePetWifiScan(count >= 0 ? count : -1);
}

