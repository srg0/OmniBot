#include <cassert>
#include <cstring>
#include <iostream>

#include "../src/smartlamp_gl2_protocol.h"

using namespace cardputer_smartlamp;

int main() {
  const char* packet =
      "GL,2,2,1,0,90,1,1,0,0,120,2,64,0,10,0,"
      "4,1,140,2,1,5,240,200,3,100,1,30,1,2,6,19,27,45";
  PresetBank bank;
  assert(parsePresetPacket(packet, bank));
  assert(bank.count == 2);
  assert(bank.selected == 1);
  assert(bank.values[0][kPresetSpeedIndex] == 120);
  assert(bank.values[0][kPresetScaleIndex] == 64);
  assert(bank.values[1][kPresetSpeedIndex] == 200);
  assert(bank.values[1][kPresetScaleIndex] == 100);

  bank.values[1][kPresetSpeedIndex] = 205;
  bank.values[1][kPresetScaleIndex] = 98;
  char output[kMaxPacketBytes] = {};
  std::size_t size = buildPresetPacket(bank, 6, 20, 15, 8, output, sizeof(output));
  assert(size == std::strlen(output));
  assert(size > 0);

  PresetBank roundTrip;
  assert(parsePresetPacket(output, roundTrip));
  assert(roundTrip.count == bank.count);
  assert(roundTrip.selected == bank.selected);
  assert(roundTrip.values[1][kPresetSpeedIndex] == 205);
  assert(roundTrip.values[1][kPresetScaleIndex] == 98);

  assert(!parsePresetPacket("CURR 1 2 3 4 1", roundTrip));
  assert(!parsePresetPacket("GL,2,0", roundTrip));
  std::cout << "GyverLamp2 protocol tests passed\n";
  return 0;
}
