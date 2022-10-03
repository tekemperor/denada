#include "text_window.h"

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

void TextWindow::get_window() {
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
}
