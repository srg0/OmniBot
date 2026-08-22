#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cardputer_smartlamp {

constexpr std::size_t kPresetSize = 13;
constexpr std::size_t kMaxPresets = 40;
constexpr std::size_t kMaxPacketBytes = 2304;
constexpr std::size_t kPresetSpeedIndex = 7;
constexpr std::size_t kPresetScaleIndex = 9;

struct PresetBank {
  uint8_t count = 0;
  uint8_t selected = 0;
  uint8_t values[kMaxPresets][kPresetSize] = {};
};

inline bool parseUnsignedField(const char*& cursor, unsigned long& value) {
  if (cursor == nullptr || *cursor == '\0') {
    return false;
  }
  char* end = nullptr;
  value = std::strtoul(cursor, &end, 10);
  if (end == cursor || (*end != ',' && *end != '\0')) {
    return false;
  }
  cursor = *end == ',' ? end + 1 : end;
  return true;
}

inline bool parsePresetPacket(const char* packet, PresetBank& output) {
  if (packet == nullptr || std::strncmp(packet, "GL,2,", 5) != 0) {
    return false;
  }
  const char* cursor = packet + 5;
  unsigned long raw = 0;
  if (!parseUnsignedField(cursor, raw) || raw == 0 || raw > kMaxPresets) {
    return false;
  }

  PresetBank parsed;
  parsed.count = static_cast<uint8_t>(raw);
  for (std::size_t preset = 0; preset < parsed.count; ++preset) {
    for (std::size_t field = 0; field < kPresetSize; ++field) {
      if (!parseUnsignedField(cursor, raw) || raw > 255) {
        return false;
      }
      parsed.values[preset][field] = static_cast<uint8_t>(raw);
    }
  }

  if (!parseUnsignedField(cursor, raw) || raw == 0 || raw > parsed.count) {
    return false;
  }
  parsed.selected = static_cast<uint8_t>(raw - 1);
  output = parsed;
  return true;
}

inline bool appendNumber(char* output, std::size_t capacity, std::size_t& used,
                         unsigned int value) {
  if (output == nullptr || used >= capacity) {
    return false;
  }
  int written = std::snprintf(output + used, capacity - used, ",%u", value);
  if (written <= 0 || static_cast<std::size_t>(written) >= capacity - used) {
    return false;
  }
  used += static_cast<std::size_t>(written);
  return true;
}

inline std::size_t buildPresetPacket(const PresetBank& bank, uint8_t weekday,
                                     uint8_t hour, uint8_t minute, uint8_t second,
                                     char* output, std::size_t capacity) {
  if (output == nullptr || capacity == 0 || bank.count == 0 ||
      bank.count > kMaxPresets || bank.selected >= bank.count) {
    return 0;
  }
  int prefix = std::snprintf(output, capacity, "GL,2,%u", bank.count);
  if (prefix <= 0 || static_cast<std::size_t>(prefix) >= capacity) {
    return 0;
  }
  std::size_t used = static_cast<std::size_t>(prefix);
  for (std::size_t preset = 0; preset < bank.count; ++preset) {
    for (std::size_t field = 0; field < kPresetSize; ++field) {
      if (!appendNumber(output, capacity, used, bank.values[preset][field])) {
        return 0;
      }
    }
  }
  if (!appendNumber(output, capacity, used, bank.selected + 1) ||
      !appendNumber(output, capacity, used, weekday) ||
      !appendNumber(output, capacity, used, hour) ||
      !appendNumber(output, capacity, used, minute) ||
      !appendNumber(output, capacity, used, second)) {
    return 0;
  }
  return used;
}

}  // namespace cardputer_smartlamp
