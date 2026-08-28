// BrainFlow: eight offline keyboard trainers, deliberately isolated from chat/OTA.
#include "../brainflow.h"
#include "../brainflow_audio.h"
namespace {
brainflow::Session gBrainFlow;
uint32_t gBrainBest[brainflow::kGameCount] = {};
bool gBrainLoaded = false;
bool gBrainResultSaved = false;
bool gBrainHelpVisible = true;
bool gBrainFinishSoundPlayed = false;
bool gBrainStartSoundPending = false;
uint32_t gBrainStartSoundAtMs = 0;

uint16_t brainAccent(uint8_t game) {
  const uint16_t colors[brainflow::kGameCount] = {
      rgb565_local(47,227,255), rgb565_local(157,102,255),
      rgb565_local(246,194,74), rgb565_local(34,209,189),
      rgb565_local(255,126,182), rgb565_local(104,214,255),
      rgb565_local(255,169,77), rgb565_local(133,232,126)};
  return colors[game % brainflow::kGameCount];
}

bool brainSoundAllowed() {
  return gUiMode == UiMode::BrainFlow && brainflow_audio::allowed(
      gRecording, gPlaybackActive, gRealtimeActive, gRealtimeRecording,
      gSubmitting, gThinking);
}

void brainPlayCue(brainflow_audio::Cue cue) {
  if (!brainSoundAllowed()) return;
  const auto cueSpec = brainflow_audio::spec(cue);
  focusTone(cueSpec.frequency, cueSpec.durationMs);
}

void brainScheduleStartCue(uint32_t now) {
  gBrainStartSoundPending = true;
  gBrainStartSoundAtMs = now + 55;
}

void serviceBrainSound(uint32_t now) {
  if (!gBrainStartSoundPending || static_cast<int32_t>(now - gBrainStartSoundAtMs) < 0) return;
  gBrainStartSoundPending = false;
  brainPlayCue(brainflow_audio::Cue::Start);
}

void brainLoadBest() {
  if (gBrainLoaded) return;
  gPrefs.begin("brainflow", true);
  for (uint8_t i = 0; i < brainflow::kGameCount; ++i) {
    char key[8];
    snprintf(key, sizeof(key), "best%u", i);
    gBrainBest[i] = gPrefs.getULong(key, 0);
  }
  gPrefs.end();
  gBrainLoaded = true;
}

void brainSaveResult() {
  if (gBrainFlow.phase != brainflow::Phase::Finished || gBrainResultSaved) return;
  gBrainResultSaved = true;
  const uint8_t index = static_cast<uint8_t>(gBrainFlow.game) % brainflow::kGameCount;
  if (gBrainFlow.score <= gBrainBest[index]) return;
  gBrainBest[index] = gBrainFlow.score;
  char key[8];
  snprintf(key, sizeof(key), "best%u", index);
  gPrefs.begin("brainflow", false);
  gPrefs.putULong(key, gBrainBest[index]);
  gPrefs.end();
}

void brainMaybeFinishCue() {
  if (gBrainFlow.phase != brainflow::Phase::Finished || gBrainFinishSoundPlayed) return;
  gBrainFinishSoundPlayed = true;
  gBrainStartSoundPending = false;
  brainPlayCue(brainflow_audio::Cue::Finish);
}

void brainTick(uint32_t now) {
  gBrainFlow.tick(now);
  brainMaybeFinishCue();
}

void brainStartRound(brainflow::Game game, uint32_t now, bool selectedFromMenu) {
  gBrainResultSaved = false;
  gBrainFinishSoundPlayed = false;
  gBrainFlow.start(game, now, esp_random());
  if (selectedFromMenu) {
    brainPlayCue(brainflow_audio::Cue::Select);
    brainScheduleStartCue(now);
  } else {
    gBrainStartSoundPending = false;
    brainPlayCue(brainflow_audio::Cue::Start);
  }
}

void serviceBrainFlow() {
  const uint32_t now = millis();
  serviceBrainSound(now);
  if (gUiMode != UiMode::BrainFlow || gLauncherVisible || gRecording ||
      gPlaybackActive || gRealtimeActive) {
    gBrainFlow.pause(now);
  } else {
    brainLoadBest();
    brainTick(now);
  }
  brainSaveResult();
}

void pauseBrainFlow() {
  gBrainFlow.pause(millis());
  brainSaveResult();
}

void brainFlowEscape() {
  gBrainStartSoundPending = false;
  if (brainflow::escapeTarget(gBrainFlow.phase) == brainflow::BackTarget::BrainFlowMenu) {
    gBrainFlow.menu(millis());
    gBrainResultSaved = false;
    gBrainFinishSoundPlayed = false;
    setStatus("BrainFlow games", 700);
    setKeyDiag("brainflow:menu");
    return;
  }
  syncLauncherToMode(UiMode::BrainFlow);
  gLauncherVisible = true;
  setStatus("Games", 700);
  setKeyDiag("brainflow:launcher");
}

bool routeBrainFlowEscape() {
  if (gUiMode != UiMode::BrainFlow || gLauncherVisible || gRecording || gPlaybackActive ||
      gRealtimeActive || gRealtimeRecording) return false;
  brainFlowEscape();
  return true;
}

bool handleBrainFlowKey(char key, const Keyboard_Class::KeysState& keys) {
  brainLoadBest();
  const uint32_t now = millis();
  brainTick(now);
  brainSaveResult();
  const bool enter = keys.enter || key == '\r' || key == '\n';
  const char lowerKey = static_cast<char>(tolower(static_cast<unsigned char>(key)));

  if (!keys.ctrl && lowerKey == 'l') {
    gBrainHelpVisible = brainflow::helpVisibleAfterKey(gBrainHelpVisible, key, keys.ctrl);
    brainPlayCue(brainflow_audio::Cue::Select);
    setStatus(gBrainHelpVisible ? "BrainFlow help" : "BrainFlow focus", 600);
    return true;
  }

  const auto phase = gBrainFlow.phase;
  if (phase == brainflow::Phase::Menu) {
    if (key >= '1' && key <= '8') {
      brainStartRound(static_cast<brainflow::Game>(key - '1'), now, true);
    } else if (enter) {
      brainStartRound(gBrainFlow.game, now, false);
    }
    return true;
  }
  if (phase == brainflow::Phase::Finished) {
    if (enter) brainStartRound(gBrainFlow.game, now, false);
    return true;
  }
  if (phase == brainflow::Phase::Paused) {
    if (enter || keys.space || key == ' ') gBrainFlow.resume(now);
    return true;
  }
  if (keys.space || key == ' ') {
    gBrainFlow.pause(now);
    return true;
  }
  if (keys.ctrl || keys.fn) return true;
  if (keys.del || key == '\b') {
    gBrainFlow.erase(now);
    return true;
  }

  bool submitted = false;
  if (brainflow::isChoiceGame(gBrainFlow.game)) {
    const int selected = lowerKey == 'a' ? 0 : lowerKey == 's' ? 1 : lowerKey == 'd' ? 2 : -1;
    submitted = gBrainFlow.submit(now, selected);
  } else if (enter) {
    submitted = gBrainFlow.submit(now);
  } else {
    gBrainFlow.type(key, now);
  }
  if (submitted) {
    gBrainStartSoundPending = false;
    brainPlayCue(gBrainFlow.lastCorrect ? brainflow_audio::Cue::Correct
                                        : brainflow_audio::Cue::Wrong);
  }
  return true;  // Never leak game keys into chat compose or network commands.
}

void brainCentered(const String& value, int y, uint16_t color, int width = 206) {
  auto& d = gCanvas;
  const String text = fitCurrentFontToWidth(value, width);
  d.setTextColor(color, rgb565_local(9,20,28));
  d.setCursor((240 - d.textWidth(text)) / 2, y);
  d.print(text);
}

void brainHelpFooter(const String& text, uint16_t panel, uint16_t muted) {
  auto& d = gCanvas;
  d.setFont(&fonts::Font0);
  if (gBrainHelpVisible) {
    brainCentered(text, 121, muted, 212);
  } else {
    d.setTextColor(muted, panel);
    d.setCursor(198, 121);
    d.print("L help");
  }
}

void renderBrainFlowUi() {
  brainLoadBest();
  auto& d = gCanvas;
  const auto& game = gBrainFlow;
  const uint16_t bg = rgb565_local(2,7,10), panel = rgb565_local(9,20,28);
  const uint16_t line = rgb565_local(40,55,68), muted = rgb565_local(130,145,155);
  const uint16_t green = rgb565_local(88,240,141), red = rgb565_local(255,104,120);
  const uint16_t accent = brainAccent(static_cast<uint8_t>(game.game));
  const bool menu = game.phase == brainflow::Phase::Menu;
  const bool feedback = game.phase == brainflow::Phase::Feedback;
  const uint16_t frame = feedback ? (game.lastCorrect ? green : red) : line;
  d.fillScreen(bg);
  d.fillRoundRect(5, 5, 230, 125, 10, panel);
  d.drawRoundRect(5, 5, 230, 125, 10, frame);
  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(14, 10);
  d.print(fitCurrentFontToWidth(menu ? "BRAINFLOW" : brainflow::name(game.game), 169));
  d.setFont(&fonts::Font0);
  d.setTextColor(muted, panel);
  const String clock = menu ? "8 GAMES" : String((game.remaining + 999) / 1000) + "s";
  d.setCursor(226 - d.textWidth(clock), 16);
  d.print(clock);
  d.drawFastHLine(14, 30, 212, line);

  const uint32_t motion = millis() / 80U;
  for (uint8_t i = 0; i < 4; ++i) {
    d.fillCircle(150 + (motion + i * 17U) % 72U, 30, 1, i == 3 ? accent : line);
  }

  if (menu) {
    for (uint8_t i = 0; i < brainflow::kGameCount; ++i) {
      const int col = i / 4;
      const int row = i % 4;
      const int x = 12 + col * 109;
      const int y = 37 + row * 19;
      const uint16_t color = brainAccent(i);
      const bool selected = i == static_cast<uint8_t>(game.game);
      d.drawRoundRect(x, y - 2, 105, 16, 4, selected ? color : line);
      if (selected) d.fillCircle(x + 99, y + 6, 2, color);
      d.setFont(&fonts::Font0);
      d.setTextColor(color, panel);
      d.setCursor(x + 5, y + 2);
      d.print(String(i + 1) + " " + brainflow::menuName(static_cast<brainflow::Game>(i)));
    }
    d.setFont(&fonts::Font0);
    brainCentered("1-8 start   L help   Esc launcher", 116, muted, 212);
  } else if (game.phase == brainflow::Phase::Paused) {
    d.setFont(&fonts::Font4);
    brainCentered("PAUSED", 43, brainAccent(2));
    d.setFont(&fonts::Font2);
    brainCentered("Time and score are safe", 78, muted);
    brainHelpFooter("Enter resume   Esc games   L hide", panel, muted);
  } else if (game.phase == brainflow::Phase::Finished) {
    d.setFont(&fonts::Font4);
    brainCentered(String(game.score), 38, accent);
    d.setFont(&fonts::Font2);
    brainCentered("ROUND COMPLETE", 70, muted);
    d.setFont(&fonts::Font0);
    const uint8_t index = static_cast<uint8_t>(game.game) % brainflow::kGameCount;
    brainCentered(String(game.correct) + " RIGHT  " + String(game.accuracy()) +
                      "%  BEST " + String(gBrainBest[index]),
                  94, green);
    brainHelpFooter("Enter replay   Esc games   L hide", panel, muted);
  } else {
    d.setFont(&fonts::Font0);
    const String hint = game.game == brainflow::Game::WordScramble
                            ? String("UNSCRAMBLE / ") + game.clue
                            : String(game.clue);
    brainCentered(hint, 35, muted);

    if (game.game == brainflow::Game::Fraction) {
      d.setFont(&fonts::Font4);
      const String left = String(game.n1) + "/" + String(game.d1);
      const String right = String(game.n2) + "/" + String(game.d2);
      d.setTextColor(accent, panel);
      d.setCursor(68 - d.textWidth(left) / 2, 49);
      d.print(left);
      d.setCursor(175 - d.textWidth(right) / 2, 49);
      d.print(right);
      d.setFont(&fonts::Font2);
      brainCentered("?", 53, muted);
      d.fillRoundRect(31, 72, 75, 4, 2, line);
      d.fillRoundRect(31, 72, 75 * game.n1 / game.d1, 4, 2, accent);
      d.fillRoundRect(137, 72, 75, 4, 2, line);
      d.fillRoundRect(137, 72, 75 * game.n2 / game.d2, 4, 2, accent);
    } else if (brainflow::isChoiceGame(game.game)) {
      if (game.game == brainflow::Game::Vocabulary ||
          game.game == brainflow::Game::OddOneOut) {
        d.setFont(&fonts::Font2);
      } else {
        d.setFont(&fonts::Font4);
      }
      brainCentered(game.prompt, 48, accent, 214);
    } else {
      d.setFont(&fonts::Font4);
      brainCentered(game.prompt, 48, accent, 214);
    }

    if (brainflow::isChoiceGame(game.game)) {
      const int buttonY = gBrainHelpVisible ? 78 : 73;
      const int buttonH = gBrainHelpVisible ? 27 : 36;
      for (uint8_t i = 0; i < 3; ++i) {
        const int x = 13 + i * 72;
        d.drawRoundRect(x, buttonY, 68, buttonH, 5, feedback && i == game.choice ? frame : line);
        d.setFont(&fonts::Font0);
        d.setTextColor(accent, panel);
        d.setCursor(x + 5, buttonY + 2);
        d.print("ASD"[i]);
        d.setFont(&fonts::Font2);
        const String label = fitCurrentFontToWidth(game.options[i], 59);
        d.setTextColor(feedback && i == game.choice ? frame : TFT_WHITE, panel);
        d.setCursor(x + (68 - d.textWidth(label)) / 2, buttonY + (gBrainHelpVisible ? 11 : 17));
        d.print(label);
      }
    } else {
      const int inputY = gBrainHelpVisible ? 79 : 73;
      const int inputH = gBrainHelpVisible ? 26 : 37;
      const uint16_t inputBg = rgb565_local(18,37,49);
      d.fillRoundRect(32, inputY, 176, inputH, 6, inputBg);
      d.drawRoundRect(32, inputY, 176, inputH, 6, feedback ? frame : accent);
      d.setFont(&fonts::Font4);
      const String value = feedback ? String(game.answer)
                                    : String(game.input) + (millis() / 450 % 2 ? "_" : " ");
      const String fitted = fitCurrentFontToWidth(value, 160);
      d.setTextColor(feedback ? frame : TFT_WHITE, inputBg);
      d.setCursor((240 - d.textWidth(fitted)) / 2, inputY + (gBrainHelpVisible ? 4 : 9));
      d.print(fitted);
    }

    d.setFont(&fonts::Font0);
    const int statusY = gBrainHelpVisible ? 108 : 113;
    if (feedback) {
      const String result = game.lastCorrect ? "NICE!" : String("ANSWER: ") + game.answer;
      brainCentered(result, statusY, frame);
    } else {
      d.setTextColor(muted, panel);
      d.setCursor(15, statusY);
      d.print("PTS " + String(game.score));
      const String streak = "STREAK " + String(game.streak);
      d.setTextColor(accent, panel);
      d.setCursor(226 - d.textWidth(streak), statusY);
      d.print(streak);
    }
    const int progressY = gBrainHelpVisible ? 116 : 128;
    d.fillRect(14, progressY, 212, 2, line);
    const int progress = 212UL * game.remaining / brainflow::kRoundMs;
    if (progress > 0) d.fillRect(14, progressY, progress, 2, accent);
    brainHelpFooter(brainflow::isChoiceGame(game.game)
                        ? "A/S/D choose  Space pause  L hide"
                        : "Enter check  Del edit  Space  L hide",
                    panel, muted);
  }
  d.setFont(&fonts::Font2);
  renderLauncherOverlay();
  renderDebugOverlay();
}

}  // namespace
