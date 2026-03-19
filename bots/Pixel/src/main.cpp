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

bool needsRestart = false;
volatile bool triggerUpload = false;

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
    
    WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
    
    int timeoutCounter = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        timeoutCounter++;
        
        if (timeoutCounter > 30) { 
            preferences.begin("wifi_creds", false);
            preferences.remove("ssid");
            preferences.remove("password");
            preferences.end();
            startBLEProvisioning();
            return;
        }
    }
    
    WiFi.setSleep(false);
    currentState = STATE_IDLE; 
    updateScreen("READY", BLUE);
    
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
            updateScreen("READY", BLUE);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

// ==========================================
//                ARDUINO SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    
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
                    lastFrameTime = 0;
                    recordedAudioLen = 0;
                    video_frame_count = 0;
                    updateScreen("RECORDING", RED); 
                } 
                else if (currentState == STATE_RECORDING) {
                    Serial.println("\n*** TAP DETECTED: STOPPING RECORD ***\n");
                    currentState = STATE_UPLOADING; 
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
                } else {
                    Serial.println("SWIPE LEFT");
                }
            } else {
                if (deltaY > 0) {
                    Serial.println("SWIPE DOWN");
                } else {
                    Serial.println("SWIPE UP");
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
    } 
}