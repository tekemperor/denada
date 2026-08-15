#include "editor.h"

Editor::Editor() : window(document.active()) {
    status_sequence = 0;
    send_requested = false;
    clear_status();
}

TextBuffer* Editor::buffer() {
    return document.active();
}

void Editor::set_status(const char* message) {
    int i = 0;
    for (; message[i] != CHAR_NUL && i < STATUS_MESSAGE_SIZE - 1; i++) {
        status_message[i] = message[i];
    }
    status_message[i] = CHAR_NUL;
    status_sequence++;
}

void Editor::clear_status() {
    status_message[0] = CHAR_NUL;
}

bool Editor::has_status() {
    return status_message[0] != CHAR_NUL;
}

void Editor::apply(const EditorAction& action) {
    TextBuffer* text = document.active();

    if (is_movement_command(action.command)) {
        apply_movement(action);
        return;
    }

    switch (action.command) {
    case EditorCommand::NONE:
        return;

    case EditorCommand::INSERT_CHARACTER:
        // Typing over a selection replaces it, which is what makes Shift+arrow
        // selection feel like a selection rather than a highlight.
        if (text->has_selection()) { text->delete_selection(); }
        if (!text->insert_character(action.character)) {
            // A full buffer used to swallow keystrokes and look identical to a
            // working one. Say so instead.
            set_status("BUFFER FULL");
            return;
        }
        text->clear_anchor();
        window.declare_column_desired();
        return;

    case EditorCommand::DELETE_BACKWARD:
        if (text->has_selection()) { text->delete_selection(); }
        else { text->delete_before_point(1); }
        window.declare_column_desired();
        return;

    case EditorCommand::DELETE_FORWARD:
        if (text->has_selection()) { text->delete_selection(); }
        else { text->delete_after_point(1); }
        window.declare_column_desired();
        return;

    case EditorCommand::SELECT_ALL:
        text->set_anchor_at(0);
        text->set_point(text->content_size());
        window.declare_column_desired();
        return;

    case EditorCommand::COPY:
        set_status(document.copy_selection() ? "COPIED" : "NO SELECTION");
        return;

    case EditorCommand::CUT:
        if (document.cut_selection()) {
            set_status("CUT");
            window.declare_column_desired();
        } else {
            set_status("NO SELECTION");
        }
        return;

    case EditorCommand::PASTE:
        if (!document.has_clipboard()) {
            set_status("CLIPBOARD EMPTY");
            return;
        }
        if (!document.paste()) { set_status("BUFFER FULL"); }
        window.declare_column_desired();
        return;

    case EditorCommand::SWITCH_BUFFER:
        if (document.select_buffer(action.index)) {
            // Each buffer keeps its own point, so the window re-derives its
            // scroll position from that on the next render.
            window.attach(document.active());
            window.declare_column_desired();
        }
        set_status(document.active()->buffer_name);
        return;

    case EditorCommand::SEND_BUFFER:
        send_requested = true;
        return;

    default:
        return;
    }
}

void Editor::apply_movement(const EditorAction& action) {
    TextBuffer* text = document.active();
    if (action.extend_selection) { text->set_anchor(); }
    else { text->clear_anchor(); }

    switch (action.command) {
    case EditorCommand::MOVE_LEFT:         text->move_point_left(); break;
    case EditorCommand::MOVE_RIGHT:        text->move_point_right(); break;
    case EditorCommand::MOVE_UP:           window.move_up(); break;
    case EditorCommand::MOVE_DOWN:         window.move_down(); break;
    case EditorCommand::MOVE_LINE_START:   window.move_to_display_line_start(); break;
    case EditorCommand::MOVE_LINE_END:     window.move_to_display_line_end(); break;
    case EditorCommand::MOVE_PAGE_UP:      window.move_page_up(); break;
    case EditorCommand::MOVE_PAGE_DOWN:    window.move_page_down(); break;
    case EditorCommand::MOVE_BUFFER_START: text->move_point_to_buffer_start(); break;
    case EditorCommand::MOVE_BUFFER_END:   text->move_point_to_buffer_end(); break;
    default: break;
    }

    // Vertical movement is the one case that must preserve the sticky column;
    // everything else redefines where "this column" is.
    if (!is_vertical_command(action.command)) { window.declare_column_desired(); }
}
