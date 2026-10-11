// Keyboard and mouse bound to the game's own actions (fire, reload, crouch, ...), not to pad buttons.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace bindings {

constexpr int kSlots = 2; // keys per action
// A key: a Windows virtual-key code (mouse buttons are VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1/2), or one
// of these.
enum : uint16_t { KEY_NONE = 0, KEY_WHEEL_UP = 0x100, KEY_WHEEL_DOWN = 0x101 };

struct Action {
    const char *ini;     // black.ini: key_<ini> = Key[,Key]
    const wchar_t *name; // shown in the dialog
    int game;            // the action's index in the game's binding table
    uint16_t defaults[kSlots];
};
const Action *actions(int *count);
uint16_t key(int action, int slot);
void set_key(int action, int slot, uint16_t k);
void reset_defaults();
bool fixed(int action, int slot); // Esc is always the first pause key (the dialog cancels with it)
bool reserved(uint16_t k);        // F2-F12: the port's own keys
std::wstring key_name(uint16_t k);

bool load_ini(const char *key, const char *value); // black.ini lines key_<ini>; false if not one of ours
void save_ini(FILE *f);

// Once per pad poll (input/xinput.cpp): the bound keys and mouse buttons pressed into the Xbox pad, through the
// game's current binding table. focus: the game window is in front; mouse_live: the mouse is captured.
void apply(uint16_t &buttons, uint8_t analog[8], int16_t &lx, int16_t &ly, int16_t &rx, int16_t &ry, bool focus,
           bool mouse_live, int wheel);
bool in_play(); // the player is being updated (not a menu, pause or cutscene)
// Tests: --press "SECONDSs:k:Name" holds a key (black.ini spelling) for the next poll, without focus.
void script_hold(const std::string &name);

void dialog(void *owner_hwnd); // Input > Key bindings... (d3d/bindings_dialog.cpp)

} // namespace bindings
