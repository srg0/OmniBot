#pragma once

#include <stdint.h>
#include <string.h>

namespace release_stability {

inline bool parseVersionCore(const char* version, uint32_t (&parts)[3]) {
  if (!version || version[0] == '\0') {
    return false;
  }
  const char* cursor = version;
  for (uint8_t index = 0; index < 3; ++index) {
    if (*cursor < '0' || *cursor > '9') {
      return false;
    }
    uint32_t value = 0;
    while (*cursor >= '0' && *cursor <= '9') {
      const uint32_t digit = static_cast<uint32_t>(*cursor - '0');
      if (value > 1000000U) {
        return false;
      }
      value = value * 10U + digit;
      ++cursor;
    }
    parts[index] = value;
    if (index < 2) {
      if (*cursor != '.') {
        return false;
      }
      ++cursor;
    }
  }
  return *cursor == '\0' || *cursor == '-';
}

// Unsigned subtraction deliberately keeps this correct across millis() wrap.
inline bool shouldStopOrdinaryRecording(bool recording, uint32_t nowMs,
                                        uint32_t startedMs, uint32_t limitMs) {
  return recording && limitMs > 0U && static_cast<uint32_t>(nowMs - startedMs) >= limitMs;
}

inline bool otaUpdateAvailable(bool parsed, const char* currentVersion,
                               const char* candidateVersion) {
  if (!parsed) {
    return false;
  }
  uint32_t current[3] = {};
  uint32_t candidate[3] = {};
  if (!parseVersionCore(currentVersion, current) || !parseVersionCore(candidateVersion, candidate)) {
    return false;
  }
  for (uint8_t index = 0; index < 3; ++index) {
    if (candidate[index] != current[index]) {
      return candidate[index] > current[index];
    }
  }
  return false;
}

}  // namespace release_stability
