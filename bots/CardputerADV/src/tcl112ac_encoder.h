#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// TCL112AC state layout, timings, and checksum are adapted from
// IRremoteESP8266's ir_Tcl.h / ir_Tcl.cpp.
// Copyright 2019, 2021, 2022 David Conran.
// Licensed under GNU LGPL v2.1 or later; see
// third_party/IRremoteESP8266-LICENSE.txt.
// Adapted for Cardputer ADV on 2026-08-22. This encoder intentionally has no
// IRsend class or global so it can coexist with Arduino-IRremote 4.7.1.

namespace cardputer_ir {

constexpr std::size_t kTcl112AcStateLength = 14;
constexpr uint16_t kTcl112AcBits = 112;
constexpr uint16_t kTcl112AcHdrMark = 3000;
constexpr uint16_t kTcl112AcHdrSpace = 1650;
constexpr uint16_t kTcl112AcBitMark = 500;
constexpr uint16_t kTcl112AcOneSpace = 1050;
constexpr uint16_t kTcl112AcZeroSpace = 325;
constexpr uint32_t kTcl112AcGap = 100000;

constexpr uint8_t kTcl112AcHeat = 1;
constexpr uint8_t kTcl112AcDry = 2;
constexpr uint8_t kTcl112AcCool = 3;
constexpr uint8_t kTcl112AcFan = 7;
constexpr uint8_t kTcl112AcAuto = 8;

constexpr uint8_t kTcl112AcFanAuto = 0;
constexpr uint8_t kTcl112AcFanMin = 1;
constexpr uint8_t kTcl112AcFanLow = 2;
constexpr uint8_t kTcl112AcFanMed = 3;
constexpr uint8_t kTcl112AcFanHigh = 5;
constexpr uint8_t kTcl112AcSwingVOff = 0;
constexpr uint8_t kTcl112AcSwingVOn = 7;
constexpr uint8_t kTcl112AcTempMin = 16;
constexpr uint8_t kTcl112AcTempMax = 31;

using Tcl112AcState = std::array<uint8_t, kTcl112AcStateLength>;

struct Tcl112AcSettings {
  bool power = false;
  uint8_t mode = kTcl112AcCool;
  uint8_t tempC = 24;
  uint8_t fan = kTcl112AcFanAuto;
  bool swingV = false;
};

inline uint8_t clampTcl112AcTemp(uint8_t tempC) {
  if (tempC < kTcl112AcTempMin) return kTcl112AcTempMin;
  if (tempC > kTcl112AcTempMax) return kTcl112AcTempMax;
  return tempC;
}

inline uint8_t normalizeTcl112AcMode(uint8_t mode) {
  switch (mode) {
    case kTcl112AcHeat:
    case kTcl112AcDry:
    case kTcl112AcCool:
    case kTcl112AcFan:
    case kTcl112AcAuto:
      return mode;
    default:
      return kTcl112AcAuto;
  }
}

inline uint8_t normalizeTcl112AcFan(uint8_t fan) {
  switch (fan) {
    case kTcl112AcFanAuto:
    case kTcl112AcFanMin:
    case kTcl112AcFanLow:
    case kTcl112AcFanMed:
    case kTcl112AcFanHigh:
      return fan;
    default:
      return kTcl112AcFanAuto;
  }
}

inline uint8_t tcl112AcChecksum(const Tcl112AcState& state) {
  uint16_t sum = 0;
  for (std::size_t i = 0; i + 1 < state.size(); ++i) sum += state[i];
  return static_cast<uint8_t>(sum & 0xFF);
}

inline Tcl112AcState buildTcl112AcState(const Tcl112AcSettings& settings) {
  // Known-good TAC09CHSD profile: On, Cool, 24C, Auto fan, TCL model bit.
  Tcl112AcState state = {
      0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x03,
      0x07, 0x40, 0x00, 0x00, 0x00, 0x80, 0x03};
  const uint8_t mode = normalizeTcl112AcMode(settings.mode);
  const uint8_t tempC = clampTcl112AcTemp(settings.tempC);
  uint8_t fan = normalizeTcl112AcFan(settings.fan);
  if (mode == kTcl112AcFan) fan = kTcl112AcFanHigh;

  state[5] = static_cast<uint8_t>(0x20 | (settings.power ? 0x04 : 0x00));
  state[6] = mode;
  state[7] = static_cast<uint8_t>(kTcl112AcTempMax - tempC);
  state[8] = static_cast<uint8_t>(0x40 | fan |
                                  ((settings.swingV ? kTcl112AcSwingVOn : kTcl112AcSwingVOff) << 3));
  state[13] = tcl112AcChecksum(state);
  return state;
}

inline bool validTcl112AcChecksum(const Tcl112AcState& state) {
  return state.back() == tcl112AcChecksum(state);
}

}  // namespace cardputer_ir
