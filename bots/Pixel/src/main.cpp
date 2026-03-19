#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h> 
#include <Arduino_GFX_Library.h>
#include "esp_camera.h"
#include <driver/i2s.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Wire.h> 
#include <RTClib.h>
#include <stdio.h>
#include <math.h>

// ==========================================
//        USER CONFIGURATION
// ==========================================
const char* backend_ip = "10.0.0.180"; 
const int backend_port = 8000;

#define BLE_SERVICE_NAME "Pixel"
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WIFI_CREDS_CHAR_UUID   "12345678-1234-5678-1234-56789abcdef0"

// ==========================================
//            PIN DEFINITIONS
// ==========================================
#define TFT_SCK    D8
#define TFT_MOSI   D10
#define TFT_MISO   D9
#define TFT_CS     D1
#define TFT_DC     D3
#define TFT_RST    -1  
#define TFT_BL     D6

// Seeed Round Display Touch & RTC Pins
#define TOUCH_INT  D7 
#define TOUCH_SDA  D4
#define TOUCH_SCL  D5
#define CHSC6X_I2C_ID 0x2e

#define I2S_WS     42
#define I2S_SD     41

// Camera Pins
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 10
#define SIOD_GPIO_NUM 40
#define SIOC_GPIO_NUM 39
#define Y9_GPIO_NUM 48
#define Y8_GPIO_NUM 11
#define Y7_GPIO_NUM 12
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 16
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 17
#define Y2_GPIO_NUM 15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM 47
#define PCLK_GPIO_NUM 13

// ==========================================
//            GLOBALS
// ==========================================
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, TFT_RST, 0, true);
Preferences preferences;
RTC_PCF8563 rtc;

#define SAMPLE_RATE 16000
#define MAX_RECORD_SECONDS 15 
#define FLASH_RECORD_SIZE (SAMPLE_RATE * MAX_RECORD_SECONDS * 2) 

uint8_t *rec_buffer = NULL; 
size_t recordedAudioLen = 0;

#define MAX_FRAMES 150
uint8_t* video_frames[MAX_FRAMES];
size_t video_frame_sizes[MAX_FRAMES];
int video_frame_count = 0;

WebSocketsClient webSocket;
bool isWsConnected = false;

#define FRAME_INTERVAL_MS 100 
unsigned long lastFrameTime = 0;

enum RobotState { STATE_IDLE, STATE_RECORDING, STATE_SETUP, STATE_UPLOADING };
volatile RobotState currentState = STATE_SETUP;

// Touch & Gesture Tracking Globals
int startX = -1, startY = -1;
int endX = -1, endY = -1;
unsigned long touchStartTime = 0;
unsigned long lastActiveTouchTime = 0;
bool isTouching = false;

// Timers & Debounce Globals
unsigned long lastTapTime = 0; 
const int tapCooldown = 500; 
unsigned long lastRtcPrintTime = 0;
bool timeScreenActive = false;
unsigned long lastTimeScreenUpdate = 0;
String lastRenderedDay = "";
String lastRenderedDate = "";
String lastRenderedTime = "";

unsigned long lastRecordingEyesUpdate = 0;
unsigned long lastIdleEyesUpdate = 0;

bool needsRestart = false;
volatile bool triggerUpload = false;
const uint8_t WIFI_CONNECT_ATTEMPTS = 3;
const uint16_t WIFI_CONNECT_TIMEOUT_MS = 15000;
const uint8_t WIFI_FAILS_BEFORE_BLE_FALLBACK = 3;
const uint8_t WIFI_FAILS_BEFORE_CREDS_CLEAR = 8;

struct IdleAnimState {
    int16_t lookX = 0;
    int16_t lookY = 0;
    int16_t targetLookX = 0;
    int16_t targetLookY = 0;
    uint32_t nextLookChangeMs = 0;
    uint32_t lookHoldUntilMs = 0;

    uint32_t blinkStartMs = 0;
    uint16_t blinkDurationMs = 0;
    uint32_t nextBlinkMs = 0;

    uint8_t expressionType = 0;
    bool expressionActive = false;
    uint32_t expressionStartMs = 0;
    uint16_t expressionDurationMs = 0;
    uint32_t nextExpressionMs = 0;
    int8_t expressionSide = 0; // -1 left, +1 right (used by wink)

    bool focusLockActive = false;
    uint32_t focusLockUntilMs = 0;

    int16_t prevEyeXOffset = 0;
    int16_t prevEyeYOffset = 0;
    int16_t prevEyeRyL = 66;
    int16_t prevEyeRyR = 66;
    bool hasPrevFrame = false;
};

