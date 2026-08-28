#pragma once

// Offline, allocation-free game model. No hardware, networking or persistent writes.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace brainflow {
enum class Game : uint8_t { Number, Fraction, Word, Meaning };
enum class Phase : uint8_t { Menu, Playing, Feedback, Paused, Finished };
enum class BackTarget : uint8_t { BrainFlowMenu, LauncherGames };

constexpr BackTarget escapeTarget(Phase phase) {
  return phase == Phase::Menu ? BackTarget::LauncherGames : BackTarget::BrainFlowMenu;
}

constexpr uint32_t kRoundMs = 60000;
constexpr uint32_t kFeedbackMs = 900;
constexpr uint8_t kInputMax = 12;
struct Word { const char* word; const char* clue; };
static constexpr Word kWords[] = {
    {"CLOUD","SKY"},{"TIGER","ANIMAL"},{"OCEAN","WATER"},{"PIANO","MUSIC"},
    {"APPLE","FRUIT"},{"TRAIN","TRAVEL"},{"CHAIR","FURNITURE"},{"BREAD","FOOD"},
    {"PLANT","GARDEN"},{"HORSE","ANIMAL"},{"LEMON","FRUIT"},{"BRUSH","ART"},
    {"GUITAR","MUSIC"},{"PLANET","SPACE"},{"FOREST","NATURE"},{"BRIDGE","CROSSING"},
    {"RABBIT","ANIMAL"},{"ROCKET","SPACE"},{"BOTTLE","CONTAINER"},{"PENCIL","WRITING"},
    {"WINDOW","HOUSE"},{"CASTLE","BUILDING"},{"ORANGE","FRUIT"},{"PURPLE","COLOR"},
    {"THUNDER","WEATHER"},{"DOLPHIN","ANIMAL"},{"FEATHER","BIRD"},{"KITCHEN","ROOM"},
    {"RAINBOW","SKY"},{"JOURNEY","TRAVEL"},{"LANTERN","LIGHT"},{"CRYSTAL","MINERAL"}};
struct Meaning { const char* term; const char* right; const char* wrong1; const char* wrong2; };
static constexpr Meaning kMeanings[] = {
    {"BRAVE","bold","timid","tired"},{"QUICK","fast","slow","heavy"},
    {"HAPPY","glad","angry","empty"},{"TINY","small","huge","loud"},
    {"QUIET","silent","noisy","wide"},{"ANGRY","mad","calm","sweet"},
    {"LARGE","big","little","soft"},{"CLEVER","smart","foolish","cold"},
    {"BEGIN","start","finish","break"},{"FINISH","end","start","carry"},
    {"REPAIR","fix","damage","hide"},{"SELECT","choose","drop","lose"},
    {"CALM","peaceful","furious","rough"},{"DIFFICULT","hard","easy","flat"},
    {"ANCIENT","old","modern","swift"},{"HONEST","truthful","false","lazy"},
    {"VACANT","empty","full","warm"},{"WEALTHY","rich","poor","weak"},
    {"FRAGILE","delicate","sturdy","noisy"},{"VIVID","bright","dull","slow"},
    {"CAUTIOUS","careful","reckless","angry"},{"SCARCE","rare","common","thick"},
    {"ASSIST","help","hinder","sleep"},{"OBSERVE","notice","ignore","invent"},
    {"EXPAND","grow","shrink","freeze"},{"REVEAL","show","hide","mend"},
    {"BRIEF","short","lengthy","bitter"},{"TIMID","shy","bold","cruel"},
    {"RAPID","swift","sluggish","faint"},{"SIMPLE","easy","complex","heavy"},
    {"POLITE","courteous","rude","weak"},{"GLOOMY","sad","cheerful","tiny"}};
constexpr uint8_t kDeckSize = 32;
inline const char* name(Game game) {
  static const char* const names[] = {"NUMBER FLOW","FRACTION PULSE","WORD FORGE","MEANING MATCH"};
  return names[static_cast<uint8_t>(game) % 4];
}

struct Session {
  Game game = Game::Number;
  Phase phase = Phase::Menu;
  Phase resumePhase = Phase::Playing;
  uint32_t rng = 1, lastTick = 0, remaining = kRoundMs, feedbackRemaining = 0;
  uint32_t score = 0;
  uint16_t attempts = 0, correct = 0, streak = 0;
  uint8_t length = 0, choice = 0, deckOffset = 0;
  bool lastCorrect = false;
  uint16_t n1 = 1, d1 = 2, n2 = 1, d2 = 2;
  char prompt[32] = {}, clue[24] = {}, answer[16] = {}, input[kInputMax + 1] = {};
  char options[3][12] = {};

