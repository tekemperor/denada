#include "text_buffer.h"

TextBuffer::TextBuffer() {
    clear();
}

void TextBuffer::clear() {
    content.clear();
    point = 0;
    anchor = -1;
    std::fill_n(buffer_name, BUFFER_NAME_SIZE, CHAR_NUL);
    is_modified = false;
}

// Modification

bool TextBuffer::insert_character(char character) {
    bool inserted = content.insert_character(point, character);
    if (!inserted) { return false; }
    point++;
    is_modified = true;
    return true;
}

bool TextBuffer::insert_text(const char* text, int length) {
    for (int i = 0; i < length; i++) {
        if (!insert_character(text[i])) { return false; }
    }
    return true;
}

bool TextBuffer::delete_before_point(int count) {
    bool deleted_everything_asked = true;
    for (int i = 0; i < count; i++) {
        if (point <= 0) { deleted_everything_asked = false; break; }
        point--;
        content.delete_char(point);
        is_modified = true;
    }
    return deleted_everything_asked;
}

bool TextBuffer::delete_after_point(int count) {
    bool deleted_everything_asked = true;
    for (int i = 0; i < count; i++) {
        if (point > content.max_index()) { deleted_everything_asked = false; break; }
        content.delete_char(point);
        is_modified = true;
    }
    return deleted_everything_asked;
}

int TextBuffer::delete_range(int start, int end) {
    start = clamp_index(start);
    end = clamp_index(end);
    if (start > end) { std::swap(start, end); }
    int deleted = 0;
    for (int i = start; i < end; i++) {
        if (start > content.max_index()) { break; }
        content.delete_char(start);
        deleted++;
        is_modified = true;
    }
    if (point > end) { point -= deleted; }
    else if (point > start) { point = start; }
    return deleted;
}

// Point movement

int TextBuffer::clamp_index(int index) {
    if (index < 0) { return 0; }
    if (index > content_size()) { return content_size(); }
    return index;
}

void TextBuffer::set_point(int index) {
    point = clamp_index(index);
}

void TextBuffer::move_point_left() {
    set_point(point - 1);
}

void TextBuffer::move_point_right() {
    set_point(point + 1);
}

void TextBuffer::move_point_to_line_start() {
    set_point(get_line_start_index(point));
}

void TextBuffer::move_point_to_line_end() {
    set_point(get_line_end_index(point));
}

void TextBuffer::move_point_to_buffer_start() {
    set_point(0);
}

void TextBuffer::move_point_to_buffer_end() {
    set_point(content_size());
}

// Selection

void TextBuffer::set_anchor() {
    // Only the first Shift+move plants the anchor; the rest extend from it.
    if (anchor < 0) { anchor = point; }
}

void TextBuffer::set_anchor_at(int index) {
    anchor = clamp_index(index);
}

void TextBuffer::clear_anchor() {
    anchor = -1;
}

bool TextBuffer::has_selection() {
    return anchor >= 0 && anchor != point;
}

int TextBuffer::selection_start() {
    if (!has_selection()) { return point; }
    return std::min(anchor, point);
}

int TextBuffer::selection_end() {
    if (!has_selection()) { return point; }
    return std::max(anchor, point);
}

int TextBuffer::copy_selection(char* destination, int destination_size) {
    int start = selection_start();
    int end = selection_end();
    int length = 0;
    for (int i = start; i < end && length < destination_size; i++) {
        destination[length++] = get_character(i);
    }
    return length;
}

void TextBuffer::delete_selection() {
    if (!has_selection()) { return; }
    delete_range(selection_start(), selection_end());
    clear_anchor();
}

// Status

char TextBuffer::get_character(int content_index) {
    // CHAR_NUL for out of bounds
    return content.get_character(content_index);
}

int TextBuffer::get_point() {
    return point;
}

int TextBuffer::get_anchor() {
    return anchor;
}

int TextBuffer::get_line_number() {
    int line_number = FIRST_LINE_NUMBER;
    for (int i = 0; i < point; i++) {
        if (get_character(i) == CHAR_EOL) { line_number++; }
    }
    return line_number;
}

int TextBuffer::get_column_number() {
    return FIRST_COLUMN_NUMBER + get_column_offset(point);
}

int TextBuffer::content_size() {
    return content.content_size();
}

bool TextBuffer::is_full() {
    return content.is_full();
}

// Persistence

const uint8_t* TextBuffer::raw_bytes() {
    return content.raw_bytes();
}

int TextBuffer::raw_size() {
    return GapBuffer::raw_size();
}

bool TextBuffer::load_raw(const uint8_t* bytes, int size) {
    if (bytes == nullptr || size != raw_size()) { return finish_raw_load(false); }
    std::copy(bytes, bytes + size, content.raw_storage());
    return finish_raw_load(true);
}

uint8_t* TextBuffer::raw_storage_for_load() {
    return content.raw_storage();
}

bool TextBuffer::finish_raw_load(bool read_succeeded) {
    anchor = -1;
    is_modified = false;
    if (!read_succeeded || !content.adopt_raw()) {
        // Refuse to interpret a record this firmware does not recognise; an
        // unreadable file becomes an empty buffer, never garbage text.
        content.clear();
        point = 0;
        return false;
    }
    // The gap sits wherever the last edit happened, so restoring the point from
    // it puts the cursor back where the writer left off without needing a
    // separate field in the record.
    point = clamp_index(content.gap_start());
    return true;
}

// Logical line helpers

int TextBuffer::get_line_start_index(int content_index) {
    content_index = clamp_index(content_index);
    for (int i = content_index - 1; i >= 0; i--) {
        if (get_character(i) == CHAR_EOL) { return i + 1; }
    }
    return 0;
}

int TextBuffer::get_line_end_index(int content_index) {
    // The position *after* the last character of the line, so that the point can
    // sit at the very end of the buffer. Returning max_index() here meant the
    // final character was unreachable.
    content_index = clamp_index(content_index);
    int size = content_size();
    for (int i = content_index; i < size; i++) {
        if (get_character(i) == CHAR_EOL) { return i; }
    }
    return size;
}

int TextBuffer::get_column_offset(int content_index) {
    content_index = clamp_index(content_index);
    int column_offset = 0;
    int start_of_line = get_line_start_index(content_index);
    for (int i = start_of_line; i < content_index; i++) {
        if (get_character(i) == CHAR_TAB) {
            // Advance to the next tab stop; a tab is not a fixed TAB_SIZE wide.
            column_offset += TAB_SIZE - (column_offset % TAB_SIZE);
        } else {
            column_offset++;
        }
    }
    return column_offset;
}
