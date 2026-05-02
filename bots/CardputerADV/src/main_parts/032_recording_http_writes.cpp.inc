
bool parseHttpStatusAndHeaders(
    WiFiClient& client,
    int& statusCode,
    int& contentLength,
    uint32_t& sampleRate,
    uint32_t timeoutMs = 30000) {
  statusCode = 0;
  contentLength = -1;
  sampleRate = kTtsDefaultSampleRate;

  unsigned long deadline = millis() + timeoutMs;
  String line;
  auto readLine = [&](String& out) -> bool {
    out = "";
    while (millis() < deadline) {
      while (client.available() > 0) {
        int raw = client.read();
        if (raw < 0) {
          break;
        }
        char c = static_cast<char>(raw);
        if (c == '\n') {
          return true;
        }
        if (out.length() < 512) {
          out += c;
        }
      }
      if (!client.connected()) {
        return out.length() > 0;
      }
      pumpBlockingNetworkUi();
    }
    return out.length() > 0;
  };

  while (millis() < deadline) {
    if (!readLine(line)) {
      break;
    }
    line.trim();
    if (line.isEmpty() || !line.startsWith("HTTP/")) {
      continue;
    }

    int firstSpace = line.indexOf(' ');
    if (firstSpace >= 0) {
      statusCode = line.substring(firstSpace + 1).toInt();
    }
    logf("[HTTP] status line=%s", line.c_str());

    while (readLine(line)) {
      if (line == "\r" || line.length() == 0) {
        break;
      }
      String header = line;
      header.trim();
      String lower = header;
      lower.toLowerCase();
      if (lower.startsWith("content-length:")) {
        contentLength = header.substring(15).toInt();
      } else if (lower.startsWith("x-audio-sample-rate:")) {
        sampleRate = static_cast<uint32_t>(header.substring(20).toInt());
      }
    }

    if (statusCode == 100) {
      statusCode = 0;
      contentLength = -1;
      sampleRate = kTtsDefaultSampleRate;
      continue;
    }
    return statusCode > 0;
  }

  return false;
}

bool writeClientAll(WiFiClient& client, const uint8_t* data, size_t length, const char* label) {
  size_t written = 0;
  unsigned long deadline = millis() + 45000;
  unsigned long lastProgress = millis();
  while (written < length && millis() < deadline) {
    if (WiFi.status() != WL_CONNECTED) {
      setHttpDiag(String(label) + ": wifi lost");
      logf("[HTTP] wifi lost during write label=%s written=%u total=%u",
           label,
           static_cast<unsigned>(written),
           static_cast<unsigned>(length));
      break;
    }
    if (!client.connected()) {
      break;
    }
    size_t chunk = length - written;
    if (chunk > kHttpWriteChunkBytes) {
      chunk = kHttpWriteChunkBytes;
    }
    size_t sent = client.write(data + written, chunk);
    if (sent == 0) {
      if (millis() - lastProgress > 3000) {
        setHttpDiag(String(label) + ": stalled");
        logf("[HTTP] stalled write label=%s written=%u total=%u",
             label,
             static_cast<unsigned>(written),
             static_cast<unsigned>(length));
        break;
      }
      pumpBlockingNetworkUi();
      continue;
    }
    written += sent;
    lastProgress = millis();
    pumpBlockingNetworkUi();
  }

  if (written != length) {
    setHttpDiag(String(label) + ": short " + String(static_cast<unsigned>(written)) + "/" +
                String(static_cast<unsigned>(length)));
    logf("[HTTP] short write label=%s written=%u total=%u", label,
         static_cast<unsigned>(written), static_cast<unsigned>(length));
    return false;
  }
  setHttpDiag(String(label) + ": ok");
  return true;
}

bool writeClientAll(WiFiClient& client, const String& text, const char* label) {
  return writeClientAll(client, reinterpret_cast<const uint8_t*>(text.c_str()), text.length(), label);
}

