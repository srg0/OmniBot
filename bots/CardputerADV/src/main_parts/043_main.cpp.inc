void stopRealtimeRecordingAndCommit() {
  if (!gRealtimeRecording) {
    return;
  }
  gRealtimeRecording = false;
  gRealtimeHandsFreeActive = false;
  gRealtimeRestartAfterPlayback = false;
  pulseButtonPress();
  drainRealtimeRecording();
  stopMic();
  gRealtimeInflightChunks.clear();
  bool sent = commitRealtimeAudioBuffer("RT");
  if (!sent) {
    appendRuntimeLog("RT", "commit send failed", true);
    logf("[RT] commit send failed connected=%d", static_cast<int>(gRealtimeConnected));
    setStatus("Realtime commit failed", 1500);
    setFaceMode(FaceMode::Error);
    gRealtimeAwaiting = false;
    return;
  }
  gRealtimeAwaiting = true;
  gRealtimeAwaitingStartedMs = millis();
  setFaceMode(FaceMode::Thinking);
  setStatus("Realtime thinking...");
  setVoiceDiag("realtime: committed");
  appendRuntimeLog("RT", "record commit", false);
  logf("[RT] record commit");
}

void toggleRealtimeVoice() {
  if (gRealtimeRecording) {
    stopRealtimeRecordingAndCommit();
    return;
  }
  if (!gRealtimeActive || !gRealtimeConnected) {
    startRealtimeSession(true);
    return;
  }
  if (gRealtimeAwaiting) {
    setStatus("Realtime waiting...", 700);
    return;
  }
  startRealtimeRecording();
}

