#pragma once

#include <stdint.h>

namespace launcher_shortcuts {
enum class Kind : uint8_t { None, Group, Submenu, InvalidSubmenu };

struct Action {
  Kind kind = Kind::None;
  uint8_t index = 0;
};

constexpr Action resolve(char key, bool ctrl, uint8_t groupCount, uint8_t submenuCount) {
  if (ctrl && key >= '1' && key <= '6') {
    const uint8_t index = static_cast<uint8_t>(key - '1');
    return {index < submenuCount ? Kind::Submenu : Kind::InvalidSubmenu, index};
  }
  if (!ctrl && key >= '1' && key <= '7') {
    const uint8_t index = static_cast<uint8_t>(key - '1');
    return {index < groupCount ? Kind::Group : Kind::None, index};
  }
  return {};
}
}  // namespace launcher_shortcuts
