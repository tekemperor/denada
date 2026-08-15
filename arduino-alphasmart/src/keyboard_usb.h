#ifndef KEYBOARD_USB_H
#define KEYBOARD_USB_H
/**
 * HID boot-protocol keyboard decoding.
 *
 * The scancode tables and OEM-to-ASCII rules are ported from
 * https://github.com/felis/USB_Host_Shield_2.0
 *
 * The translation half is deliberately pure: parse() turns an 8-byte report
 * into EditorActions and touches nothing else, so the entire key map is
 * testable on the host without a USB stack or a keyboard.
 */
#include "config.h"
#include "editor_command.h"
#include "keyboard_buffer_usb.h"
#include "keycodes_usb.h"
#include <pgmspace.h>
#include <cstdint>
#include <cstdio>
#define VALUE_BETWEEN(v, l, h) (((v) > (l)) && ((v) < (h)))
#define VALUE_WITHIN(v, l, h) (((v) >= (l)) && ((v) <= (h)))

#define KEY_MOD_ANY_SHIFT (KEY_MOD_LSHIFT | KEY_MOD_RSHIFT)
#define KEY_MOD_ANY_CTRL (KEY_MOD_LCTRL | KEY_MOD_RCTRL | KEY_MOD_LMETA | KEY_MOD_RMETA)

// The most actions a single report can produce, one per key slot.
#define KEYBOARD_MAX_ACTIONS KBDINFO_MAX_KEYS

class KeyboardInputHandler
{
public:
    KeyboardInputHandler();

    bool bmCapsLock;
    bool bmNumLock;
    bool bmScrollLock;

    // Decodes one report, emitting actions only for newly pressed keys.
    // Returns how many were written to `actions`.
    int parse(const uint8_t *report, EditorAction *actions, int max_actions);

    // What a key means right now. Public because key repeat re-issues it.
    EditorAction translate(uint8_t modifiers, uint8_t key);

    uint8_t oem_to_ascii(uint8_t modifiers, uint8_t key);

    // For auto-repeat: the key still held from the last report, or KEY_NONE.
    uint8_t held_key();
    uint8_t held_modifiers();

    void reset();

private:
    static const uint8_t numKeys[10];
    static const uint8_t symKeysUp[12];
    static const uint8_t symKeysLo[12];
    static const uint8_t padKeys[5];

    uint8_t previous_report[KBDINFO_SIZE];

    bool was_held(uint8_t key);
    void handle_locking_keys(uint8_t key);
};

#endif // KEYBOARD_USB_H
