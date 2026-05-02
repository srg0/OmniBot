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
