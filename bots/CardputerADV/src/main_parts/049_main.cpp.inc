  if (escDown) {
    handleGlobalEscape();
  } else if (topicPrevCombo) {
    bool ok = switchTopicRelative(-1, true);
    if (ok && gUiMode != UiMode::Contexts) {
      setStatus("Topic " + gContexts[gSelectedContext].shortName, 900);
    }
	  } else if (topicNextCombo) {
	    bool ok = switchTopicRelative(1, true);
	    if (ok && gUiMode != UiMode::Contexts) {
	      setStatus("Topic " + gContexts[gSelectedContext].shortName, 900);
	    }
  } else if (optionViewToggle) {
    togglePulseChatDisplay();
	  } else if (tabPrevCombo) {
	    switchLauncherGroupAndApply(-1);
  } else if (tabNextCombo) {
    switchLauncherGroupAndApply(1);
  } else if (tabDown) {
    if (gLauncherVisible) {
      moveLauncherSubSelection(1);
      const LauncherGroup& group = kLauncherGroups[gLauncherSelection];
      setStatus(group.items[gLauncherSubSelection].label, 500);
      setKeyDiag("menu:sub");
    } else {
      gLauncherVisible = true;
      setStatus("Menu", 700);
      setKeyDiag("menu:on");
    }
  } else if (gLauncherVisible && leftDown) {
    moveLauncherSubSelection(-1);
  } else if (gLauncherVisible && rightDown) {
    moveLauncherSubSelection(1);
  } else if (gLauncherVisible && upDown) {
    moveLauncherSelection(-1);
  } else if (gLauncherVisible && downDown) {
    moveLauncherSelection(1);
  } else if (!gLauncherVisible && gPlaybackActive && upDown) {
    adjustSpeakerVolume(24);
    setKeyDiag("vol:up");
  } else if (!gLauncherVisible && gPlaybackActive && downDown) {
    adjustSpeakerVolume(-24);
    setKeyDiag("vol:down");
  } else if (gUiMode == UiMode::Contexts && (leftDown || upDown)) {
    handleTypingKey(',', keys);
  } else if (gUiMode == UiMode::Contexts && (rightDown || downDown)) {
    handleTypingKey('.', keys);
  } else if (gUiMode == UiMode::Notes && (leftDown || upDown)) {
    handleTypingKey(',', keys);
  } else if (gUiMode == UiMode::Notes && (rightDown || downDown)) {
    handleTypingKey('.', keys);
  } else if (spaceDown && keys.ctrl) {
    handleTypingKey(' ', keys);
  } else if (enterDown) {
    handleTypingKey('\n', keys);
  } else if (delDown) {
    handleTypingKey('\b', keys);
  } else if (charDown) {
    handleTypingKey(curWordChar, keys);
  } else if (keys.fn || keys.ctrl || keys.alt || keys.opt) {
    setKeyDiag("mods=" + keyboardSignatureFromState());
  }

  pEnter = keys.enter;
  pDel = keys.del;
  pTab = keys.tab;
  pSpace = keys.space;
  pUp = upHeld;
  pDown = downHeld;
  pLeft = leftHeld;
	  pRight = rightHeld;
	  pEsc = escHeld;
  pOpt = keys.opt;
	  pWordSignature = wordSignature;
	}

bool ctrlAlonePressedNow() {
  return gKeyboardState.ctrl && !gKeyboardState.fn && !gKeyboardState.shift &&
         !gKeyboardState.alt && !gKeyboardState.opt && !gKeyboardState.tab &&
         !gKeyboardState.enter && !gKeyboardState.del && !gKeyboardState.space &&
         gKeyboardState.word.empty();
}

void updateVoiceTrigger() {
  bool held = ctrlAlonePressedNow();
  bool ctrlComboActive = gKeyboardState.ctrl &&
                         (!gKeyboardState.word.empty() || gKeyboardState.space ||
                          gKeyboardState.tab || gKeyboardState.enter ||
                          gKeyboardState.del || gKeyboardState.fn ||
                          gKeyboardState.shift || gKeyboardState.alt ||
                          gKeyboardState.opt);
  if (gRecording && millis() - gRecordStartMs >= currentRecordLimitMs()) {
    logf("[VOICE] auto-stop at max duration");
    stopRecordingAndSend();
    return;
  }
  if (ctrlComboActive) {
    gCtrlSoloCandidate = false;
    gLastCtrlTapMs = 0;
  }
  if (held && !gCtrlSoloHeld) {
    gCtrlSoloHeld = true;
    gCtrlSoloCandidate = !ctrlComboActive;
    uint32_t now = millis();
    if (gCtrlSoloCandidate && gLastCtrlTapMs != 0 &&
        now - gLastCtrlTapMs <= kCtrlDoubleTapWindowMs) {
      gLastCtrlTapMs = 0;
      gCtrlSoloCandidate = false;
      if (gRecording) {
        logf("[VOICE] ctrl double tap stop");
        stopRecordingAndSend();
      } else {
        logf("[VOICE] ctrl double tap start");
        startRecording();
      }
      return;
    }
    if (gCtrlSoloCandidate) {
      gLastCtrlTapMs = now;
      setStatus(gRecording ? "Ctrl x2 stop" : "Ctrl x2 start", 600);
    }
    return;
  }
  if (gCtrlSoloHeld && !held) {
    gCtrlSoloHeld = false;
    gCtrlSoloCandidate = false;
  }
  if (gLastCtrlTapMs != 0 && millis() - gLastCtrlTapMs > kCtrlDoubleTapWindowMs) {
    gLastCtrlTapMs = 0;
  }
}

