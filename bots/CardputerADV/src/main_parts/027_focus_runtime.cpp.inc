
void saveFocusSettings() {
  gPrefs.begin(kPrefsFocusNs, false);
  gPrefs.putUInt("focus", gFocusSettings.focusSec);
  gPrefs.putUInt("short", gFocusSettings.shortBreakSec);
  gPrefs.putUInt("long", gFocusSettings.longBreakSec);
  gPrefs.putUChar("cycles", gFocusSettings.cyclesPerRound);
  gPrefs.putBool("auto", gFocusSettings.autoStart);
  gPrefs.putBool("metro", gFocusSettings.metronome);
  gPrefs.putUShort("bpm", gFocusSettings.bpm);
  gPrefs.end();
}

void focusEnterState(FocusState state, bool resetTimer = true) {
  gFocus.state = state;
  if (state != FocusState::Paused && state != FocusState::Stopped) {
    gFocus.stateBeforePause = state;
  }
  if (resetTimer) {
    gFocus.sessionTotalSec = focusDurationForState(state);
    gFocus.remainingSec = gFocus.sessionTotalSec;
  }
  gFocus.lastTickMs = millis();
  gFocus.nextMetronomeMs = millis() + 100;
  setStatus(String("Focus ") + focusStateName(state), 1000);
  if (!playFocusCue(state)) {
    if (state == FocusState::Focus) {
      focusTone(1047, 120);
    } else if (state == FocusState::Reflect) {
      focusTone(659, 160);
    } else if (state == FocusState::ShortBreak || state == FocusState::LongBreak) {
      focusTone(880, 120);
    }
  }
  logf("[FOCUS] state=%s remain=%u cycle=%u/%u",
       focusStateName(state),
       static_cast<unsigned>(gFocus.remainingSec),
       static_cast<unsigned>(gFocus.cycle),
       static_cast<unsigned>(gFocusSettings.cyclesPerRound));
}

void focusStartOrResume() {
  if (gFocus.state == FocusState::Paused) {
    focusEnterState(gFocus.stateBeforePause, false);
    return;
  }
  if (gFocus.state == FocusState::Stopped) {
    if (gFocus.remainingSec == 0) {
      gFocus.remainingSec = gFocusSettings.focusSec;
    }
    gFocus.sessionTotalSec = gFocus.remainingSec;
    focusEnterState(FocusState::Focus, false);
    return;
  }
  if (focusStateRuns(gFocus.state)) {
    gFocus.stateBeforePause = gFocus.state;
    gFocus.state = FocusState::Paused;
    setStatus("Focus paused", 1000);
    focusTone(523, 90);
  }
}

void focusReset() {
  gFocus.state = FocusState::Stopped;
  gFocus.stateBeforePause = FocusState::Focus;
  gFocus.remainingSec = gFocusSettings.focusSec;
  gFocus.sessionTotalSec = gFocusSettings.focusSec;
  gFocus.lastTickMs = millis();
  setStatus("Focus reset", 1000);
  focusTone(440, 90);
}

void focusSnooze(uint32_t seconds = kFocusSnoozeSec) {
  gFocus.remainingSec = min<uint32_t>(gFocus.remainingSec + seconds, kFocusMaxDurationSec);
  gFocus.sessionTotalSec = max<uint32_t>(gFocus.sessionTotalSec, gFocus.remainingSec);
  setStatus("+5 min", 900);
}

void focusAdvance() {
  FocusState ended = gFocus.state;
  if (ended == FocusState::Focus) {
    ++gFocus.completedToday;
    gFocus.focusedTodaySec += gFocus.sessionTotalSec;
    if ((gFocus.cycle % gFocusSettings.cyclesPerRound) == 0) {
      focusEnterState(FocusState::LongBreak);
    } else {
      focusEnterState(FocusState::ShortBreak);
    }
    return;
  }
  if (ended == FocusState::ShortBreak) {
    ++gFocus.cycle;
    if (gFocusSettings.autoStart) {
      focusEnterState(FocusState::Focus);
    } else {
      gFocus.state = FocusState::Stopped;
      gFocus.remainingSec = gFocusSettings.focusSec;
      gFocus.sessionTotalSec = gFocusSettings.focusSec;
      setStatus("Next focus ready", 1200);
    }
    return;
  }
  if (ended == FocusState::LongBreak) {
    focusEnterState(FocusState::Reflect);
    return;
  }
  if (ended == FocusState::Reflect) {
    gFocus.cycle = 1;
    if (gFocusSettings.autoStart) {
      focusEnterState(FocusState::Focus);
    } else {
      gFocus.state = FocusState::Stopped;
      gFocus.remainingSec = gFocusSettings.focusSec;
      gFocus.sessionTotalSec = gFocusSettings.focusSec;
      setStatus("Round complete", 1400);
    }
  }
}

