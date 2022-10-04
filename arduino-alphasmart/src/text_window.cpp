#include "text_window.h"
#include <Arduino.h>

TextWindow::TextWindow(TextBuffer* text_buffer) {
    buffer = text_buffer;
    clear();
}

void TextWindow::clear() {
    win_start = 0;
    cursor_row = 0;
    cursor_col = 0;
    // contents
    std::fill_n(&contents[0][0], WINDOW_HEIGHT * WINDOW_WIDTH, 0);
}

int TextWindow::update_window_contents() {
  int char_loc = win_start;
  char character = CHAR_NUL;
  for (int row = 0; row < WINDOW_HEIGHT; row++) {
    for (int col = 0; col < WINDOW_WIDTH; col++) {
      if (buffer->get_point() == char_loc) {
        cursor_row = row;
        cursor_col = col;
      }
      character = buffer->get_character(char_loc++);
      contents[row][col] = character;
      if (character == CHAR_EOL) {
        for (;col < WINDOW_WIDTH; col++) {
          contents[row][col] = CHAR_NUL;
        }
      }
      if (character == CHAR_TAB) {
        int tab_excess = (col + TAB_SIZE) % TAB_SIZE;
        int next_stop = col + TAB_SIZE - tab_excess;
        for (;col < next_stop && col < WINDOW_WIDTH; col++) {
          contents[row][col] = CHAR_NUL;
        }
        col--;
      }
    }
  }
  return char_loc;
}

void TextWindow::update_window() {
  while (win_start > buffer->get_point())
    win_start = buffer->get_prev_window_line_start_index(win_start);
  int last_index = 0;
  int next_index = update_window_contents();
  while (next_index > last_index && next_index <= buffer->get_point()) {
    Serial.printf("(%d,%d,%d)\n",win_start,buffer->get_point(),next_index);
    last_index = next_index;
    win_start = buffer->get_next_window_line_start_index(win_start);
    next_index = update_window_contents();
  }
}