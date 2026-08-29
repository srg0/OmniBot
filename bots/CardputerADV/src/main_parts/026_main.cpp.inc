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
    sig += c;
  }
  return sig;
}

void updateImuMotion() {
  if (!gImuEnabled) {
    gEyeLookX *= 0.9f;
    gEyeLookY *= 0.9f;
    gEyePanelShiftX *= 0.9f;
    gEyePanelShiftY *= 0.9f;
    return;
  }

  uint32_t now = millis();
  if (now - gLastImuSampleMs < kImuSampleIntervalMs) {
    return;
  }
  gLastImuSampleMs = now;

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) {
    return;
  }

  float targetLookX = clampValue(-ay * 18.0f, -18.0f, 18.0f);
  float targetLookY = clampValue(ax * 14.0f, -14.0f, 14.0f);
  float targetPanelX = clampValue(-ay * 6.0f, -6.0f, 6.0f);
  float targetPanelY = clampValue(ax * 4.0f, -4.0f, 4.0f);

  gEyeLookX = gEyeLookX * 0.78f + targetLookX * 0.22f;
  gEyeLookY = gEyeLookY * 0.78f + targetLookY * 0.22f;
  gEyePanelShiftX = gEyePanelShiftX * 0.8f + targetPanelX * 0.2f;
  gEyePanelShiftY = gEyePanelShiftY * 0.8f + targetPanelY * 0.2f;
}

std::vector<String> wrapText(const String& raw, int32_t pixelLimit) {
  std::vector<String> out;
  String normalized = raw;
  normalized.replace('\r', ' ');
  normalized.replace('\n', ' ');
  normalized.trim();

  if (normalized.isEmpty()) {
    out.push_back("");
    return out;
  }

  std::vector<String> words;
  int start = 0;
  while (start < normalized.length()) {
    while (start < normalized.length() && normalized[start] == ' ') {
      ++start;
    }
    if (start >= normalized.length()) {
      break;
    }
    int end = start;
    while (end < normalized.length() && normalized[end] != ' ') {
      ++end;
    }
    words.push_back(normalized.substring(start, end));
    start = end;
  }

  String current;
  for (const auto& word : words) {
    String candidate = current.isEmpty() ? word : current + " " + word;
    if (current.isEmpty() || gCanvas.textWidth(candidate) <= pixelLimit) {
      current = candidate;
      continue;
    }
    out.push_back(current);
    current = word;
  }

  if (!current.isEmpty()) {
    out.push_back(current);
  }
  if (out.empty()) {
    out.push_back("");
  }
  return out;
}

void applyCompileTimeDefaults() {
  if (gWifiSsid.isEmpty() && strlen(DEFAULT_WIFI_SSID) > 0) {
    gWifiSsid = DEFAULT_WIFI_SSID;
  }
  if (gWifiPassword.isEmpty() && strlen(DEFAULT_WIFI_PASSWORD) > 0) {
    gWifiPassword = DEFAULT_WIFI_PASSWORD;
  }
  if (gWifiSsid.isEmpty()) {
    activateTemporaryMidaWifi("missing-credentials");
  }
  if (gHubHost.isEmpty() && strlen(DEFAULT_HUB_HOST) > 0) {
    gHubHost = DEFAULT_HUB_HOST;
  }
  uint16_t defaultPort = compileTimeHubPort();
  if (gHubPort == kDefaultHubPort && defaultPort != 0) {
    gHubPort = defaultPort;
  }
  if (gDeviceId.isEmpty() && strlen(DEFAULT_DEVICE_ID) > 0) {
    gDeviceId = DEFAULT_DEVICE_ID;
  }
  if (gDeviceToken.isEmpty() && strlen(DEFAULT_DEVICE_TOKEN) > 0) {
    gDeviceToken = DEFAULT_DEVICE_TOKEN;
  }
}

