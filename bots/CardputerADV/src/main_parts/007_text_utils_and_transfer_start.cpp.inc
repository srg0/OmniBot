void removeLastUtf8Char(String& value) {
  if (value.isEmpty()) {
    return;
  }
  int pos = value.length() - 1;
  while (pos > 0 && (static_cast<uint8_t>(value[pos]) & 0xC0) == 0x80) {
    --pos;
  }
  value.remove(pos);
}

void removeFirstUtf8Char(String& value) {
  if (value.isEmpty()) {
    return;
  }
  int pos = 1;
  while (pos < value.length() && (static_cast<uint8_t>(value[pos]) & 0xC0) == 0x80) {
    ++pos;
  }
  value.remove(0, pos);
}

const char* translateRuLayoutChar(char key) {
  switch (key) {
    case '`': return u8"ё";
    case '~': return u8"Ё";
    case 'q': return u8"й";
    case 'Q': return u8"Й";
    case 'w': return u8"ц";
    case 'W': return u8"Ц";
    case 'e': return u8"у";
    case 'E': return u8"У";
    case 'r': return u8"к";
    case 'R': return u8"К";
    case 't': return u8"е";
    case 'T': return u8"Е";
    case 'y': return u8"н";
    case 'Y': return u8"Н";
    case 'u': return u8"г";
    case 'U': return u8"Г";
    case 'i': return u8"ш";
    case 'I': return u8"Ш";
    case 'o': return u8"щ";
    case 'O': return u8"Щ";
    case 'p': return u8"з";
    case 'P': return u8"З";
    case '[': return u8"х";
    case '{': return u8"Х";
    case ']': return u8"ъ";
    case '}': return u8"Ъ";
    case 'a': return u8"ф";
    case 'A': return u8"Ф";
    case 's': return u8"ы";
    case 'S': return u8"Ы";
    case 'd': return u8"в";
    case 'D': return u8"В";
    case 'f': return u8"а";
    case 'F': return u8"А";
    case 'g': return u8"п";
    case 'G': return u8"П";
    case 'h': return u8"р";
    case 'H': return u8"Р";
    case 'j': return u8"о";
    case 'J': return u8"О";
    case 'k': return u8"л";
    case 'K': return u8"Л";
    case 'l': return u8"д";
    case 'L': return u8"Д";
    case ';': return u8"ж";
    case ':': return u8"Ж";
    case '\'': return u8"э";
    case '"': return u8"Э";
    case 'z': return u8"я";
    case 'Z': return u8"Я";
    case 'x': return u8"ч";
    case 'X': return u8"Ч";
    case 'c': return u8"с";
    case 'C': return u8"С";
    case 'v': return u8"м";
    case 'V': return u8"М";
    case 'b': return u8"и";
    case 'B': return u8"И";
    case 'n': return u8"т";
    case 'N': return u8"Т";
    case 'm': return u8"ь";
    case 'M': return u8"Ь";
    case ',': return u8"б";
    case '<': return u8"Б";
    case '.': return u8"ю";
    case '>': return u8"Ю";
    case '/': return ".";
    case '?': return ",";
    default: break;
  }
  static char passthrough[2] = {0, 0};
  passthrough[0] = key;
  passthrough[1] = 0;
  return passthrough;
}

String translateInputChar(char key) {
  if (gKeyboardLayout == KeyboardLayout::Ru) {
    return String(translateRuLayoutChar(key));
  }
  return String(key);
}

bool hasValidSystemTime() {
  return time(nullptr) > 1700000000;
}

void ensureBangkokTimezone() {
  if (gTimeZoneConfigured) {
    return;
  }
  setenv("TZ", kBangkokTz, 1);
  tzset();
  gTimeZoneConfigured = true;
}

void persistRtcFromSystemTime() {
  if (!M5.Rtc.isEnabled() || !hasValidSystemTime()) {
    return;
  }
  time_t now = time(nullptr);
  struct tm localNow;
  localtime_r(&now, &localNow);
  M5.Rtc.setDateTime(&localNow);
}

String tailUtf8ToFit(const String& raw, int32_t pixelLimit) {
  String trimmed = raw;
  while (!trimmed.isEmpty() && gCanvas.textWidth(trimmed) > pixelLimit) {
    removeFirstUtf8Char(trimmed);
  }
  return trimmed;
}