IdleAnimState idleAnim;

// ==========================================
//          UI HELPER
// ==========================================
void updateScreen(String text, uint16_t color) {
    gfx->fillScreen(BLACK);
    gfx->setTextColor(color);
    gfx->setTextSize(3);
    
    int cursorX = 120 - (text.length() * 9); 
    if (cursorX < 10) cursorX = 10;
    
    gfx->setCursor(cursorX, 110);
    gfx->print(text);
}

static void fillOval(int16_t xc, int16_t yc, int16_t rx, int16_t ry, uint16_t color) {
    if (rx <= 0 || ry <= 0) return;

    int32_t rx2 = (int32_t)rx * (int32_t)rx;
    int32_t ry2 = (int32_t)ry * (int32_t)ry;

    // Scanline ellipse fill.
    for (int16_t dy = -ry; dy <= ry; dy++) {
        int32_t dy2 = (int32_t)dy * (int32_t)dy;
        int32_t t = ry2 - dy2;
        if (t < 0) continue;

        // x = rx * sqrt(1 - dy^2 / ry^2) = sqrt(rx^2 * (1 - dy^2/ry^2))
        int32_t x = (int32_t)(sqrtf((float)(rx2 * t) / (float)ry2) + 0.5f);
        if (x < 0) continue;

        gfx->drawFastHLine(xc - (int16_t)x, yc + dy, (int16_t)(2 * x + 1), color);
    }
}

void drawEyes(int16_t eyeXOffset, int16_t eyeYOffset, int16_t eyeRyL, int16_t eyeRyR, uint16_t color) {
    const int16_t baseY = 112;
    const int16_t rx = 46;
    const int16_t cxL = 68 + eyeXOffset;
    const int16_t cxR = 172 + eyeXOffset;

    fillOval(cxL, baseY + eyeYOffset, rx, eyeRyL, color);
    fillOval(cxR, baseY + eyeYOffset, rx, eyeRyR, color);
}

static bool ovalSpanAtY(int16_t xc, int16_t yc, int16_t rx, int16_t ry, int16_t y, int16_t &x1, int16_t &x2) {
    if (rx <= 0 || ry <= 0) return false;

    int16_t dy = y - yc;
    if (dy < -ry || dy > ry) return false;

    int32_t rx2 = (int32_t)rx * (int32_t)rx;
    int32_t ry2 = (int32_t)ry * (int32_t)ry;
    int32_t dy2 = (int32_t)dy * (int32_t)dy;
    int32_t t = ry2 - dy2;
    if (t < 0) return false;

    int16_t half = (int16_t)(sqrtf((float)(rx2 * t) / (float)ry2) + 0.5f);
    x1 = xc - half;
    x2 = xc + half;
    return true;
}

static void drawSpanIfValid(int16_t y, int16_t x1, int16_t x2, uint16_t color) {
    if (x2 < x1) return;
    gfx->drawFastHLine(x1, y, x2 - x1 + 1, color);
}

static void drawSpanDelta(
    int16_t y,
    bool hadOld, int16_t oldX1, int16_t oldX2,
    bool hasNew, int16_t newX1, int16_t newX2,
    uint16_t onColor, uint16_t offColor
) {
    if (hadOld && !hasNew) {
        drawSpanIfValid(y, oldX1, oldX2, offColor);
        return;
    }
    if (!hadOld && hasNew) {
        drawSpanIfValid(y, newX1, newX2, onColor);
        return;
    }
    if (!hadOld && !hasNew) return;

    // Both exist. Only update non-overlapping segments.
    drawSpanIfValid(y, oldX1, min(oldX2, (int16_t)(newX1 - 1)), offColor);
    drawSpanIfValid(y, max(oldX1, (int16_t)(newX2 + 1)), oldX2, offColor);
    drawSpanIfValid(y, newX1, min(newX2, (int16_t)(oldX1 - 1)), onColor);
    drawSpanIfValid(y, max(newX1, (int16_t)(oldX2 + 1)), newX2, onColor);
}

