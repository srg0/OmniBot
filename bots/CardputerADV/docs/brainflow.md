# BrainFlow — 0.2.140-dev

Four original, offline keyboard trainers. Open **Tab → Games → BrainFlow**.

| Key | Trainer | Interaction |
| --- | --- | --- |
| 1 | Number Flow | Type an integer, Enter. Addition, subtraction, multiplication, exact division; difficulty increases. |
| 2 | Fraction Pulse | Compare two proper fractions: A smaller, S equal, D larger. Bars show proportions. |
| 3 | Word Forge | Rebuild a shuffled English word using its category clue, then Enter. 32-word deck. |
| 4 | Meaning Match | Choose the synonym using A/S/D. 32 English vocabulary prompts. |

Rounds last 60 seconds. Space pauses; Enter resumes. Tab/launcher, voice actions,
and leaving the screen also pause the round. Escape returns to the game menu;
Escape again opens the launcher on Games with BrainFlow selected. Delete edits
typed answers. Each submission is graded
once; a 900 ms feedback screen reveals the answer. High scores are stored once
per completed record-breaking round, in a new `brainflow` NVS namespace. Existing
settings, saved games, Wi-Fi, tokens, storage and partition layout are unchanged.
All four games are usable offline; no LLM request, account, assets or downloads
are required during play. Word decks are English, not a complete language course.

## Research and design boundaries

Public Elevate documentation informed the broad training mechanics:

- [Division](https://support.elevateapp.com/hc/en-us/articles/4403005980059-How-to-play-Division): practice mental division.
- [Equivalence](https://support.elevateapp.com/hc/en-us/articles/4403005985179-How-to-play-Equivalence): compare and match proportions.
- [Spelling](https://support.elevateapp.com/hc/en-us/articles/4402979771547-How-to-play-Spelling): short word-completion challenges.
- [Synonyms](https://support.elevateapp.com/hc/en-us/articles/4402972860059-How-to-play-Synonyms): vocabulary under time pressure.

These are adaptations, not ports or replicas. No Elevate assets, branding, paid
content, score formulas or proprietary word decks were copied. The UI uses the
existing Cardputer dark/cyan/violet/amber/teal palette, a large central challenge,
bounded feedback, a countdown strip and lightweight time-based animation.
`brainflow-preview.html` is a design mockup, not a hardware emulator.

## Reproduce

From the repository root:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -fstack-protector-all bots/CardputerADV/tests/brainflow_test.cpp -o /tmp/brainflow-unit
/tmp/brainflow-unit
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -fstack-protector-all bots/CardputerADV/tests/brainflow_navigation_test.cpp -o /tmp/brainflow-navigation
/tmp/brainflow-navigation
node bots/CardputerADV/tests/run_brainflow_gate.mjs
```

Build with the existing Cardputer provisioning wrapper, one build at a time.
Do not build with empty device identity or token. The release is an OTA **app**
image for ESP32-S3, 8 MB flash, existing 3 MB slots; it is not a bootloader or a
full-flash image. Publishing a candidate does not establish that a device has
installed or booted it.

## Release model and limitations

Scope: **canary_entry**, meaningful temporal/state risk, isolated to BrainFlow.
Actor: one device loop owns game state; keyboard and elapsed-time inputs are
serialized. No network, lease, queue or concurrency contract was changed.
States: Menu → Playing → Feedback → Playing; Playing/Feedback → Paused → previous
phase; elapsed deadline → Finished; Escape from any round state → Menu; Escape
from Menu → launcher/Games. Finished has no scoring
transition until an explicit new round. Power loss abandons the in-memory round;
only completed high scores persist, without overwriting any existing namespace.

Invariants: bounded/NUL-terminated input, no late or duplicate score, no input
outside the allowed alphabet, no invalid choice, frozen paused time, unsigned
clock-wrap safety, score/counter saturation, and feedback/deadline equality.

The executable gate enumerates the full product of 5 phases × 4 games × 13 input
lengths × 8 actions × 6 elapsed boundary representatives × 3 score classes × 3
feedback boundaries × 2 visibility states × 3 clock-origin representatives.
The raw clock-origin domain contains all 2^32 values. Unsigned time subtraction
is translation-invariant, so three origins (zero, just before wrap, signed
boundary) represent that domain. This is the only raw-equivalent expansion:
**964,821,453,373,440 represented combinations, not executions**. Elapsed and
score classes are the explicitly bounded test domain, not a claim that all
possible question content or arbitrary hardware failures have been enumerated.

- Symmetry: absolute clock origin is interchangeable under modular translation.
- Equivalence: declared elapsed/score/feedback classes exercise their guards.
- Partial order: one owner, one event at a time; there are no independent
  concurrent mutations to reorder. Keyboard-versus-tick order is exercised by
  method calls and deterministic traces.
- Full reduced Cartesian enumeration covers every pair and triple of declared
  dimensions. The harness records the complete induced abstract graph.
- Four retained seeds exercise one million additional temporal/input transitions.
- Each of 12 named production fences is removed in a separately compiled mutant;
  each must produce a real assertion/counterexample, not a compilation failure.

Host tests cannot prove keyboard ergonomics, speaker behavior, physical display
quality, NVS power-loss behavior, or the board's boot/OTA outcome. AddressSanitizer
could not initialize on the test Mac; UBSan and stack protection were used, with
explicit buffer-boundary fixtures. This is not an ASan pass.

## Canary and rollback

Owner: personal device operator. Entry requires clean model/unit/regression
checks, successful build, image below 3,145,728 bytes, and an authenticated
manifest/download SHA match. Candidate publishing keeps installation confirmation
enabled; no forced install or downgrade and no gateway restart.

Synthetic device canary: after a user-confirmed install, open each game, answer
one correct and one wrong question, pause/resume, open the launcher and return,
finish one round, then verify a normal voice turn. Pass: responsive keyboard,
legible unclipped screens, correct scoring, preserved pause time, healthy boot.

Natural canary: play one full round of each trainer during normal device use,
with a voice interruption; observe serial/bridge boot diagnostics and ensure no
reset, watchdog, lost settings or background network request caused by a game.

Stop/rollback triggers: boot failure, watchdog, input leaking to chat, corrupted
settings, incorrect math or broken pause. Restore the saved previous manifest
atomically and retain both binaries; the old candidate remains available. Do not
claim a device rollback without explicit device installation/boot evidence.
Broad-release readiness requires those physical canaries; host tests alone are
only a gate for publishing the confirmation-required candidate.