  uint32_t random(uint32_t bound) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return bound ? rng % bound : 0;
  }
  uint8_t level() const { return static_cast<uint8_t>(correct / 4 > 3 ? 3 : correct / 4); }
  uint8_t accuracy() const { return attempts ? static_cast<uint8_t>(100UL * correct / attempts) : 0; }
  void question() {
    length = 0; input[0] = 0; clue[0] = 0; answer[0] = 0;
    const uint8_t difficulty = level();
    if (game == Game::Number) {
      const uint32_t bound = 10 + difficulty * 20;
      uint32_t a = 2 + random(bound), b = 2 + random(9 + difficulty), value = 0;
      char op = '+';
      switch (random(4)) {
        case 0: value = a + b; break;
        case 1: if (a < b) {const uint32_t t=a; a=b; b=t;} op='-'; value=a-b; break;
        case 2: a=2+random(5+difficulty*3); op='x'; value=a*b; break;
        default: value=a; a*=b; op='/'; break;
      }
      snprintf(prompt,sizeof(prompt),"%lu %c %lu",static_cast<unsigned long>(a),op,static_cast<unsigned long>(b));
      snprintf(answer,sizeof(answer),"%lu",static_cast<unsigned long>(value));
      snprintf(clue,sizeof(clue),"CALCULATE / LEVEL %u",difficulty+1);
    } else if (game == Game::Fraction) {
      d1=2+random(5+difficulty*2); n1=1+random(d1-1);
      d2=2+random(5+difficulty*2); n2=1+random(d2-1);
      if (attempts % 3 == 0) {const uint16_t scale=2+random(3); n2=n1*scale; d2=d1*scale;}
      const uint32_t left=static_cast<uint32_t>(n1)*d2, right=static_cast<uint32_t>(n2)*d1;
      choice=left<right?0:left==right?1:2;
      snprintf(prompt,sizeof(prompt),"%u/%u  ?  %u/%u",n1,d1,n2,d2);
      snprintf(answer,sizeof(answer),"%u/%u %c %u/%u",n1,d1,choice==0?'<':choice==1?'=':'>',n2,d2);
      strcpy(clue,"COMPARE THE FRACTIONS");
    } else if (game == Game::Word) {
      // Odd stride visits all 32 items before repeating. Clues disambiguate anagrams.
      const Word& w=kWords[(deckOffset + attempts*13U) % kDeckSize];
      strcpy(answer,w.word); strcpy(prompt,w.word); strcpy(clue,w.clue);
      const uint8_t size=static_cast<uint8_t>(strlen(prompt));
      for (uint8_t i=size-1;i>0;--i) {uint8_t j=random(i+1);char t=prompt[i];prompt[i]=prompt[j];prompt[j]=t;}
      if (strcmp(prompt,answer)==0) {char t=prompt[0];memmove(prompt,prompt+1,size-1);prompt[size-1]=t;}
    } else {
      const Meaning& m=kMeanings[(deckOffset + attempts*13U) % kDeckSize];
      strcpy(prompt,m.term); strcpy(clue,"SAME MEANING / ENGLISH");
      choice=random(3); strcpy(options[choice],m.right);
      strcpy(options[(choice+1)%3],m.wrong1); strcpy(options[(choice+2)%3],m.wrong2);
      strcpy(answer,m.right);
    }
  }
  void start(Game selected, uint32_t now, uint32_t seed) {
    *this=Session{}; game=static_cast<Game>(static_cast<uint8_t>(selected)%4);
    rng=seed?seed:1; deckOffset=random(kDeckSize); lastTick=now; phase=Phase::Playing; question();
  }
  void tick(uint32_t now) {
    const uint32_t elapsed=now-lastTick; lastTick=now;
    if (phase!=Phase::Playing && phase!=Phase::Feedback) return;
    if (elapsed>=remaining) {remaining=0;phase=Phase::Finished;return;}
    remaining-=elapsed;
    if (phase==Phase::Feedback) {
      if (elapsed>=feedbackRemaining) {feedbackRemaining=0;phase=Phase::Playing;question();}
      else feedbackRemaining-=elapsed;
    }
  }
  void pause(uint32_t now) {
    tick(now);
    if (phase==Phase::Playing || phase==Phase::Feedback) {resumePhase=phase;phase=Phase::Paused;}
  }
  void resume(uint32_t now) {if (phase==Phase::Paused) {phase=resumePhase;lastTick=now;}}
  void menu(uint32_t now) {phase=Phase::Menu;lastTick=now;length=0;input[0]=0;}
  bool type(char key, uint32_t now) {
    tick(now);
    if (phase!=Phase::Playing || (game!=Game::Number && game!=Game::Word)) return false;
    if (key>='a'&&key<='z')key-=('a'-'A');
    if (game==Game::Number ? (key<'0'||key>'9') : (key<'A'||key>'Z')) return false;
    if (length>=kInputMax) return false;
    input[length++]=key;input[length]=0;return true;
  }
  void erase(uint32_t now) {tick(now);if(phase==Phase::Playing && length)input[--length]=0;}
  bool submit(uint32_t now, int selected=-1) {
    tick(now);
    if (phase!=Phase::Playing) return false;
    if (game==Game::Fraction || game==Game::Meaning) {
      if(selected<0||selected>2)return false;
      lastCorrect=selected==choice;
    } else {
      if(!length)return false;
      const char* normalized=input;
      if(game==Game::Number)while(normalized[0]=='0'&&normalized[1])++normalized;
      lastCorrect=strcmp(normalized,answer)==0;
    }
    if(attempts<60000)++attempts;
    if(lastCorrect) {
      if(correct<attempts)++correct;
      if(streak<99)++streak;
      const uint32_t gain=50+5*(streak>10?10:streak);
      score=score>999999-gain?999999:score+gain;
    } else streak=0;
    phase=Phase::Feedback;feedbackRemaining=kFeedbackMs;
    return true;
  }
};
static_assert(sizeof(Session)<256,"BrainFlow must remain a bounded local state");
}  // namespace brainflow
