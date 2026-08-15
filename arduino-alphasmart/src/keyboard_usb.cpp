#include "keyboard_usb.h"

const uint8_t KeyboardInputHandler::numKeys[10] PROGMEM = {'!', '@', '#', '$', '%', '^', '&', '*', '(', ')'};
const uint8_t KeyboardInputHandler::symKeysUp[12] PROGMEM = {'_', '+', '{', '}', '|', '~', ':', '"', '~', '<', '>', '?'};
const uint8_t KeyboardInputHandler::symKeysLo[12] PROGMEM = {'-', '=', '[', ']', '\\', ' ', ';', '\'', '`', ',', '.', '/'};
const uint8_t KeyboardInputHandler::padKeys[5] PROGMEM = {'/', '*', '-', '+', CHAR_EOL};

KeyboardInputHandler::KeyboardInputHandler()
{
    bmCapsLock = false;
    bmNumLock = false;
    bmScrollLock = false;
    reset();
}

void KeyboardInputHandler::reset()
{
    for (int i = 0; i < KBDINFO_SIZE; i++)
    {
        previous_report[i] = KEY_NONE;
    }
}

bool KeyboardInputHandler::was_held(uint8_t key)
{
    for (int i = KBDINFO_FIRST_KEY_INDEX; i < KBDINFO_SIZE; i++)
    {
        if (previous_report[i] == key)
        {
            return true;
        }
    }
    return false;
}

void KeyboardInputHandler::handle_locking_keys(uint8_t key)
{
    if (key == KEY_NUMLOCK)
    {
        bmNumLock = !bmNumLock;
    }
    if (key == KEY_CAPSLOCK)
    {
        bmCapsLock = !bmCapsLock;
    }
    if (key == KEY_SCROLLLOCK)
    {
        bmScrollLock = !bmScrollLock;
    }
}

uint8_t KeyboardInputHandler::held_key()
{
    for (int i = KBDINFO_FIRST_KEY_INDEX; i < KBDINFO_SIZE; i++)
    {
        if (previous_report[i] != KEY_NONE && previous_report[i] != KEY_ERR_OVF)
        {
            return previous_report[i];
        }
    }
    return KEY_NONE;
}

uint8_t KeyboardInputHandler::held_modifiers()
{
    return previous_report[KBDINFO_MODIFIERS_INDEX];
}

int KeyboardInputHandler::parse(const uint8_t *report, EditorAction *actions, int max_actions)
{
    // A rollover report means the keyboard could not say which keys are down.
    // Acting on it would emit garbage.
    if (report[KBDINFO_FIRST_KEY_INDEX] == KEY_ERR_OVF)
    {
        return 0;
    }

    uint8_t modifiers = report[KBDINFO_MODIFIERS_INDEX];
    int count = 0;
    for (int i = KBDINFO_FIRST_KEY_INDEX; i < KBDINFO_SIZE; i++)
    {
        uint8_t key = report[i];
        if (key == KEY_NONE || key == KEY_ERR_OVF)
        {
            continue;
        }
        // Present in the previous report means still held, not pressed again.
        if (was_held(key))
        {
            continue;
        }
        handle_locking_keys(key);
        if (count < max_actions)
        {
            EditorAction action = translate(modifiers, key);
            if (action.command != EditorCommand::NONE)
            {
                actions[count++] = action;
            }
        }
    }

    for (int i = 0; i < KBDINFO_SIZE; i++)
    {
        previous_report[i] = report[i];
    }
    return count;
}