void serviceRealtimeVoice() {
  if (!gRealtimeActive) {
    return;
  }
  uint32_t now = millis();
  if (!gRealtimeConnected && gRealtimeSessionStartedMs != 0 &&
      now - gRealtimeSessionStartedMs > 15000) {
    logf("[RT] connect timeout active=%d free=%u largest=%u",
         static_cast<int>(gRealtimeActive),
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    gRealtimeSmokeActive = false;
    gRealtimeSmokeTextPending = false;
    closeRealtimeSession("connect timeout");
    setStatus("Realtime connect timeout", 1800);
    setFaceMode(FaceMode::Error);
    return;
  }
  if (gRealtimeAwaiting && gRealtimeAwaitingStartedMs != 0 &&
      now - gRealtimeAwaitingStartedMs > 45000) {
    logf("[RT] response timeout connected=%d free=%u largest=%u",
         static_cast<int>(gRealtimeConnected),
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    gRealtimeSmokeActive = false;
    gRealtimeSmokeTextPending = false;
    closeRealtimeSession("response timeout");
    setStatus("Realtime response timeout", 1800);
    setFaceMode(FaceMode::Error);
    return;
  }
  gRealtimeWs.loop();
  serviceRealtimeAudioSmoke();
  serviceRealtimeRecording(true);
  if (gRealtimeRestartAfterPlayback && gRealtimeHandsFreeActive &&
      gRealtimeActive && gRealtimeConnected && !gRealtimeRecording &&
      !gRealtimeAwaiting && !gPlaybackActive &&
      (gRealtimeNextListenMs == 0 || now >= gRealtimeNextListenMs)) {
    gRealtimeRestartAfterPlayback = false;
    logf("[RT] auto restart listening free=%u largest=%u",
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    startRealtimeRecording();
  }
}

bool writeVoiceSmokePcm(fs::FS& fs, const String& path, size_t sampleCount) {
  File file = fs.open(path, "w");
  if (!file) {
    return false;
  }
  int16_t chunk[256];
  size_t writtenSamples = 0;
  while (writtenSamples < sampleCount) {
    size_t n = std::min(static_cast<size_t>(256), sampleCount - writtenSamples);
    for (size_t i = 0; i < n; ++i) {
      size_t sampleIndex = writtenSamples + i;
      int period = (sampleIndex / 1200) % 2 == 0 ? 18 : 29;
      int16_t amp = static_cast<int16_t>((sampleIndex % period) < (period / 2) ? 1500 : -1500);
      if (sampleIndex < 800) {
        amp = static_cast<int16_t>((static_cast<int32_t>(amp) * static_cast<int32_t>(sampleIndex)) / 800);
      }
      if (sampleCount - sampleIndex < 800) {
        amp = static_cast<int16_t>((static_cast<int32_t>(amp) *
                                    static_cast<int32_t>(sampleCount - sampleIndex)) / 800);
      }
      chunk[i] = amp;
    }
    size_t bytes = n * sizeof(int16_t);
    if (file.write(reinterpret_cast<const uint8_t*>(chunk), bytes) != bytes) {
      file.close();
      fsRemove(fs, path);
      return false;
    }
    writtenSamples += n;
  }
  file.flush();
  file.close();
  return true;
}

bool findLatestUserVoiceNote(VoiceNote& noteOut) {
  loadVoiceNotes();
  for (const auto& note : gVoiceNotes) {
    if (!note.assistant && !note.audioPath.isEmpty()) {
      noteOut = note;
      return true;
    }
  }
  return false;
}

bool copyWavDataToPcmFile(fs::FS& fs, const String& wavPath, const String& pcmPath,
                          WavInfo& infoOut) {
  File wav = fs.open(wavPath, "r");
  if (!wav) {
    infoOut.error = "open wav";
    return false;
  }
  if (!parseWavInfo(wav, infoOut)) {
    wav.close();
    return false;
  }
  if (infoOut.sampleRate != kMicSampleRate) {
    infoOut.error = "sample rate " + String(static_cast<unsigned>(infoOut.sampleRate));
    wav.close();
    return false;
  }
  if (!wav.seek(infoOut.dataOffset)) {
    infoOut.error = "seek data";
    wav.close();
    return false;
  }

  fsRemove(fs, pcmPath);
  File pcm = fs.open(pcmPath, "w");
  if (!pcm) {
    infoOut.error = "open pcm";
    wav.close();
    return false;
  }

  uint8_t buffer[512];
  uint32_t remaining = infoOut.dataBytes;
  while (remaining > 0) {
    size_t chunk = min<size_t>(sizeof(buffer), remaining);
    int got = wav.read(buffer, chunk);
    if (got <= 0) {
      infoOut.error = "read wav";
      pcm.close();
      wav.close();
      fsRemove(fs, pcmPath);
      return false;
    }
    size_t written = pcm.write(buffer, static_cast<size_t>(got));
    if (written != static_cast<size_t>(got)) {
      infoOut.error = "write pcm";
      pcm.close();
      wav.close();
      fsRemove(fs, pcmPath);
      return false;
    }
    remaining -= static_cast<uint32_t>(got);
    pumpBlockingNetworkUi();
  }
  pcm.flush();
  pcm.close();
  wav.close();
  return true;
}

bool playVoiceSmokeResponse(const String& response, const char* logPrefix) {
  DynamicJsonDocument resp(4096);
  auto err = deserializeJson(resp, response);
  if (err) {
    logf("[SMOKE] %s result ok=0 stage=json_parse response_bytes=%u err=%s",
         logPrefix,
         static_cast<unsigned>(response.length()),
         err.c_str());
    pushError("Smoke voice parse failed.");
    setFaceMode(FaceMode::Error);
    setStatus("Smoke parse failed", 1800);
    return false;
  }

  String transcript = resp["transcript"] | resp["text"] | "";
  String reply = resp["reply"] | resp["full_text"] | "";
  String shortText = resp["short_text"] | "";
  String deviceActionsJson;
  if (!resp["device_actions"].isNull()) {
    serializeJson(resp["device_actions"], deviceActionsJson);
  }
  if (reply.isEmpty()) {
    reply = shortText;
  }

  String audioUrl;
  String audioPath;
  String audioTitle;
  String audioSha;
  uint32_t audioSize = 0;
  if (resp["audio"].is<JsonObject>()) {
    JsonObject audio = resp["audio"].as<JsonObject>();
    audioUrl = audio["url"] | "";
    audioPath = audio["path"] | "";
    audioTitle = audio["title"] | "";
    audioSha = audio["sha256"] | "";
    audioSize = audio["size"] | 0;
  }

  if (!transcript.isEmpty()) {
    pushUser(transcript);
    setVoiceDiag("smoke: transcript ok");
  }
  if (!reply.isEmpty()) {
    appendAssistantReply(reply);
  }
  bool played = false;
  if (!reply.isEmpty()) {
    played = downloadAndPlayResponseAudio(audioUrl, audioPath, audioSha, audioSize, audioTitle, reply);
    if (!played) {
      played = requestAndPlayTts(reply);
    }
  }
  if (!deviceActionsJson.isEmpty()) {
    executeDeviceActionsJson(deviceActionsJson);
  }

  logf("[SMOKE] %s result ok=%d transcript_chars=%u reply_chars=%u audio=%d played=%d",
       logPrefix,
       reply.isEmpty() ? 0 : 1,
       static_cast<unsigned>(transcript.length()),
       static_cast<unsigned>(reply.length()),
       static_cast<int>(!audioUrl.isEmpty() || !audioPath.isEmpty()),
       static_cast<int>(played));
  if (reply.isEmpty()) {
    setFaceMode(FaceMode::Idle);
    setStatus("Smoke no reply", 1600);
    return false;
  }
  if (!played) {
    setFaceMode(FaceMode::Idle);
    setStatus("Smoke text only", 1600);
  }
  return true;
}

bool runVoiceSmokeFromDevice() {
  if (gRecording || gSubmitting || gThinking || gRealtimeRecording) {
    logf("[SMOKE] voice busy rec=%d submit=%d think=%d rt_rec=%d",
         static_cast<int>(gRecording),
         static_cast<int>(gSubmitting),
         static_cast<int>(gThinking),
         static_cast<int>(gRealtimeRecording));
    return false;
  }
  if (!ensureWifiForHttp("voice-smoke", 5000) || gHubHost.isEmpty()) {
    logf("[SMOKE] voice no wifi/hub");
    return false;
  }
  if (!gStorageReady || !ensureVoiceStorage()) {
    logf("[SMOKE] voice no storage");
    return false;
  }
  fs::FS* fs = activeVoiceFs();
  if (!fs) {
    logf("[SMOKE] voice no fs");
    return false;
  }
  const String path = "/voice/__smoke_voice.pcm";
  const size_t samples = kMicSampleRate * 2;
  const size_t bytes = samples * sizeof(int16_t);
  fsRemove(*fs, path);
  if (!writeVoiceSmokePcm(*fs, path, samples)) {
    logf("[SMOKE] voice file write failed");
    return false;
  }
  int statusCode = -1;
  String response;
  String turnId = makeClientTurnId("smoke");
  gSubmitting = true;
  gThinking = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Voice smoke upload...");
  render();
  bool prevRenderPump = gBlockingRenderPumpEnabled;
  gBlockingRenderPumpEnabled = true;
  gLastBlockingRenderPumpMs = 0;
  pumpBlockingNetworkUi();
  logf("[SMOKE] voice start bytes=%u turn=%s", static_cast<unsigned>(bytes), turnId.c_str());
  bool ok = postRawVoiceTurn(nullptr, path, bytes, turnId, statusCode, response);
  gBlockingRenderPumpEnabled = prevRenderPump;
  gSubmitting = false;
  gThinking = false;
  fsRemove(*fs, path);
  DynamicJsonDocument doc(1024);
  auto err = deserializeJson(doc, response);
  unsigned transcriptChars = 0;
  unsigned replyChars = 0;
  if (!err) {
    transcriptChars = String(doc["transcript"] | doc["text"] | "").length();
    replyChars = String(doc["reply"] | doc["full_text"] | "").length();
  }
  logf("[SMOKE] voice result ok=%d code=%d response_bytes=%u transcript_chars=%u reply_chars=%u diag=%s",
       static_cast<int>(ok),
       statusCode,
       static_cast<unsigned>(response.length()),
       transcriptChars,
       replyChars,
       gLastHttpDiag.c_str());
  setFaceMode(ok && statusCode >= 200 && statusCode < 300 ? FaceMode::Idle : FaceMode::Error);
  setStatus(ok ? "Voice smoke done" : "Voice smoke failed", 1800);
  return ok && statusCode >= 200 && statusCode < 300;
}

bool runLiveVoiceSmokeFromDevice() {
  if (gRecording || gSubmitting || gThinking || gRealtimeRecording) {
    logf("[SMOKE] voice live busy rec=%d submit=%d think=%d rt_rec=%d",
         static_cast<int>(gRecording),
         static_cast<int>(gSubmitting),
         static_cast<int>(gThinking),
         static_cast<int>(gRealtimeRecording));
    return false;
  }
  if (!ensureWifiForHttp("voice-live-smoke", 5000) || gHubHost.isEmpty()) {
    logf("[SMOKE] voice live no wifi/hub");
    return false;
  }
  if (!gStorageReady || !ensureVoiceStorage()) {
    logf("[SMOKE] voice live no storage");
    return false;
  }
  fs::FS* fs = activeVoiceFs();
  if (!fs) {
    logf("[SMOKE] voice live no fs");
    return false;
  }

  VoiceNote note;
  if (!findLatestUserVoiceNote(note)) {
    logf("[SMOKE] voice live no_user_note count=%u", static_cast<unsigned>(gVoiceNotes.size()));
    pushError("No recorded user voice note.");
    setStatus("Record a voice note first", 2200);
    return false;
  }

  const String pcmPath = "/voice/__live_smoke.pcm";
  WavInfo wav;
  if (!copyWavDataToPcmFile(*fs, note.audioPath, pcmPath, wav)) {
    logf("[SMOKE] voice live wav failed path=%s err=%s",
         note.audioPath.c_str(),
         wav.error.c_str());
    pushError("Smoke WAV read failed: " + wav.error);
    setStatus("Smoke WAV failed", 1800);
    return false;
  }

  int statusCode = -1;
  String response;
  String turnId = makeClientTurnId("live");
  gSubmitting = true;
  gThinking = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Live voice smoke...");
  render();
  pauseHubWebSocketForVoice();
  bool prevRenderPump = gBlockingRenderPumpEnabled;
  gBlockingRenderPumpEnabled = true;
  gLastBlockingRenderPumpMs = 0;
  pumpBlockingNetworkUi();
  logf("[SMOKE] voice live start note=%s bytes=%u ms=%u turn=%s",
       note.audioPath.c_str(),
       static_cast<unsigned>(wav.dataBytes),
       static_cast<unsigned>(note.durationMs),
       turnId.c_str());
  bool transportOk = postRawVoiceTurn(nullptr, pcmPath, wav.dataBytes, turnId, statusCode, response);
  gBlockingRenderPumpEnabled = prevRenderPump;
  fsRemove(*fs, pcmPath);
  gSubmitting = false;
  gThinking = false;
  resumeHubWebSocketAfterVoice();

  if (!transportOk || statusCode <= 0) {
    String transportError = gLastHttpDiag;
    if (transportError.isEmpty()) {
      transportError = statusCode <= 0 ? HTTPClient::errorToString(statusCode) : String("unknown");
    }
    logf("[SMOKE] voice live result ok=0 stage=transport code=%d err=%s bytes=%u",
         statusCode,
         transportError.c_str(),
         static_cast<unsigned>(wav.dataBytes));
    pushError("Live smoke transport failed: " + transportError);
    setFaceMode(FaceMode::Error);
    setStatus("Live smoke transport failed", 2200);
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    logf("[SMOKE] voice live result ok=0 stage=http code=%d body=%s",
         statusCode,
         response.c_str());
    pushError("Live smoke HTTP error: " + String(statusCode));
    setFaceMode(FaceMode::Error);
    setStatus("Live smoke HTTP failed", 2000);
    return false;
  }

  return playVoiceSmokeResponse(response, "voice live");
}

void runRealtimeSmokeFromDevice(bool verifyTls = true) {
  if (gRealtimeActive || gRealtimeRecording) {
    closeRealtimeSession("smoke restart");
    delay(50);
  }
  if (!ensureWifiForHttp("realtime-smoke", 5000) || gHubHost.isEmpty()) {
    logf("[SMOKE] realtime no wifi/hub");
    return;
  }
  gRealtimeSmokeActive = true;
  gRealtimeSmokeTextPending = true;
  gRealtimeTlsVerify = verifyTls;
  logf("[SMOKE] realtime start verify=%s", verifyTls ? "ca" : "insecure");
  startRealtimeSession(false);
}

void runRealtimeAudioSmokeFromDevice(bool verifyTls = true) {
  if (gRealtimeActive || gRealtimeRecording) {
    closeRealtimeSession("audio smoke restart");
    delay(50);
  }
  if (!ensureWifiForHttp("realtime-audio-smoke", 5000) || gHubHost.isEmpty()) {
    logf("[SMOKE] realtime audio no wifi/hub");
    return;
  }
  gRealtimeSmokeActive = true;
  gRealtimeSmokeTextPending = false;
  gRealtimeSmokeAudioPending = true;
  gRealtimeTlsVerify = verifyTls;
  logf("[SMOKE] realtime audio start verify=%s", verifyTls ? "ca" : "insecure");
  startRealtimeSession(false);
}

bool readProbeLine(WiFiClient& client, String& line, uint32_t timeoutMs) {
  line = "";
  uint32_t deadline = millis() + timeoutMs;
  while (millis() < deadline) {
    while (client.available() > 0) {
      char ch = static_cast<char>(client.read());
      if (ch == '\r') {
        continue;
      }
      if (ch == '\n') {
        return true;
      }
      if (line.length() < 240) {
        line += ch;
      }
    }
    if (!client.connected() && line.isEmpty()) {
      return false;
    }
    delay(1);
    yield();
  }
  return !line.isEmpty();
}

void runRealtimeUpgradeProbe(bool verifyTls = true) {
  if (!ensureWifiForHttp("realtime-probe", 5000) || gHubHost.isEmpty()) {
    logf("[PROBE] rt no wifi/hub");
    return;
  }
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  WiFiClient* client = nullptr;
  if (hubUsesTls()) {
    if (verifyTls) {
      configureSecureClient(secureClient);
    } else {
      secureClient.setInsecure();
    }
    secureClient.setTimeout(8);
    client = &secureClient;
  } else {
    plainClient.setTimeout(8);
    client = &plainClient;
  }

  logf("[PROBE] rt connect host=%s port=%u verify=%s free=%u largest=%u",
       gHubHost.c_str(),
       gHubPort,
       hubUsesTls() ? (verifyTls ? "ca" : "insecure") : "none",
       static_cast<unsigned>(ESP.getFreeHeap()),
       static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
  if (!client->connect(gHubHost.c_str(), gHubPort)) {
    logf("[PROBE] rt connect_failed free=%u largest=%u",
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    return;
  }

  String hostHeader = gHubHost;
  if ((!hubUsesTls() && gHubPort != 80) || (hubUsesTls() && gHubPort != 443)) {
    hostHeader += ":" + String(gHubPort);
  }
  String request;
  request.reserve(512);
  request += "GET ";
  request += kRealtimeWsPath;
  request += " HTTP/1.1\r\nHost: ";
  request += hostHeader;
  request += "\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\n";
  request += "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n";
  request += "User-Agent: OpenClaw-Cardputer-Probe/";
  request += kAppVersion;
  request += "\r\n\r\n";
  if (!writeClientAll(*client, request, "rt_probe_headers")) {
    client->stop();
    logf("[PROBE] rt write_failed");
    return;
  }

  String line;
  if (!readProbeLine(*client, line, 8000)) {
    logf("[PROBE] rt no_status connected=%d", static_cast<int>(client->connected()));
    client->stop();
    return;
  }
  logf("[PROBE] rt status=%s", line.c_str());
  uint8_t headerCount = 0;
  while (headerCount < 12 && readProbeLine(*client, line, 1500)) {
    if (line.isEmpty()) {
      break;
    }
    logf("[PROBE] rt header=%s", line.c_str());
    ++headerCount;
  }
  client->stop();
}

void printHostDiag() {
  logf("[DBG] version=%s git=%s heap_free=%u heap_largest=%u wifi=%d ip=%s hub=%s:%u tls=%d rt_verify=%s rt_active=%d rt_connected=%d rt_recording=%d rt_awaiting=%d rt_hf=%d rt_restart=%d voice=%s http=%s",
       kAppVersion,
       kBuildGitSha,
       static_cast<unsigned>(ESP.getFreeHeap()),
       static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
       static_cast<int>(WiFi.status() == WL_CONNECTED),
       WiFi.localIP().toString().c_str(),
       gHubHost.c_str(),
       gHubPort,
       static_cast<int>(hubUsesTls()),
       gRealtimeTlsVerify ? "ca" : "insecure",
       static_cast<int>(gRealtimeActive),
       static_cast<int>(gRealtimeConnected),
       static_cast<int>(gRealtimeRecording),
       static_cast<int>(gRealtimeAwaiting),
       static_cast<int>(gRealtimeHandsFreeActive),
       static_cast<int>(gRealtimeRestartAfterPlayback),
       gLastVoiceDiag.c_str(),
       gLastHttpDiag.c_str());
}

bool handleHostSerialCommand(String line) {
  line.trim();
  if (line.isEmpty()) {
    return false;
  }
  logf("[HOST] cmd=%s", line.c_str());
  String cmd = line;
  cmd.toLowerCase();
  if (cmd == "diag" || cmd == "/diag" || cmd == "dbg" || cmd == "/dbg") {
    printHostDiag();
    return true;
  }
  if (cmd == "voice.smoke" || cmd == "/voice.smoke" || cmd == "voice smoke") {
    runVoiceSmokeFromDevice();
    return true;
  }
  if (cmd == "voice.smoke.live" || cmd == "/voice.smoke.live" || cmd == "voice live smoke") {
    runLiveVoiceSmokeFromDevice();
    return true;
  }
  if (cmd == "rt.smoke" || cmd == "/rt.smoke" || cmd == "realtime.smoke" || cmd == "realtime smoke") {
    runRealtimeSmokeFromDevice(true);
    return true;
  }
  if (cmd == "rt.smoke.insecure" || cmd == "/rt.smoke.insecure") {
    runRealtimeSmokeFromDevice(false);
    return true;
  }
  if (cmd == "rt.smoke.audio" || cmd == "/rt.smoke.audio" ||
      cmd == "realtime.smoke.audio" || cmd == "realtime audio smoke") {
    runRealtimeAudioSmokeFromDevice(true);
    return true;
  }
  if (cmd == "rt.smoke.audio.insecure" || cmd == "/rt.smoke.audio.insecure") {
    runRealtimeAudioSmokeFromDevice(false);
    return true;
  }
  if (cmd == "rt.probe" || cmd == "/rt.probe" || cmd == "realtime.probe") {
    runRealtimeUpgradeProbe(true);
    return true;
  }
  if (cmd == "rt.probe.insecure" || cmd == "/rt.probe.insecure") {
    runRealtimeUpgradeProbe(false);
    return true;
  }
  if (cmd == "rt.close" || cmd == "/rt.close" || cmd == "realtime.close") {
    gRealtimeSmokeActive = false;
    gRealtimeSmokeTextPending = false;
    gRealtimeSmokeAudioPending = false;
    gRealtimeSmokeAudioStreaming = false;
    closeRealtimeSession("host close");
    logf("[SMOKE] realtime closed by host");
    return true;
  }
  return handleLocalCommand(line);
}

void handleHostSerialInput() {
  while (Serial.available() > 0) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      String line = gSerialControlBuffer;
      gSerialControlBuffer = "";
      handleHostSerialCommand(line);
      continue;
    }
    if (gSerialControlBuffer.length() < 220) {
      gSerialControlBuffer += ch;
    } else {
      gSerialControlBuffer = "";
      logf("[HOST] input overflow");
    }
  }
}

void maintainConnectivity() {
  bool wifiNow = WiFi.status() == WL_CONNECTED;

  if (!wifiNow) {
    if (gWifiReady) {
      gWifiReady = false;
      gWsReady = false;
      setStatus("Wi-Fi lost", 1500);
      pushError("Wi-Fi disconnected.");
      logf("[WIFI] disconnected");
    }
    uint32_t now = millis();
    if (now - gLastWifiReconnectMs >= kWifiReconnectIntervalMs && !gSubmitting && !gRecording) {
      gLastWifiReconnectMs = now;
      logf("[WIFI] background reconnect attempt");
      ensureWifiForHttp("background-reconnect", 3000);
    }
    return;
  }

  if (!gWifiReady) {
    gWifiReady = true;
    setStatus("Wi-Fi restored", 1200);
    pushSystem("Wi-Fi restored: " + WiFi.localIP().toString());
    logf("[WIFI] restored ip=%s", WiFi.localIP().toString().c_str());
    syncClockFromNtp(true);
    if (kUseHubWebSocket && !gBleActive && !gHubHost.isEmpty() && !gWsReady && !gResumeWsAfterVoice) {
      logf("[WS] restart after wifi restore");
      startHubWebSocket();
    }
  }
}

bool handleLocalCommand(const String& input) {
  String cmd = input;
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "/help" || cmd == "help" || cmd == "commands" || cmd == "actions" ||
      cmd == "команды" || cmd == "помощь") {
    pushSystem("Ctrl+Space = EN/RU");
    pushSystem("Voice: Ctrl x2, Ctrl+V, or hold G0");
    pushSystem("Tab = next screen, Tab+Down/Right = next app");
    pushSystem("Ctrl+L = launcher map, arrows = group/item");
    pushSystem("Ctrl+D = debug; type: voice focus topics pet notes");
    pushSystem("Pet: F food P play C clean S sleep H hunt");
    pushSystem("Agent actions: focus, ui.open, audio/voice, topics, pet, settings");
    pushSystem("Examples: start pomodoro 25; open library; next topic");
    return true;
  }

  if (cmd == "/home" || cmd == "/pulse") {
    setUiMode(UiMode::Home);
    return true;
  }

  if (cmd == "/status") {
    pushSystem("device_id=" + gDeviceId);
    pushSystem("wifi=" + String(gWifiReady ? WiFi.localIP().toString() : "offline"));
    pushSystem("hub=" + (gHubHost.isEmpty() ? String("unset") : gHubHost + ":" + String(gHubPort)));
    pushSystem("ctx=" + currentConversationKey());
    return true;
  }

  if (cmd == "/btsetup") {
    pushSystem("Switching to BLE provisioning mode.");
    startBleProvisioning();
    return true;
  }

  if (cmd == "/clearwifi") {
    pushSystem("Clearing Wi-Fi credentials and rebooting.");
    clearWifiCreds();
    delay(400);
    ESP.restart();
    return true;
  }

  if (cmd == "/dbg") {
    gDebugOverlayVisible = !gDebugOverlayVisible;
    setStatus(gDebugOverlayVisible ? "Debug overlay on" : "Debug overlay off", 1000);
    return true;
  }

  if (cmd == "/face") {
    setUiMode(UiMode::Face);
    return true;
  }

  if (cmd == "/chat") {
    setUiMode(UiMode::ChatFull);
    return true;
  }

  if (cmd == "/hero" || cmd == "/pet" || cmd == "/tama") {
    setUiMode(UiMode::Hero);
    return true;
  }

  if (cmd == "/petreset" || cmd == "/tamareset") {
    resetPet(true);
    setUiMode(UiMode::Hero);
    return true;
  }

  if (cmd == "/clock") {
    setUiMode(UiMode::Home);
    return true;
  }

  if (cmd == "/voice") {
    setUiMode(UiMode::Voice);
    return true;
  }

  if (cmd == "/battery") {
    setUiMode(UiMode::Battery);
    return true;
  }

  if (cmd == "/focus") {
    setUiMode(UiMode::Focus);
    return true;
  }

  if (cmd == "/focusreset" || cmd == "/freset") {
    focusReset();
    setUiMode(UiMode::Focus);
    return true;
  }

  if (cmd == "/library") {
    scanAudioAssets();
    setUiMode(UiMode::Library);
    return true;
  }

  if (cmd == "/assets") {
    loadAssetManifestSummary();
    scanAudioAssets();
    setUiMode(UiMode::Assets);
    return true;
  }

  if (cmd == "/ota") {
    loadOtaManifestSummary();
    setUiMode(UiMode::Ota);
    return true;
  }

  if (cmd == "/contexts" || cmd == "/topics") {
    loadConversationContexts();
    setUiMode(UiMode::Contexts);
    startTopicTask(true, false, true, "Syncing topics...");
    return true;
  }

  if (cmd == "/settings") {
    setUiMode(UiMode::Settings);
    return true;
  }

  if (cmd == "/syncassets") {
    fetchRemoteAssetManifest();
    syncAssetsFromManifest();
    setUiMode(UiMode::Assets);
    return true;
  }

  if (cmd == "/fetchota") {
    fetchRemoteOtaManifest();
    setUiMode(UiMode::Ota);
    return true;
  }

  if (cmd == "/sendlogs") {
    uploadRuntimeLogs();
    setUiMode(UiMode::Logs);
    return true;
  }

  if (cmd == "/logs") {
    setUiMode(UiMode::Logs);
    return true;
  }
