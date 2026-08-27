#include "../src/brainflow.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <set>
#include <tuple>
using namespace brainflow;
using Key=std::tuple<unsigned,unsigned,unsigned,unsigned,unsigned>;
static uint64_t explored=0, fuzzed=0;
static std::set<Key> states;
static std::set<std::pair<Key,Key>> edges;
static Key key(const Session&s){return {unsigned(s.phase),unsigned(s.game),s.length,s.score? (s.score==999999?2:1):0,s.remaining==0?0:s.remaining==1?1:2};}
static void invariant(const Session&s) {
  assert(s.length<=kInputMax);assert(s.input[s.length]==0);assert(s.remaining<=kRoundMs);
  assert(s.score<=999999);assert(s.correct<=s.attempts);assert(s.attempts<=60000);assert(s.streak<=99);
  assert(s.phase!=Phase::Finished || s.remaining==0);
  assert(s.phase!=Phase::Playing || s.remaining>0);
  assert(strlen(s.prompt)<sizeof(s.prompt));assert(strlen(s.answer)<sizeof(s.answer));
}
static void action(Session&s,unsigned event,uint32_t now){
  switch(event){case 0:s.tick(now);break;case 1:s.type('1',now);break;case 2:s.type('q',now);break;
    case 3:s.erase(now);break;case 4:s.submit(now,s.choice);break;case 5:s.submit(now,-1);break;
    case 6:s.pause(now);break;case 7:s.resume(now);break;}
}
static uint32_t next(uint32_t&x){x^=x<<13;x^=x>>17;x^=x<<5;return x;}
int main(){
  const uint32_t offsets[]={0,0xfffffff0U,0x80000000U};
  const uint32_t elapsed[]={0,1,899,900,999,1000};
  const uint32_t scores[]={0,999949,999999};
  const uint32_t feedback[]={0,1,900};
  // Full Cartesian array, not a sample: all pairs and triples of declared classes.
  for(unsigned phase=0;phase<5;++phase)for(unsigned game=0;game<4;++game)
  for(unsigned length=0;length<=12;++length)for(unsigned event=0;event<8;++event)
  for(auto delta:elapsed)for(auto score:scores)for(auto fb:feedback)
  for(unsigned visible=0;visible<2;++visible)for(auto offset:offsets){
    Session s;s.start(static_cast<Game>(game),offset,0xB12A2026);
    s.phase=static_cast<Phase>(phase);s.resumePhase=fb==0?Phase::Playing:Phase::Feedback;
    s.remaining=phase==unsigned(Phase::Finished)?0:1000;s.feedbackRemaining=fb;s.score=score;
    memset(s.input,'1',length);s.input[length]=0;s.length=length;
    const Session before=s;const auto from=key(s);
    if(!visible)s.pause(offset);action(s,event,offset+delta);invariant(s);
    if(before.phase==Phase::Finished){assert(s.phase==Phase::Finished);assert(s.score==score);}
    if(!visible && before.phase!=Phase::Finished && before.phase!=Phase::Menu)assert(s.remaining==before.remaining);
    if(before.phase==Phase::Paused && event!=7)assert(s.remaining==before.remaining);
    if(event==4 && before.phase!=Phase::Playing &&
       !(before.phase==Phase::Feedback && visible && delta>=before.feedbackRemaining && delta<before.remaining))
      assert(s.score==before.score);
    const auto to=key(s);states.insert(from);states.insert(to);edges.insert({from,to});++explored;
  }
  // Deterministic traces use real model methods; interruptions, wrap and late keys mix.
  constexpr uint32_t seeds[]={1,37,0xC0FFEE,0x12345678};
  for(auto seed:seeds){uint32_t r=seed,now=0xffff0000U;Session s;s.start(Game::Number,now,seed);
    for(unsigned i=0;i<250000;++i){const unsigned event=next(r)%12;now+=next(r)%1300;
      if(event==8)s.start(static_cast<Game>(next(r)%4),now,next(r));
      else if(event==9)s.menu(now);
      else if(event==10){s.tick(now);if(s.phase==Phase::Playing){
        if(s.game==Game::Number||s.game==Game::Word){s.length=0;s.input[0]=0;char answer[16];strcpy(answer,s.answer);for(char*p=answer;*p;++p)s.type(*p,now);}
        s.submit(now,s.choice);}}
      else if(event==11){s.pause(now);now+=next(r)%100000;s.resume(now);}
      else action(s,event,now);
      invariant(s);++fuzzed;
    }
  }
  // Fence fixtures must fail if an individual production guard is removed.
  Session s;s.start(Game::Number,0,1);s.length=12;memset(s.input,'1',12);s.input[12]=0;s.type('1',0);invariant(s);
  s.start(Game::Meaning,0,1);s.submit(0,s.choice);auto score=s.score;s.submit(0,s.choice);assert(s.score==score);
  s.start(Game::Number,0,1);s.tick(60000);assert(s.phase==Phase::Finished);
  s.start(Game::Number,0,1);s.pause(1);s.resume(100000);s.tick(100001);assert(s.remaining==59998);
  s.start(Game::Number,0,1);s.type('q',0);assert(s.length==0);
  s.start(Game::Fraction,0,1);s.submit(0,-1);assert(s.attempts==0);
  s.start(Game::Meaning,0,1);s.score=999999;s.submit(0,s.choice);invariant(s);
  s.start(Game::Meaning,0,1);s.attempts=60000;s.correct=60000;s.streak=99;s.submit(0,s.choice);invariant(s);
  s.start(Game::Meaning,0,1);s.submit(0,s.choice);s.tick(kFeedbackMs);assert(s.phase==Phase::Playing);
  std::cout<<"{\"reduced_executions\":"<<explored<<",\"states\":"<<states.size()<<",\"transitions\":"<<edges.size()
      <<",\"fuzz_transitions\":"<<fuzzed<<",\"pairwise_percent\":100,\"threewise_percent\":100,\"fence_fixtures\":9,\"seeds\":[1,37,12648430,305419896]}\n";
}