EditorAction KeyboardInputHandler::translate(uint8_t modifiers, uint8_t key)
{
    EditorAction action;
    bool shift = (modifiers & KEY_MOD_ANY_SHIFT) != 0;
    bool control = (modifiers & KEY_MOD_ANY_CTRL) != 0;

    // F1-F8 select a buffer; that single touch is the whole buffer-switch UI.
    if (VALUE_WITHIN(key, KEY_F1, KEY_F8))
    {
        action.command = EditorCommand::SWITCH_BUFFER;
        action.index = key - KEY_F1;
        return action;
    }
    if (key == KEY_F9)
    {
        action.command = EditorCommand::SEND_BUFFER;
        return action;
    }

    if (control)
    {
        switch (key)
        {
        case KEY_A:
            action.command = EditorCommand::SELECT_ALL;
            return action;
        case KEY_C:
            action.command = EditorCommand::COPY;
            return action;
        case KEY_X:
            action.command = EditorCommand::CUT;
            return action;
        case KEY_V:
            action.command = EditorCommand::PASTE;
            return action;
        case KEY_HOME:
            action.command = EditorCommand::MOVE_BUFFER_START;
            action.extend_selection = shift;
            return action;
        case KEY_END:
            action.command = EditorCommand::MOVE_BUFFER_END;
            action.extend_selection = shift;
            return action;
        default:
            break;
        }
    }

    switch (key)
    {
    case KEY_LEFT:
        action.command = EditorCommand::MOVE_LEFT;
        break;
    case KEY_RIGHT:
        action.command = EditorCommand::MOVE_RIGHT;
        break;
    case KEY_UP:
        action.command = EditorCommand::MOVE_UP;
        break;
    case KEY_DOWN:
        action.command = EditorCommand::MOVE_DOWN;
        break;
    case KEY_HOME:
        action.command = EditorCommand::MOVE_LINE_START;
        break;
    case KEY_END:
        action.command = EditorCommand::MOVE_LINE_END;
        break;
    case KEY_PAGEUP:
        action.command = EditorCommand::MOVE_PAGE_UP;
        break;
    case KEY_PAGEDOWN:
        action.command = EditorCommand::MOVE_PAGE_DOWN;
        break;
    case KEY_BACKSPACE:
        action.command = EditorCommand::DELETE_BACKWARD;
        return action;
    case KEY_DELETE:
        action.command = EditorCommand::DELETE_FORWARD;
        return action;
    default:
        break;
    }
    if (action.command != EditorCommand::NONE)
    {
        // Shift turns any movement into a selection extension.
        action.extend_selection = shift;
        return action;
    }

    // Ctrl chords that are not bound above must not fall through and type a
    // literal character.
    if (control)
    {
        return action;
    }

    uint8_t character = oem_to_ascii(modifiers, key);
    if (character != 0)
    {
        action.command = EditorCommand::INSERT_CHARACTER;
        action.character = (char)character;
    }
    return action;
}

uint8_t KeyboardInputHandler::oem_to_ascii(uint8_t mod, uint8_t key)
{
    uint8_t shift = (mod & KEY_MOD_ANY_SHIFT);
    if (VALUE_WITHIN(key, KEY_A, KEY_Z) && (bmCapsLock || shift)) // Letters Upper Case
        return (key - KEY_A + 'A');
    if (VALUE_WITHIN(key, KEY_A, KEY_Z) && !(bmCapsLock || shift)) // Letters Lower Case
        return (key - KEY_A + 'a');
    if (VALUE_WITHIN(key, KEY_1, KEY_0) && (shift)) // Number Row Symbols
        return ((uint8_t)pgm_read_byte(&numKeys[key - KEY_1]));
    if (VALUE_WITHIN(key, KEY_1, KEY_0) && !(shift)) // Number Row Numbers
        return ((key == KEY_0) ? '0' : key - KEY_1 + '1');
    if (key == KEY_ENTER) // Enter Key
        return (CHAR_EOL);
    if (key == KEY_TAB) // Tab Key
        return (CHAR_TAB);
    if (key == KEY_SPACE) // Spacebar
        return (CHAR_SPC);
    if (VALUE_WITHIN(key, KEY_MINUS, KEY_SLASH) && (shift)) // Symbols "Upper"
        return (uint8_t)pgm_read_byte(&symKeysUp[key - KEY_MINUS]);
    if (VALUE_WITHIN(key, KEY_MINUS, KEY_SLASH) && !(shift)) // Symbols Base
        return (uint8_t)pgm_read_byte(&symKeysLo[key - KEY_MINUS]);
    if (VALUE_WITHIN(key, KEY_KPSLASH, KEY_KPENTER)) // Keypad Symbols
        return (uint8_t)pgm_read_byte(&padKeys[key - KEY_KPSLASH]);
    if (VALUE_WITHIN(key, KEY_KP1, KEY_KP0) && bmNumLock) // Keypad Numbers
        return ((key == KEY_KP0) ? '0' : key - KEY_KP1 + '1');
    if ((key == KEY_KPDOT) && (bmNumLock)) // Keypad Dot
        return ('.');
    // safe default for unknown keys
    return (0);
}
