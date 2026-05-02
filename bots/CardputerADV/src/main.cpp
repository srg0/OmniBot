#include <Arduino.h>
#include <Adafruit_TCA8418.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <base64.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>

extern "C" {
#include "libb64/cdecode.h"
}

#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <deque>
#include <cmath>
#include <time.h>
#include <vector>
#include <memory>


namespace {

#include "main_parts/001_config_and_hero_idle.cpp.inc"
#include "main_parts/002_hero_sprites_and_actions.cpp.inc"
#include "main_parts/003_data_models.cpp.inc"
#include "main_parts/004_runtime_state_and_forwards.cpp.inc"
#include "main_parts/005_core_forwards_and_logging.cpp.inc"
#include "main_parts/006_ota_guard_and_time.cpp.inc"
#include "main_parts/007_text_utils_and_transfer_start.cpp.inc"
#include "main_parts/008_transfer_status_and_face.cpp.inc"
#include "main_parts/009_chat_topic_emoji.cpp.inc"
#include "main_parts/010_topic_title_and_storage.cpp.inc"
#include "main_parts/011_notes_and_emoji_cache.cpp.inc"
#include "main_parts/012_emoji_download_and_voice_paths.cpp.inc"
#include "main_parts/013_voice_notes_and_audio_folders.cpp.inc"
#include "main_parts/014_audio_assets_and_manifests.cpp.inc"
#include "main_parts/015_runtime_logs_and_ota_write.cpp.inc"
#include "main_parts/016_ota_apply_and_topic_names.cpp.inc"
#include "main_parts/017_context_catalog.cpp.inc"
#include "main_parts/018_topic_history_and_wav_start.cpp.inc"
#include "main_parts/019_wav_persistence.cpp.inc"
#include "main_parts/020_wav_playback_and_actions_start.cpp.inc"
#include "main_parts/021_device_action_helpers.cpp.inc"
#include "main_parts/022_device_actions_and_launcher_sync.cpp.inc"
#include "main_parts/023_ui_mode_launcher_input.cpp.inc"
#include "main_parts/024_keyboard_tca_and_config.cpp.inc"
#include "main_parts/025_preferences_buffers_speaker.cpp.inc"
#include "main_parts/026_focus_timing.cpp.inc"
#include "main_parts/027_pet_names_and_state.cpp.inc"
#include "main_parts/028_pet_lifecycle.cpp.inc"
#include "main_parts/029_pet_wifi_and_rest.cpp.inc"
#include "main_parts/030_pet_service_and_storage_migration.cpp.inc"
#include "main_parts/031_storage_and_recording_start.cpp.inc"
#include "main_parts/032_recording_http_parser.cpp.inc"
#include "main_parts/033_hub_client_and_voice_turn.cpp.inc"
#include "main_parts/034_tts_request.cpp.inc"
#include "main_parts/035_text_turn_sync_async.cpp.inc"
#include "main_parts/036_text_turn_queue.cpp.inc"
#include "main_parts/037_recording_submit_and_ble.cpp.inc"
#include "main_parts/038_ble_wifi_and_hub_json.cpp.inc"
#include "main_parts/039_hub_ws_and_realtime_start.cpp.inc"
#include "main_parts/040_realtime_json_session.cpp.inc"
#include "main_parts/041_realtime_voice_and_commands.cpp.inc"
#include "main_parts/042_launcher_and_voice_keys.cpp.inc"
#include "main_parts/043_notes_assets_topics_keys.cpp.inc"
#include "main_parts/044_topic_task_and_settings.cpp.inc"
#include "main_parts/045_escape_and_typing_start.cpp.inc"
#include "main_parts/046_typing_voice_blink_eye.cpp.inc"
#include "main_parts/047_eye_glyphs.cpp.inc"
#include "main_parts/048_pet_render_and_launcher_preview.cpp.inc"
#include "main_parts/049_debug_and_launcher_overlay.cpp.inc"
#include "main_parts/050_battery_and_widgets.cpp.inc"
#include "main_parts/051_focus_library_audio_ui.cpp.inc"
#include "main_parts/052_audio_folders_and_notes_ui.cpp.inc"
#include "main_parts/053_assets_ota_and_widgets.cpp.inc"
#include "main_parts/054_settings_and_contexts_ui.cpp.inc"
#include "main_parts/055_topic_overlay_and_pulse_button.cpp.inc"
#include "main_parts/056_pulse_chat_and_clock.cpp.inc"
#include "main_parts/057_voice_chat_ble_ui.cpp.inc"
#include "main_parts/058_render_and_boot_splash.cpp.inc"

}  // namespace

#include "main_parts/059_entrypoints.cpp.inc"
