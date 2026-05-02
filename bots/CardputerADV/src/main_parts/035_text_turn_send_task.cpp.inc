bool sendTextTurn(const String& text) {
  if ((!gWifiReady && !ensureWifiForHttp("text-turn", 3000)) || gHubHost.isEmpty()) {
    pushError("Hub is not configured.");
    return false;
  }

  HTTPClient http;
  String url = hubBaseUrl() + kDeviceTextTurnPath;
  String turnId = makeClientTurnId("txt");
  http.setTimeout(kTurnHttpTimeoutMs);
  std::unique_ptr<WiFiClientSecure> secureClient;
  bool beginOk = false;
  if (hubUsesTls()) {
    secureClient.reset(new WiFiClientSecure());
    secureClient->setInsecure();
    secureClient->setHandshakeTimeout(12);
    beginOk = http.begin(*secureClient, url);
  } else {
    beginOk = http.begin(url);
  }
  if (!beginOk) {
    pushError("Failed to open HTTP connection.");
    return false;
  }

  DynamicJsonDocument doc(768);
  doc["message"] = text;
  doc["device_id"] = gDeviceId;
  doc["firmware_version"] = kAppVersion;
  doc["conversation_key"] = currentConversationKey();
  doc["input_type"] = "text";
  doc["reply_audio"] = false;
  doc["save_reply_audio"] = false;
  doc["client_msg_id"] = turnId;
  doc["turn_id"] = turnId;

  String body;
  serializeJson(doc, body);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Client-Msg-Id", turnId);
  http.addHeader("X-Turn-Id", turnId);
  http.addHeader("X-Conversation-Key", currentConversationKey());
  if (!gDeviceToken.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + gDeviceToken);
  }

  gSubmitting = true;
  gThinking = true;
  gAssistantPendingVisible = true;
  setFaceMode(FaceMode::Thinking);
  setStatus("Sending...");
  render();
  gLastRenderMs = millis();
  delay(10);

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  gSubmitting = false;
  gThinking = false;
  gAssistantPendingVisible = false;

  if (code < 200 || code >= 300) {
    logf("[TEXT] hub error code=%d body=%s", code, response.c_str());
    if (code <= 0) {
      String transportError = HTTPClient::errorToString(code);
      pushError("Hub transport failed: " + transportError);
      setHttpDiag("text: " + transportError);
    } else if (code == 504) {
      pushError("Assistant reply timeout.");
      setHttpDiag("text: assistant timeout");
    } else {
      pushError("Hub HTTP error: " + String(code));
      setHttpDiag("text: http " + String(code));
    }
    if (!response.isEmpty()) {
      pushError(response);
    }
    setFaceMode(FaceMode::Error);
    setStatus("Text turn failed", 2000);
    return false;
  }

  DynamicJsonDocument resp(2048);
  auto err = deserializeJson(resp, response);
  if (err) {
    pushError("Could not parse hub response.");
    setFaceMode(FaceMode::Error);
    setStatus("Bad response", 2000);
    return false;
  }

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
    executeDeviceActionsJson(deviceActionsJson);
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

void updateBatterySnapshot(bool force) {
  uint32_t now = millis();
  if (!force && gLastBatterySampleMs != 0 && now - gLastBatterySampleMs < kBatterySampleIntervalMs) {
    return;
  }
  gLastBatterySampleMs = now;
  gBatterySnapshot.level = M5.Power.getBatteryLevel();
  gBatterySnapshot.voltageMv = M5.Power.getBatteryVoltage();
  gBatterySnapshot.currentMa = M5.Power.getBatteryCurrent();
  gBatterySnapshot.charging = M5.Power.isCharging();
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

  if (payload->url.isEmpty()) {
    result->error = "Hub is not configured.";
  } else if (!ensureWifiForHttp("text-turn", 3000)) {
    result->error = "Wi-Fi unavailable.";
  } else {
    HTTPClient http;
    std::unique_ptr<WiFiClientSecure> secureClient;
    http.setTimeout(kTurnHttpTimeoutMs);
    bool beginOk = false;
    if (hubUsesTls()) {
      secureClient.reset(new WiFiClientSecure());
      secureClient->setInsecure();
      secureClient->setHandshakeTimeout(12);
      beginOk = http.begin(*secureClient, payload->url);
    } else {
      beginOk = http.begin(payload->url);
    }
    if (!beginOk) {
      result->error = "Failed to open HTTP connection.";
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
