#ifndef EDITOR_COMMAND_H
#define EDITOR_COMMAND_H
#include "config.h"

// The vocabulary shared between "a key was pressed" and "the document changed".
//
// Keeping this in its own header is what lets the keyboard be tested without a
// keyboard: translating a HID report into commands is pure logic, and so is
// applying a command to a document. Only the two ends of that chain need real
// hardware.
enum class EditorCommand {
    NONE,
    INSERT_CHARACTER,
    DELETE_BACKWARD,
    DELETE_FORWARD,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LINE_START,
    MOVE_LINE_END,
    MOVE_PAGE_UP,
    MOVE_PAGE_DOWN,
    MOVE_BUFFER_START,
    MOVE_BUFFER_END,
    SELECT_ALL,
    COPY,
    CUT,
    PASTE,
    SWITCH_BUFFER,
    SEND_BUFFER,
};

struct EditorAction {
    EditorCommand command;
    char character;         // for INSERT_CHARACTER
    int index;              // for SWITCH_BUFFER
    bool extend_selection;  // set when Shift is held with a movement command

    EditorAction()
        : command(EditorCommand::NONE), character(CHAR_NUL), index(0),
          extend_selection(false) {}
};

// True for commands that only move the point, which are the ones Shift extends
// a selection across and the ones that must not disturb the sticky column.
inline bool is_movement_command(EditorCommand command) {
    switch (command) {
    case EditorCommand::MOVE_LEFT:
    case EditorCommand::MOVE_RIGHT:
    case EditorCommand::MOVE_UP:
    case EditorCommand::MOVE_DOWN:
    case EditorCommand::MOVE_LINE_START:
    case EditorCommand::MOVE_LINE_END:
    case EditorCommand::MOVE_PAGE_UP:
    case EditorCommand::MOVE_PAGE_DOWN:
    case EditorCommand::MOVE_BUFFER_START:
    case EditorCommand::MOVE_BUFFER_END:
        return true;
    default:
        return false;
    }
}

// Vertical movement keeps the sticky column; everything else re-declares it.
inline bool is_vertical_command(EditorCommand command) {
    switch (command) {
    case EditorCommand::MOVE_UP:
    case EditorCommand::MOVE_DOWN:
    case EditorCommand::MOVE_PAGE_UP:
    case EditorCommand::MOVE_PAGE_DOWN:
        return true;
    default:
        return false;
    }
}

#endif // EDITOR_COMMAND_H