void loadConfig() {
  gPrefs.begin(kPrefsWifiNs, true);
  gWifiSsid = gPrefs.getString("ssid", "");
  gWifiPassword = gPrefs.getString("password", "");
  gPrefs.end();

  gPrefs.begin(kPrefsHubNs, true);
  gHubHost = gPrefs.getString("ip", "");
  gHubPort = static_cast<uint16_t>(gPrefs.getUInt("port", kDefaultHubPort));
  gPrefs.end();

  gPrefs.begin(kPrefsDeviceNs, true);
  gDisplayName = gPrefs.getString("display_name", kDefaultDisplayName);
  String savedId = gPrefs.getString("device_id", "");
  gDeviceToken = gPrefs.getString("device_token", "");
  gPrefs.end();

  if (strlen(DEFAULT_DEVICE_ID) > 0 && savedId != DEFAULT_DEVICE_ID) {
    savedId = DEFAULT_DEVICE_ID;
    gPrefs.begin(kPrefsDeviceNs, false);
    gPrefs.putString("device_id", savedId);
    gPrefs.end();
  }
  if (savedId.isEmpty()) {
    gDeviceId = "cardputer_adv_" + chipIdSuffix();
    gPrefs.begin(kPrefsDeviceNs, false);
    gPrefs.putString("device_id", gDeviceId);
    gPrefs.end();
  } else {
    gDeviceId = savedId;
  }

  applyCompileTimeDefaults();
  migrateBridgeEndpoint();
}

void loadDeviceSettings() {
  gPrefs.begin(kPrefsDeviceNs, true);
  gDeviceSettings.autoUpdateFirmware = gPrefs.getBool("fw_auto", false);
  gDeviceSettings.checkUpdatesOnBoot = gPrefs.getBool("fw_boot", true);
  gDeviceSettings.beepOnUpdate = gPrefs.getBool("fw_beep", true);
  gDeviceSettings.minBatteryForUpdate = static_cast<uint8_t>(gPrefs.getUChar("fw_bat", 40));
  gDeviceSettings.clockFace = static_cast<uint8_t>(gPrefs.getUChar("clk_face", 0));
  gDeviceSettings.displayBrightnessPct = static_cast<uint8_t>(gPrefs.getUChar("lcd_pct", kDefaultDisplayBrightnessPct));
  gDeviceSettings.screensaverEnabled = gPrefs.getBool("scr_on", true);
  gDeviceSettings.screensaverStyle = static_cast<uint8_t>(gPrefs.getUChar("scr_style", kDefaultScreensaverStyle));
  gDeviceSettings.screensaverTimeoutMin = static_cast<uint8_t>(gPrefs.getUChar("scr_min", kDefaultScreensaverTimeoutMin));
  gDeviceSettings.screensaverDimAfterMin = static_cast<uint8_t>(gPrefs.getUChar("scr_dim_m", kDefaultScreensaverDimAfterMin));
  gDeviceSettings.screensaverDimBrightnessPct = static_cast<uint8_t>(gPrefs.getUChar("scr_dim_p", kDefaultScreensaverDimBrightnessPct));
  gDeviceSettings.screenAutoOffEnabled = gPrefs.getBool("lcd_off", false);
  gDeviceSettings.screenOffAfterMin = static_cast<uint8_t>(gPrefs.getUChar("lcd_off_m", kDefaultScreenOffAfterMin));
  gDeviceSettings.screensaverClockOnWake = gPrefs.getBool("scr_clk", true);
  gDeviceSettings.notificationVolumePct = static_cast<uint8_t>(gPrefs.getUChar("vol_notif", kDefaultNotificationVolumePct));
  gDeviceSettings.updateChannel = gPrefs.getString("fw_chan", "dev");
  gDeviceSettings.audioLanguage = gPrefs.getString("aud_lang", "ru");
  gPrefs.end();
  gDeviceSettings.minBatteryForUpdate = clampValue<uint8_t>(gDeviceSettings.minBatteryForUpdate, 5, 95);
  gDeviceSettings.clockFace %= kClockFaceCount;
  gDeviceSettings.displayBrightnessPct = clampValue<uint8_t>(gDeviceSettings.displayBrightnessPct, 0, 100);
  gDeviceSettings.screensaverStyle %= kScreensaverStyleCount;
  gDeviceSettings.screensaverTimeoutMin = clampValue<uint8_t>(gDeviceSettings.screensaverTimeoutMin, 1, 60);
  gDeviceSettings.screensaverDimAfterMin = clampValue<uint8_t>(gDeviceSettings.screensaverDimAfterMin, 1, 120);
  gDeviceSettings.screensaverDimBrightnessPct = clampValue<uint8_t>(gDeviceSettings.screensaverDimBrightnessPct, 0, 100);
  gDeviceSettings.screenOffAfterMin = clampValue<uint8_t>(gDeviceSettings.screenOffAfterMin, 5, 240);
  gDeviceSettings.notificationVolumePct = clampValue<uint8_t>(gDeviceSettings.notificationVolumePct, 0, 100);
  if (gDeviceSettings.updateChannel.isEmpty()) {
    gDeviceSettings.updateChannel = "dev";
  }
  if (gDeviceSettings.audioLanguage.isEmpty()) {
    gDeviceSettings.audioLanguage = "ru";
  }
}

