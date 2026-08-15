#ifndef TEXT_DOCUMENT_H
#define TEXT_DOCUMENT_H
#include "text_buffer.h"

// The eight buffers and the clipboard that moves text between them.
//
// All eight live in RAM at once. That is the whole reason buffer switching can
// be instant, which the wiki lists as a requirement: swapping is an index
// change, not a load.
class TextDocument {
public:
    TextBuffer buffers[BUFFER_COUNT];
    char clipboard[CLIPBOARD_SIZE];
    int clipboard_length;

    TextDocument();
    void clear();

    TextBuffer* active();
    int get_active_index();
    // Returns false for an out-of-range index or a switch to the current
    // buffer, so callers can skip a redraw that would change nothing.
    bool select_buffer(int);

    bool has_clipboard();
    // Each returns false when there was no selection to act on.
    bool copy_selection();
    bool cut_selection();
    // False when the clipboard is empty or the buffer had no room.
    bool paste();

private:
    int active_index;
};

#endif // TEXT_DOCUMENT_H