static void drawEyeDelta(
    int16_t oldCx,
    int16_t newCx,
    int16_t oldYc,
    int16_t newYc,
    int16_t rx,
    int16_t oldRy,
    int16_t newRy,
    uint16_t onColor,
    uint16_t offColor
) {
    int16_t yMin = min((int16_t)(oldYc - oldRy), (int16_t)(newYc - newRy));
    int16_t yMax = max((int16_t)(oldYc + oldRy), (int16_t)(newYc + newRy));

    for (int16_t y = yMin; y <= yMax; y++) {
        int16_t oldX1 = 0, oldX2 = -1;
        int16_t newX1 = 0, newX2 = -1;
        bool hadOld = ovalSpanAtY(oldCx, oldYc, rx, oldRy, y, oldX1, oldX2);
        bool hasNew = ovalSpanAtY(newCx, newYc, rx, newRy, y, newX1, newX2);
        drawSpanDelta(y, hadOld, oldX1, oldX2, hasNew, newX1, newX2, onColor, offColor);
    }
}

void drawEyesDelta(
    int16_t oldXOffset,
    int16_t oldYOffset,
    int16_t oldRyL,
    int16_t oldRyR,
    int16_t newXOffset,
    int16_t newYOffset,
    int16_t newRyL,
    int16_t newRyR,
    uint16_t onColor
) {
    const int16_t baseY = 112;
    const int16_t rx = 46;
    const int16_t oldCxL = 68 + oldXOffset;
    const int16_t oldCxR = 172 + oldXOffset;
    const int16_t newCxL = 68 + newXOffset;
    const int16_t newCxR = 172 + newXOffset;
    const int16_t oldYc = baseY + oldYOffset;
    const int16_t newYc = baseY + newYOffset;

    drawEyeDelta(oldCxL, newCxL, oldYc, newYc, rx, oldRyL, newRyL, onColor, BLACK);
    drawEyeDelta(oldCxR, newCxR, oldYc, newYc, rx, oldRyR, newRyR, onColor, BLACK);
}

static int16_t triPulse(uint32_t elapsed, uint16_t durationMs, int16_t amplitude) {
    if (durationMs == 0 || elapsed >= durationMs) return 0;
    float t = (float)elapsed / (float)durationMs;   // 0..1
    float tri = 1.0f - fabsf(2.0f * t - 1.0f);      // 0..1..0
    return (int16_t)(tri * (float)amplitude);
}

void showIdleEyes() {
    gfx->fillScreen(BLACK);

    idleAnim.lookX = 0;
    idleAnim.lookY = 0;
    idleAnim.targetLookX = 0;
    idleAnim.targetLookY = 0;
    idleAnim.lookHoldUntilMs = 0;
    idleAnim.nextLookChangeMs = millis() + (uint32_t)random(600, 1800);
    idleAnim.blinkStartMs = 0;
    idleAnim.blinkDurationMs = 0;
    idleAnim.nextBlinkMs = millis() + (uint32_t)random(2200, 5200);
    idleAnim.expressionType = 0;
    idleAnim.expressionActive = false;
    idleAnim.expressionStartMs = 0;
    idleAnim.expressionDurationMs = 0;
    idleAnim.nextExpressionMs = millis() + (uint32_t)random(1800, 4800);
    idleAnim.expressionSide = 0;
    idleAnim.focusLockActive = false;
    idleAnim.focusLockUntilMs = 0;
    idleAnim.prevEyeXOffset = 0;
    idleAnim.prevEyeYOffset = 0;
    idleAnim.prevEyeRyL = 66;
    idleAnim.prevEyeRyR = 66;
    idleAnim.hasPrevFrame = true;

    drawEyes(0, 0, 66, 66, CYAN);
}

