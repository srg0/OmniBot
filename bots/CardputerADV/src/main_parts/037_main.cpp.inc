  String reply = resp["reply"] | resp["full_text"] | resp["text"] | "";
  String shortText = resp["short_text"] | "";
  String deviceActionsJson;
  if (!resp["device_actions"].isNull()) {
    serializeJson(resp["device_actions"], deviceActionsJson);
  }
  if (reply.isEmpty()) {
    reply = shortText;
  }
  if (!reply.isEmpty()) {
    appendAssistantReply(reply);
    String audioUrl;
    String audioPath;
    String audioTitle;
    String audioSha;
    uint32_t audioSize = 0;
    bool hasAudio = resp["audio"].is<JsonObject>();
    if (hasAudio) {
      JsonObject audio = resp["audio"].as<JsonObject>();
      audioUrl = audio["url"] | "";
      audioPath = audio["path"] | "";
      audioTitle = audio["title"] | "";
      audioSha = audio["sha256"] | "";
      audioSize = audio["size"] | 0;
    }
    if (hasAudio) {
      downloadAndPlayResponseAudio(audioUrl, audioPath, audioSha, audioSize, audioTitle, reply);
    } else if (shouldSpeakAssistantTextReply(reply)) {
      requestAndPlayTts(reply);
    }
    logf("[TEXT] reply=%s", reply.c_str());
  } else {
    pushSystem("Hub accepted the turn.");
    logf("[TEXT] accepted without reply");
  }
  if (!deviceActionsJson.isEmpty()) {
    executeOrDeferDeviceActionsJson(deviceActionsJson);
  }
  setFaceMode(FaceMode::Idle);
  setStatus("Reply received", 1200);
  return true;
}

const char* batteryChargeLabel(m5::Power_Class::is_charging_t charging) {
  switch (charging) {
    case m5::Power_Class::is_charging:
      return "Charging";
    case m5::Power_Class::is_discharging:
      return "Discharging";
    case m5::Power_Class::charge_unknown:
    default:
      return "Unknown";
  }
}

bool batteryExternalPowerPresent() {
  return gBatterySnapshot.vbusMv > 4000 ||
         gBatterySnapshot.charging == m5::Power_Class::is_charging;
}

int32_t estimateBatteryLevelFromVoltage(int16_t voltageMv) {
  if (voltageMv <= 0) {
    return -1;
  }
  if (voltageMv <= 3300) {
    return 0;
  }
  if (voltageMv >= 4200) {
    return 100;
  }
  return clampValue<int32_t>((static_cast<int32_t>(voltageMv) - 3300) * 100 / 900, 0, 100);
}

BatteryHistorySample batteryHistorySampleAt(uint8_t index) {
  if (index >= gBatteryHistoryCount || gBatteryHistoryCount == 0) {
    return BatteryHistorySample();
  }
  uint8_t oldest = (gBatteryHistoryHead + kBatteryHistoryCapacity - gBatteryHistoryCount) % kBatteryHistoryCapacity;
  uint8_t slot = (oldest + index) % kBatteryHistoryCapacity;
  return gBatteryHistory[slot];
}

void updateBatteryDrainEstimate() {
  gBatteryDrainKnown = false;
  gBatteryDrainPctPerHour = 0.0f;
  gBatteryDrainMvPerHour = 0;
  gBatteryEtaMinutes = 0;

  if (gBatteryHistoryCount < 2) {
    return;
  }
  BatteryHistorySample latest = batteryHistorySampleAt(gBatteryHistoryCount - 1);
  if (latest.ms == 0 || latest.externalPower) {
    return;
  }

  BatteryHistorySample baseline;
  bool found = false;
  for (int i = static_cast<int>(gBatteryHistoryCount) - 2; i >= 0; --i) {
    BatteryHistorySample candidate = batteryHistorySampleAt(static_cast<uint8_t>(i));
    if (candidate.ms == 0 || candidate.externalPower || candidate.ms >= latest.ms) {
      continue;
    }
    if (latest.ms - candidate.ms >= kBatteryDrainMinWindowMs) {
      baseline = candidate;
      found = true;
      break;
    }
  }
  if (!found) {
    return;
  }

  uint32_t dtMs = latest.ms - baseline.ms;
  if (dtMs == 0) {
    return;
  }
  int16_t pctDrop = (baseline.level >= 0 && latest.level >= 0) ? baseline.level - latest.level : 0;
  int16_t mvDrop = (baseline.voltageMv > 0 && latest.voltageMv > 0) ? baseline.voltageMv - latest.voltageMv : 0;
  if (pctDrop <= 0 && mvDrop <= 0) {
    return;
  }

  if (pctDrop > 0) {
    gBatteryDrainPctPerHour = (static_cast<float>(pctDrop) * 3600000.0f) / static_cast<float>(dtMs);
    if (latest.level > 0 && gBatteryDrainPctPerHour > 0.05f) {
      long etaMinutes = static_cast<long>((static_cast<float>(latest.level) / gBatteryDrainPctPerHour) * 60.0f);
      gBatteryEtaMinutes = static_cast<uint16_t>(clampValue<long>(etaMinutes, 1, 999));
    }
  }
  if (mvDrop > 0) {
    long mvPerHour = (static_cast<long>(mvDrop) * 3600000L) / static_cast<long>(dtMs);
    gBatteryDrainMvPerHour = static_cast<int16_t>(clampValue<long>(mvPerHour, 0, 32767));
  }
  gBatteryDrainKnown = true;
}

