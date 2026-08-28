#pragma once

#include <stdint.h>

namespace brainflow_audio {
enum class Cue : uint8_t { Start, Select, Correct, Wrong, Finish };

struct Spec {
  uint16_t frequency;
  uint16_t durationMs;
};

constexpr Spec spec(Cue cue) {
  switch (cue) {
    case Cue::Start: return {1047, 75};
    case Cue::Select: return {740, 35};
    case Cue::Correct: return {1320, 70};
    case Cue::Wrong: return {330, 85};
    case Cue::Finish: return {784, 110};
  }
  return {880, 55};
}

constexpr bool allowed(bool recording, bool playback, bool realtime,
                       bool realtimeRecording, bool submitting, bool thinking) {
  return !recording && !playback && !realtime && !realtimeRecording &&
         !submitting && !thinking;
}
}  // namespace brainflow_audio
