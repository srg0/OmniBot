
void resetPet(bool fullReset) {
  petSetDefaults(fullReset);
  updatePetMood();
  setStatus(fullReset ? "Pet reset" : "Pet restored", 1200);
  appendRuntimeLog("PET", fullReset ? "reset full" : "reset stats", true);
  savePetState(true);
}

void revivePet(const char* reason) {
  bool wasDead = !gPet.alive || gPet.activity == PetActivity::Dead || gPet.mood == PetMood::Dead;
  gPet.alive = true;
  gPet.sick = false;
  gPet.hunger = max<uint8_t>(gPet.hunger, 68);
  gPet.happiness = max<uint8_t>(gPet.happiness, 64);
  gPet.health = max<uint8_t>(gPet.health, 76);
  gPet.energy = max<uint8_t>(gPet.energy, 72);
  gPet.cleanliness = max<uint8_t>(gPet.cleanliness, 82);
  gPet.discipline = max<uint8_t>(gPet.discipline, 42);
  gPet.poop = 0;
  gPet.careMistakes = 0;
  gPet.restPhase = PetRestPhase::None;
  gPet.env.scanPending = false;
  if (gPet.ageMinutes >= 3 && gPet.stage == PetStage::Egg) {
    gPet.stage = PetStage::Baby;
  }
  gPet.activity = PetActivity::Medicine;
  gPet.actionUntilMs = millis() + 3200;
  updatePetMood();
  focusTone(1319, 80);
  focusTone(1760, 90);
  setStatus(wasDead ? "Pet revived" : "Pet restored", 1400);
  appendRuntimeLog("PET", String("revived reason=") + (reason && reason[0] ? reason : "manual"), true);
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

bool applyPetCare(PetCareAction action) {
  if (!gPet.alive) {
    setStatus("Pet is gone. /petreset", 1600);
    return false;
  }
  gPet.lastInteractionMs = millis();
  switch (action) {
    case PetCareAction::Feed: {
      int before = gPet.hunger;
      gPet.hunger = petClampStat(gPet.hunger + 22);
      gPet.energy = petClampStat(gPet.energy - 4);
      gPet.happiness = petClampStat(gPet.happiness + 4);
      gPet.cleanliness = petClampStat(gPet.cleanliness - 2);
      if (before > 88) {
        gPet.health = petClampStat(gPet.health - 4);
        gPet.poop = min<uint8_t>(kPetMaxPoop, gPet.poop + 1);
        setStatus("Pet overfed", 1100);
      } else {
        if (before < 25) {
          gPet.health = petClampStat(gPet.health + 4);
        }
        if (random(0, 100) < 35 && gPet.poop < kPetMaxPoop) {
          ++gPet.poop;
        }
        setStatus("Pet fed", 900);
      }
      setPetActivity(PetActivity::Eating);
      focusTone(988, 70);
      break;
    }
    case PetCareAction::Play:
      if (gPet.energy < 10 || gPet.health < 15) {
        setStatus("Too tired to play", 1100);
        return false;
      }
      gPet.happiness = petClampStat(gPet.happiness + 18);
      gPet.energy = petClampStat(gPet.energy - 14);
      gPet.hunger = petClampStat(gPet.hunger - 8);
      gPet.cleanliness = petClampStat(gPet.cleanliness - 4);
      gPet.discipline = petClampStat(gPet.discipline + 2);
      setPetActivity(PetActivity::Playing);
      setStatus("Pet played", 900);
      focusTone(1175, 80);
      break;
    case PetCareAction::Clean:
      gPet.cleanliness = 100;
      gPet.poop = 0;
      gPet.health = petClampStat(gPet.health + 6);
      gPet.happiness = petClampStat(gPet.happiness + 2);
      setPetActivity(PetActivity::Cleaning);
      setStatus("Pet clean", 900);
      focusTone(1397, 70);
      break;
    case PetCareAction::Sleep:
      if (gPet.activity == PetActivity::Sleeping) {
        wakePet();
      } else {
        beginPetRest();
      }
      break;
    case PetCareAction::Medicine:
      if (!gPet.sick && gPet.health > 64) {
        setStatus("No medicine needed", 1000);
        return false;
      }
      gPet.health = petClampStat(gPet.health + 28);
      gPet.energy = petClampStat(gPet.energy - 3);
      gPet.happiness = petClampStat(gPet.happiness - 4);
      if (gPet.health > 42) {
        gPet.sick = false;
      }
      setPetActivity(PetActivity::Medicine);
      setStatus("Medicine given", 1000);
      focusTone(784, 100);
      break;
    case PetCareAction::Discipline: {
      bool fair = gPet.poop >= 3 || gPet.cleanliness < 30 || gPet.discipline < 20 || gPet.hunger > 92;
      gPet.discipline = petClampStat(gPet.discipline + (fair ? 16 : 5));
      gPet.happiness = petClampStat(gPet.happiness - (fair ? 5 : 12));
      if (!fair) {
        gPet.careMistakes = petClampStat(gPet.careMistakes + 1);
      }
      setPetActivity(PetActivity::Discipline);
      setStatus(fair ? "Discipline understood" : "Unfair discipline", 1200);
      focusTone(fair ? 880 : 262, 90);
      break;
    }
    case PetCareAction::Hunt:
      return startPetWifiScan(PetActivity::Hunting);
    case PetCareAction::Explore:
      return startPetWifiScan(PetActivity::Exploring);
  }
  updatePetMood();
  updatePetEvolution();
  maybePetDeath();
  savePetState(true);
  return true;
}
