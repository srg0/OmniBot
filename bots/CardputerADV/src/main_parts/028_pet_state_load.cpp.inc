
uint8_t petCareAverage() {
  return static_cast<uint8_t>((gPet.hunger + gPet.happiness + gPet.health +
                               gPet.energy + gPet.cleanliness + gPet.discipline) / 6);
}

void petSetDefaults(bool fullReset) {
  uint32_t keepAge = fullReset ? 0 : gPet.ageMinutes;
  uint32_t keepBorn = fullReset ? petEpochNow() : gPet.bornEpoch;
  gPet = PetRuntime();
  gPet.hunger = 70;
  gPet.happiness = 70;
  gPet.health = 75;
  gPet.energy = 80;
  gPet.cleanliness = 80;
  gPet.discipline = 45;
  gPet.traitCuriosity = static_cast<uint8_t>(random(45, 91));
  gPet.traitActivity = static_cast<uint8_t>(random(35, 86));
  gPet.traitStress = static_cast<uint8_t>(random(20, 76));
  gPet.ageMinutes = keepAge;
  gPet.bornEpoch = keepBorn;
  gPet.lastSavedEpoch = petEpochNow();
  gPet.lastTickMs = millis();
  gPet.lastMinuteMs = millis();
  gPet.lastSaveMs = 0;
  gPet.nextDecisionMs = static_cast<uint32_t>(random(kPetDecisionMinMs, kPetDecisionMaxMs));
  gPet.stage = gPet.ageMinutes < 3 ? PetStage::Egg : PetStage::Baby;
  gPet.mood = PetMood::Calm;
  gPet.activity = PetActivity::Idle;
}

void savePetState(bool force) {
  uint32_t nowMs = millis();
  if (!force && nowMs - gPet.lastSaveMs < kPetAutoSaveMs) {
    return;
  }
  uint32_t nowEpoch = petEpochNow();
  gPrefs.begin(kPrefsPetNs, false);
  gPrefs.putUChar("magic", 0xC1);
  gPrefs.putUChar("hun", gPet.hunger);
  gPrefs.putUChar("hap", gPet.happiness);
  gPrefs.putUChar("hea", gPet.health);
  gPrefs.putUChar("ene", gPet.energy);
  gPrefs.putUChar("cln", gPet.cleanliness);
  gPrefs.putUChar("disc", gPet.discipline);
  gPrefs.putUChar("poop", gPet.poop);
  gPrefs.putUChar("mist", gPet.careMistakes);
  gPrefs.putUChar("cur", gPet.traitCuriosity);
  gPrefs.putUChar("act", gPet.traitActivity);
  gPrefs.putUChar("str", gPet.traitStress);
  gPrefs.putUInt("age", gPet.ageMinutes);
  gPrefs.putUInt("born", gPet.bornEpoch);
  gPrefs.putUInt("epoch", nowEpoch);
  gPrefs.putBool("alive", gPet.alive);
  gPrefs.putBool("sick", gPet.sick);
  gPrefs.putUChar("stage", static_cast<uint8_t>(gPet.stage));
  gPrefs.end();
  gPet.lastSavedEpoch = nowEpoch;
  gPet.lastSaveMs = nowMs;
}

void updatePetMood() {
  if (!gPet.alive) {
    gPet.mood = PetMood::Dead;
    gPet.activity = PetActivity::Dead;
    return;
  }
  if (gPet.activity == PetActivity::Sleeping) {
    gPet.mood = PetMood::Sleepy;
    return;
  }
  if (gPet.sick || gPet.health < 28) {
    gPet.mood = PetMood::Sick;
    return;
  }
  if (gPet.hunger < 25) {
    gPet.mood = PetMood::Hungry;
    return;
  }
  if (gPet.cleanliness < 25 || gPet.poop >= 4) {
    gPet.mood = PetMood::Dirty;
    return;
  }
  if (gPet.energy < 18) {
    gPet.mood = PetMood::Sleepy;
    return;
  }
  if (gPet.happiness < 30) {
    gPet.mood = PetMood::Bored;
    return;
  }
  if (gPet.env.hiddenCount > 0 || gPet.env.openCount > 0) {
    gPet.mood = PetMood::Curious;
    return;
  }
  uint8_t avg = petCareAverage();
  if (avg > 82 && gPet.env.netCount > 3) {
    gPet.mood = PetMood::Excited;
  } else if (avg > 64) {
    gPet.mood = PetMood::Happy;
  } else {
    gPet.mood = PetMood::Calm;
  }
}