void updateFocusTimer() {
  if (!focusStateRuns(gFocus.state)) {
    return;
  }
  uint32_t now = millis();
  if (now < gFocus.lastTickMs) {
    gFocus.lastTickMs = now;
    return;
  }
  uint32_t elapsedSec = (now - gFocus.lastTickMs) / 1000;
  if (elapsedSec == 0) {
    return;
  }
  gFocus.lastTickMs += elapsedSec * 1000;
  if (elapsedSec < gFocus.remainingSec) {
    gFocus.remainingSec -= elapsedSec;
    return;
  }
  gFocus.remainingSec = 0;
  focusAdvance();
}

void serviceFocusMetronome() {
  if (!gFocusSettings.metronome || gFocus.state != FocusState::Focus || gRecording ||
      gPlaybackActive || gSubmitting || gThinking) {
    return;
  }
  uint32_t now = millis();
  if (now < gFocus.nextMetronomeMs) {
    return;
  }
  uint32_t beatMs = 60000UL / max<uint16_t>(1, gFocusSettings.bpm);
  gFocus.nextMetronomeMs = now + beatMs;
  focusTone(660, 35);
}

uint8_t petClampStat(int value) {
  return static_cast<uint8_t>(clampValue<int>(value, 0, 100));
}

uint32_t petEpochNow() {
  if (!hasValidSystemTime()) {
    return 0;
  }
  time_t now = time(nullptr);
  return now > 0 ? static_cast<uint32_t>(now) : 0;
}

const char* petStageName(PetStage stage) {
  switch (stage) {
    case PetStage::Egg:
      return "EGG";
    case PetStage::Baby:
      return "BABY";
    case PetStage::Teen:
      return "TEEN";
    case PetStage::Adult:
      return "ADULT";
    case PetStage::Elder:
    default:
      return "ELDER";
  }
}

const char* petMoodName(PetMood mood) {
  switch (mood) {
    case PetMood::Happy:
      return "HAPPY";
    case PetMood::Hungry:
      return "HUNGRY";
    case PetMood::Bored:
      return "BORED";
    case PetMood::Curious:
      return "CURIOUS";
    case PetMood::Sick:
      return "SICK";
    case PetMood::Excited:
      return "EXCITED";
    case PetMood::Sleepy:
      return "SLEEPY";
    case PetMood::Dirty:
      return "DIRTY";
    case PetMood::Dead:
      return "DEAD";
    case PetMood::Calm:
    default:
      return "CALM";
  }
}

const char* petActivityName(PetActivity activity) {
  switch (activity) {
    case PetActivity::Eating:
      return "EATING";
    case PetActivity::Playing:
      return "PLAY";
    case PetActivity::Cleaning:
      return "CLEAN";
    case PetActivity::Sleeping:
      return "SLEEP";
    case PetActivity::Hunting:
      return "HUNT";
    case PetActivity::Exploring:
      return "EXPLORE";
    case PetActivity::Medicine:
      return "MED";
    case PetActivity::Discipline:
      return "DISC";
    case PetActivity::Evolving:
      return "EVOLVE";
    case PetActivity::Dead:
      return "DEAD";
    case PetActivity::Idle:
    default:
      return "IDLE";
  }
}

String petAgeText() {
  uint32_t days = gPet.ageMinutes / 1440UL;
  uint32_t hours = (gPet.ageMinutes / 60UL) % 24UL;
  uint32_t minutes = gPet.ageMinutes % 60UL;
  if (days > 0) {
    return String(days) + "d " + String(hours) + "h";
  }
  if (hours > 0) {
    return String(hours) + "h " + String(minutes) + "m";
  }
  return String(minutes) + "m";
}
