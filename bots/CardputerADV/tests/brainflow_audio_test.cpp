#include "../src/brainflow_audio.h"

#include <array>
#include <cassert>
#include <iostream>
#include <set>

using brainflow_audio::Cue;

int main() {
  constexpr std::array<Cue, 5> cues = {
      Cue::Start, Cue::Select, Cue::Correct, Cue::Wrong, Cue::Finish};
  std::set<unsigned> frequencies;
  unsigned checks = 0;
  for (Cue cue : cues) {
    const auto value = brainflow_audio::spec(cue);
    assert(value.frequency >= 250 && value.frequency <= 1500);
    assert(value.durationMs >= 25 && value.durationMs <= 120);
    frequencies.insert(value.frequency);
    checks += 2;
  }
  assert(frequencies.size() == cues.size());
  ++checks;

  assert(brainflow_audio::allowed(false, false, false, false, false, false));
  assert(!brainflow_audio::allowed(true, false, false, false, false, false));
  assert(!brainflow_audio::allowed(false, true, false, false, false, false));
  assert(!brainflow_audio::allowed(false, false, true, false, false, false));
  assert(!brainflow_audio::allowed(false, false, false, true, false, false));
  assert(!brainflow_audio::allowed(false, false, false, false, true, false));
  assert(!brainflow_audio::allowed(false, false, false, false, false, true));
  checks += 7;
  std::cout << "brainflow_audio_checks=" << checks << '\n';
}