void updateIdleEyesAnimation() {
    const uint32_t now = millis();

    // Throttle redraw to avoid unnecessary TFT work.
    if (now - lastIdleEyesUpdate < 50) return;
    lastIdleEyesUpdate = now;

    // Every so often, pick a new "look around" target then hold.
    if (idleAnim.focusLockActive && now >= idleAnim.focusLockUntilMs) {
        idleAnim.focusLockActive = false;
        idleAnim.targetLookX = 0;
        idleAnim.targetLookY = 0;
        idleAnim.lookHoldUntilMs = now + (uint32_t)random(300, 700);
    }

    if (!idleAnim.focusLockActive && now >= idleAnim.nextLookChangeMs && now >= idleAnim.lookHoldUntilMs) {
        idleAnim.targetLookX = random(-10, 11);
        idleAnim.targetLookY = random(-5, 6);
        idleAnim.lookHoldUntilMs = now + (uint32_t)random(180, 450);
        idleAnim.nextLookChangeMs = now + (uint32_t)random(900, 2400);
    }

    // Smoothly chase target to make motion feel natural.
    idleAnim.lookX += (idleAnim.targetLookX - idleAnim.lookX) / 3;
    idleAnim.lookY += (idleAnim.targetLookY - idleAnim.lookY) / 3;

    // Baseline blink fallback when no special expression is running.
    if (!idleAnim.expressionActive && idleAnim.blinkStartMs == 0 && now >= idleAnim.nextBlinkMs) {
        idleAnim.blinkStartMs = now;
        idleAnim.blinkDurationMs = (uint16_t)random(120, 240);
        idleAnim.nextBlinkMs = now + (uint32_t)random(2500, 7000);
    }

    // Start a random expression at long-ish intervals.
    if (!idleAnim.expressionActive && now >= idleAnim.nextExpressionMs) {
        int roll = random(1000);
        idleAnim.expressionActive = true;
        idleAnim.expressionStartMs = now;
        idleAnim.expressionSide = (random(2) == 0) ? -1 : 1;

        // 1=double blink, 3=focus lock, 4=wink, 5=sleepy cycle
        if (roll < 420) {
            idleAnim.expressionType = 1;
            idleAnim.expressionDurationMs = 320; // 120 + 80 + 120
        } else if (roll < 760) {
            idleAnim.expressionType = 3;
            idleAnim.expressionDurationMs = (uint16_t)random(500, 1201);
            idleAnim.focusLockActive = true;
            idleAnim.focusLockUntilMs = now + idleAnim.expressionDurationMs;
            int16_t x = (random(2) == 0) ? random(-14, -7) : random(7, 15);
            idleAnim.targetLookX = x;
            idleAnim.targetLookY = random(-2, 3);
            idleAnim.lookHoldUntilMs = idleAnim.focusLockUntilMs;
        } else if (roll < 900) {
            idleAnim.expressionType = 4;
            idleAnim.expressionDurationMs = (uint16_t)random(120, 181);
        } else {
            idleAnim.expressionType = 5;
            idleAnim.expressionDurationMs = 500; // two heavy squints
        }
    }

    int16_t eyeRyL = 66;
    int16_t eyeRyR = 66;

    // Baseline single blink.
    if (idleAnim.blinkStartMs != 0) {
        uint32_t elapsed = now - idleAnim.blinkStartMs;
        if (elapsed >= idleAnim.blinkDurationMs) {
            idleAnim.blinkStartMs = 0;
        } else {
            int16_t sq = triPulse(elapsed, idleAnim.blinkDurationMs, 54);
            eyeRyL -= sq;
            eyeRyR -= sq;
        }
    }

    // Expression overlays.
    if (idleAnim.expressionActive) {
        uint32_t e = now - idleAnim.expressionStartMs;

        if (e >= idleAnim.expressionDurationMs) {
            idleAnim.expressionActive = false;
            idleAnim.expressionType = 0;
            idleAnim.nextExpressionMs = now + (uint32_t)random(2200, 7000);
        } else {
            if (idleAnim.expressionType == 1) {
                // Double blink: pulse, short pause, pulse.
                int16_t sq = 0;
                if (e < 120) sq = triPulse(e, 120, 56);
                else if (e >= 200 && e < 320) sq = triPulse(e - 200, 120, 56);
                eyeRyL -= sq;
                eyeRyR -= sq;
            } else if (idleAnim.expressionType == 4) {
                // Wink: one eye closes deeply.
                int16_t wink = triPulse(e, idleAnim.expressionDurationMs, 62);
                if (idleAnim.expressionSide < 0) eyeRyL -= wink;
                else eyeRyR -= wink;
            } else if (idleAnim.expressionType == 5) {
                // Sleepy cycle: two heavier, slower squints.
                int16_t sq = 0;
                if (e < 190) sq = triPulse(e, 190, 58);
                else if (e >= 310 && e < 500) sq = triPulse(e - 310, 190, 58);
                eyeRyL -= sq;
                eyeRyR -= sq;
            }
        }
    }

    if (eyeRyL < 8) eyeRyL = 8;
    if (eyeRyR < 8) eyeRyR = 8;

    // Differential redraw: update only pixels that changed between frames.
    if (!idleAnim.hasPrevFrame) {
        drawEyes(idleAnim.lookX, idleAnim.lookY, eyeRyL, eyeRyR, CYAN);
    } else {
        drawEyesDelta(
            idleAnim.prevEyeXOffset,
            idleAnim.prevEyeYOffset,
            idleAnim.prevEyeRyL,
            idleAnim.prevEyeRyR,
            idleAnim.lookX,
            idleAnim.lookY,
            eyeRyL,
            eyeRyR,
            CYAN
        );
    }

    idleAnim.prevEyeXOffset = idleAnim.lookX;
    idleAnim.prevEyeYOffset = idleAnim.lookY;
    idleAnim.prevEyeRyL = eyeRyL;
    idleAnim.prevEyeRyR = eyeRyR;
    idleAnim.hasPrevFrame = true;
}