void maybePetDeath() {
  if (!gPet.alive) {
    return;
  }
  bool collapse = gPet.health == 0 ||
                  (gPet.hunger == 0 && gPet.energy == 0 && gPet.cleanliness < 15) ||
                  gPet.careMistakes >= 20;
  if (!collapse) {
    return;
  }
  gPet.alive = false;
  gPet.sick = false;
  gPet.activity = PetActivity::Dead;
  gPet.mood = PetMood::Dead;
  gPet.restPhase = PetRestPhase::None;
  setStatus("Pet lost. /petreset", 2500);
  appendRuntimeLog("PET", "dead age=" + String(gPet.ageMinutes) +
                          " mistakes=" + String(gPet.careMistakes), true);
  savePetState(true);
}

void updatePetEvolution() {
  if (!gPet.alive || gPet.activity == PetActivity::Dead) {
    return;
  }
  PetStage target = gPet.stage;
  uint8_t avg = petCareAverage();
  if (gPet.ageMinutes >= 360 && avg >= 42) {
    target = PetStage::Elder;
  } else if (gPet.ageMinutes >= 120 && avg >= 40) {
    target = PetStage::Adult;
  } else if (gPet.ageMinutes >= 30 && avg >= 35) {
    target = PetStage::Teen;
  } else if (gPet.ageMinutes >= 3) {
    target = PetStage::Baby;
  } else {
    target = PetStage::Egg;
  }
  if (static_cast<uint8_t>(target) <= static_cast<uint8_t>(gPet.stage)) {
    return;
  }
  gPet.stage = target;
  gPet.activity = PetActivity::Evolving;
  gPet.actionUntilMs = millis() + 4500;
  setStatus(String("Pet evolved: ") + petStageName(target), 1600);
  focusTone(1568, 120);
  appendRuntimeLog("PET", String("evolved ") + petStageName(target), true);
  savePetState(true);
}

void applyPetElapsedMinute(bool offline) {
  if (!gPet.alive) {
    return;
  }
  ++gPet.ageMinutes;

  bool sleeping = gPet.activity == PetActivity::Sleeping;
  if (sleeping) {
    gPet.hunger = petClampStat(gPet.hunger - 1);
    gPet.happiness = petClampStat(gPet.happiness - 1);
    gPet.energy = petClampStat(gPet.energy + 8);
    gPet.cleanliness = petClampStat(gPet.cleanliness - 1);
    if (gPet.hunger > 20 && gPet.cleanliness > 25) {
      gPet.health = petClampStat(gPet.health + 2);
    }
  } else {
    uint8_t happyDrain = (gPet.env.lastScanMs == 0 || millis() - gPet.env.lastScanMs > 180000) ? 2 : 1;
    gPet.hunger = petClampStat(gPet.hunger - 2);
    gPet.happiness = petClampStat(gPet.happiness - happyDrain);
    gPet.energy = petClampStat(gPet.energy - 1);
    gPet.cleanliness = petClampStat(gPet.cleanliness - 1);
  }

  if ((gPet.ageMinutes % 9UL) == 0 && gPet.poop < kPetMaxPoop) {
    uint8_t chance = offline ? 25 : static_cast<uint8_t>(12 + max<int>(0, gPet.hunger - 65) / 3);
    if (random(0, 100) < chance) {
      ++gPet.poop;
      gPet.cleanliness = petClampStat(gPet.cleanliness - 10);
    }
  }
  if ((gPet.ageMinutes % 10UL) == 0 && gPet.discipline > 0) {
    gPet.discipline = petClampStat(gPet.discipline - 1);
  }

  if (gPet.hunger < 18 || gPet.cleanliness < 18 || gPet.poop >= 4) {
    gPet.health = petClampStat(gPet.health - 3);
  } else if (gPet.happiness < 18 || gPet.energy < 10) {
    gPet.health = petClampStat(gPet.health - 1);
  } else if (!sleeping && gPet.health < 92 && gPet.hunger > 55 && gPet.cleanliness > 55) {
    gPet.health = petClampStat(gPet.health + 1);
  }

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

  if (!gPet.alive) {
    // A dead persisted pet otherwise blocks every care action. Revive once on
    // boot/update, preserving age and stage so the companion is recoverable.
    revivePet("boot");
    return;
  }

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
