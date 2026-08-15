#ifndef EDITOR_H
#define EDITOR_H
#include "editor_command.h"
#include "text_document.h"
#include "text_window.h"

#define STATUS_MESSAGE_SIZE (WINDOW_WIDTH + 1)

// Applies editor commands to the document and window.
//
// Deliberately free of any Arduino dependency, including millis(): a status
// message records only *that* it changed, via status_sequence, and the hardware
// layer decides how long to leave it up. That keeps the whole command layer
// runnable under the host tests.
class Editor {
public:
    TextDocument document;
    TextWindow window;
    char status_message[STATUS_MESSAGE_SIZE];
    unsigned int status_sequence;  // increments on every new message
    bool send_requested;           // set by SEND_BUFFER, cleared by the caller

    Editor();
    TextBuffer* buffer();
    void apply(const EditorAction&);
    void set_status(const char*);
    void clear_status();
    bool has_status();

private:
    void apply_movement(const EditorAction&);
};

#endif // EDITOR_H