void showRecordingEyesSquint(int16_t ry) {
    gfx->fillScreen(BLACK);

    // Same geometry as idle eyes, but yellow and squinting (ry shrinks).
    const int16_t yc = 112;
    const int16_t rx = 46;
    const int16_t cxL = 68;
    const int16_t cxR = 172;

    fillOval(cxL, yc, rx, ry, YELLOW);
    fillOval(cxR, yc, rx, ry, YELLOW);
}

void resetTimeScreenCache() {
    lastRenderedDay = "";
    lastRenderedDate = "";
    lastRenderedTime = "";
}

void drawCenteredTextLine(const String& text, int16_t y, uint8_t textSize, uint16_t color) {
    const int16_t charW = 6 * textSize;
    const int16_t lineH = 8 * textSize;
    int16_t x = 120 - ((int16_t)text.length() * charW) / 2;
    if (x < 8) x = 8;
    gfx->fillRect(0, y, 240, lineH, BLACK);
    gfx->setTextSize(textSize);
    gfx->setTextColor(color);
    gfx->setCursor(x, y);
    gfx->print(text);
}

void drawDeltaTimeLine(const String& previous, const String& current, int16_t y, uint8_t textSize, uint16_t color) {
    if (previous.length() != current.length()) {
        drawCenteredTextLine(current, y, textSize, color);
        return;
    }

    const int16_t charW = 6 * textSize;
    const int16_t charH = 8 * textSize;
    int16_t xStart = 120 - ((int16_t)current.length() * charW) / 2;
    if (xStart < 8) xStart = 8;

    gfx->setTextSize(textSize);
    gfx->setTextColor(color);

    for (uint16_t i = 0; i < current.length(); i++) {
        if (current.charAt(i) == previous.charAt(i)) continue;
        int16_t x = xStart + ((int16_t)i * charW);
        gfx->fillRect(x, y, charW, charH, BLACK);
        gfx->setCursor(x, y);
        gfx->print(current.charAt(i));
    }
}

void updateRecordingEyesAnimation() {
    // Static red eyes while recording (no spinner animation).
    gfx->fillScreen(BLACK);

    const int16_t yc = 112;
    const int16_t rx = 46;
    const int16_t ry = 66;
    const int16_t cxL = 68;
    const int16_t cxR = 172;

    fillOval(cxL, yc, rx, ry, RED);
    fillOval(cxR, yc, rx, ry, RED);
}

void updateTimeScreen() {
    DateTime now = rtc.now();
    static const char* weekdayNames[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };
    static const char* monthNames[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    char timeBuf[12];   // "HH:MM:SS AM" + null
    char dateBuf[16];   // "Mon DD, YYYY" + null
    const char* dayName = weekdayNames[now.dayOfTheWeek()];
    const char* monthName = monthNames[now.month() - 1];
    int hour24 = now.hour();
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    const char* meridiem = (hour24 >= 12) ? "PM" : "AM";

    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d %s", hour12, now.minute(), now.second(), meridiem);
    snprintf(dateBuf, sizeof(dateBuf), "%s %02d, %04d", monthName, now.day(), now.year());
    String dayText = String(dayName);
    String dateText = String(dateBuf);
    String timeText = String(timeBuf);

    if (lastRenderedDay != dayText) {
        drawCenteredTextLine(dayText, 58, 2, WHITE);
        lastRenderedDay = dayText;
    }

    if (lastRenderedDate != dateText) {
        drawCenteredTextLine(dateText, 92, 2, WHITE);
        lastRenderedDate = dateText;
    }

    if (lastRenderedTime.length() == 0) {
        drawCenteredTextLine(timeText, 132, 3, WHITE);
    } else {
        drawDeltaTimeLine(lastRenderedTime, timeText, 132, 3, WHITE);
    }
    lastRenderedTime = timeText;
}

// ==========================================
//          HARDWARE SETUP
// ==========================================
void setupMicrophone() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 16, 
        .dma_buf_len = 128,  
        .use_apll = false
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void captureVideoFrame() {
    if (!isWsConnected) return; 
    if (millis() - lastFrameTime < FRAME_INTERVAL_MS) return;
    lastFrameTime = millis();

    if (video_frame_count >= MAX_FRAMES) return;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;

    uint8_t* frame_buf = (uint8_t*)ps_malloc(fb->len);
    if (frame_buf) {
        memcpy(frame_buf, fb->buf, fb->len);
        video_frames[video_frame_count] = frame_buf;
        video_frame_sizes[video_frame_count] = fb->len;
        video_frame_count++;
    } 
    
    esp_camera_fb_return(fb);
}

void processAudio() {
    size_t bytesRead = 0;
    if (recordedAudioLen + 512 > FLASH_RECORD_SIZE) return;
    
    i2s_read(I2S_NUM_0, rec_buffer + 44 + recordedAudioLen, 512, &bytesRead, portMAX_DELAY);
    if (bytesRead > 0) {
        recordedAudioLen += bytesRead;
    }
}

// ==========================================
//      BLE PROVISIONING CALLBACKS
// ==========================================
class ProvisionCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue().c_str();

      if (rxValue.length() > 0) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, rxValue);
        
        if (!error) {
            String new_ssid = doc["ssid"].as<String>();
            String new_pass = doc["password"].as<String>();
            
            if(new_ssid.length() > 0) {
                preferences.begin("wifi_creds", false); 
                preferences.putString("ssid", new_ssid);
                preferences.putString("password", new_pass);
                preferences.end();
                
                needsRestart = true;
            }
        }
      }
    }
};

