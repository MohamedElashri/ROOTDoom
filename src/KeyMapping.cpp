// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/KeyMapping.h"
#include <KeySymbols.h>
#include "doomkeys.h"

namespace rootdoom {
unsigned char DoomKey(std::uint32_t symbol)
{
    switch (symbol) {
    case kKey_Left: return KEY_LEFTARROW;
    case kKey_Right: return KEY_RIGHTARROW;
    case kKey_Up: return KEY_UPARROW;
    case kKey_Down: return KEY_DOWNARROW;
    case kKey_Return: case kKey_Enter: return KEY_ENTER;
    case kKey_Escape: return KEY_ESCAPE;
    case kKey_Space: return KEY_USE;
    case kKey_Control: return KEY_FIRE;
    case kKey_Shift: return KEY_RSHIFT;
    case kKey_Alt: return KEY_RALT;
    case kKey_Tab: case kKey_Backtab: return KEY_TAB;
    case kKey_Backspace: return KEY_BACKSPACE;
    case kKey_Pause: return KEY_PAUSE;
    case kKey_F1: return KEY_F1;
    case kKey_F2: return KEY_F2;
    case kKey_F3: return KEY_F3;
    case kKey_F4: return KEY_F4;
    case kKey_F5: return KEY_F5;
    case kKey_F6: return KEY_F6;
    case kKey_F7: return KEY_F7;
    case kKey_F8: return KEY_F8;
    case kKey_F9: return KEY_F9;
    case kKey_F10: return KEY_F10;
    case kKey_F11: return KEY_F11;
    case kKey_F12: return KEY_F12;
    case kKey_Plus: return KEY_EQUALS;
    default:
        if (symbol >= 'A' && symbol <= 'Z') symbol += 'a' - 'A';
        return symbol >= 33 && symbol <= 126 ? static_cast<unsigned char>(symbol) : 0;
    }
}
} // namespace rootdoom
