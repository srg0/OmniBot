#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

#include "../src/daikin280_encoder.h"
#include "../src/tcl112ac_encoder.h"

using namespace cardputer_ir;

void testTcl112Ac() {
  static_assert(kTcl112AcStateLength == 14);
  static_assert(kTcl112AcBits == 112);
  Tcl112AcSettings settings;
  settings.power = true;
  auto state = buildTcl112AcState(settings);
  const Tcl112AcState knownGoodCool24 = {
      0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x03,
      0x07, 0x40, 0x00, 0x00, 0x00, 0x80, 0x03};
  assert(state == knownGoodCool24);
  assert(validTcl112AcChecksum(state));

  settings.power = false;
  state = buildTcl112AcState(settings);
  assert((state[5] & 0x04) == 0);
  assert(validTcl112AcChecksum(state));

  settings.power = true;
  settings.tempC = 0;
  state = buildTcl112AcState(settings);
  assert(state[7] == 15);  // 31C - 16C
  settings.tempC = 255;
  state = buildTcl112AcState(settings);
  assert(state[7] == 0);  // 31C - 31C

  settings.mode = kTcl112AcHeat;
  settings.fan = kTcl112AcFanMed;
  settings.swingV = true;
  state = buildTcl112AcState(settings);
  assert((state[6] & 0x0F) == kTcl112AcHeat);
  assert((state[8] & 0x07) == kTcl112AcFanMed);
  assert(((state[8] >> 3) & 0x07) == kTcl112AcSwingVOn);
  assert(validTcl112AcChecksum(state));

  settings.mode = kTcl112AcFan;
  settings.fan = kTcl112AcFanAuto;
  state = buildTcl112AcState(settings);
  assert((state[8] & 0x07) == kTcl112AcFanHigh);
}

void testDaikin280() {
  static_assert(kDaikin280StateLength == 35);
  static_assert(kDaikin280Bits == 280);
  static_assert(kDaikin280Section1Length + kDaikin280Section2Length +
                    kDaikin280Section3Length ==
                kDaikin280StateLength);
  Daikin280Settings settings;
  settings.power = true;
  auto state = buildDaikin280State(settings);
  assert((state[21] & 0x01) == 1);
  assert(((state[21] >> 4) & 0x07) == kDaikin280Cool);
  assert(state[22] == 48);
  assert((state[24] >> 4) == kDaikin280FanAuto);
  assert(validDaikin280Checksums(state));

  settings.power = false;
  state = buildDaikin280State(settings);
  assert((state[21] & 0x01) == 0);
  assert(validDaikin280Checksums(state));

  settings.power = true;
  settings.tempC = 0;
  state = buildDaikin280State(settings);
  assert(state[22] == kDaikin280TempMin * 2);
  settings.tempC = 255;
  state = buildDaikin280State(settings);
  assert(state[22] == kDaikin280TempMax * 2);

  settings.mode = kDaikin280Heat;
  settings.fan = 5;
  settings.swingV = true;
  settings.quiet = true;
  settings.powerful = false;
  state = buildDaikin280State(settings);
  assert(((state[21] >> 4) & 0x07) == kDaikin280Heat);
  assert((state[24] >> 4) == 5);
  assert((state[24] & 0x0F) == kDaikin280SwingOn);
  assert((state[29] & 0x20) != 0);
  assert((state[29] & 0x01) == 0);
  assert(validDaikin280Checksums(state));
}

int main() {
  testTcl112Ac();
  testDaikin280();
  std::cout << "IR AC protocol tests passed: TCL112AC 112-bit, Daikin 280-bit\n";
  return 0;
}
