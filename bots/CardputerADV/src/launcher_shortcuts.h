#pragma once

#include <stdint.h>

namespace launcher_shortcuts {
enum class Kind : uint8_t { None, Group, Submenu, InvalidSubmenu };

struct Action {
  Kind kind;
  uint8_t index;

  Action(Kind valueKind = Kind::None, uint8_t valueIndex = 0)
      : kind(valueKind), index(valueIndex) {}
};

inline Action resolve(char key, bool ctrl, uint8_t groupCount, uint8_t submenuCount) {
  if (ctrl && key >= '1' && key <= '6') {
    const uint8_t index = static_cast<uint8_t>(key - '1');
    return Action(index < submenuCount ? Kind::Submenu : Kind::InvalidSubmenu, index);
  }
  if (!ctrl && key >= '1' && key <= '7') {
    const uint8_t index = static_cast<uint8_t>(key - '1');
    return Action(index < groupCount ? Kind::Group : Kind::None, index);
  }
  return Action();
}
}  // namespace launcher_shortcuts
