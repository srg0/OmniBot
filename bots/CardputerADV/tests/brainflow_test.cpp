#include "../src/brainflow.h"
#include <cassert>
#include <iostream>
#include <set>
#include <algorithm>
#include <string>
using namespace brainflow;
static unsigned long checks=0;
#define CHECK(x) do {++checks; if(!(x)){std::cerr<<"FAILED line "<<__LINE__<<": " #x <<"\n";return 1;}}while(0)

int main() {
  Session s;
  CHECK(sizeof(s)<256);
  for(unsigned game=0;game<4;++game) {
    std::set<std::string> deck;
    for(unsigned round=0;round<32;++round) {
      s.start(static_cast<Game>(game),100,37);s.attempts=round;s.correct=round;s.question();
      CHECK(strlen(s.prompt)<sizeof(s.prompt));CHECK(strlen(s.answer)<sizeof(s.answer));
      if(game==0) {
        unsigned a,b,v;char op;
        CHECK(sscanf(s.prompt,"%u %c %u",&a,&op,&b)==3);CHECK(sscanf(s.answer,"%u",&v)==1);
        CHECK(op=='+'?v==a+b:op=='-'?v==a-b:op=='x'?v==a*b:b&&a%b==0&&v==a/b);
        for(const char* p=s.answer;*p;++p)s.type(*p,101);
        CHECK(s.submit(102));CHECK(s.lastCorrect);
      } else if(game==1) {
        CHECK(s.n1>0&&s.n1<s.d1&&s.n2>0&&s.n2<s.d2);
        const unsigned a=s.n1*s.d2,b=s.n2*s.d1;
        CHECK(s.choice==(a<b?0:a==b?1:2));CHECK(s.submit(102,s.choice));CHECK(s.lastCorrect);
      } else if(game==2) {
        std::string a=s.answer,b=s.prompt;CHECK(a!=b);
        std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());CHECK(a==b);
        deck.insert(s.answer);for(const char* p=s.answer;*p;++p)s.type(*p+('a'-'A'),101);
        CHECK(s.submit(102));CHECK(s.lastCorrect);
      } else {
        deck.insert(s.prompt);CHECK(strcmp(s.answer,s.options[s.choice])==0);
        CHECK(strcmp(s.options[0],s.options[1])&&strcmp(s.options[0],s.options[2])&&strcmp(s.options[1],s.options[2]));
        CHECK(s.submit(102,s.choice));CHECK(s.lastCorrect);
      }
      const auto score=s.score;CHECK(!s.submit(103,s.choice));CHECK(s.score==score);
    }
    if(game>=2)CHECK(deck.size()==32);
  }
  // Empty input, boundaries, deadline equality, unsigned rollover, pause and resume.
  s.start(Game::Number,0xfffffff0U,0);CHECK(!s.submit(0xfffffff0U));
  for(unsigned i=0;i<100;++i)s.type('9',0xfffffff0U);
  CHECK(s.length==kInputMax);CHECK(s.input[kInputMax]==0);
  s.tick(0x20U);CHECK(s.remaining==kRoundMs-48);
  s.pause(0x30U);auto remaining=s.remaining;s.tick(0xf0000030U);CHECK(s.remaining==remaining);
  s.resume(0xf0000030U);s.tick(0xf0000031U);CHECK(s.remaining==remaining-1);
  s.tick(0xf0000031U+s.remaining);CHECK(s.phase==Phase::Finished);CHECK(!s.submit(0xf000ffffU));
  s.start(Game::Word,0,88);s.type('?',0);CHECK(s.length==0);
  for(const char*p=s.answer;*p;++p)s.type(*p,0);CHECK(s.submit(0));
  s.pause(100);CHECK(s.resumePhase==Phase::Feedback);remaining=s.feedbackRemaining;
  s.resume(9000);s.tick(9000+remaining-1);CHECK(s.phase==Phase::Feedback);s.tick(9000+remaining);CHECK(s.phase==Phase::Playing);
  s.start(Game::Number,0,99);strcpy(s.answer,"0");s.type('0',0);s.type('0',0);CHECK(s.submit(0)&&s.lastCorrect);
  s.start(Game::Number,0,99);s.type('9',60000);CHECK(s.phase==Phase::Finished&&s.length==0);
  std::cout<<"brainflow_checks="<<checks<<" state_bytes="<<sizeof(Session)<<"\n";
}