void recordBatteryHistorySample(uint32_t now, bool force) {
  if (!force && gLastBatteryHistorySampleMs != 0 &&
      now - gLastBatteryHistorySampleMs < kBatteryHistorySampleIntervalMs) {
    updateBatteryDrainEstimate();
    return;
  }
  if (gBatterySnapshot.level < 0 && gBatterySnapshot.voltageMv <= 0) {
    updateBatteryDrainEstimate();
    return;
  }

  BatteryHistorySample sample;
  sample.ms = now;
  sample.level = static_cast<int16_t>(gBatterySnapshot.level);
  sample.voltageMv = gBatterySnapshot.voltageMv;
  sample.externalPower = batteryExternalPowerPresent();
  gBatteryHistory[gBatteryHistoryHead] = sample;
  gBatteryHistoryHead = (gBatteryHistoryHead + 1) % kBatteryHistoryCapacity;
  if (gBatteryHistoryCount < kBatteryHistoryCapacity) {
    ++gBatteryHistoryCount;
  }
  gLastBatteryHistorySampleMs = now;
  updateBatteryDrainEstimate();
}

void updateBatterySnapshot(bool force) {
  uint32_t now = millis();
  if (!force && gLastBatterySampleMs != 0 && now - gLastBatterySampleMs < kBatterySampleIntervalMs) {
    return;
  }
  gLastBatterySampleMs = now;
  int32_t rawLevel = M5.Power.getBatteryLevel();
  int16_t voltageMv = M5.Power.getBatteryVoltage();
  int16_t vbusMv = M5.Power.getVBUSVoltage();
  gBatterySnapshot.rawLevel = rawLevel;
  gBatterySnapshot.level = rawLevel;
  gBatterySnapshot.levelEstimated = false;
  if ((rawLevel <= 0 || rawLevel > 100) && voltageMv > 0) {
    int32_t estimated = estimateBatteryLevelFromVoltage(voltageMv);
    if (estimated >= 0) {
      gBatterySnapshot.level = estimated;
      gBatterySnapshot.levelEstimated = true;
    }
  } else if (rawLevel <= 0 && voltageMv <= 0 && vbusMv > 4000) {
    gBatterySnapshot.level = -1;
  }
  gBatterySnapshot.voltageMv = voltageMv;
  gBatterySnapshot.vbusMv = vbusMv;
  gBatterySnapshot.currentMa = M5.Power.getBatteryCurrent();
  gBatterySnapshot.charging = M5.Power.isCharging();
  recordBatteryHistorySample(now, force);
}

