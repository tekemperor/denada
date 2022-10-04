#include "text_buffer.h"
#include <Arduino.h>

TextBuffer::TextBuffer() {
    clear();
}

void TextBuffer::clear() {
    content = GapBuffer();
    point = 0;
    actual_line_offset = 0;
    actual_column_offset = 0;
    desired_column_offset = 0;
    std::fill_n(buffer_name, BUFFER_NAME_SIZE, CHAR_NUL); 
    std::fill_n(file_name, BUFFER_FILENAME_SIZE, CHAR_NUL);
    is_modified = false; 
}

// Modification

void TextBuffer::insert_character(char character) {
    bool success = content.insert_character(point, character);
    if (success) { move_point_right_explicit(); }
}

void TextBuffer::delete_characters(int count) {
    int direction = (count <  0) ? -1 : 1;
    for (int i = 0; i < direction * count; i++) {
        if(direction <  0){ move_point_left_explicit(); }
        content.delete_char(point);
    }
}

// Navigation
void TextBuffer::move_point_left_explicit() {
    move_point_left_implicit();
    declare_column_desired();
}
void TextBuffer::move_point_right_explicit() {
    move_point_right_implicit();
    declare_column_desired();
}


void TextBuffer::move_point_up() {
    if (point <= 0) { return; }
    point = get_line_start_index(point);
    if (point <= 0) {
        actual_column_offset = 0;
        desired_column_offset = actual_column_offset;
        return;
    }
    point--;  // skip over the newline character
    actual_line_offset--;
    actual_column_offset = get_column_offset(point);
    int original_desired_column_offset = desired_column_offset;
    while (actual_column_offset > original_desired_column_offset) {
        move_point_left_implicit();
    }
}

void TextBuffer::move_point_down() {
    if (point > content.max_index()) { return; }
    point = get_line_end_index(point);
    point++;
    if (point > content.max_index()) {
        actual_column_offset = get_column_offset(point);
        desired_column_offset = actual_column_offset;
        return;
    }
    actual_line_offset++;
    point = get_line_end_index(point);
    int original_desired_column_offset = desired_column_offset;
    while (actual_column_offset > original_desired_column_offset) {
        move_point_left_implicit();
    }
}

void TextBuffer::move_point_home() {
    while(actual_column_offset > 0) {
        move_point_left_implicit();
    }
    declare_column_desired();
}

void TextBuffer::move_point_end() {
    int line_end = get_line_end_index(point);
    while(point < line_end) {
        move_point_right_implicit();
    }
    declare_column_desired();
}

void TextBuffer::window_move_point_up(int width) {
    int desired_apparent_column_offset = desired_column_offset % width;
    while(actual_column_offset % width != 0){
        move_point_left_implicit();
    }
    move_point_left_implicit();
    while(actual_column_offset % width > desired_apparent_column_offset) {
        move_point_left_implicit();
    }
}
void TextBuffer::window_move_point_down(int width) {
    int desired_apparent_column_offset = desired_column_offset % width;
    move_point_right_implicit();
    while(actual_column_offset % width != 0) {
        move_point_right_implicit();
    }
    int initial_line_offset = actual_line_offset;
    while(point <= content.max_index() &&
          initial_line_offset == actual_line_offset &&
          actual_column_offset % width < desired_apparent_column_offset) {
        move_point_right_implicit();
    }
    if (initial_line_offset < actual_line_offset ||
        actual_column_offset % width > desired_apparent_column_offset) {
        move_point_left_implicit();
    }
}
void TextBuffer::window_move_point_page_up(int width, int height) {
    for (int i = 0; i < height; i++) {
        window_move_point_up(width);
    }
}
void TextBuffer::window_move_point_page_down(int width, int height) {
    for (int i = 0; i < height; i++) {
        window_move_point_down(width);
    }
}


// Status
char TextBuffer::get_character(int content_index) {
    // CHAR_NUL for out of bounds
    return content.get_character(content_index);
}

int TextBuffer::get_point() {
    return point;
}

int TextBuffer::get_line_number() {
    return FIRST_LINE_NUMBER + actual_line_offset ;
}

int TextBuffer::get_column_number() {
    return FIRST_COLUMN_NUMBER + actual_column_offset;
}

int TextBuffer::content_size(){
    return content.content_size();
}

// Helpers

