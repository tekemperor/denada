#include "text_window.h"
#include <Arduino.h>

TextWindow::TextWindow(TextBuffer* text_buffer) {
    buffer = text_buffer;
    clear();
}

void TextWindow::attach(TextBuffer* text_buffer) {
    buffer = text_buffer;
    clear();
}

void TextWindow::clear() {
    win_start = 0;
    cursor_row = 0;
    cursor_col = 0;
    desired_column = 0;
    std::fill_n(&contents[0][0], WINDOW_HEIGHT * WINDOW_WIDTH, CHAR_NUL);
}

// Word wrap

int TextWindow::next_line_start(int line_start) {
    int size = buffer->content_size();
    if (line_start >= size) { return size; }
    if (line_start < 0) { line_start = 0; }

    int column = 0;
    int break_after_whitespace = -1;  // index just past a whitespace run
    for (int i = line_start; i < size; i++) {
        char character = buffer->get_character(i);
        if (character == CHAR_EOL) { return i + 1; }

        int width = (character == CHAR_TAB) ? TAB_SIZE - (column % TAB_SIZE) : 1;
        if (column + width > WINDOW_WIDTH) {
            // This character will not fit. Prefer breaking at the last word
            // boundary; fall back to a hard break for a word longer than the
            // whole window, which is the only way to keep making progress.
            if (break_after_whitespace > line_start) { return break_after_whitespace; }
            return (i > line_start) ? i : line_start + 1;
        }
        column += width;
        if (character == CHAR_SPC || character == CHAR_TAB) { break_after_whitespace = i + 1; }
    }
    return size;
}

int TextWindow::prev_line_start(int line_start) {
    if (line_start <= 0) { return 0; }
    // Re-wrap from the top of the logical line, because where the previous
    // display line begins depends on where every break before it landed.
    int cursor = buffer->get_line_start_index(line_start - 1);
    while (true) {
        int next = next_line_start(cursor);
        if (next >= line_start || next <= cursor) { return cursor; }
        cursor = next;
    }
}

bool TextWindow::is_line_start_valid(int line_start) {
    int size = buffer->content_size();
    if (line_start < size) { return true; }
    if (line_start <= 0) { return true; }  // an empty buffer still has one line
    // A line can begin at the very end of the content only when a newline put
    // it there. Otherwise "the position after the last character" is the end of
    // the last line, not the start of a new one.
    return buffer->get_character(size - 1) == CHAR_EOL;
}

int TextWindow::line_start_containing(int index) {
    int size = buffer->content_size();
    if (index <= 0) { return 0; }
    if (index > size) { index = size; }
    int cursor = buffer->get_line_start_index(index);
    while (true) {
        int next = next_line_start(cursor);
        if (next > index) { return cursor; }
        if (next <= cursor) { return cursor; }
        if (!is_line_start_valid(next)) { return cursor; }
        cursor = next;
    }
}

int TextWindow::last_line_start() {
    return line_start_containing(buffer->content_size());
}

int TextWindow::display_line_limit(int line_start) {
    int end = next_line_start(line_start);
    if (end <= line_start) { return end; }
    if (buffer->get_character(end - 1) == CHAR_EOL) { return end - 1; }
    if (end >= buffer->content_size()) { return end; }
    // Soft wrap: the spaces the break consumed render at the end of this line,
    // but the point should not park past them or it would draw one row lower.
    int limit = end;
    while (limit > line_start) {
        char character = buffer->get_character(limit - 1);
        if (character != CHAR_SPC && character != CHAR_TAB) { break; }
        limit--;
    }
    return limit;
}

int TextWindow::position_at_column(int line_start, int target_column) {
    int limit = display_line_limit(line_start);
    int column = 0;
    int i = line_start;
    while (i < limit) {
        char character = buffer->get_character(i);
        int width = (character == CHAR_TAB) ? TAB_SIZE - (column % TAB_SIZE) : 1;
        if (column + width > target_column) { break; }
        column += width;
        i++;
    }
    return i;
}

// Display-line movement

void TextWindow::declare_column_desired() {
    int line_start = line_start_containing(buffer->get_point());
    desired_column = buffer->get_column_offset(buffer->get_point()) -
                     buffer->get_column_offset(line_start);
}

void TextWindow::move_up() {
    int line_start = line_start_containing(buffer->get_point());
    if (line_start <= 0) {
        buffer->set_point(0);
        return;
    }
    buffer->set_point(position_at_column(prev_line_start(line_start), desired_column));
}

void TextWindow::move_down() {
    int line_start = line_start_containing(buffer->get_point());
    if (line_start >= last_line_start()) {
        buffer->set_point(buffer->content_size());
        return;
    }
    buffer->set_point(position_at_column(next_line_start(line_start), desired_column));
}

void TextWindow::move_page_up() {
    for (int i = 0; i < WINDOW_HEIGHT; i++) { move_up(); }
}

void TextWindow::move_page_down() {
    for (int i = 0; i < WINDOW_HEIGHT; i++) { move_down(); }
}

void TextWindow::move_to_display_line_start() {
    buffer->set_point(line_start_containing(buffer->get_point()));
}

void TextWindow::move_to_display_line_end() {
    buffer->set_point(display_line_limit(line_start_containing(buffer->get_point())));
}

// Rendering

void TextWindow::scroll_to_point() {
    int target = line_start_containing(buffer->get_point());
    if (target < win_start) {
        win_start = target;
        return;
    }
    int line = win_start;
    for (int row = 0; row < WINDOW_HEIGHT; row++) {
        if (line == target) { return; }
        int next = next_line_start(line);
        if (next <= line) { break; }
        line = next;
    }
    // The point is below the window, so put its line on the bottom row.
    win_start = target;
    for (int row = 0; row < WINDOW_HEIGHT - 1; row++) {
        win_start = prev_line_start(win_start);
    }
}

void TextWindow::render() {
    scroll_to_point();
    std::fill_n(&contents[0][0], WINDOW_HEIGHT * WINDOW_WIDTH, CHAR_NUL);
    cursor_row = -1;
    cursor_col = -1;

    int point = buffer->get_point();
    int final_line = last_line_start();
    int line = win_start;
    for (int row = 0; row < WINDOW_HEIGHT; row++) {
        int line_end = next_line_start(line);
        int column = 0;
        int i = line;
        while (i < line_end && column < WINDOW_WIDTH) {
            // Assigned rather than tested-first so that a point sitting exactly
            // on a soft-wrap boundary resolves to the start of the lower row,
            // which is where a reader expects to see it.
            if (point == i) { cursor_row = row; cursor_col = column; }
            char character = buffer->get_character(i);
            if (character == CHAR_EOL) { i++; break; }
            if (character == CHAR_TAB) {
                int next_stop = column + TAB_SIZE - (column % TAB_SIZE);
                while (column < next_stop && column < WINDOW_WIDTH) {
                    contents[row][column++] = CHAR_SPC;
                }
                i++;
                continue;
            }
            contents[row][column++] = character;
            i++;
        }
        if (point == i && column < WINDOW_WIDTH) { cursor_row = row; cursor_col = column; }
        // Rows past the final display line are blank, and must not keep
        // matching the point -- otherwise a point at the end of the buffer gets
        // claimed by every remaining row and drawn on the last one.
        if (line >= final_line) { break; }
        line = line_end;
    }

#if DEBUG_WINDOW_TRACE
    Serial.printf("[win %d point %d cursor %d,%d]\n", win_start, point, cursor_row, cursor_col);
#endif
}
