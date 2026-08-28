#include "../src/brainflow.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

using namespace brainflow;

int main() {
  constexpr std::array<Phase, 4> kRoundStates = {
      Phase::Playing, Phase::Feedback, Phase::Paused, Phase::Finished};

  uint32_t checks = 0;
  for (uint8_t gameSlot = 0; gameSlot < kGameCount; ++gameSlot) {
    for (Phase phase : kRoundStates) {
      // Navigation is phase-based, so every 0.2.140 game slot shares this stack.
      assert(gameSlot < kGameCount);
      assert(escapeTarget(phase) == BackTarget::BrainFlowMenu);
      ++checks;

      phase = Phase::Menu;
      assert(escapeTarget(phase) == BackTarget::LauncherGames);
      ++checks;
    }
  }

  assert(escapeTarget(Phase::Menu) == BackTarget::LauncherGames);
  ++checks;
  std::cout << "brainflow_navigation_checks=" << checks
            << " game_slots=" << static_cast<unsigned>(kGameCount) << '\n';
}
