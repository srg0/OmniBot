  } else if (gUiMode == UiMode::ChatFull || gUiMode == UiMode::ChatFace) {
    setUiMode(UiMode::Home);
    setStatus("View: pulse", 900);
  } else {
    setUiMode(UiMode::Home);
    setStatus("View: pulse", 900);
  }
  showTopicOverlay(0, 1200);
}

void switchLauncherGroupAndApply(int delta) {
  syncLauncherToMode(gUiMode);
  gLauncherVisible = false;
  gLauncherSelection = (gLauncherSelection + delta + kLauncherGroupCount) % kLauncherGroupCount;
  uint8_t count = kLauncherGroups[gLauncherSelection].count;
  gLauncherSubSelection = clampValue<int>(gLauncherLastSub[gLauncherSelection], 0, count - 1);
  applyLauncherSelection();
}

size_t replyVisualLineCount(const String& reply) {
  String full = "AI: " + reply;
  auto wrapped = wrapText(full, 216);
  return wrapped.size();
}

void appendAssistantReply(const String& reply) {
  String displayReply = cleanReplyForDevice(reply);
  if (displayReply.isEmpty()) {
    return;
  }
  flashActivityLed(0, 180, 0);
  if (replyVisualLineCount(displayReply) > kVoicePreviewLineThreshold) {
    pushAssistant(trimPreview(displayReply) + " [voice]");
  } else {
    pushAssistant(displayReply);
  }
}

bool setupKeyboardInput() {
  memset(gTcaPressedKeys, 0, sizeof(gTcaPressedKeys));
  Wire1.begin(kTca8418SdaPin, kTca8418SclPin);
  Wire1.setClock(400000);
  Wire1.setTimeOut(50);
  delay(50);
  gUseTcaKeyboard = gTcaKeyboard.begin(kTca8418I2cAddr, &Wire1);
  if (gUseTcaKeyboard) {
    gTcaKeyboard.matrix(7, 8);
    gTcaKeyboard.flush();
    pinMode(kTca8418IntPin, INPUT_PULLUP);
    gTcaKeyboard.enableInterrupts();
    attachInterrupt(digitalPinToInterrupt(kTca8418IntPin), onTcaKeyboardInterrupt, FALLING);
    gTcaInterruptPending = true;
    logf("[KEY] using TCA8418 keyboard");
    setKeyDiag("kbd:tca");
    return true;
  }

  Wire1.end();
  M5Cardputer.Keyboard.begin();
  logf("[KEY] TCA8418 unavailable, fallback to matrix keyboard");
  setKeyDiag("kbd:matrix");
  return false;
}

void synthesizeTcaKeyboardState() {
  gKeyboardState.reset();

  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 14; ++col) {
      if (!gTcaPressedKeys[row][col]) {
        continue;
      }
      uint8_t keyFirst = static_cast<uint8_t>(_key_value_map[row][col].value_first);
      switch (keyFirst) {
        case KEY_FN:
          gKeyboardState.fn = true;
          break;
        case KEY_LEFT_SHIFT:
          gKeyboardState.shift = true;
          gKeyboardState.modifier_keys.push_back(KEY_LEFT_SHIFT);
          break;
        case KEY_LEFT_CTRL:
          gKeyboardState.ctrl = true;
          gKeyboardState.modifier_keys.push_back(KEY_LEFT_CTRL);
          break;
        case KEY_LEFT_ALT:
          gKeyboardState.alt = true;
          gKeyboardState.modifier_keys.push_back(KEY_LEFT_ALT);
          break;
        case KEY_OPT:
          gKeyboardState.opt = true;
          break;
      }
    }
  }

  for (auto modifier : gKeyboardState.modifier_keys) {
    gKeyboardState.modifiers |= (1 << (modifier - 0x80));
  }

  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 14; ++col) {
      if (!gTcaPressedKeys[row][col]) {
        continue;
      }
      uint8_t keyFirst = static_cast<uint8_t>(_key_value_map[row][col].value_first);
      switch (keyFirst) {
        case KEY_FN:
        case KEY_LEFT_SHIFT:
        case KEY_LEFT_CTRL:
        case KEY_LEFT_ALT:
        case KEY_OPT:
          continue;
        case KEY_TAB:
          gKeyboardState.tab = true;
          gKeyboardState.hid_keys.push_back(KEY_TAB);
          continue;
        case KEY_BACKSPACE:
          gKeyboardState.del = true;
          gKeyboardState.hid_keys.push_back(KEY_BACKSPACE);
          continue;
        case KEY_ENTER:
          gKeyboardState.enter = true;
          gKeyboardState.hid_keys.push_back(KEY_ENTER);
          continue;
        default:
          break;
      }

      char printable = gKeyboardState.shift ? _key_value_map[row][col].value_second
                                            : _key_value_map[row][col].value_first;
      if (printable == ' ') {
        gKeyboardState.space = true;
      }
      if (printable >= 32 && printable < 127) {
        gKeyboardState.word.push_back(printable);
        uint8_t hid = _kb_asciimap[static_cast<uint8_t>(printable)];
        if (hid) {
          gKeyboardState.hid_keys.push_back(hid);
        }
      }
    }
  }
}

