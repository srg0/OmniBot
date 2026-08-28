#pragma once

// Offline, allocation-free game model. No hardware, networking or persistent writes.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace brainflow {
enum class Game : uint8_t {
  Arithmetic,
  Fraction,
  WordScramble,
  Vocabulary,
  Synonyms,
  Estimate,
  Sequence,
  OddOneOut,
};
enum class Phase : uint8_t { Menu, Playing, Feedback, Paused, Finished };
enum class BackTarget : uint8_t { BrainFlowMenu, LauncherGames };

constexpr uint8_t kGameCount = 8;
constexpr uint32_t kRoundMs = 60000;
constexpr uint32_t kFeedbackMs = 900;
constexpr uint8_t kInputMax = 16;

constexpr BackTarget escapeTarget(Phase phase) {
  return phase == Phase::Menu ? BackTarget::LauncherGames : BackTarget::BrainFlowMenu;
}

constexpr bool helpVisibleAfterKey(bool visible, char key, bool ctrl) {
  return !ctrl && (key == 'l' || key == 'L') ? !visible : visible;
}

constexpr bool isTextGame(Game game) {
  return game == Game::Arithmetic || game == Game::WordScramble;
}

constexpr bool isChoiceGame(Game game) {
  return !isTextGame(game);
}

inline const char* name(Game game) {
  static const char* const names[kGameCount] = {
      "ARITHMETIC", "FRACTION PULSE", "WORD SCRAMBLE", "VOCABULARY",
      "SYNONYMS", "ESTIMATE", "LOGIC SEQUENCE", "ODD ONE OUT"};
  return names[static_cast<uint8_t>(game) % kGameCount];
}

inline const char* menuName(Game game) {
  static const char* const names[kGameCount] = {
      "ARITHMETIC", "FRACTIONS", "SCRAMBLE", "VOCAB",
      "SYNONYMS", "ESTIMATE", "SEQUENCE", "ODD ONE"};
  return names[static_cast<uint8_t>(game) % kGameCount];
}

struct Word {
  const char* word;
  const char* clue;
};

static constexpr Word kWords[] = {
    {"CLOUD","SKY"},{"TIGER","ANIMAL"},{"OCEAN","WATER"},{"PIANO","MUSIC"},
    {"APPLE","FRUIT"},{"TRAIN","TRAVEL"},{"CHAIR","FURNITURE"},{"BREAD","FOOD"},
    {"PLANT","GARDEN"},{"HORSE","ANIMAL"},{"LEMON","FRUIT"},{"BRUSH","ART"},
    {"GUITAR","MUSIC"},{"PLANET","SPACE"},{"FOREST","NATURE"},{"BRIDGE","CROSSING"},
    {"RABBIT","ANIMAL"},{"ROCKET","SPACE"},{"BOTTLE","CONTAINER"},{"PENCIL","WRITING"},
    {"WINDOW","HOUSE"},{"CASTLE","BUILDING"},{"ORANGE","FRUIT"},{"PURPLE","COLOR"},
    {"THUNDER","WEATHER"},{"DOLPHIN","ANIMAL"},{"FEATHER","BIRD"},{"KITCHEN","ROOM"},
    {"RAINBOW","SKY"},{"JOURNEY","TRAVEL"},{"LANTERN","LIGHT"},{"CRYSTAL","MINERAL"}};

struct ChoiceQuestion {
  const char* prompt;
  const char* right;
  const char* wrong1;
  const char* wrong2;
};

