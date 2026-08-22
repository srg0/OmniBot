#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cardputer_ir {

// Historical generic Daikin protocol retained from Cardputer release ff0ff40.
constexpr std::size_t kDaikin280StateLength = 35;
constexpr std::size_t kDaikin280Section1Length = 8;
constexpr std::size_t kDaikin280Section2Length = 8;
constexpr std::size_t kDaikin280Section3Length =
    kDaikin280StateLength - kDaikin280Section1Length - kDaikin280Section2Length;
constexpr uint16_t kDaikin280Bits = 280;
constexpr uint16_t kDaikin280HdrMark = 3650;
constexpr uint16_t kDaikin280HdrSpace = 1623;
constexpr uint16_t kDaikin280BitMark = 428;
constexpr uint16_t kDaikin280ZeroSpace = 428;
constexpr uint16_t kDaikin280OneSpace = 1280;
constexpr uint16_t kDaikin280Gap = 29000;

constexpr uint8_t kDaikin280Auto = 0;
constexpr uint8_t kDaikin280Dry = 2;
constexpr uint8_t kDaikin280Cool = 3;
constexpr uint8_t kDaikin280Heat = 4;
constexpr uint8_t kDaikin280FanOnly = 6;
constexpr uint8_t kDaikin280TempMin = 18;
constexpr uint8_t kDaikin280TempMax = 30;
constexpr uint8_t kDaikin280FanAuto = 0xA;
constexpr uint8_t kDaikin280FanQuiet = 0xB;
constexpr uint8_t kDaikin280SwingOff = 0x0;
constexpr uint8_t kDaikin280SwingOn = 0xF;

using Daikin280State = std::array<uint8_t, kDaikin280StateLength>;

struct Daikin280Settings {
  bool power = false;
  uint8_t mode = kDaikin280Cool;
  uint8_t tempC = 24;
  uint8_t fan = kDaikin280FanAuto;
  bool swingV = false;
  bool quiet = false;
  bool powerful = false;
};

inline uint8_t clampDaikin280Temp(uint8_t tempC) {
  if (tempC < kDaikin280TempMin) return kDaikin280TempMin;
  if (tempC > kDaikin280TempMax) return kDaikin280TempMax;
  return tempC;
}

inline uint8_t daikin280SumBytes(const uint8_t* data, std::size_t length) {
  uint16_t sum = 0;
  for (std::size_t i = 0; i < length; ++i) sum += data[i];
  return static_cast<uint8_t>(sum & 0xFF);
}

inline Daikin280State buildDaikin280State(const Daikin280Settings& settings) {
  Daikin280State state{};
  state[0] = 0x11;
  state[1] = 0xDA;
  state[2] = 0x27;
  state[4] = 0xC5;
  state[8] = 0x11;
  state[9] = 0xDA;
  state[10] = 0x27;
  state[12] = 0x42;
  state[16] = 0x11;
  state[17] = 0xDA;
  state[18] = 0x27;
  state[21] = static_cast<uint8_t>(0x08 | (settings.power ? 0x01 : 0x00) |
                                   ((settings.mode & 0x07) << 4));
  state[22] = static_cast<uint8_t>(clampDaikin280Temp(settings.tempC) * 2);
  state[24] = static_cast<uint8_t>(((settings.fan & 0x0F) << 4) |
                                   (settings.swingV ? kDaikin280SwingOn : kDaikin280SwingOff));
  state[27] = 0x06;
  state[28] = 0x60;
  state[29] = static_cast<uint8_t>((settings.powerful ? 0x01 : 0x00) |
                                   (settings.quiet ? 0x20 : 0x00));
  state[31] = 0xC0;
  state[7] = daikin280SumBytes(state.data(), kDaikin280Section1Length - 1);
  state[15] = daikin280SumBytes(state.data() + kDaikin280Section1Length,
                                kDaikin280Section2Length - 1);
  state[34] = daikin280SumBytes(
      state.data() + kDaikin280Section1Length + kDaikin280Section2Length,
      kDaikin280Section3Length - 1);
  return state;
}

inline bool validDaikin280Checksums(const Daikin280State& state) {
  return state[7] == daikin280SumBytes(state.data(), kDaikin280Section1Length - 1) &&
         state[15] == daikin280SumBytes(state.data() + kDaikin280Section1Length,
                                       kDaikin280Section2Length - 1) &&
         state[34] == daikin280SumBytes(
             state.data() + kDaikin280Section1Length + kDaikin280Section2Length,
             kDaikin280Section3Length - 1);
}

}  // namespace cardputer_ir