void scheduleBlink() {
  gNextBlinkMs = millis() + 1800 + static_cast<uint32_t>(random(1200, 4200));
}

void setStatus(const String& text, uint32_t ttlMs = 0) {
  gStatusText = text;
  gStatusUntilMs = ttlMs ? millis() + ttlMs : 0;
}

void renderTransferUi() {
  auto& d = gCanvas;
  const uint16_t bg = rgb565_local(2, 6, 12);
  const uint16_t panel = rgb565_local(7, 13, 22);
  const uint16_t ink = rgb565_local(224, 239, 246);
  const uint16_t muted = rgb565_local(117, 136, 150);
  const uint16_t track = rgb565_local(22, 35, 47);
  uint32_t now = millis();
  uint32_t elapsed = gTransferStartedMs ? (now - gTransferStartedMs) : 0;
  int pct = gTransferTotal ? clampValue<int>((gTransferDone * 100ULL) / gTransferTotal, 0, 100) : -1;
  float pulse = (sinf(now * 0.008f) + 1.0f) * 0.5f;
  int sweep = static_cast<int>((now / 18) % 172);

  d.fillScreen(bg);
  d.fillRoundRect(8, 8, 224, 119, 14, panel);
  d.drawRoundRect(8, 8, 224, 119, 14, gTransferAccent);
  d.drawRoundRect(12, 12, 216, 111, 11, rgb565_local(18, 33, 45));

  d.setFont(&fonts::Font2);
  d.setTextSize(1);
  d.setTextColor(gTransferAccent, panel);
  d.setCursor(18, 18);
  d.print(trimPreview(gTransferTitle, 22));
  d.setTextColor(muted, panel);
  String version = String("v") + kAppVersion;
  d.setCursor(222 - d.textWidth(version), 18);
  d.print(version);

  int16_t cx = 54;
  int16_t cy = 64;
  for (int i = 0; i < 8; ++i) {
    float a = now * 0.006f + i * 0.785f;
    int16_t x = cx + static_cast<int16_t>(cosf(a) * (18 + i % 2));
    int16_t y = cy + static_cast<int16_t>(sinf(a) * 18);
    uint16_t c = (i % 2) ? rgb565_local(255, 205, 96) : gTransferAccent;
    d.fillCircle(x, y, 2 + ((i + now / 180) % 2), c);
  }
  d.fillCircle(cx, cy, 10 + static_cast<int>(pulse * 2.0f), rgb565_local(10, 27, 35));
  d.drawCircle(cx, cy, 13, gTransferAccent);

  d.setFont(&fonts::Font4);
  d.setTextColor(ink, panel);
  String pctText = pct >= 0 ? String(pct) + "%" : String("...");
  d.setCursor(92, 43);
  d.print(pctText);

  d.setFont(&fonts::Font2);
  d.setTextColor(muted, panel);
  String detail = gTransferDetail.isEmpty() ? "please wait" : gTransferDetail;
  d.setCursor(92, 70);
  d.print(trimPreview(detail, 24));

  if (gTransferTotal > 0) {
    d.setCursor(92, 84);
    d.print(compactBytes(gTransferDone));
    d.print("/");
    d.print(compactBytes(gTransferTotal));
  } else {
    d.setCursor(92, 84);
    d.print(String(elapsed / 1000) + "s");
  }

  int barX = 18;
  int barY = 104;
  int barW = 204;
  d.fillRoundRect(barX, barY, barW, 8, 4, track);
  if (pct >= 0) {
    int fillW = max<int>(4, (barW * pct) / 100);
    d.fillRoundRect(barX, barY, fillW, 8, 4, gTransferAccent);
    d.fillRoundRect(barX + max<int>(0, fillW - 18), barY + 2, min<int>(18, fillW), 4, 2, TFT_WHITE);
  } else {
    d.fillRoundRect(barX + sweep, barY, 32, 8, 4, gTransferAccent);
  }

  d.setTextColor(muted, panel);
  d.setCursor(18, 116);
  d.print("do not power off");
  gCanvas.pushSprite(0, 0);
}

void beginTransferUi(const String& title, const String& detail, uint32_t total, uint16_t accent) {
  gTransferUiActive = true;
  gTransferTitle = title;
  gTransferDetail = detail;
  gTransferDone = 0;
  gTransferTotal = total;
  gTransferStartedMs = millis();
  gTransferLastRenderMs = 0;
  gTransferAccent = accent;
  renderTransferUi();
}

