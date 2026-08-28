// BrainFlow: four offline keyboard trainers, deliberately isolated from chat/OTA.
#include "../brainflow.h"
namespace {
brainflow::Session gBrainFlow;
uint32_t gBrainBest[4] = {};
bool gBrainLoaded = false;
bool gBrainResultSaved = false;

uint16_t brainAccent(uint8_t game) {
  const uint16_t colors[] = {rgb565_local(47,227,255),rgb565_local(157,102,255),
                            rgb565_local(246,194,74),rgb565_local(34,209,189)};
  return colors[game % 4];
}

void brainLoadBest() {
  if (gBrainLoaded) return;
  gPrefs.begin("brainflow", true);
  for (uint8_t i=0;i<4;++i) {char key[8];snprintf(key,sizeof(key),"best%u",i);gBrainBest[i]=gPrefs.getULong(key,0);}
  gPrefs.end();gBrainLoaded=true;
}

void brainSaveResult() {
  if (gBrainFlow.phase != brainflow::Phase::Finished || gBrainResultSaved) return;
  gBrainResultSaved=true;
  const uint8_t index=static_cast<uint8_t>(gBrainFlow.game);
  if (gBrainFlow.score<=gBrainBest[index]) return;
  gBrainBest[index]=gBrainFlow.score;
  char key[8];snprintf(key,sizeof(key),"best%u",index);
  gPrefs.begin("brainflow",false);gPrefs.putULong(key,gBrainBest[index]);gPrefs.end();
}

void serviceBrainFlow() {
  const uint32_t now=millis();
  if (gUiMode!=UiMode::BrainFlow || gLauncherVisible || gRecording || gPlaybackActive || gRealtimeActive) {
    gBrainFlow.pause(now);
  } else {
    brainLoadBest();gBrainFlow.tick(now);
  }
  brainSaveResult();
}

void pauseBrainFlow() { gBrainFlow.pause(millis());brainSaveResult(); }

void brainFlowEscape() {
  if (brainflow::escapeTarget(gBrainFlow.phase)==brainflow::BackTarget::BrainFlowMenu) {
    gBrainFlow.menu(millis());gBrainResultSaved=false;
    setStatus("BrainFlow games",700);setKeyDiag("brainflow:menu");
    return;
  }
  syncLauncherToMode(UiMode::BrainFlow);
  gLauncherVisible=true;
  setStatus("Games",700);setKeyDiag("brainflow:launcher");
}

bool routeBrainFlowEscape() {
  if (gUiMode!=UiMode::BrainFlow || gLauncherVisible || gRecording || gPlaybackActive ||
      gRealtimeActive || gRealtimeRecording) return false;
  brainFlowEscape();
  return true;
}

bool handleBrainFlowKey(char key, const Keyboard_Class::KeysState& keys) {
  brainLoadBest();
  const uint32_t now=millis();
  gBrainFlow.tick(now);brainSaveResult();
  const auto phase=gBrainFlow.phase;
  const bool enter=keys.enter||key=='\r'||key=='\n';
  if (phase==brainflow::Phase::Menu || phase==brainflow::Phase::Finished) {
    if ((phase==brainflow::Phase::Menu && key>='1'&&key<='4') || enter) {
      const auto game=phase==brainflow::Phase::Menu&&key>='1'&&key<='4'
          ? static_cast<brainflow::Game>(key-'1') : gBrainFlow.game;
      gBrainResultSaved=false;gBrainFlow.start(game,now,esp_random());
    }
    return true;
  }
  if (phase==brainflow::Phase::Paused) {
    if (enter || keys.space || key==' ')gBrainFlow.resume(now);
    return true;
  }
  if (keys.space || key==' ') {gBrainFlow.pause(now);return true;}
  if (keys.ctrl || keys.fn) return true;
  if (keys.del || key=='\b') {gBrainFlow.erase(now);return true;}
  if (gBrainFlow.game==brainflow::Game::Fraction || gBrainFlow.game==brainflow::Game::Meaning) {
    key=static_cast<char>(tolower(static_cast<unsigned char>(key)));
    int choice=key=='a'?0:key=='s'?1:key=='d'?2:-1;
    gBrainFlow.submit(now,choice);
  } else if (enter) gBrainFlow.submit(now);
  else gBrainFlow.type(key,now);
  return true; // Never leak game keys into chat compose or network commands.
}

void brainCentered(const String& value,int y,uint16_t color,int width=206) {
  auto& d=gCanvas;
  const String text=fitCurrentFontToWidth(value,width);
  d.setTextColor(color,rgb565_local(9,20,28));
  d.setCursor((240-d.textWidth(text))/2,y);d.print(text);
}

void renderBrainFlowUi() {
  brainLoadBest();
  auto& d=gCanvas;const auto& game=gBrainFlow;
  const uint16_t bg=rgb565_local(2,7,10), panel=rgb565_local(9,20,28);
  const uint16_t line=rgb565_local(40,55,68), muted=rgb565_local(130,145,155);
  const uint16_t green=rgb565_local(88,240,141), red=rgb565_local(255,104,120);
  const uint16_t accent=brainAccent(static_cast<uint8_t>(game.game));
  const bool menu=game.phase==brainflow::Phase::Menu, feedback=game.phase==brainflow::Phase::Feedback;
  const uint16_t frame=feedback?(game.lastCorrect?green:red):line;
  d.fillScreen(bg);d.fillRoundRect(5,5,230,125,10,panel);d.drawRoundRect(5,5,230,125,10,frame);
  d.setFont(&fonts::Font2);d.setTextColor(accent,panel);d.setCursor(14,10);
  d.print(fitCurrentFontToWidth(menu?"BRAINFLOW":brainflow::name(game.game),169));
  d.setFont(&fonts::Font0);d.setTextColor(muted,panel);
  String clock=menu?"4 GAMES":String((game.remaining+999)/1000)+"s";
  d.setCursor(226-d.textWidth(clock),16);d.print(clock);d.drawFastHLine(14,30,212,line);
  if(game.phase==brainflow::Phase::Playing) {
    const uint32_t step=millis()/90;
    for(uint8_t i=0;i<3;++i)d.fillCircle(188+(step+i*12)%36,30,1,accent);
  }
  if(menu) {
    for(uint8_t i=0;i<4;++i) {
      const int y=37+i*18;const uint16_t color=brainAccent(i);
      d.drawRoundRect(13,y-2,214,16,4,i==static_cast<uint8_t>(game.game)?color:line);
      d.setTextColor(color,panel);d.setCursor(19,y+2);
      d.print(String(i+1)+"  "+brainflow::name(static_cast<brainflow::Game>(i)));
    }
    brainCentered("1-4 start   Esc launcher",118,muted);
  } else if(game.phase==brainflow::Phase::Paused) {
    d.setFont(&fonts::Font4);brainCentered("PAUSED",46,brainAccent(2));
    d.setFont(&fonts::Font0);brainCentered("Time is safe. Take a breath.",84,muted);
    brainCentered("Enter resume   Esc games",118,muted);
  } else if(game.phase==brainflow::Phase::Finished) {
    d.setFont(&fonts::Font4);brainCentered(String(game.score),39,accent);
    d.setFont(&fonts::Font0);brainCentered("ROUND COMPLETE",72,muted);
    brainCentered(String(game.correct)+" RIGHT  "+String(game.accuracy())+"%  BEST "+String(gBrainBest[static_cast<uint8_t>(game.game)]),92,green);
    brainCentered("Enter replay   Esc games",118,muted);
  } else {
    d.setFont(&fonts::Font0);
    const String hint=game.game==brainflow::Game::Word?String("UNSCRAMBLE / ")+game.clue:String(game.clue);
    brainCentered(hint,37,muted);
    if(game.game==brainflow::Game::Fraction) {
      d.setFont(&fonts::Font4);
      const String left=String(game.n1)+"/"+String(game.d1), right=String(game.n2)+"/"+String(game.d2);
      d.setTextColor(accent,panel);d.setCursor(68-d.textWidth(left)/2,50);d.print(left);
      d.setCursor(175-d.textWidth(right)/2,50);d.print(right);
      d.setFont(&fonts::Font2);brainCentered("?",54,muted);
      d.fillRoundRect(30,82,76,4,2,line);d.fillRoundRect(30,82,76*game.n1/game.d1,4,2,accent);
      d.fillRoundRect(137,82,76,4,2,line);d.fillRoundRect(137,82,76*game.n2/game.d2,4,2,accent);
      d.setFont(&fonts::Font0);brainCentered("A less  S equal  D more",94,TFT_WHITE);
    } else if(game.game==brainflow::Game::Meaning) {
      d.setFont(&fonts::Font4);brainCentered(game.prompt,50,accent);
      d.setFont(&fonts::Font0);
      for(uint8_t i=0;i<3;++i) {
        const int x=14+i*72;
        d.drawRoundRect(x,79,68,25,4,line);
        d.setTextColor(accent,panel);d.setCursor(x+31,82);d.print("ASD"[i]);
        const String label=game.options[i];
        d.setTextColor(TFT_WHITE,panel);d.setCursor(x+(68-d.textWidth(label))/2,94);d.print(label);
      }
    } else {
      d.setFont(&fonts::Font4);brainCentered(game.prompt,50,accent);
      d.fillRoundRect(41,82,158,22,5,rgb565_local(18,37,49));d.drawRoundRect(41,82,158,22,5,feedback?frame:accent);
      d.setFont(&fonts::Font2);
      const String input=feedback?String(game.answer):String(game.input)+(millis()/450%2?"_":" ");
      // Text stays inside the input panel, with a measured maximum width.
      const String fitted=fitCurrentFontToWidth(input,142);
      d.setTextColor(feedback?frame:TFT_WHITE,rgb565_local(18,37,49));d.setCursor((240-d.textWidth(fitted))/2,85);d.print(fitted);
    }
    d.setFont(&fonts::Font0);
    if(feedback) {
      const String result=game.lastCorrect?"NICE!":String("ANSWER: ")+game.answer;
      brainCentered(result,106,frame);
    } else {
      d.setTextColor(muted,panel);d.setCursor(15,106);d.print("PTS "+String(game.score));
      String streak="STREAK "+String(game.streak);d.setTextColor(accent,panel);d.setCursor(226-d.textWidth(streak),106);d.print(streak);
    }
    d.fillRect(14,116,212,2,line);const int progress=212UL*game.remaining/brainflow::kRoundMs;
    if(progress>0)d.fillRect(14,116,progress,2,accent);
    const bool choice=game.game==brainflow::Game::Fraction || game.game==brainflow::Game::Meaning;
    brainCentered(choice?"A/S/D choose  Space pause":"Enter check Del edit Space pause",121,muted);
  }
  d.setFont(&fonts::Font2);renderLauncherOverlay();renderDebugOverlay();
}

}  // namespace
