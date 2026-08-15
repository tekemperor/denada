#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H
#include "gap_buffer.h"
#include <algorithm>
#include <cstring>

// One editable document: content, the point, and a selection anchor.
//
// TextBuffer deals only in *logical* lines, the ones delimited by newlines. How
// text folds into a 20-column screen is TextWindow's problem. Keeping the split
// here is what makes word wrap possible at all: a wrapped line does not begin at
// a fixed multiple of the screen width, so anything that reasoned about
// position as "column offset % window width" was working from a false premise.
//
// Column offsets are computed on demand by scanning from the start of the line
// rather than mutated incrementally as the point moves. The incremental version
// carried three separate and mutually inconsistent rules for how wide a tab is,
// and any disagreement between them desynced the cursor permanently.
class TextBuffer {
    public:
    GapBuffer content;
    char buffer_name[BUFFER_NAME_SIZE];
    bool is_modified;

    TextBuffer();
    void clear();

    // Modification. Each returns false if the buffer was too full to take the
    // whole edit, so a dropped keystroke can be surfaced instead of vanishing.
    bool insert_character(char);
    bool insert_text(const char*, int);
    bool delete_before_point(int);
    bool delete_after_point(int);
    int delete_range(int, int);

    // Point movement
    void set_point(int);
    void move_point_left();
    void move_point_right();
    void move_point_to_line_start();
    void move_point_to_line_end();
    void move_point_to_buffer_start();
    void move_point_to_buffer_end();

    // Selection
    void set_anchor();
    void set_anchor_at(int);
    void clear_anchor();
    bool has_selection();
    int selection_start();
    int selection_end();
    int copy_selection(char*, int);
    void delete_selection();

    // Status
    char get_character(int);
    int get_point();
    int get_anchor();
    int get_line_number();
    int get_column_number();
    int content_size();
    bool is_full();

    // Persistence
    const uint8_t* raw_bytes();
    static int raw_size();
    bool load_raw(const uint8_t*, int);
    uint8_t* raw_storage_for_load();
    bool finish_raw_load(bool);

    // Logical line helpers
    int get_line_start_index(int);
    int get_line_end_index(int);
    int get_column_offset(int);

    private:
    int point;
    int anchor;  // -1 when there is no selection

    int clamp_index(int);
};
#endif  // TEXT_BUFFER_H