void updateG0VoiceTrigger() {
  bool pressed = M5.BtnA.isPressed();
  if (gUiMode == UiMode::Ota) {
    if (pressed && !gG0VoiceHeld) {
      gG0VoiceHeld = true;
      pulseButtonPress();
      setStatus("Release G0 to apply OTA", 1200);
      appendRuntimeLog("OTA", "G0 press", false);
      return;
    }
    if (!pressed && gG0VoiceHeld) {
      gG0VoiceHeld = false;
      setStatus("OTA apply requested", 700);
      appendRuntimeLog("OTA", "apply requested by G0", true);
      render();
      applyOtaFromManifest();
      return;
    }
    return;
  }
  if (pressed && !gG0VoiceHeld) {
    gG0VoiceHeld = true;
    gLastCtrlTapMs = 0;
    gCtrlSoloCandidate = false;
    if (!gRecording) {
      logf("[VOICE] G0 hold start");
      startRecording();
    }
    return;
  }
  if (!pressed && gG0VoiceHeld) {
    gG0VoiceHeld = false;
    if (gRecording) {
      logf("[VOICE] G0 hold stop");
      stopRecordingAndSend();
    }
  }
}

void updateBlink() {
  uint32_t now = millis();
  if (gBlinkEndMs == 0 && now >= gNextBlinkMs) {
    gBlinkEndMs = now + 120;
  } else if (gBlinkEndMs != 0 && now >= gBlinkEndMs) {
    gBlinkEndMs = 0;
    scheduleBlink();
  }
}

void drawEye(int16_t cx, int16_t cy, int16_t halfW, int16_t halfH, float openness,
             uint16_t irisColor, int16_t pupilOffsetX, int16_t pupilOffsetY) {
  auto& d = gCanvas;
  int16_t eyeH = max<int16_t>(3, static_cast<int16_t>(halfH * openness));
  if (eyeH <= 4) {
    d.drawFastHLine(cx - halfW, cy, halfW * 2, irisColor);
    return;
  }

  d.fillRoundRect(cx - halfW, cy - eyeH, halfW * 2, eyeH * 2, 8, TFT_BLACK);
  d.drawRoundRect(cx - halfW, cy - eyeH, halfW * 2, eyeH * 2, 8, irisColor);
  d.fillRoundRect(cx - halfW + 2, cy - eyeH + 2, halfW * 2 - 4, eyeH * 2 - 4, 7, TFT_DARKGREY);
  d.fillCircle(cx + pupilOffsetX, cy + pupilOffsetY, max<int16_t>(3, eyeH / 3), irisColor);
  d.fillCircle(cx + pupilOffsetX, cy + pupilOffsetY, max<int16_t>(2, eyeH / 5), TFT_BLACK);
}

void drawHeartGlyph(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  auto& d = gCanvas;
  int16_t r = max<int16_t>(1, size / 3);
  d.fillCircle(cx - r, cy - r / 2, r, color);
  d.fillCircle(cx + r, cy - r / 2, r, color);
  d.fillTriangle(cx - size, cy, cx + size, cy, cx, cy + size, color);
}

void drawSparkGlyph(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  auto& d = gCanvas;
  d.drawLine(cx - size, cy, cx + size, cy, color);
  d.drawLine(cx, cy - size, cx, cy + size, color);
  d.drawLine(cx - size + 1, cy - size + 1, cx + size - 1, cy + size - 1, color);
  d.drawLine(cx - size + 1, cy + size - 1, cx + size - 1, cy - size + 1, color);
}

void drawArcEye(int16_t cx, int16_t cy, int16_t halfW, int16_t arch, uint16_t color,
                bool centerHigh, uint8_t thickness = 2) {
  auto& d = gCanvas;
  if (halfW <= 1) {
    d.drawPixel(cx, cy, color);
    return;
  }
  for (int16_t dx = -halfW; dx <= halfW; ++dx) {
    float ratio = 1.0f - (static_cast<float>(dx * dx) / static_cast<float>(halfW * halfW));
    if (ratio < 0.0f) {
      ratio = 0.0f;
    }
    int16_t curve = static_cast<int16_t>(arch * ratio);
    int16_t py = centerHigh ? (cy - curve) : (cy + curve);
    d.fillRect(cx + dx, py - thickness / 2, 1, max<uint8_t>(1, thickness), color);
  }
}

void drawBrowStroke(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, uint8_t thickness = 2) {
  auto& d = gCanvas;
  int16_t half = max<int16_t>(0, thickness / 2);
  for (int16_t t = -half; t <= half; ++t) {
    d.drawLine(x1, y1 + t, x2, y2 + t, color);
  }
}

