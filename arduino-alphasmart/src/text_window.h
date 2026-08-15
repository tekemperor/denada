#ifndef TEXT_WINDOW_H
#define TEXT_WINDOW_H
#include "text_buffer.h"

// The viewport: word wrap, scrolling, and vertical movement.
//
// Everything here is expressed in *display* lines. A display line begins either
// after a newline or at a word-wrap break, so its start is not predictable from
// arithmetic on the window width -- it has to be found by walking the text. That
// walk is next_line_start(), and it is the one primitive the rest of the class
// is built from.
class TextWindow {
  public:
    TextBuffer* buffer;
    int win_start;              // content index of the top display line
    int cursor_row;             // -1 when the point is not on screen
    int cursor_col;
    int desired_column;         // sticky column that survives vertical movement
    char contents[WINDOW_HEIGHT][WINDOW_WIDTH];

    TextWindow(TextBuffer*);
    void clear();
    void attach(TextBuffer*);

    // Word wrap. next_line_start() always advances, which is what keeps every
    // loop built on it terminating.
    int next_line_start(int);
    int prev_line_start(int);
    int line_start_containing(int);
    int last_line_start();
    bool is_line_start_valid(int);
    int display_line_limit(int);
    int position_at_column(int, int);

    // Display-line movement
    void move_up();
    void move_down();
    void move_page_up();
    void move_page_down();
    void move_to_display_line_start();
    void move_to_display_line_end();
    void declare_column_desired();

    void scroll_to_point();
    void render();
};

#endif  // TEXT_WINDOW_H
