#include "text_document.h"

TextDocument::TextDocument() {
    clear();
}

void TextDocument::clear() {
    for (int i = 0; i < BUFFER_COUNT; i++) {
        buffers[i].clear();
        // Buffers are addressed by the function key that selects them, so the
        // name a user sees matches the key they pressed.
        buffers[i].buffer_name[0] = 'F';
        buffers[i].buffer_name[1] = (char)('1' + i);
        buffers[i].buffer_name[2] = CHAR_NUL;
    }
    std::fill_n(clipboard, CLIPBOARD_SIZE, CHAR_NUL);
    clipboard_length = 0;
    active_index = 0;
}

TextBuffer* TextDocument::active() {
    return &buffers[active_index];
}

int TextDocument::get_active_index() {
    return active_index;
}

bool TextDocument::select_buffer(int index) {
    if (index < 0 || index >= BUFFER_COUNT) { return false; }
    if (index == active_index) { return false; }
    // Leaving a buffer drops its selection: the anchor would otherwise still be
    // set the next time this buffer came back, with no highlight on screen to
    // explain why typing replaced a run of text.
    active()->clear_anchor();
    active_index = index;
    return true;
}

bool TextDocument::has_clipboard() {
    return clipboard_length > 0;
}

bool TextDocument::copy_selection() {
    if (!active()->has_selection()) { return false; }
    clipboard_length = active()->copy_selection(clipboard, CLIPBOARD_SIZE);
    return clipboard_length > 0;
}

bool TextDocument::cut_selection() {
    if (!copy_selection()) { return false; }
    active()->delete_selection();
    return true;
}

bool TextDocument::paste() {
    if (!has_clipboard()) { return false; }
    if (active()->has_selection()) { active()->delete_selection(); }
    return active()->insert_text(clipboard, clipboard_length);
}