static constexpr ChoiceQuestion kVocabulary[] = {
    {"NOT AFRAID","BRAVE","TIMID","TIRED"},{"MOVING FAST","QUICK","HEAVY","EMPTY"},
    {"FEELING JOY","HAPPY","ANGRY","ROUGH"},{"VERY SMALL","TINY","LOUD","WIDE"},
    {"MAKING NO NOISE","QUIET","VIVID","RAPID"},{"FEELING MAD","ANGRY","CALM","POLITE"},
    {"GREATER IN SIZE","LARGE","BRIEF","FRAGILE"},{"ABLE TO LEARN","CLEVER","VACANT","ANCIENT"},
    {"TO START","BEGIN","FINISH","IGNORE"},{"TO COME TO AN END","FINISH","EXPAND","ASSIST"},
    {"TO FIX DAMAGE","REPAIR","REVEAL","SHRINK"},{"TO MAKE A CHOICE","SELECT","OBSERVE","HINDER"},
    {"PEACEFUL STATE","CALM","FURIOUS","GLOOMY"},{"NOT EASY","DIFFICULT","SIMPLE","RAPID"},
    {"VERY OLD","ANCIENT","MODERN","VIVID"},{"TELLING THE TRUTH","HONEST","FALSE","LAZY"},
    {"NOT OCCUPIED","VACANT","WEALTHY","CAUTIOUS"},{"HAVING MUCH MONEY","WEALTHY","SCARCE","TIMID"},
    {"EASILY BROKEN","FRAGILE","STURDY","POLITE"},{"BRIGHT AND CLEAR","VIVID","DULL","BRIEF"},
    {"ACTING WITH CARE","CAUTIOUS","RECKLESS","RAPID"},{"HARD TO FIND","SCARCE","COMMON","SIMPLE"},
    {"TO GIVE HELP","ASSIST","HINDER","REVEAL"},{"TO WATCH CLOSELY","OBSERVE","IGNORE","INVENT"},
    {"TO GROW LARGER","EXPAND","SHRINK","FREEZE"},{"TO MAKE KNOWN","REVEAL","HIDE","MEND"},
    {"SHORT IN LENGTH","BRIEF","ANCIENT","WEALTHY"},{"LACKING COURAGE","TIMID","BOLD","VIVID"},
    {"HAPPENING FAST","RAPID","SLUGGISH","VACANT"},{"EASY TO UNDERSTAND","SIMPLE","COMPLEX","FRAGILE"},
    {"SHOWING GOOD MANNERS","POLITE","RUDE","SCARCE"},{"DARK OR SAD","GLOOMY","CHEERFUL","QUICK"}};

static constexpr ChoiceQuestion kSynonyms[] = {
    {"BRAVE","BOLD","TIMID","TIRED"},{"QUICK","FAST","SLOW","HEAVY"},
    {"HAPPY","GLAD","ANGRY","EMPTY"},{"TINY","SMALL","HUGE","LOUD"},
    {"QUIET","SILENT","NOISY","WIDE"},{"ANGRY","MAD","CALM","SWEET"},
    {"LARGE","BIG","LITTLE","SOFT"},{"CLEVER","SMART","FOOLISH","COLD"},
    {"BEGIN","START","FINISH","BREAK"},{"FINISH","END","START","CARRY"},
    {"REPAIR","FIX","DAMAGE","HIDE"},{"SELECT","CHOOSE","DROP","LOSE"},
    {"CALM","PEACEFUL","FURIOUS","ROUGH"},{"DIFFICULT","HARD","EASY","FLAT"},
    {"ANCIENT","OLD","MODERN","SWIFT"},{"HONEST","TRUTHFUL","FALSE","LAZY"},
    {"VACANT","EMPTY","FULL","WARM"},{"WEALTHY","RICH","POOR","WEAK"},
    {"FRAGILE","DELICATE","STURDY","NOISY"},{"VIVID","BRIGHT","DULL","SLOW"},
    {"CAUTIOUS","CAREFUL","RECKLESS","ANGRY"},{"SCARCE","RARE","COMMON","THICK"},
    {"ASSIST","HELP","HINDER","SLEEP"},{"OBSERVE","NOTICE","IGNORE","INVENT"},
    {"EXPAND","GROW","SHRINK","FREEZE"},{"REVEAL","SHOW","HIDE","MEND"},
    {"BRIEF","SHORT","LENGTHY","BITTER"},{"TIMID","SHY","BOLD","CRUEL"},
    {"RAPID","SWIFT","SLUGGISH","FAINT"},{"SIMPLE","EASY","COMPLEX","HEAVY"},
    {"POLITE","COURTEOUS","RUDE","WEAK"},{"GLOOMY","SAD","CHEERFUL","TINY"}};

