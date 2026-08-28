#include "../src/brainflow.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

using namespace brainflow;
static unsigned long checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { \
  std::cerr << "FAILED line " << __LINE__ << ": " #x << "\n"; return 1; \
} } while (0)

static bool distinctOptions(const Session& session) {
  return strcmp(session.options[0], session.options[1]) != 0 &&
         strcmp(session.options[0], session.options[2]) != 0 &&
         strcmp(session.options[1], session.options[2]) != 0;
}

int main() {
  Session session;
  CHECK(kGameCount == 8);
  CHECK(sizeof(session) < 256);

  for (unsigned game = 0; game < kGameCount; ++game) {
    std::set<std::string> deck;
    for (unsigned round = 0; round < 32; ++round) {
      session.start(static_cast<Game>(game), 100, 37);
      session.attempts = round;
      session.correct = round;
      session.question();
      CHECK(strlen(session.prompt) < sizeof(session.prompt));
      CHECK(strlen(session.clue) < sizeof(session.clue));
      CHECK(strlen(session.answer) < sizeof(session.answer));
      CHECK(session.prompt[0] != 0);
      CHECK(session.answer[0] != 0);

      if (game == static_cast<unsigned>(Game::Arithmetic)) {
        unsigned a, b, value;
        char op;
        CHECK(sscanf(session.prompt, "%u %c %u", &a, &op, &b) == 3);
        CHECK(sscanf(session.answer, "%u", &value) == 1);
        CHECK(op == '+' ? value == a + b :
              op == '-' ? value == a - b :
              op == 'x' ? value == a * b : b && a % b == 0 && value == a / b);
        for (const char* cursor = session.answer; *cursor; ++cursor) session.type(*cursor, 101);
        CHECK(session.submit(102));
        CHECK(session.lastCorrect);
      } else if (game == static_cast<unsigned>(Game::Fraction)) {
        CHECK(session.n1 > 0 && session.n1 < session.d1);
        CHECK(session.n2 > 0 && session.n2 < session.d2);
        const unsigned left = session.n1 * session.d2;
        const unsigned right = session.n2 * session.d1;
        CHECK(session.choice == (left < right ? 0 : left == right ? 1 : 2));
        CHECK(session.submit(102, session.choice));
        CHECK(session.lastCorrect);
      } else if (game == static_cast<unsigned>(Game::WordScramble)) {
        std::string answer = session.answer;
        std::string prompt = session.prompt;
        CHECK(answer != prompt);
        std::sort(answer.begin(), answer.end());
        std::sort(prompt.begin(), prompt.end());
        CHECK(answer == prompt);
        deck.insert(session.answer);
        for (const char* cursor = session.answer; *cursor; ++cursor) {
          session.type(*cursor + ('a' - 'A'), 101);
        }
        CHECK(session.submit(102));
        CHECK(session.lastCorrect);
      } else {
        CHECK(isChoiceGame(session.game));
        CHECK(session.choice < 3);
        CHECK(distinctOptions(session));
        CHECK(strcmp(session.answer, session.options[session.choice]) == 0);

        if (game == static_cast<unsigned>(Game::Vocabulary) ||
            game == static_cast<unsigned>(Game::Synonyms)) {
          deck.insert(session.prompt);
        } else if (game == static_cast<unsigned>(Game::Estimate)) {
          unsigned a, b, answer;
          CHECK(sscanf(session.prompt, "%u + %u", &a, &b) == 2);
          CHECK(sscanf(session.answer, "%u", &answer) == 1);
          CHECK(session.d2 == a + b);
          CHECK(answer == ((a + b + 5) / 10) * 10);
          CHECK(session.d1 == answer);
        } else if (game == static_cast<unsigned>(Game::Sequence)) {
          unsigned v0, v1, v2, v3, answer;
          CHECK(sscanf(session.prompt, "%u %u %u %u", &v0, &v1, &v2, &v3) == 4);
          CHECK(sscanf(session.answer, "%u", &answer) == 1);
          CHECK(v0 == session.n1);
          CHECK(v1 - v0 == session.d1);
          CHECK(v2 - v1 == session.n2);
          CHECK(v3 - v2 == session.d1);
          CHECK(answer == v3 + session.n2);
          CHECK(session.d2 == answer);
        } else {
          deck.insert(session.answer);
          CHECK(strcmp(session.prompt, "WHICH ONE IS ODD?") == 0);
          CHECK(strncmp(session.clue, "TWO ARE ", 8) == 0);
        }

        CHECK(session.submit(102, session.choice));
        CHECK(session.lastCorrect);
      }

      const auto score = session.score;
      CHECK(!session.submit(103, session.choice));
      CHECK(session.score == score);
    }
    if (game == static_cast<unsigned>(Game::WordScramble) ||
        game == static_cast<unsigned>(Game::Vocabulary) ||
        game == static_cast<unsigned>(Game::Synonyms)) {
      CHECK(deck.size() == 32);
    }
    if (game == static_cast<unsigned>(Game::OddOneOut)) CHECK(deck.size() == kOddDeckSize);

    session.start(static_cast<Game>(game), 0, 19);
    session.tick(kRoundMs);
    CHECK(session.phase == Phase::Finished);
    CHECK(session.remaining == 0);
  }

  // Empty input, boundaries, deadline equality, unsigned rollover, pause and resume.
  session.start(Game::Arithmetic, 0xfffffff0U, 0);
  CHECK(!session.submit(0xfffffff0U));
  for (unsigned i = 0; i < 100; ++i) session.type('9', 0xfffffff0U);
  CHECK(session.length == kInputMax);
  CHECK(session.input[kInputMax] == 0);
  session.tick(0x20U);
  CHECK(session.remaining == kRoundMs - 48);
  session.pause(0x30U);
  auto remaining = session.remaining;
  session.tick(0xf0000030U);
  CHECK(session.remaining == remaining);
  session.resume(0xf0000030U);
  session.tick(0xf0000031U);
  CHECK(session.remaining == remaining - 1);
  session.tick(0xf0000031U + session.remaining);
  CHECK(session.phase == Phase::Finished);
  CHECK(!session.submit(0xf000ffffU));

  session.start(Game::WordScramble, 0, 88);
  session.type('?', 0);
  CHECK(session.length == 0);
  for (const char* cursor = session.answer; *cursor; ++cursor) session.type(*cursor, 0);
  CHECK(session.submit(0));
  session.pause(100);
  CHECK(session.resumePhase == Phase::Feedback);
  remaining = session.feedbackRemaining;
  session.resume(9000);
  session.tick(9000 + remaining - 1);
  CHECK(session.phase == Phase::Feedback);
  session.tick(9000 + remaining);
  CHECK(session.phase == Phase::Playing);

  session.start(Game::Arithmetic, 0, 99);
  strcpy(session.answer, "0");
  session.type('0', 0);
  session.type('0', 0);
  CHECK(session.submit(0) && session.lastCorrect);
  session.start(Game::Arithmetic, 0, 99);
  session.type('9', 60000);
  CHECK(session.phase == Phase::Finished && session.length == 0);

  // L only toggles presentation. It cannot consume time, alter score, phase or input.
  session.start(Game::Estimate, 500, 77);
  const Session beforeHelp = session;
  bool helpVisible = true;
  helpVisible = helpVisibleAfterKey(helpVisible, 'l', false);
  CHECK(!helpVisible);
  CHECK(memcmp(&beforeHelp, &session, sizeof(Session)) == 0);
  helpVisible = helpVisibleAfterKey(helpVisible, 'L', false);
  CHECK(helpVisible);
  CHECK(helpVisibleAfterKey(helpVisible, 'l', true) == helpVisible);
  session.tick(510);
  CHECK(session.remaining == kRoundMs - 10);

  std::cout << "brainflow_checks=" << checks << " state_bytes=" << sizeof(Session)
            << " games=" << static_cast<unsigned>(kGameCount) << "\n";
}