void pollTcaKeyboardEvents() {
  if (!gUseTcaKeyboard) {
    return;
  }

  int safety = 32;
  while (safety-- > 0) {
    bool pendingIrq = gTcaInterruptPending || digitalRead(kTca8418IntPin) == LOW;
    int available = gTcaKeyboard.available();
    if (!pendingIrq && available <= 0) {
      break;
    }
    gTcaInterruptPending = false;

    int keyEvent = gTcaKeyboard.getEvent();
    if (keyEvent <= 0) {
      if (available <= 0) {
        break;
      }
      continue;
    }

    bool pressed = (keyEvent & 0x80) != 0;
    uint8_t rawValue = static_cast<uint8_t>(keyEvent & 0x7F);
    uint8_t row = 0xFF;
    uint8_t col = 0xFF;
    mapTcaRawKeyToPhysical(rawValue, row, col);
    if (row >= 4 || col >= 14) {
      continue;
    }

    gTcaPressedKeys[row][col] = pressed;
    logf("[KEY:TCA] raw=%u pressed=%d row=%u col=%u",
         static_cast<unsigned>(rawValue),
         pressed ? 1 : 0,
         static_cast<unsigned>(row),
         static_cast<unsigned>(col));
  }

  uint8_t intStatus = gTcaKeyboard.readRegister(TCA8418_REG_INT_STAT);
  if (intStatus & TCA8418_REG_STAT_OVR_FLOW_INT) {
    logf("[KEY:TCA] overflow -> flush");
    gTcaKeyboard.flush();
    memset(gTcaPressedKeys, 0, sizeof(gTcaPressedKeys));
  } else if (intStatus != 0) {
    gTcaKeyboard.readRegister(TCA8418_REG_GPIO_INT_STAT_1);
    gTcaKeyboard.readRegister(TCA8418_REG_GPIO_INT_STAT_2);
    gTcaKeyboard.readRegister(TCA8418_REG_GPIO_INT_STAT_3);
    gTcaKeyboard.writeRegister(TCA8418_REG_INT_STAT, 0x0F);
  }

  synthesizeTcaKeyboardState();
}

void refreshKeyboardState() {
  if (gUseTcaKeyboard) {
    pollTcaKeyboardEvents();
  } else {
    M5Cardputer.Keyboard.updateKeyList();
    M5Cardputer.Keyboard.updateKeysState();
    gKeyboardState = M5Cardputer.Keyboard.keysState();
  }
  String sig = keyboardSignatureFromState();
  if (sig != gLastKeySignature) {
    gLastKeySignature = sig;
    setKeyDiag(sig.isEmpty() ? "-" : sig);
    if (!sig.isEmpty()) {
      logf("[KEY] %s", sig.c_str());
    }
  }
}

String keyboardSignatureFromState() {
  String sig;
  sig.reserve(48);
  if (gKeyboardState.fn) {
    sig += "fn|";
  }
  if (gKeyboardState.shift) {
    sig += "shift|";
  }
  if (gKeyboardState.ctrl) {
    sig += "ctrl|";
  }
  if (gKeyboardState.alt) {
    sig += "alt|";
  }
  if (gKeyboardState.opt) {
    sig += "opt|";
  }
  if (gKeyboardState.tab) {
    sig += "tab|";
  }
  if (gKeyboardState.enter) {
    sig += "enter|";
  }
  if (gKeyboardState.del) {
    sig += "del|";
  }
  for (char c : gKeyboardState.word) {