int TextBuffer::find_first_in_forward(int start_index, char* character_list) {
    int content_size = content.content_size();
    int list_size = strlen(character_list);
    for (int index = start_index; index < content_size; index++) {
        for (int i = 0; i < list_size; i++) {
            if (character_list[i] == get_character(index)) { return index; }
        }
    }
    return -1;
}
int TextBuffer::find_first_in_backward(int start_index, char* character_list) {
    int list_size = strlen(character_list);
    for (int index = start_index; index >= 0; index--) {
        for (int i = 0; i < list_size; i++) {
            if (character_list[i] == get_character(index)) { return index; }
        }
    }
    return -1;
}
int TextBuffer::find_first_not_in_forward(int start_index, char* character_list) {
    int content_size = content.content_size();
    int list_size = strlen(character_list);
    bool match_found;
    for (int index = start_index; index < content_size; index++) {
        match_found = false;
        for (int i = 0; i < list_size; i++) {
            if (character_list[i] == get_character(index)) { match_found = true; }
        }
        if (match_found == false) { return index; }
    }
    return -1;

}
int TextBuffer::find_first_not_in_backward(int start_index, char* character_list) {
    int list_size = strlen(character_list);
    bool match_found;
    for (int index = start_index; index >= 0; index--) {
        match_found = false;
        for (int i = 0; i < list_size; i++) {
            if (character_list[i] == get_character(index)) { match_found = true; }
        }
        if (match_found == false) { return index; }
    }
    return -1;
}

int TextBuffer::get_prev_window_line_start_index(int current_line_start_index) {
    int last_start = current_line_start_index - WINDOW_WIDTH;
    while (last_start >= 0 && get_next_window_line_start_index(last_start) < current_line_start_index)
        last_start++;
    return std::max(last_start, 0);
}

int TextBuffer::get_next_window_line_start_index(int current_line_start_index) {
    int worst_case_index = current_line_start_index + WINDOW_WIDTH;
    return std::min(worst_case_index, content.content_size());
    int last_start = current_line_start_index;
    int last_end = find_first_in_forward(last_start, WHITESPACE) - 1;
    int current_start = last_start;
    int current_end = last_end;
    int extra_space = 0;
    int column_offset = 0;
    while (current_end >= 0 &&
           current_end + extra_space < worst_case_index &&
           current_end < content_size())
    {
        last_start = current_start;
        last_end = current_end;
        current_start = find_first_not_in_forward(current_end + 1, WHITESPACE);
        current_end = find_first_in_forward(current_start, WHITESPACE) - 1;
        for(int i = last_end + 1; i < current_start; i++){
            Serial.printf("a:%d,%d,%d\n", current_start, current_end, i);
            if (i == CHAR_TAB)
                extra_space += TAB_SIZE - i % TAB_SIZE;
            if (i == CHAR_EOL || i + extra_space - current_line_start_index > WINDOW_WIDTH)
                return std::min({i + 1, worst_case_index, content.max_index()});
        }
    }
    Serial.println("b");
    return std::min({last_end + 2, current_line_start_index + WINDOW_WIDTH, content.max_index()});
}

int TextBuffer::get_line_start_index(int content_index) {
    // will return -1 on first line, which is consistent with the return logic
    char character_list[2] = { CHAR_EOL, CHAR_NUL }; // strings are null terminated
    int last_line_end = find_first_in_backward(content_index, character_list);
    return last_line_end + 1;
}

int TextBuffer::get_line_end_index(int content_index) {
    char character_list[2] = { CHAR_EOL, CHAR_NUL }; // strings are null terminated
    int next_newline = find_first_in_forward(content_index, character_list);
    return (next_newline < 0) ? content.max_index() : next_newline;
}

int TextBuffer::get_column_offset(int content_index) {
    int column_offset = 0;
    int start_of_line = get_line_start_index(content_index);
    char character_passed;
    for (int i = start_of_line; i < content_index; i++) {
        character_passed = get_character(i);
        if (character_passed == CHAR_TAB){
            column_offset += TAB_SIZE;
        }
        else {
            column_offset++;
        }
    }
    return column_offset;
}

void TextBuffer::move_point_left_implicit() {
    if (point <= 0) { return; }
    point--;
    char character_passed = get_character(point);
    if ( character_passed == CHAR_EOL) {
        actual_line_offset--;
        actual_column_offset = get_column_offset(point);
    }
    else if ( character_passed == CHAR_TAB ) {
        actual_column_offset -= TAB_SIZE;
    } else {
        actual_column_offset--;
    }
}

void TextBuffer::move_point_right_implicit() {
    if (point > content.max_index()) { return; }
    char character_passed = get_character(point);
    point++;
    if ( character_passed == CHAR_EOL) {
        actual_line_offset++;
        actual_column_offset = 0;
    }
    else if ( character_passed == CHAR_TAB ) {
        actual_column_offset += TAB_SIZE;
        actual_column_offset -= actual_column_offset % TAB_SIZE;
    } else {
        actual_column_offset++;
    }
}

void TextBuffer::declare_column_desired() {
    desired_column_offset = actual_column_offset;
}