struct OddQuestion {
  const char* first;
  const char* second;
  const char* odd;
  const char* clue;
};

static constexpr OddQuestion kOddQuestions[] = {
    {"CAT","DOG","APPLE","TWO ARE ANIMALS"},{"RED","BLUE","CHAIR","TWO ARE COLORS"},
    {"PIANO","GUITAR","CARROT","TWO ARE INSTRUMENTS"},{"CIRCLE","SQUARE","TIGER","TWO ARE SHAPES"},
    {"MONDAY","FRIDAY","PURPLE","TWO ARE DAYS"},{"OAK","PINE","ROCKET","TWO ARE TREES"},
    {"BUS","TRAIN","LEMON","TWO ARE TRANSPORT"},{"EAGLE","ROBIN","BOTTLE","TWO ARE BIRDS"},
    {"MILK","WATER","PENCIL","TWO ARE DRINKS"},{"SPOON","FORK","PLANET","TWO ARE UTENSILS"},
    {"HAPPY","GLAD","WINDOW","TWO ARE FEELINGS"},{"RUN","WALK","CASTLE","TWO ARE ACTIONS"},
    {"WINTER","SUMMER","RABBIT","TWO ARE SEASONS"},{"GOLD","SILVER","KITCHEN","TWO ARE METALS"},
    {"EYE","EAR","LANTERN","TWO ARE BODY PARTS"},{"MARS","VENUS","BREAD","TWO ARE PLANETS"}};

constexpr uint8_t kDeckSize = 32;
constexpr uint8_t kOddDeckSize = sizeof(kOddQuestions) / sizeof(kOddQuestions[0]);

struct Session {
  Game game = Game::Arithmetic;
  Phase phase = Phase::Menu;
  Phase resumePhase = Phase::Playing;
  uint32_t rng = 1, lastTick = 0, remaining = kRoundMs, feedbackRemaining = 0;
  uint32_t score = 0;
  uint16_t attempts = 0, correct = 0, streak = 0;
  uint8_t length = 0, choice = 0, deckOffset = 0;
  bool lastCorrect = false;
  uint16_t n1 = 1, d1 = 2, n2 = 1, d2 = 2;
  char prompt[40] = {}, clue[28] = {}, answer[20] = {};
  char input[kInputMax + 1] = {};
  char options[3][16] = {};