void startBLEProvisioning() {
    currentState = STATE_SETUP;
    updateScreen("BLE SETUP", BLUE);
    
    BLEDevice::init(BLE_SERVICE_NAME);
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         WIFI_CREDS_CHAR_UUID,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
                                       
    pCharacteristic->setCallbacks(new ProvisionCallbacks());
    pService->start();
    
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
}

void setupWiFi() {
    preferences.begin("wifi_creds", true); 
    String saved_ssid = preferences.getString("ssid", "");
    String saved_pass = preferences.getString("password", "");
    preferences.end();
    
    if (saved_ssid == "") {
        startBLEProvisioning();
        return;
    }
    
    updateScreen("CONNECTING", YELLOW);
    esp_wifi_set_max_tx_power(78); 
    WiFi.mode(WIFI_STA);

    bool connected = false;
    for (uint8_t attempt = 0; attempt < WIFI_CONNECT_ATTEMPTS; attempt++) {
        WiFi.disconnect(true, false);
        delay(80);
        WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());

        uint32_t attemptStart = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - attemptStart) < WIFI_CONNECT_TIMEOUT_MS) {
            delay(250);
        }

        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
        }
    }

    if (!connected) {
        preferences.begin("wifi_meta", false);
        uint8_t failCount = preferences.getUChar("fail_count", 0);
        if (failCount < 255) failCount++;
        preferences.putUChar("fail_count", failCount);
        preferences.end();

        if (failCount >= WIFI_FAILS_BEFORE_CREDS_CLEAR) {
            // Last-resort recovery after many failed boots.
            preferences.begin("wifi_creds", false);
            preferences.remove("ssid");
            preferences.remove("password");
            preferences.end();

            preferences.begin("wifi_meta", false);
            preferences.putUChar("fail_count", 0);
            preferences.end();
        }

        if (failCount >= WIFI_FAILS_BEFORE_BLE_FALLBACK) {
            startBLEProvisioning();
        } else {
            updateScreen("WIFI RETRY", YELLOW);
            delay(800);
            ESP.restart();
        }
        return;
    }
    
    preferences.begin("wifi_meta", false);
    preferences.putUChar("fail_count", 0);
    preferences.end();

    WiFi.setSleep(false);
    currentState = STATE_IDLE; 
    showIdleEyes();
    
    webSocket.begin(backend_ip, backend_port, "/ws/stream");
    webSocket.onEvent([](WStype_t type, uint8_t * payload, size_t length) {
        switch(type) {
            case WStype_DISCONNECTED:
                isWsConnected = false;
                break;
            case WStype_CONNECTED:
                isWsConnected = true;
                break;
            case WStype_TEXT:
            case WStype_BIN:
            case WStype_ERROR:          
            case WStype_FRAGMENT_TEXT_START:
            case WStype_FRAGMENT_BIN_START:
            case WStype_FRAGMENT:
            case WStype_FRAGMENT_FIN:
                break;
        }
    });

    webSocket.setReconnectInterval(5000); 
}

void triggerGeminiProcessing() {
    if (isWsConnected) {
        uint8_t header = 0x03; 
        webSocket.sendBIN(&header, 1);
    }
}

