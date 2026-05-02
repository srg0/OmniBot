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
#include "main_parts/005_service_forwards_and_identity.cpp.inc"
#include "main_parts/006_ota_guard_and_partition_utils.cpp.inc"
#include "main_parts/007_utf8_text_and_transfer_start.cpp.inc"
#include "main_parts/008_transfer_face_status_wrap.cpp.inc"
#include "main_parts/009_chat_lines_and_emoji_decode.cpp.inc"
#include "main_parts/010_visual_lines_fs_notes_start.cpp.inc"
#include "main_parts/011_notes_and_emoji_cache.cpp.inc"
#include "main_parts/012_emoji_assets_http_hash.cpp.inc"
#include "main_parts/013_ranged_download_and_voice_meta.cpp.inc"
#include "main_parts/014_audio_assets_and_manifests.cpp.inc"
#include "main_parts/015_remote_assets_logs_ota_guard.cpp.inc"
#include "main_parts/016_ota_partition_writer.cpp.inc"
#include "main_parts/017_ota_apply_and_topic_names.cpp.inc"
#include "main_parts/018_context_catalog.cpp.inc"
#include "main_parts/019_topics_and_wav_persist_start.cpp.inc"
#include "main_parts/020_wav_persistence_and_voice_index.cpp.inc"
#include "main_parts/021_playback_and_action_modes.cpp.inc"
#include "main_parts/022_action_pet_helpers.cpp.inc"
#include "main_parts/023_device_actions_and_launcher_nav.cpp.inc"
#include "main_parts/024_launcher_and_keyboard_input.cpp.inc"
#include "main_parts/025_keyboard_signature_and_config.cpp.inc"
#include "main_parts/026_pcm_buffers_and_focus_load.cpp.inc"
#include "main_parts/027_focus_runtime.cpp.inc"
#include "main_parts/028_pet_state_load.cpp.inc"
#include "main_parts/029_pet_actions_and_wifi.cpp.inc"
#include "main_parts/030_pet_service_and_storage.cpp.inc"
#include "main_parts/031_storage_and_recording_start.cpp.inc"
#include "main_parts/032_recording_http_writes.cpp.inc"
#include "main_parts/033_hub_client_and_raw_voice.cpp.inc"
#include "main_parts/034_tts_request.cpp.inc"
#include "main_parts/035_text_turn_send_task.cpp.inc"
#include "main_parts/036_text_turn_queue.cpp.inc"
#include "main_parts/037_recording_submit_and_ble_start.cpp.inc"
#include "main_parts/038_ble_wifi_and_ws_event.cpp.inc"
#include "main_parts/039_hub_ws_and_realtime_io.cpp.inc"
#include "main_parts/040_realtime_json_and_commit.cpp.inc"
#include "main_parts/041_realtime_toggle_and_commands.cpp.inc"
#include "main_parts/042_launcher_voice_focus_keys.cpp.inc"
#include "main_parts/043_notes_and_topic_task_start.cpp.inc"
#include "main_parts/044_topic_task_settings_escape.cpp.inc"
#include "main_parts/045_typing_dispatcher.cpp.inc"
#include "main_parts/046_voice_trigger_and_eye_start.cpp.inc"
#include "main_parts/047_eye_pair_and_pet_color.cpp.inc"
#include "main_parts/048_pet_drawing_and_debug.cpp.inc"
#include "main_parts/049_launcher_overlay_battery_header.cpp.inc"
#include "main_parts/050_storage_widgets_and_focus_ui.cpp.inc"
#include "main_parts/051_library_audio_notes_header.cpp.inc"
#include "main_parts/052_notes_assets_widgets.cpp.inc"
#include "main_parts/053_ota_contexts_ui.cpp.inc"
#include "main_parts/054_settings_logs_pet_ui.cpp.inc"
#include "main_parts/055_pulse_arcs_chat_clock.cpp.inc"
#include "main_parts/056_voice_chat_ble_ui.cpp.inc"
#include "main_parts/057_render_and_boot_splash.cpp.inc"

}  // namespace

#include "main_parts/058_entrypoints.cpp.inc"
