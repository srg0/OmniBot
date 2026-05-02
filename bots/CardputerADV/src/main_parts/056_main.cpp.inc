  if (edit) {
    d.fillRoundRect(12, 98, 216, 18, 7, rgb565_local(2, 17, 22));
    d.drawRoundRect(12, 98, 216, 18, 7, gNoteInputBuffer.isEmpty() ? line : green);
    d.setTextColor(gNoteInputBuffer.isEmpty() ? muted : TFT_WHITE, rgb565_local(2, 17, 22));
    d.setFont(&fonts::efontCN_12);
    String input = gNoteInputBuffer.isEmpty() ? "type note line..." : tailUtf8ToFit(gNoteInputBuffer, 190);
    d.setCursor(20, 101);
    d.print(input);
    d.setFont(&fonts::Font0);
    d.setTextColor(green, panel);
    d.setCursor(16, 119);
    d.print("Enter save line  Ctrl+S done");
  } else {
    d.setFont(&fonts::Font0);
    d.setTextColor(muted, panel);
    d.setCursor(16, 119);
    d.print("< > scroll  E append  L list");
  }
  renderLauncherOverlay();
  renderDebugOverlay();
}

void drawServiceOrb(int16_t cx, int16_t cy, uint16_t accent, uint16_t secondary, const String& label, bool active) {
  auto& d = gCanvas;
  uint32_t now = millis();
  float t = now / 1000.0f;
  int pulse = active ? static_cast<int>((sinf(t * 4.0f) + 1.0f) * 2.0f) : 0;
  d.fillCircle(cx, cy, 37, rgb565_local(3, 12, 18));
  d.drawCircle(cx, cy, 34 + pulse, rgb565_local(18, 45, 58));
  d.drawArc(cx, cy, 31, 25, static_cast<int>(t * 42.0f) % 360,
            (static_cast<int>(t * 42.0f) + 250) % 360, accent);
  d.drawArc(cx, cy, 20, 15, (220 - static_cast<int>(t * 64.0f)) % 360,
            (40 - static_cast<int>(t * 64.0f)) % 360, secondary);
  d.fillCircle(cx, cy, 12, active ? accent : rgb565_local(13, 31, 40));
  d.setFont(&fonts::Font0);
  d.setTextColor(active ? rgb565_local(2, 6, 12) : accent, active ? accent : rgb565_local(13, 31, 40));
  d.setCursor(cx - d.textWidth(label) / 2, cy - 3);
  d.print(label);
  d.setFont(&fonts::Font2);
}

void drawInfoRow(int16_t x, int16_t y, int16_t w, const String& label, const String& value,
                 uint16_t bg, uint16_t labelColor, uint16_t valueColor) {
  auto& d = gCanvas;
  d.fillRoundRect(x, y, w, 12, 4, bg);
  d.setFont(&fonts::Font0);
  d.setTextColor(labelColor, bg);
  d.setCursor(x + 6, y + 3);
  d.print(fitCurrentFontToWidth(label, 52));
  String fitted = fitCurrentFontToWidth(value, w - 68);
  d.setTextColor(valueColor, bg);
  d.setCursor(x + w - 7 - d.textWidth(fitted), y + 3);
  d.print(fitted);
  d.setFont(&fonts::Font2);
}

void drawActionChip(int16_t x, int16_t y, int16_t w, const String& key, const String& label,
                    uint16_t accent, uint16_t bg) {
  auto& d = gCanvas;
  d.fillRoundRect(x, y, w, 14, 5, bg);
  d.drawRoundRect(x, y, w, 14, 5, accent);
  d.setFont(&fonts::Font0);
  d.setTextColor(accent, bg);
  d.setCursor(x + 5, y + 4);
  d.print(key);
  d.setTextColor(TFT_LIGHTGREY, bg);
  d.setCursor(x + 18, y + 4);
  d.print(fitCurrentFontToWidth(label, w - 23));
  d.setFont(&fonts::Font2);
}

void renderAssetsUi() {
  if (!gSdReady) {
    renderSdRequiredBanner("Asset Sync");
    renderLauncherOverlay();
    renderDebugOverlay();
    return;
  }
  auto& d = gCanvas;
  fs::FS* fs = activeVoiceFs();
  const bool indexReady = fs && fsExists(*fs, kAudioIndexPath);
  const bool ready = gAssetManifest.parsed && gAssetManifest.fileCount > 0;
  const uint16_t bg = rgb565_local(2, 7, 10);
  const uint16_t panel = rgb565_local(7, 14, 24);
  const uint16_t panel2 = rgb565_local(10, 24, 34);
  const uint16_t row = rgb565_local(13, 28, 40);
  const uint16_t cyan = rgb565_local(47, 227, 255);
  const uint16_t green = rgb565_local(88, 240, 141);
  const uint16_t amber = rgb565_local(255, 194, 74);
  const uint16_t muted = rgb565_local(126, 143, 157);

  d.fillScreen(bg);
  d.fillRoundRect(6, 5, 228, 124, 14, panel);
  d.drawRoundRect(6, 5, 228, 124, 14, ready ? green : amber);
  d.drawRoundRect(10, 9, 220, 116, 11, rgb565_local(18, 34, 48));

  d.setFont(&fonts::Font2);
  d.setTextColor(green, panel);
  d.setCursor(16, 14);
  d.print("ASSET SYNC");
  d.setFont(&fonts::Font0);
  String state = ready ? "READY" : (gAssetManifest.present ? "CHECK" : "FETCH");
  d.setTextColor(ready ? green : amber, panel);
  d.setCursor(224 - d.textWidth(state), 17);
  d.print(state);

  drawServiceOrb(48, 66, green, cyan, "AS", ready);

  int x = 91;
  drawInfoRow(x, 34, 128, "manifest", gAssetManifest.present ? (gAssetManifest.parsed ? "ok" : "bad") : "missing",
              row, muted, ready ? green : amber);
  drawInfoRow(x, 49, 128, "version", gAssetManifest.version.isEmpty() ? "-" : gAssetManifest.version,
              row, muted, TFT_WHITE);
  drawInfoRow(x, 64, 128, "files", String(gAssetManifest.fileCount) + " / " + compactBytes(gAssetManifest.totalBytes),
              row, muted, cyan);
  drawInfoRow(x, 79, 128, "storage", String(gStorageReady ? gStorageLabel : "off") + (indexReady ? " index" : ""),
              row, muted, gStorageReady ? green : amber);

  if (!gAssetManifest.error.isEmpty()) {
    d.setFont(&fonts::Font0);
    d.setTextColor(amber, panel);
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