// ==========================================
//     FREERTOS UPLOAD TASK (CORE 0)
// ==========================================
void uploadDataTask(void * pvParameters) {
    for(;;) {
        if (triggerUpload) {
            
            if (!isWsConnected) {
                for (int i=0; i<video_frame_count; i++) {
                    if (video_frames[i]) {
                        free(video_frames[i]);
                        video_frames[i] = NULL;
                    }
                }
                recordedAudioLen = 0;
                video_frame_count = 0;
            } else {
                
                size_t offset = 0;
                while(offset < recordedAudioLen) {
                    size_t chunk = 4096; 
                    if (offset + chunk > recordedAudioLen) chunk = recordedAudioLen - offset;
                    
                    uint8_t* packet = (uint8_t*)malloc(chunk + 1);
                    if (packet) {
                        packet[0] = 0x01;
                        memcpy(packet + 1, rec_buffer + 44 + offset, chunk);
                        webSocket.sendBIN(packet, chunk + 1);
                        free(packet);
                    }
                    offset += chunk;
                    webSocket.loop(); 
                    vTaskDelay(pdMS_TO_TICKS(1)); 
                }

                for (int i=0; i<video_frame_count; i++) {
                    size_t fsize = video_frame_sizes[i];
                    uint8_t* packet = (uint8_t*)malloc(fsize + 1);
                    if (packet) {
                        packet[0] = 0x02;
                        memcpy(packet + 1, video_frames[i], fsize);
                        webSocket.sendBIN(packet, fsize + 1);
                        free(packet);
                    }
                    
                    if (video_frames[i]) {
                        free(video_frames[i]);
                        video_frames[i] = NULL;
                    }
                    
                    webSocket.loop();
                    vTaskDelay(pdMS_TO_TICKS(1));
                }

                recordedAudioLen = 0;
                video_frame_count = 0;
                triggerGeminiProcessing();
            }

            triggerUpload = false;
            currentState = STATE_IDLE;
            showIdleEyes();
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

// ==========================================
//                ARDUINO SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    randomSeed((uint32_t)esp_random());
    
    pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
    gfx->begin(80000000); 
    updateScreen("BOOTING...", WHITE);
    
    // Initialize native I2C for the CHSC6X Touch Controller & RTC
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    pinMode(TOUCH_INT, INPUT_PULLUP);

    // Initialize RTC
    if (!rtc.begin()) {
        Serial.println("[ERROR] Couldn't find RTC!");
    } else {
        if (rtc.lostPower()) {
            Serial.println("[RTC] Power lost or first boot, setting time to compile time!");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        Serial.println("[RTC] Initialized successfully.");
    }

    rec_buffer = (uint8_t *)ps_malloc(FLASH_RECORD_SIZE + 44);
    if (rec_buffer == NULL) {
        updateScreen("MEM ERROR", RED);
        while(1);
    }

    setupMicrophone();
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 15; config.pin_d1 = 17; config.pin_d2 = 18; config.pin_d3 = 16;
    config.pin_d4 = 14; config.pin_d5 = 12; config.pin_d6 = 11; config.pin_d7 = 48;
    config.pin_xclk = 10; config.pin_pclk = 13; config.pin_vsync = 38; config.pin_href = 47;
    config.pin_sccb_sda = 40; config.pin_sccb_scl = 39; config.pin_pwdn = -1; config.pin_reset = -1;
    config.xclk_freq_hz = 20000000; config.frame_size = FRAMESIZE_VGA; 
    config.pixel_format = PIXFORMAT_JPEG; config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM; 
    config.jpeg_quality = 20; 
    config.fb_count = 2;
    esp_camera_init(&config);

    setupWiFi();

    xTaskCreatePinnedToCore(
        uploadDataTask, "Upload Task", 10000, NULL, 1, NULL, 0
    );
}

// ==========================================
//                ARDUINO LOOP
// ==========================================
void loop() {
    // --- RTC SERIAL TICKER ---
    if (millis() - lastRtcPrintTime > 1000) {
        lastRtcPrintTime = millis();
        DateTime now = rtc.now();
        
        Serial.printf("[RTC] %04d/%02d/%02d %02d:%02d:%02d\n", 
                      now.year(), now.month(), now.day(), 
                      now.hour(), now.minute(), now.second());
    }

    // --- Time Screen UI Refresh ---
    if (timeScreenActive && currentState == STATE_IDLE) {
        if (millis() - lastTimeScreenUpdate > 1000) {
            lastTimeScreenUpdate = millis();
            updateTimeScreen();
        }
    }

    if (currentState != STATE_SETUP) {
        webSocket.loop();
    }

    // ========================================================
    //      Native CHSC6X Touch & Gesture Implementation
    // ========================================================
    bool currentTouchState = digitalRead(TOUCH_INT);
    bool gestureReady = false;
    
    // 1. Read Data on Interrupt Pulse (Active LOW)
    if (currentTouchState == LOW) {
        Wire.requestFrom(CHSC6X_I2C_ID, 5);
        if (Wire.available() == 5) {
            uint8_t points = Wire.read(); 
            Wire.read(); // Event ID
            int tempX = Wire.read(); 
            Wire.read(); // Reserved ID
            int tempY = Wire.read(); 
            
            if (points > 0) {
                if (!isTouching) {
                    isTouching = true;
                    startX = tempX;
                    startY = tempY;
                    touchStartTime = millis();
                }
                endX = tempX;
                endY = tempY;
                lastActiveTouchTime = millis();
            } 
            else if (points == 0 && isTouching) {
                // The IC explicitly sent a "lifted" event
                gestureReady = true;
            }
        }
    }

    // 2. Timeout Fallback
    // If we haven't received a pulse in 50ms, assume the finger was lifted
    if (isTouching && (millis() - lastActiveTouchTime > 50)) {
        gestureReady = true;
    }

    // 3. Evaluate the fully completed gesture
    if (gestureReady) {
        isTouching = false; // Reset state for the next touch
        
        int deltaX = endX - startX;
        int deltaY = endY - startY;
        unsigned long touchDuration = millis() - touchStartTime;
        
        int absX = abs(deltaX);
        int absY = abs(deltaY);
        
        // Tuning Thresholds
        int minSwipeDist = 30;  // Pixels to travel to trigger a swipe
        int maxTapDist = 20;    // Max drift allowed during a tap
        int maxTapTime = 500;   // Max milliseconds for a tap
        
        // Evaluate TAP
        if (absX <= maxTapDist && absY <= maxTapDist && touchDuration < maxTapTime) {
            
            // Apply debounce cooldown
            if (millis() - lastTapTime > tapCooldown) {
                lastTapTime = millis();
                
                if (currentState == STATE_IDLE) {
                    Serial.println("\n*** TAP DETECTED: STARTING RECORD ***\n");
                    currentState = STATE_RECORDING;
                    timeScreenActive = false;
                    resetTimeScreenCache();
                    lastRecordingEyesUpdate = 0; // force immediate redraw
                    lastFrameTime = 0;
                    recordedAudioLen = 0;
                    video_frame_count = 0;
                    updateRecordingEyesAnimation();
                } 
                else if (currentState == STATE_RECORDING) {
                    Serial.println("\n*** TAP DETECTED: STOPPING RECORD ***\n");
                    currentState = STATE_UPLOADING; 
                    timeScreenActive = false;
                    resetTimeScreenCache();
                    updateScreen("SENDING", YELLOW);
                    triggerUpload = true; 
                }
            }
        }
        // Evaluate SWIPE
        else if (absX > minSwipeDist || absY > minSwipeDist) {
            if (absX > absY) {
                if (deltaX > 0) {
                    Serial.println("SWIPE RIGHT");
                    if (timeScreenActive && currentState == STATE_IDLE) {
                        timeScreenActive = false;
                        resetTimeScreenCache();
                        showIdleEyes();
                    }
                } else {
                    Serial.println("SWIPE LEFT");
                    // Swipe left -> show time screen (RTC)
                    if (currentState == STATE_IDLE) {
                        timeScreenActive = true;
                        gfx->fillScreen(BLACK);
                        resetTimeScreenCache();
                        lastTimeScreenUpdate = 0;
                        updateTimeScreen();
                    }
                }
            } else {
                if (deltaY > 0) {
                    Serial.println("SWIPE DOWN");
                    if (timeScreenActive && currentState == STATE_IDLE) {
                        timeScreenActive = false;
                        resetTimeScreenCache();
                        showIdleEyes();
                    }
                } else {
                    Serial.println("SWIPE UP");
                    if (timeScreenActive && currentState == STATE_IDLE) {
                        timeScreenActive = false;
                        resetTimeScreenCache();
                        showIdleEyes();
                    }
                }
            }
        }
    }

    // ========================================================
    //      State Machine
    // ========================================================
    if(currentState == STATE_SETUP) {
        if (needsRestart) {
            updateScreen("SAVED!", GREEN);
            delay(1000);
            ESP.restart();
        }
        delay(100);
        return; 
    }

    if (currentState == STATE_RECORDING) {
        processAudio();
        captureVideoFrame(); 
    } else if (currentState == STATE_IDLE && !timeScreenActive) {
        updateIdleEyesAnimation();
    }
}