void saveDeviceSettings() {
  gPrefs.begin(kPrefsDeviceNs, false);
  gPrefs.putBool("fw_auto", gDeviceSettings.autoUpdateFirmware);
  gPrefs.putBool("fw_boot", gDeviceSettings.checkUpdatesOnBoot);
  gPrefs.putBool("fw_beep", gDeviceSettings.beepOnUpdate);
  gPrefs.putUChar("fw_bat", gDeviceSettings.minBatteryForUpdate);
  gPrefs.putUChar("clk_face", gDeviceSettings.clockFace % kClockFaceCount);
  gPrefs.putUChar("lcd_pct", gDeviceSettings.displayBrightnessPct);
  gPrefs.putBool("scr_on", gDeviceSettings.screensaverEnabled);
  gPrefs.putUChar("scr_style", gDeviceSettings.screensaverStyle % kScreensaverStyleCount);
  gPrefs.putUChar("scr_min", gDeviceSettings.screensaverTimeoutMin);
  gPrefs.putUChar("scr_dim_m", gDeviceSettings.screensaverDimAfterMin);
  gPrefs.putUChar("scr_dim_p", gDeviceSettings.screensaverDimBrightnessPct);
  gPrefs.putBool("lcd_off", gDeviceSettings.screenAutoOffEnabled);
  gPrefs.putUChar("lcd_off_m", gDeviceSettings.screenOffAfterMin);
  gPrefs.putBool("scr_clk", gDeviceSettings.screensaverClockOnWake);
  gPrefs.putUChar("vol_notif", gDeviceSettings.notificationVolumePct);
  gPrefs.putString("fw_chan", gDeviceSettings.updateChannel);
  gPrefs.putString("aud_lang", gDeviceSettings.audioLanguage);
  gPrefs.end();
}

void loadAlarmSettings() {
  gPrefs.begin(kPrefsAlarmNs, true);
  gAlarmSettings.hour = static_cast<uint8_t>(gPrefs.getUChar("hour", 7));
  gAlarmSettings.minute = static_cast<uint8_t>(gPrefs.getUChar("minute", 30));
  gAlarmSettings.enabled = gPrefs.getBool("enabled", false);
  gPrefs.end();
  if (gAlarmSettings.hour > 23) {
    gAlarmSettings.hour = 7;
  }
  if (gAlarmSettings.minute > 59) {
    gAlarmSettings.minute = 30;
  }
}

void saveAlarmSettings() {
  gPrefs.begin(kPrefsAlarmNs, false);
  gPrefs.putUChar("hour", gAlarmSettings.hour);
  gPrefs.putUChar("minute", gAlarmSettings.minute);
  gPrefs.putBool("enabled", gAlarmSettings.enabled);
  gPrefs.end();
}

void saveWifiCreds(const String& ssid, const String& password) {
  gPrefs.begin(kPrefsWifiNs, false);
  gPrefs.putString("ssid", ssid);
  gPrefs.putString("password", password);
  gPrefs.end();
  gWifiSsid = ssid;
  gWifiPassword = password;
}

void saveHubEndpoint(const String& host, uint16_t port) {
  gPrefs.begin(kPrefsHubNs, false);
  gPrefs.putString("ip", host);
  gPrefs.putUInt("port", port);
  gPrefs.end();
  gHubHost = host;
  gHubPort = port;
}

void saveDeviceToken(const String& token) {
  gPrefs.begin(kPrefsDeviceNs, false);
  gPrefs.putString("device_token", token);
  gPrefs.end();
  gDeviceToken = token;
}

void clearWifiCreds() {
  gPrefs.begin(kPrefsWifiNs, false);
  gPrefs.remove("ssid");
  gPrefs.remove("password");
  gPrefs.end();
  gPrefs.begin(kPrefsHubNs, false);
  gPrefs.remove("ip");
  gPrefs.remove("port");
  gPrefs.end();
  gWifiSsid = "";
  gWifiPassword = "";
  gHubHost = "";
  gHubPort = kDefaultHubPort;
}
