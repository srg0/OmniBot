    d.setCursor(18, 99);
    d.print(fitCurrentFontToWidth(gAssetManifest.error, 202));
  } else {
    int fillW = ready ? min<int>(128, max<int>(12, gAssetManifest.fileCount * 128 / max<int>(1, gAssetManifest.fileCount))) : 24 + ((millis() / 90) % 82);
    d.fillRoundRect(91, 100, 128, 7, 4, rgb565_local(19, 35, 46));
    d.fillRoundRect(92, 101, fillW, 5, 3, ready ? green : amber);
  }

  drawActionChip(17, 112, 58, "F", "fetch", green, panel2);
  drawActionChip(81, 112, 58, "S", "sync", cyan, panel2);
  drawActionChip(145, 112, 58, "R", "load", amber, panel2);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderOtaUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Firmware OTA");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  auto& d = gCanvas;
  const bool hasCandidate = gOtaManifest.parsed && !gOtaManifest.version.isEmpty();
  const bool updateReady = hasCandidate && gOtaManifest.version != kAppVersion;
  const uint16_t bg = rgb565_local(3, 5, 10);
  const uint16_t panel = rgb565_local(12, 12, 20);
  const uint16_t panel2 = rgb565_local(25, 16, 24);
  const uint16_t row = rgb565_local(18, 24, 35);
  const uint16_t red = rgb565_local(255, 83, 92);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t muted = rgb565_local(126, 143, 157);
  const uint16_t accent = gOtaPendingVerify ? amber : (updateReady ? red : green);

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 14, panel);
  d.drawRoundRect(6, 5, 228, 124, 14, accent);
  d.drawRoundRect(10, 9, 220, 116, 11, rgb565_local(42, 36, 48));

  d.setFont(&fonts::Font2);
  d.setTextColor(accent, panel);
  d.setCursor(16, 14);
  d.print("FIRMWARE OTA");
  d.setFont(&fonts::Font0);
  String state = gOtaPendingVerify ? "VERIFY" : updateReady ? "UPDATE" : hasCandidate ? "CURRENT" : "FETCH";
  d.setTextColor(accent, panel);
  d.setCursor(224 - d.textWidth(state), 17);
  d.print(state);

  drawServiceOrb(48, 66, accent, cyan, "OTA", updateReady || gOtaPendingVerify);

  int x = 91;
  drawInfoRow(x, 34, 128, "current", String(kAppVersion), row, muted, TFT_WHITE);
  drawInfoRow(x, 49, 128, "candidate", gOtaManifest.version.isEmpty() ? "-" : gOtaManifest.version,
              row, muted, updateReady ? red : green);
  drawInfoRow(x, 64, 128, "size", gOtaManifest.size ? compactBytes(gOtaManifest.size) : "-",
              row, muted, cyan);
  drawInfoRow(x, 79, 128, "slot", gOtaRunningSlot + " " + (gOtaPendingVerify ? "pending" : gOtaBootState),
              row, muted, gOtaPendingVerify ? amber : muted);

  d.setFont(&fonts::Font0);
  d.setTextColor(gOtaConfirmedThisBoot ? green : (gOtaPendingVerify ? amber : muted), panel);
  d.setCursor(18, 99);
  if (!gOtaManifest.error.isEmpty()) {
    d.setTextColor(amber, panel);
    d.print(fitCurrentFontToWidth(gOtaManifest.error, 202));
  } else if (gOtaPendingVerify) {
    d.print("new app is pending verification");
  } else if (updateReady) {
    d.print("ready to download and flash");
  } else if (hasCandidate) {
    d.print("installed version is current");
  } else {
    d.print("fetch manifest from bridge");
  }

  if (gOtaPendingVerify) {
    drawActionChip(17, 112, 58, "C", "confirm", green, panel2);
    drawActionChip(81, 112, 58, "X", "rollback", red, panel2);
    drawActionChip(145, 112, 58, "F", "fetch", cyan, panel2);
  } else {
    drawActionChip(17, 112, 58, "F", "fetch", cyan, panel2);
    drawActionChip(81, 112, 58, "A", "apply", red, panel2);
    drawActionChip(145, 112, 58, "R", "check", amber, panel2);
  }
  renderLauncherOverlay();
  renderDebugOverlay();
}

String settingLabel(uint8_t index) {
  switch (index) {
    case 0:
      return "Focus";
    case 1:
      return "Short break";
    case 2:
      return "Long break";
    case 3:
      return "Cycles";
    case 4:
      return "Auto focus";
    case 5:
      return "Auto OTA";
    case 6:
      return "BPM";
    case 7:
      return "OTA battery";
    case 8:
      return "Audio lang";
    case 9:
      return "Home face";
    default:
      return "-";
  }
}

String settingValue(uint8_t index) {
  switch (index) {
    case 0:
      return String(gFocusSettings.focusSec / 60) + "m";
    case 1:
      return String(gFocusSettings.shortBreakSec / 60) + "m";
    case 2:
      return String(gFocusSettings.longBreakSec / 60) + "m";
    case 3:
      return String(gFocusSettings.cyclesPerRound);
    case 4:
      return gFocusSettings.autoStart ? "on" : "off";
    case 5:
      return gDeviceSettings.autoUpdateFirmware ? "on" : "off";
    case 6:
      return String(gFocusSettings.bpm);
    case 7:
      return String(gDeviceSettings.minBatteryForUpdate) + "%";
    case 8:
      return gDeviceSettings.audioLanguage;
    case 9:
      return clockFaceName(gDeviceSettings.clockFace);
    default:
      return "-";
  }
}

void renderSettingsUi() {
  drawCardHeader("Settings", 0x7BEF);
  auto& d = gCanvas;
  gSelectedSetting = clampValue<int>(gSelectedSetting, 0, kSettingsItemCount - 1);
  constexpr int visible = 7;
  int start = max<int>(0, min<int>(gSelectedSetting - visible / 2, kSettingsItemCount - visible));
  d.setFont(&fonts::Font0);
  for (int i = 0; i < visible; ++i) {
    int idx = start + i;
    if (idx >= kSettingsItemCount) {
      break;
    }
    int y = 34 + i * 11;
    bool selected = idx == gSelectedSetting;
    uint16_t bg = selected ? 0x4208 : 0x0841;
    d.fillRoundRect(14, y - 1, 212, 10, 3, bg);
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, bg);
    d.setCursor(18, y + 1);
    d.print(fitCurrentFontToWidth(settingLabel(idx), 126));
    String value = settingValue(idx);
    String fittedValue = fitCurrentFontToWidth(value, 72);
    int w = d.textWidth(fittedValue);
    d.setCursor(max<int>(144, 222 - w), y + 1);
    d.print(fittedValue);
  }
  d.setFont(&fonts::Font2);
  drawTinyFooter(0x0841, TFT_LIGHTGREY, "< > pick  +/- edit  Space toggle", 16, 116, 208);
  renderLauncherOverlay();
  renderDebugOverlay();
}

void renderContextsUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(3, 6, 12);
  const uint16_t panel = rgb565_local(8, 14, 24);
  const uint16_t panel2 = rgb565_local(12, 22, 34);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t violet = rgb565_local(170, 130, 255);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t green = rgb565_local(100, 255, 170);
  const uint16_t muted = rgb565_local(135, 151, 166);

  d.fillScreen(bg);
  uint32_t now = millis();
  int meshPhase = static_cast<int>((now / 90) % 24);
  for (int x = -24; x < 260; x += 24) {
    d.drawFastVLine(x + meshPhase, 0, 135, rgb565_local(7, 18, 28));
  }