  uint32_t random(uint32_t bound) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return bound ? rng % bound : 0;
  }

  uint8_t level() const {
    return static_cast<uint8_t>(correct / 4 > 3 ? 3 : correct / 4);
  }

  uint8_t accuracy() const {
    return attempts ? static_cast<uint8_t>(100UL * correct / attempts) : 0;
  }

  void setChoices(const char* right, const char* wrong1, const char* wrong2) {
    choice = static_cast<uint8_t>(random(3));
    snprintf(options[choice], sizeof(options[choice]), "%s", right);
    snprintf(options[(choice + 1) % 3], sizeof(options[0]), "%s", wrong1);
    snprintf(options[(choice + 2) % 3], sizeof(options[0]), "%s", wrong2);
    snprintf(answer, sizeof(answer), "%s", right);
  }

  void setNumericChoices(uint16_t right, uint16_t step) {
    const uint16_t lower = right > step ? right - step : right + step * 2;
    const uint16_t upper = right + step;
    char rightText[16], lowerText[16], upperText[16];
    snprintf(rightText, sizeof(rightText), "%u", right);
    snprintf(lowerText, sizeof(lowerText), "%u", lower);
    snprintf(upperText, sizeof(upperText), "%u", upper);
    setChoices(rightText, lowerText, upperText);
  }

  void question() {
    length = 0;
    input[0] = 0;
    prompt[0] = 0;
    clue[0] = 0;
    answer[0] = 0;
    memset(options, 0, sizeof(options));
    const uint8_t difficulty = level();

    if (game == Game::Arithmetic) {
      const uint32_t bound = 10 + difficulty * 20;
      uint32_t a = 2 + random(bound), b = 2 + random(9 + difficulty), value = 0;
      char op = '+';
      switch (random(4)) {
        case 0: value = a + b; break;
        case 1: if (a < b) {const uint32_t t = a; a = b; b = t;} op = '-'; value = a - b; break;
        case 2: a = 2 + random(5 + difficulty * 3); op = 'x'; value = a * b; break;
        default: value = a; a *= b; op = '/'; break;
      }
      snprintf(prompt, sizeof(prompt), "%lu %c %lu", static_cast<unsigned long>(a), op,
               static_cast<unsigned long>(b));
      snprintf(answer, sizeof(answer), "%lu", static_cast<unsigned long>(value));
      snprintf(clue, sizeof(clue), "CALCULATE / LEVEL %u", difficulty + 1);
      return;
    }

    if (game == Game::Fraction) {
      d1 = 2 + random(5 + difficulty * 2);
      n1 = 1 + random(d1 - 1);
      d2 = 2 + random(5 + difficulty * 2);
      n2 = 1 + random(d2 - 1);
      if (attempts % 3 == 0) {
        const uint16_t scale = 2 + random(3);
        n2 = n1 * scale;
        d2 = d1 * scale;
      }
      const uint32_t left = static_cast<uint32_t>(n1) * d2;
      const uint32_t right = static_cast<uint32_t>(n2) * d1;
      choice = left < right ? 0 : left == right ? 1 : 2;
      snprintf(prompt, sizeof(prompt), "%u/%u  ?  %u/%u", n1, d1, n2, d2);
      snprintf(answer, sizeof(answer), "%u/%u %c %u/%u", n1, d1,
               choice == 0 ? '<' : choice == 1 ? '=' : '>', n2, d2);
      snprintf(options[0], sizeof(options[0]), "LESS");
      snprintf(options[1], sizeof(options[1]), "EQUAL");
      snprintf(options[2], sizeof(options[2]), "MORE");
      snprintf(clue, sizeof(clue), "COMPARE THE FRACTIONS");
      return;
    }

    if (game == Game::WordScramble) {
      const Word& word = kWords[(deckOffset + attempts * 13U) % kDeckSize];
      snprintf(answer, sizeof(answer), "%s", word.word);
      snprintf(prompt, sizeof(prompt), "%s", word.word);
      snprintf(clue, sizeof(clue), "%s", word.clue);
      const uint8_t size = static_cast<uint8_t>(strlen(prompt));
      for (uint8_t i = size - 1; i > 0; --i) {
        const uint8_t j = static_cast<uint8_t>(random(i + 1));
        const char temp = prompt[i];
        prompt[i] = prompt[j];
        prompt[j] = temp;
      }
      if (strcmp(prompt, answer) == 0) {
        const char temp = prompt[0];
        memmove(prompt, prompt + 1, size - 1);
        prompt[size - 1] = temp;
      }
      return;
    }

    if (game == Game::Vocabulary || game == Game::Synonyms) {
      const ChoiceQuestion* deck = game == Game::Vocabulary ? kVocabulary : kSynonyms;
      const ChoiceQuestion& item = deck[(deckOffset + attempts * 13U) % kDeckSize];
      snprintf(prompt, sizeof(prompt), "%s", item.prompt);
      snprintf(clue, sizeof(clue), "%s",
               game == Game::Vocabulary ? "CHOOSE THE WORD" : "CHOOSE THE SYNONYM");
      setChoices(item.right, item.wrong1, item.wrong2);
      return;
    }

    if (game == Game::Estimate) {
      n1 = 12 + random(78 + difficulty * 30);
      n2 = 12 + random(78 + difficulty * 30);
      const uint16_t exact = n1 + n2;
      d1 = static_cast<uint16_t>(((exact + 5) / 10) * 10);
      d2 = exact;
      snprintf(prompt, sizeof(prompt), "%u + %u  ~ ?", n1, n2);
      snprintf(clue, sizeof(clue), "ESTIMATE / NEAREST 10");
      setNumericChoices(d1, 10);
      return;
    }

    if (game == Game::Sequence) {
      n1 = 2 + random(12 + difficulty * 5);
      d1 = 2 + random(4 + difficulty * 2);
      const bool alternating = random(2) != 0;
      n2 = alternating ? static_cast<uint16_t>(d1 + 1 + random(4 + difficulty)) : d1;
      const uint16_t v1 = n1 + d1;
      const uint16_t v2 = v1 + n2;
      const uint16_t v3 = v2 + d1;
      d2 = v3 + n2;
      snprintf(prompt, sizeof(prompt), "%u  %u  %u  %u  ?", n1, v1, v2, v3);
      snprintf(clue, sizeof(clue), "%s", alternating ? "+A +B REPEAT" : "CONTINUE / SAME STEP");
      setNumericChoices(d2, static_cast<uint16_t>(n2 > 1 ? n2 : 2));
      return;
    }

    const OddQuestion& item = kOddQuestions[(deckOffset + attempts * 5U) % kOddDeckSize];
    snprintf(prompt, sizeof(prompt), "WHICH ONE IS ODD?");
    snprintf(clue, sizeof(clue), "%s", item.clue);
    setChoices(item.odd, item.first, item.second);
  }

  void start(Game selected, uint32_t now, uint32_t seed) {
    *this = Session{};
    game = static_cast<Game>(static_cast<uint8_t>(selected) % kGameCount);
    rng = seed ? seed : 1;
    deckOffset = static_cast<uint8_t>(random(kDeckSize));
    lastTick = now;
    phase = Phase::Playing;
    question();
  }

  void tick(uint32_t now) {
    const uint32_t elapsed = now - lastTick;
    lastTick = now;
    if (phase != Phase::Playing && phase != Phase::Feedback) return;
    if (elapsed >= remaining) {
      remaining = 0;
      phase = Phase::Finished;
      return;
    }
    remaining -= elapsed;
    if (phase == Phase::Feedback) {
      if (elapsed >= feedbackRemaining) {
        feedbackRemaining = 0;
        phase = Phase::Playing;
        question();
      } else {
        feedbackRemaining -= elapsed;
      }
    }
  }

  void pause(uint32_t now) {
    tick(now);
    if (phase == Phase::Playing || phase == Phase::Feedback) {
      resumePhase = phase;
      phase = Phase::Paused;
    }
  }

  void resume(uint32_t now) {
    if (phase == Phase::Paused) {
      phase = resumePhase;
      lastTick = now;
    }
  }

  void menu(uint32_t now) {
    phase = Phase::Menu;
    lastTick = now;
    length = 0;
    input[0] = 0;
  }

  bool type(char key, uint32_t now) {
    tick(now);
    if (phase != Phase::Playing || !isTextGame(game)) return false;
    if (key >= 'a' && key <= 'z') key -= ('a' - 'A');
    if (game == Game::Arithmetic ? (key < '0' || key > '9') : (key < 'A' || key > 'Z')) {
      return false;
    }
    if (length >= kInputMax) return false;
    input[length++] = key;
    input[length] = 0;
    return true;
  }

  void erase(uint32_t now) {
    tick(now);
    if (phase == Phase::Playing && length) input[--length] = 0;
  }

  bool submit(uint32_t now, int selected = -1) {
    tick(now);
    if (phase != Phase::Playing) return false;
    if (isChoiceGame(game)) {
      if (selected < 0 || selected > 2) return false;
      lastCorrect = selected == choice;
    } else {
      if (!length) return false;
      const char* normalized = input;
      if (game == Game::Arithmetic) {
        while (normalized[0] == '0' && normalized[1]) ++normalized;
      }
      lastCorrect = strcmp(normalized, answer) == 0;
    }
    if (attempts < 60000) ++attempts;
    if (lastCorrect) {
      if (correct < attempts) ++correct;
      if (streak < 99) ++streak;
      const uint32_t gain = 50 + 5 * (streak > 10 ? 10 : streak);
      score = score > 999999 - gain ? 999999 : score + gain;
    } else {
      streak = 0;
    }
    phase = Phase::Feedback;
    feedbackRemaining = kFeedbackMs;
    return true;
  }
};

static_assert(sizeof(Session) < 256, "BrainFlow must remain a bounded local state");
}  // namespace brainflow