bool writeClientAllStaged(WiFiClient& client, const uint8_t* data, size_t length, const char* label) {
  uint8_t stage[kHttpStageChunkBytes];
  size_t written = 0;
  unsigned long deadline = millis() + 45000;
  unsigned long lastProgress = millis();

  while (written < length && millis() < deadline) {
    if (WiFi.status() != WL_CONNECTED) {
      setHttpDiag(String(label) + ": wifi lost");
      logf("[HTTP] staged wifi lost label=%s written=%u total=%u",
           label,
           static_cast<unsigned>(written),
           static_cast<unsigned>(length));
      break;
    }
    if (!client.connected()) {
      setHttpDiag(String(label) + ": disconnected");
      break;
    }

    size_t chunk = length - written;
    if (chunk > sizeof(stage)) {
      chunk = sizeof(stage);
    }
    memcpy(stage, data + written, chunk);
    size_t sent = client.write(stage, chunk);
    if (sent == 0) {
      if (millis() - lastProgress > 3000) {
        setHttpDiag(String(label) + ": stalled");
        logf("[HTTP] staged stalled label=%s written=%u total=%u",
             label,
             static_cast<unsigned>(written),
             static_cast<unsigned>(length));
        break;
      }
      pumpBlockingNetworkUi();
      continue;
    }
    written += sent;
    lastProgress = millis();
    if ((written % (8 * 1024)) == 0 || written == length) {
      setHttpDiag(String(label) + ": " + String(static_cast<unsigned>(written / 1024)) + "k/" +
                  String(static_cast<unsigned>(length / 1024)) + "k");
    }
    pumpBlockingNetworkUi();
  }

  if (written != length) {
    setHttpDiag(String(label) + ": short " + String(static_cast<unsigned>(written)) + "/" +
                String(static_cast<unsigned>(length)));
    logf("[HTTP] staged short write label=%s written=%u total=%u",
         label,
         static_cast<unsigned>(written),
         static_cast<unsigned>(length));
    return false;
  }
  setHttpDiag(String(label) + ": ok " + String(static_cast<unsigned>(length / 1024)) + "k");
  return true;
}

bool writeClientAllFileStaged(WiFiClient& client, File& file, size_t length, const char* label) {
  uint8_t stage[kHttpStageChunkBytes];
  size_t written = 0;
  unsigned long deadline = millis() + 90000;
  unsigned long lastProgress = millis();

  while (written < length && millis() < deadline) {
    if (WiFi.status() != WL_CONNECTED) {
      setHttpDiag(String(label) + ": wifi lost");
      return false;
    }
    if (!client.connected()) {
      setHttpDiag(String(label) + ": disconnected");
      return false;
    }
    size_t chunk = length - written;
    if (chunk > sizeof(stage)) {
      chunk = sizeof(stage);
    }
    size_t readBytes = file.read(stage, chunk);
    if (readBytes == 0) {
      break;
    }
    size_t sentFromStage = 0;
    while (sentFromStage < readBytes && millis() < deadline) {
      if (WiFi.status() != WL_CONNECTED) {
        setHttpDiag(String(label) + ": wifi lost");
        return false;
      }
      if (!client.connected()) {
        setHttpDiag(String(label) + ": disconnected");
        return false;
      }
      size_t sent = client.write(stage + sentFromStage, readBytes - sentFromStage);
      if (sent == 0) {
        if (millis() - lastProgress > 5000) {
          setHttpDiag(String(label) + ": stalled");
          return false;
        }
        pumpBlockingNetworkUi();
        continue;
      }
      sentFromStage += sent;
      written += sent;
      lastProgress = millis();
      if ((written % (8 * 1024)) == 0 || written == length) {
        setHttpDiag(String(label) + ": " + String(static_cast<unsigned>(written / 1024)) + "k/" +
                    String(static_cast<unsigned>(length / 1024)) + "k");
      }
      pumpBlockingNetworkUi();
    }
  }

  if (written != length) {
    setHttpDiag(String(label) + ": short " + String(static_cast<unsigned>(written)) + "/" +
                String(static_cast<unsigned>(length)));
    return false;
  }
  setHttpDiag(String(label) + ": ok " + String(static_cast<unsigned>(length / 1024)) + "k");
  return true;
}
