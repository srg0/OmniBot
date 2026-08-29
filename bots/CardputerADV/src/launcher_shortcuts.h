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

// M5Cardputer reports Ctrl+number as a shifted printable symbol (!, @, ...),
// while hid_keys still contains the physical number-row code. Normalize from
// HID before resolving the launcher shortcut.
inline char digitFromHid(uint8_t rawCode) {
  const uint8_t code = rawCode & 0x7fU;
  if (code >= 0x1eU && code <= 0x26U) {
    return static_cast<char>('1' + (code - 0x1eU));
  }
  return code == 0x27U ? '0' : '\0';
}

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
