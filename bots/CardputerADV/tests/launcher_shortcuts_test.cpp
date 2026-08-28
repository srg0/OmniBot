#include "../src/launcher_shortcuts.h"

#include <cassert>
#include <iostream>

using launcher_shortcuts::Kind;

int main() {
  unsigned checks = 0;
  for (char key = '1'; key <= '7'; ++key) {
    const auto action = launcher_shortcuts::resolve(key, false, 7, 6);
    assert(action.kind == Kind::Group);
    assert(action.index == static_cast<unsigned>(key - '1'));
    checks += 2;
  }
  for (char key = '1'; key <= '6'; ++key) {
    const auto action = launcher_shortcuts::resolve(key, true, 7, 6);
    assert(action.kind == Kind::Submenu);
    assert(action.index == static_cast<unsigned>(key - '1'));
    checks += 2;
  }

  // Ctrl has priority: Ctrl+3 opens submenu 3 and must never select group 3.
  assert(launcher_shortcuts::resolve('3', true, 7, 6).kind == Kind::Submenu);
  assert(launcher_shortcuts::resolve('6', true, 7, 3).kind == Kind::InvalidSubmenu);
  assert(launcher_shortcuts::resolve('7', true, 7, 6).kind == Kind::None);
  assert(launcher_shortcuts::resolve('8', false, 7, 6).kind == Kind::None);
  checks += 4;

  std::cout << "launcher_shortcut_checks=" << checks << '\n';
}