void textTurnTask(void* arg) {
  std::unique_ptr<TextTurnTaskPayload> payload(static_cast<TextTurnTaskPayload*>(arg));
  std::unique_ptr<TextTurnTaskResult> result(new TextTurnTaskResult());
  if (!result) {
    gTextTurnTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  result->response.reserve(2048);
  bool releasedCanvasForText = false;

  if (payload->url.isEmpty()) {
    result->error = "Hub is not configured.";
  } else if (!ensureWifiForHttp("text-turn", 3000)) {
    result->error = "Wi-Fi unavailable.";
  } else {
    prepareNetworkMemory("text", false);
    releasedCanvasForText = releaseUiCanvasForNetwork("text");

    HTTPClient http;
    std::unique_ptr<WiFiClientSecure> secureClient;
    http.setTimeout(kTurnHttpTimeoutMs);
    http.setReuse(false);
    bool beginOk = false;
    logf("[TEXT] preconnect free=%u largest=%u url=%s",
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
         payload->url.c_str());
    if (hubUsesTls()) {
      secureClient.reset(new WiFiClientSecure());
      configureSecureClient(*secureClient);
      beginOk = http.begin(*secureClient, payload->url);
    } else {
      beginOk = http.begin(payload->url);
    }
    if (!beginOk) {
      result->error = "Failed to open HTTP connection. largest=" +
                      String(static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
      logf("[TEXT] begin failed free=%u largest=%u",
           static_cast<unsigned>(ESP.getFreeHeap()),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    } else {
      DynamicJsonDocument doc(768);
      doc["message"] = payload->text;
      doc["device_id"] = payload->deviceId;
      doc["firmware_version"] = kAppVersion;
      doc["conversation_key"] = payload->conversationKey;
      doc["input_type"] = "text";
      doc["reply_audio"] = false;
      doc["save_reply_audio"] = false;
      doc["client_msg_id"] = payload->clientMsgId;
      doc["turn_id"] = payload->turnId;

      String body;
      serializeJson(doc, body);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("X-Conversation-Key", payload->conversationKey);
      http.addHeader("X-Client-Msg-Id", payload->clientMsgId);
      http.addHeader("X-Turn-Id", payload->turnId);
      if (!payload->deviceToken.isEmpty()) {
        http.addHeader("Authorization", "Bearer " + payload->deviceToken);
      }
      int code = http.POST(body);
      result->statusCode = code;
      result->response = http.getString();
      http.end();
      if (code <= 0) {
        logf("[TEXT] post transport code=%d err=%s free=%u largest=%u",
             code,
             HTTPClient::errorToString(code).c_str(),
             static_cast<unsigned>(ESP.getFreeHeap()),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
      }

      if (code < 200 || code >= 300) {
        if (code <= 0) {
          result->error = "Hub transport failed: " + HTTPClient::errorToString(code);
        } else if (code == 504) {
          result->error = "Assistant reply timeout.";
        } else {
          result->error = "Hub HTTP error: " + String(code);
        }
      } else {
        DynamicJsonDocument resp(4096);
        auto err = deserializeJson(resp, result->response);
        result->parsed = !err;
        if (err) {
          result->error = "Could not parse hub response.";
        } else {
          String reply = resp["reply"] | resp["full_text"] | resp["text"] | "";
          String shortText = resp["short_text"] | "";
          if (reply.isEmpty()) {
            reply = shortText;
          }
          result->shortText = shortText;
          result->telegramDelivered = resp["telegram"]["delivered"] | false;
          if (!resp["device_actions"].isNull()) {
            serializeJson(resp["device_actions"], result->deviceActionsJson);
            result->hasDeviceActions = !result->deviceActionsJson.isEmpty();
          }
          if (resp["audio"].is<JsonObject>()) {
            result->hasAudio = true;
            JsonObject audio = resp["audio"].as<JsonObject>();
            result->audioUrl = audio["url"] | "";
            result->audioPath = audio["path"] | "";
            result->audioTitle = audio["title"] | "";
            result->audioSha256 = audio["sha256"] | "";
            result->audioSize = audio["size"] | 0;
          }
          if (!reply.isEmpty()) {
          result->hasReply = true;
            result->reply = reply;
          result->ok = true;
        } else {
          result->ok = true;
          }
        }
      }
    }
  }

  if (releasedCanvasForText) {
    restoreUiCanvasAfterNetwork();
  }

  portENTER_CRITICAL(&gTextTurnMux);
  if (gCompletedTextTurn == nullptr) {
    gCompletedTextTurn = result.release();
  } else {
    delete gCompletedTextTurn;
    gCompletedTextTurn = result.release();
  }
  portEXIT_CRITICAL(&gTextTurnMux);
  vTaskDelete(nullptr);
}

void maybeStartNextTextTurn() {
  if (gTextTurnTaskHandle != nullptr || gCompletedTextTurn != nullptr || !gActiveTextTurn.isEmpty()) {
    return;
  }
  if (gPendingTextTurns.empty()) {
    if (!gRecording) {
      gSubmitting = false;
      gThinking = false;
      gAssistantPendingVisible = false;
      if (!gPlaybackActive) {
        setFaceMode(FaceMode::Idle);
      }
    }
    return;
  }

  std::unique_ptr<TextTurnTaskPayload> payload(new TextTurnTaskPayload());
  if (!payload) {
    pushError("Send queue allocation failed.");
    gPendingTextTurns.pop_front();
    gSubmitting = false;
    gThinking = false;
    gAssistantPendingVisible = false;
    setFaceMode(FaceMode::Error);
    setStatus("Queue failed", 1500);
    return;
  }

  payload->text = gPendingTextTurns.front();
  gPendingTextTurns.pop_front();
  payload->url = hubBaseUrl() + kDeviceTextTurnPath;
  payload->deviceId = gDeviceId;
  payload->deviceToken = gDeviceToken;
  payload->conversationKey = currentConversationKey();
  payload->clientMsgId = makeClientTurnId("txt");
  payload->turnId = payload->clientMsgId;

  gActiveTextTurn = payload->text;
  gSubmitting = true;
  gThinking = true;
  gAssistantPendingVisible = true;
  setFaceMode(FaceMode::Thinking);
  setStatus(gPendingTextTurns.empty() ? "Sending..." : ("Sending... +" + String(gPendingTextTurns.size())));

  BaseType_t taskOk = xTaskCreate(
      textTurnTask,
      "text-turn",
      kTextTurnTaskStackSize,
      payload.release(),
      1,
      &gTextTurnTaskHandle);
