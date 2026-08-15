// Host tests for the DeNada editor core.
//
// Every bug these lock down was found by reading the 2022 code, and most of them
// are invisible until you are holding the device: two of the navigation helpers
// spun forever at the buffer edges, which on hardware is not a wrong answer but
// a hang.
#include "test_util.h"

#include <Arduino.h>

#include "config.h"
#include "editor.h"
#include "gap_buffer.h"
#include "keyboard_usb.h"
#include "text_buffer.h"
#include "text_document.h"
#include "text_output.h"
#include "text_window.h"

#include <string>
#include <vector>

HostSerial Serial;

// Helpers

static void type(TextBuffer &buffer, const std::string &text) {
    for (char character : text) { buffer.insert_character(character); }
}

static std::string contents_of(TextBuffer &buffer) {
    std::string text;
    for (int i = 0; i < buffer.content_size(); i++) { text += buffer.get_character(i); }
    return text;
}

// Renders one row with empty cells shown as '.', so a real trailing space is
// distinguishable from an unwritten cell.
static std::string row_text(TextWindow &window, int row) {
    std::string text;
    for (int column = 0; column < WINDOW_WIDTH; column++) {
        char character = window.contents[row][column];
        text += (character == CHAR_NUL) ? '.' : character;
    }
    return text;
}

static std::vector<int> wrap_points(TextWindow &window, TextBuffer &buffer) {
    std::vector<int> starts;
    int line = 0;
    starts.push_back(line);
    while (line < buffer.content_size()) {
        int next = window.next_line_start(line);
        if (next <= line) { break; }
        line = next;
        starts.push_back(line);
    }
    return starts;
}

// Gap buffer

static void test_gap_buffer() {
    TEST_CASE("gap buffer stores and retrieves text");
    GapBuffer gap;
    CHECK_EQ(gap.content_size(), 0);
    CHECK(gap.insert_character(0, 'a'));
    CHECK(gap.insert_character(1, 'c'));
    CHECK(gap.insert_character(1, 'b'));
    CHECK_EQ(gap.content_size(), 3);
    CHECK_EQ(gap.get_character(0), 'a');
    CHECK_EQ(gap.get_character(1), 'b');
    CHECK_EQ(gap.get_character(2), 'c');

    TEST_CASE("gap buffer refuses out-of-range deletes");
    // delete_char() used to move the gap to the end and then consume the byte
    // past the content, which is inside the gap -- a write past the live text.
    CHECK_EQ(gap.delete_char(3), CHAR_NUL);
    CHECK_EQ(gap.delete_char(-1), CHAR_NUL);
    CHECK_EQ(gap.content_size(), 3);
    CHECK_EQ(gap.delete_char(1), 'b');
    CHECK_EQ(gap.content_size(), 2);
    CHECK_EQ(gap.get_character(1), 'c');

    TEST_CASE("gap buffer reports fullness instead of dropping silently");
    GapBuffer full;
    int capacity = full.capacity();
    for (int i = 0; i < capacity; i++) { CHECK(full.insert_character(i, 'x')); }
    CHECK(full.is_full());
    CHECK_EQ(full.insert_character(capacity, 'y'), false);
    CHECK_EQ(full.content_size(), capacity);
}

// Text buffer

static void test_text_buffer_editing() {
    TEST_CASE("text buffer inserts at the point");
    TextBuffer buffer;
    type(buffer, "hello");
    CHECK_EQ(contents_of(buffer), "hello");
    CHECK_EQ(buffer.get_point(), 5);

    TEST_CASE("backspace deletes behind the point");
    buffer.delete_before_point(2);
    CHECK_EQ(contents_of(buffer), "hel");
    CHECK_EQ(buffer.get_point(), 3);

    TEST_CASE("backspace at the start of the buffer is a no-op, not a crash");
    buffer.set_point(0);
    CHECK_EQ(buffer.delete_before_point(1), false);
    CHECK_EQ(contents_of(buffer), "hel");
    CHECK_EQ(buffer.get_point(), 0);

    TEST_CASE("forward delete at the end of the buffer is a no-op");
    buffer.move_point_to_buffer_end();
    CHECK_EQ(buffer.delete_after_point(1), false);
    CHECK_EQ(contents_of(buffer), "hel");

    TEST_CASE("forward delete removes the character at the point");
    buffer.set_point(0);
    CHECK(buffer.delete_after_point(1));
    CHECK_EQ(contents_of(buffer), "el");
    CHECK_EQ(buffer.get_point(), 0);
}

static void test_text_buffer_lines() {
    TextBuffer buffer;
    type(buffer, "one\ntwo\nthree");

    TEST_CASE("logical line boundaries");
    CHECK_EQ(buffer.get_line_start_index(0), 0);
    CHECK_EQ(buffer.get_line_start_index(3), 0);
    CHECK_EQ(buffer.get_line_start_index(4), 4);
    CHECK_EQ(buffer.get_line_start_index(6), 4);
    CHECK_EQ(buffer.get_line_end_index(0), 3);
    CHECK_EQ(buffer.get_line_end_index(5), 7);

    TEST_CASE("end of the last line is the end of the buffer, not one short");
    // get_line_end_index() used to return max_index(), which made the final
    // character of the buffer unreachable by End.
    CHECK_EQ(buffer.get_line_end_index(9), 13);
    buffer.set_point(9);
    buffer.move_point_to_line_end();
    CHECK_EQ(buffer.get_point(), 13);

    TEST_CASE("line and column numbers");
    buffer.set_point(5);
    CHECK_EQ(buffer.get_line_number(), 2);
    CHECK_EQ(buffer.get_column_number(), 2);
}

static void test_tab_columns() {
    TEST_CASE("a tab advances to the next tab stop, consistently");
    // Three functions used to disagree about how wide a tab is: one added
    // TAB_SIZE, one snapped to a stop, one subtracted a flat TAB_SIZE. Any
    // disagreement desynced the cursor for the rest of the session.
    TextBuffer buffer;
    type(buffer, "a\tb");
    CHECK_EQ(buffer.get_column_offset(0), 0);
    CHECK_EQ(buffer.get_column_offset(1), 1);
    CHECK_EQ(buffer.get_column_offset(2), TAB_SIZE);
    CHECK_EQ(buffer.get_column_offset(3), TAB_SIZE + 1);

    TEST_CASE("a tab at a tab stop advances a full tab");
    TextBuffer aligned;
    type(aligned, "\t");
    CHECK_EQ(aligned.get_column_offset(1), TAB_SIZE);
}

// Word wrap

static void test_word_wrap() {
    TextBuffer buffer;
    TextWindow window(&buffer);
    type(buffer, "The quick brown fox jumps over the lazy dog");

    TEST_CASE("wrap breaks at word boundaries, not mid-word");
    std::vector<int> starts = wrap_points(window, buffer);
    CHECK_EQ((int)starts.size(), 4);
    CHECK_EQ(starts[0], 0);
    CHECK_EQ(starts[1], 20);
    CHECK_EQ(starts[2], 40);
    CHECK_EQ(starts[3], 43);  // the end sentinel: "dog" runs 40..42

    TEST_CASE("wrapped rows render as whole words");
    buffer.set_point(0);
    window.render();
    CHECK_EQ(row_text(window, 0), "The quick brown fox ");
    CHECK_EQ(row_text(window, 1), "jumps over the lazy ");
    CHECK_EQ(row_text(window, 2), "dog.................");
    CHECK_EQ(row_text(window, 3), "....................");
}

static void test_wrap_edge_cases() {
    TEST_CASE("a word longer than the window hard-breaks so wrap still advances");
    TextBuffer buffer;
    TextWindow window(&buffer);
    type(buffer, "supercalifragilisticexpialidocious");
    CHECK_EQ(window.next_line_start(0), WINDOW_WIDTH);
    CHECK_EQ(window.next_line_start(WINDOW_WIDTH), 34);

    TEST_CASE("newlines start a new display line");
    TextBuffer lines;
    TextWindow line_window(&lines);
    type(lines, "a\nb\nc");
    CHECK_EQ(line_window.next_line_start(0), 2);
    CHECK_EQ(line_window.next_line_start(2), 4);
    CHECK_EQ(line_window.next_line_start(4), 5);

    TEST_CASE("an empty buffer has exactly one display line");
    TextBuffer empty;
    TextWindow empty_window(&empty);
    CHECK_EQ(empty_window.next_line_start(0), 0);
    CHECK_EQ(empty_window.line_start_containing(0), 0);
    CHECK_EQ(empty_window.prev_line_start(0), 0);

    TEST_CASE("a trailing newline opens a real final line");
    TextBuffer trailing;
    TextWindow trailing_window(&trailing);
    type(trailing, "hi\n");
    CHECK_EQ(trailing_window.line_start_containing(3), 3);
    trailing.move_point_to_buffer_end();
    trailing_window.render();
    CHECK_EQ(trailing_window.cursor_row, 1);
    CHECK_EQ(trailing_window.cursor_col, 0);

    TEST_CASE("finding the line containing a position agrees with walking forward");
    TextBuffer prose;
    TextWindow prose_window(&prose);
    type(prose, "The quick brown fox jumps over the lazy dog");
    CHECK_EQ(prose_window.line_start_containing(0), 0);
    CHECK_EQ(prose_window.line_start_containing(19), 0);
    CHECK_EQ(prose_window.line_start_containing(20), 20);
    CHECK_EQ(prose_window.line_start_containing(39), 20);
    CHECK_EQ(prose_window.line_start_containing(40), 40);
    CHECK_EQ(prose_window.prev_line_start(40), 20);
    CHECK_EQ(prose_window.prev_line_start(20), 0);
}

// Vertical movement

static void test_vertical_movement() {
    TextBuffer buffer;
    TextWindow window(&buffer);
    type(buffer, "hello\nworld\nagain");

    TEST_CASE("down then up returns to the original column");
    buffer.set_point(3);  // "hel|lo"
    window.declare_column_desired();
    window.move_down();
    CHECK_EQ(buffer.get_point(), 9);  // "wor|ld"
    window.move_up();
    CHECK_EQ(buffer.get_point(), 3);

    TEST_CASE("move_down lands at the desired column, not the end of the line");
    // The 2022 move_point_down() jumped straight to get_line_end_index() of the
    // next line, so down-arrow always slammed the cursor to end of line.
    buffer.set_point(0);
    window.declare_column_desired();
    window.move_down();
    CHECK_EQ(buffer.get_point(), 6);

    TEST_CASE("the desired column survives a short intervening line");
    TextBuffer ragged;
    TextWindow ragged_window(&ragged);
    type(ragged, "longest line\nab\nlongest line");
    ragged.set_point(10);  // column 10 on line 1
    ragged_window.declare_column_desired();
    ragged_window.move_down();
    CHECK_EQ(ragged.get_point(), 15);  // clamped to the end of "ab"
    ragged_window.move_down();
    CHECK_EQ(ragged.get_point(), 26);  // back out to column 10
}

static void test_navigation_edges() {
    TEST_CASE("moving up on the first line terminates");
    // window_move_point_up() used to spin forever here: it looped until the
    // column hit a multiple of the width, while the underlying move was a no-op
    // at index 0. On the device that is a hang, not a wrong cursor.
    TextBuffer buffer;
    TextWindow window(&buffer);
    type(buffer, "abc");
    buffer.set_point(1);
    window.declare_column_desired();
    window.move_up();
    CHECK_EQ(buffer.get_point(), 0);
    window.move_up();
    CHECK_EQ(buffer.get_point(), 0);

    TEST_CASE("moving down on the last line terminates");
    buffer.move_point_to_buffer_end();
    window.declare_column_desired();
    window.move_down();
    CHECK_EQ(buffer.get_point(), 3);
    window.move_down();
    CHECK_EQ(buffer.get_point(), 3);

    TEST_CASE("paging past either end terminates");
    window.move_page_up();
    CHECK_EQ(buffer.get_point(), 0);
    window.move_page_down();
    CHECK_EQ(buffer.get_point(), 3);

    TEST_CASE("moving up and down in an empty buffer terminates");
    TextBuffer empty;
    TextWindow empty_window(&empty);
    empty_window.move_up();
    empty_window.move_down();
    empty_window.move_page_up();
    empty_window.move_page_down();
    CHECK_EQ(empty.get_point(), 0);

    TEST_CASE("home and end work on the display line, not the logical line");
    TextBuffer prose;
    TextWindow prose_window(&prose);
    type(prose, "The quick brown fox jumps over the lazy dog");
    prose.set_point(25);
    prose_window.move_to_display_line_start();
    CHECK_EQ(prose.get_point(), 20);
    prose_window.move_to_display_line_end();
    CHECK_EQ(prose.get_point(), 39);
}

// Scrolling

static void test_scrolling() {
    TextBuffer buffer;
    TextWindow window(&buffer);
    type(buffer, "one\ntwo\nthree\nfour\nfive\nsix");

    TEST_CASE("the window follows the point downward");
    // The 2022 update_window() only ever scrolled forward one line per redraw
    // and never followed the point; the commit message was "not following
    // point".
    buffer.move_point_to_buffer_end();
    window.render();
    CHECK(window.cursor_row >= 0);
    CHECK_EQ(row_text(window, WINDOW_HEIGHT - 1), "six.................");

    TEST_CASE("the window follows the point back up");
    buffer.move_point_to_buffer_start();
    window.render();
    CHECK_EQ(window.win_start, 0);
    CHECK_EQ(window.cursor_row, 0);
    CHECK_EQ(window.cursor_col, 0);
    CHECK_EQ(row_text(window, 0), "one.................");

    TEST_CASE("the cursor is always on screen after a render");
    for (int point = 0; point <= buffer.content_size(); point++) {
        buffer.set_point(point);
        window.render();
        if (window.cursor_row < 0 || window.cursor_col < 0) {
            std::printf("       point %d rendered off screen\n", point);
        }
        CHECK(window.cursor_row >= 0);
        CHECK(window.cursor_row < WINDOW_HEIGHT);
        CHECK(window.cursor_col >= 0);
        CHECK(window.cursor_col < WINDOW_WIDTH);
    }
}

static void test_tab_rendering() {
    TEST_CASE("tabs render as spaces out to the next tab stop");
    TextBuffer buffer;
    TextWindow window(&buffer);
    type(buffer, "a\tb");
    buffer.set_point(0);
    window.render();
    CHECK_EQ(row_text(window, 0), "a   b...............");
}

// Keyboard translation

// Builds an 8-byte HID boot-protocol report: modifiers, reserved, then keys.
static void make_report(uint8_t *report, uint8_t modifiers, uint8_t key) {
    for (int i = 0; i < KBDINFO_SIZE; i++) { report[i] = KEY_NONE; }
    report[KBDINFO_MODIFIERS_INDEX] = modifiers;
    report[KBDINFO_FIRST_KEY_INDEX] = key;
}

static EditorAction single_action(KeyboardInputHandler &keyboard, uint8_t modifiers, uint8_t key) {
    uint8_t report[KBDINFO_SIZE];
    EditorAction actions[KEYBOARD_MAX_ACTIONS];
    make_report(report, modifiers, key);
    int count = keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS);
    // Release everything, so the next press is seen as a new one.
    uint8_t released[KBDINFO_SIZE];
    make_report(released, 0, KEY_NONE);
    EditorAction ignored[KEYBOARD_MAX_ACTIONS];
    keyboard.parse(released, ignored, KEYBOARD_MAX_ACTIONS);
    if (count < 1) { return EditorAction(); }
    return actions[0];
}

static void test_keyboard_translation() {
    KeyboardInputHandler keyboard;

    TEST_CASE("printable keys insert characters");
    EditorAction action = single_action(keyboard, 0, KEY_A);
    CHECK(action.command == EditorCommand::INSERT_CHARACTER);
    CHECK_EQ(action.character, 'a');
    action = single_action(keyboard, KEY_MOD_LSHIFT, KEY_A);
    CHECK_EQ(action.character, 'A');

    TEST_CASE("enter and tab insert their characters");
    CHECK_EQ(single_action(keyboard, 0, KEY_ENTER).character, CHAR_EOL);
    CHECK_EQ(single_action(keyboard, 0, KEY_TAB).character, CHAR_TAB);

    TEST_CASE("arrow keys produce movement, which they never did before");
    // The 2022 parser handled printable characters and backspace and nothing
    // else, so every arrow key was silently discarded.
    CHECK(single_action(keyboard, 0, KEY_LEFT).command == EditorCommand::MOVE_LEFT);
    CHECK(single_action(keyboard, 0, KEY_RIGHT).command == EditorCommand::MOVE_RIGHT);
    CHECK(single_action(keyboard, 0, KEY_UP).command == EditorCommand::MOVE_UP);
    CHECK(single_action(keyboard, 0, KEY_DOWN).command == EditorCommand::MOVE_DOWN);
    CHECK(single_action(keyboard, 0, KEY_HOME).command == EditorCommand::MOVE_LINE_START);
    CHECK(single_action(keyboard, 0, KEY_END).command == EditorCommand::MOVE_LINE_END);
    CHECK(single_action(keyboard, 0, KEY_PAGEUP).command == EditorCommand::MOVE_PAGE_UP);
    CHECK(single_action(keyboard, 0, KEY_PAGEDOWN).command == EditorCommand::MOVE_PAGE_DOWN);

    TEST_CASE("deletion keys");
    CHECK(single_action(keyboard, 0, KEY_BACKSPACE).command == EditorCommand::DELETE_BACKWARD);
    CHECK(single_action(keyboard, 0, KEY_DELETE).command == EditorCommand::DELETE_FORWARD);

    TEST_CASE("shift turns movement into selection");
    action = single_action(keyboard, KEY_MOD_LSHIFT, KEY_RIGHT);
    CHECK(action.command == EditorCommand::MOVE_RIGHT);
    CHECK(action.extend_selection);
    CHECK_EQ(single_action(keyboard, 0, KEY_RIGHT).extend_selection, false);

    TEST_CASE("control plus home/end reaches the ends of the buffer");
    CHECK(single_action(keyboard, KEY_MOD_LCTRL, KEY_HOME).command ==
          EditorCommand::MOVE_BUFFER_START);
    CHECK(single_action(keyboard, KEY_MOD_LCTRL, KEY_END).command ==
          EditorCommand::MOVE_BUFFER_END);

    TEST_CASE("clipboard chords");
    CHECK(single_action(keyboard, KEY_MOD_LCTRL, KEY_C).command == EditorCommand::COPY);
    CHECK(single_action(keyboard, KEY_MOD_LCTRL, KEY_X).command == EditorCommand::CUT);
    CHECK(single_action(keyboard, KEY_MOD_LCTRL, KEY_V).command == EditorCommand::PASTE);
    CHECK(single_action(keyboard, KEY_MOD_LCTRL, KEY_A).command == EditorCommand::SELECT_ALL);

    TEST_CASE("an unbound control chord types nothing");
    CHECK(single_action(keyboard, KEY_MOD_LCTRL, KEY_Z).command == EditorCommand::NONE);

    TEST_CASE("F1 through F8 select buffers, F9 sends");
    for (int i = 0; i < BUFFER_COUNT; i++) {
        action = single_action(keyboard, 0, KEY_F1 + i);
        CHECK(action.command == EditorCommand::SWITCH_BUFFER);
        CHECK_EQ(action.index, i);
    }
    CHECK(single_action(keyboard, 0, KEY_F9).command == EditorCommand::SEND_BUFFER);
}

static void test_keyboard_reports() {
    KeyboardInputHandler keyboard;
    uint8_t report[KBDINFO_SIZE];
    EditorAction actions[KEYBOARD_MAX_ACTIONS];

    TEST_CASE("a held key does not re-fire on every report");
    make_report(report, 0, KEY_A);
    CHECK_EQ(keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS), 1);
    CHECK_EQ(keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS), 0);
    CHECK_EQ(keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS), 0);

    TEST_CASE("releasing and pressing again fires once more");
    make_report(report, 0, KEY_NONE);
    CHECK_EQ(keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS), 0);
    make_report(report, 0, KEY_A);
    CHECK_EQ(keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS), 1);

    TEST_CASE("a rollover report is ignored rather than decoded as garbage");
    make_report(report, 0, KEY_ERR_OVF);
    CHECK_EQ(keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS), 0);

    TEST_CASE("held key is reported for auto-repeat");
    keyboard.reset();
    make_report(report, 0, KEY_BACKSPACE);
    keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS);
    CHECK_EQ(keyboard.held_key(), KEY_BACKSPACE);
    make_report(report, 0, KEY_NONE);
    keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS);
    CHECK_EQ(keyboard.held_key(), KEY_NONE);

    TEST_CASE("two keys pressed in one report both fire");
    keyboard.reset();
    make_report(report, 0, KEY_A);
    report[KBDINFO_FIRST_KEY_INDEX + 1] = KEY_B;
    CHECK_EQ(keyboard.parse(report, actions, KEYBOARD_MAX_ACTIONS), 2);
}

// Editor: selection and clipboard

static EditorAction command_action(EditorCommand command, bool extend = false) {
    EditorAction action;
    action.command = command;
    action.extend_selection = extend;
    return action;
}

static EditorAction insert_action(char character) {
    EditorAction action;
    action.command = EditorCommand::INSERT_CHARACTER;
    action.character = character;
    return action;
}

static void type_into(Editor &editor, const std::string &text) {
    for (char character : text) { editor.apply(insert_action(character)); }
}

static void test_selection() {
    Editor editor;
    type_into(editor, "hello world");

    TEST_CASE("shift plus movement builds a selection");
    editor.buffer()->set_point(0);
    editor.apply(command_action(EditorCommand::MOVE_RIGHT, true));
    editor.apply(command_action(EditorCommand::MOVE_RIGHT, true));
    editor.apply(command_action(EditorCommand::MOVE_RIGHT, true));
    editor.apply(command_action(EditorCommand::MOVE_RIGHT, true));
    editor.apply(command_action(EditorCommand::MOVE_RIGHT, true));
    CHECK(editor.buffer()->has_selection());
    CHECK_EQ(editor.buffer()->selection_start(), 0);
    CHECK_EQ(editor.buffer()->selection_end(), 5);

    TEST_CASE("plain movement drops the selection");
    editor.apply(command_action(EditorCommand::MOVE_RIGHT));
    CHECK_EQ(editor.buffer()->has_selection(), false);

    TEST_CASE("copy then paste duplicates the selected run");
    editor.buffer()->set_point(0);
    for (int i = 0; i < 5; i++) { editor.apply(command_action(EditorCommand::MOVE_RIGHT, true)); }
    editor.apply(command_action(EditorCommand::COPY));
    editor.buffer()->move_point_to_buffer_end();
    editor.buffer()->clear_anchor();
    editor.apply(command_action(EditorCommand::PASTE));
    CHECK_EQ(contents_of(*editor.buffer()), "hello worldhello");

    TEST_CASE("cut removes the selection and keeps it on the clipboard");
    Editor cutter;
    type_into(cutter, "abcdef");
    cutter.buffer()->set_point(0);
    for (int i = 0; i < 3; i++) { cutter.apply(command_action(EditorCommand::MOVE_RIGHT, true)); }
    cutter.apply(command_action(EditorCommand::CUT));
    CHECK_EQ(contents_of(*cutter.buffer()), "def");
    CHECK_EQ(cutter.buffer()->get_point(), 0);
    cutter.buffer()->move_point_to_buffer_end();
    cutter.apply(command_action(EditorCommand::PASTE));
    CHECK_EQ(contents_of(*cutter.buffer()), "defabc");

    TEST_CASE("typing over a selection replaces it");
    Editor typer;
    type_into(typer, "abcdef");
    typer.buffer()->set_point(0);
    for (int i = 0; i < 3; i++) { typer.apply(command_action(EditorCommand::MOVE_RIGHT, true)); }
    typer.apply(insert_action('X'));
    CHECK_EQ(contents_of(*typer.buffer()), "Xdef");

    TEST_CASE("backspace over a selection deletes the whole selection");
    Editor deleter;
    type_into(deleter, "abcdef");
    deleter.buffer()->set_point(0);
    for (int i = 0; i < 3; i++) { deleter.apply(command_action(EditorCommand::MOVE_RIGHT, true)); }
    deleter.apply(command_action(EditorCommand::DELETE_BACKWARD));
    CHECK_EQ(contents_of(*deleter.buffer()), "def");

    TEST_CASE("select all covers the buffer");
    Editor selector;
    type_into(selector, "one\ntwo");
    selector.apply(command_action(EditorCommand::SELECT_ALL));
    CHECK_EQ(selector.buffer()->selection_start(), 0);
    CHECK_EQ(selector.buffer()->selection_end(), 7);

    TEST_CASE("copying with no selection reports rather than silently doing nothing");
    Editor quiet;
    type_into(quiet, "abc");
    quiet.apply(command_action(EditorCommand::COPY));
    CHECK_EQ(std::string(quiet.status_message), "NO SELECTION");
}

static void test_buffers() {
    Editor editor;

    TEST_CASE("the eight buffers hold independent text");
    for (int i = 0; i < BUFFER_COUNT; i++) {
        EditorAction action;
        action.command = EditorCommand::SWITCH_BUFFER;
        action.index = i;
        editor.apply(action);
        type_into(editor, "buffer " + std::to_string(i));
    }
    for (int i = 0; i < BUFFER_COUNT; i++) {
        EditorAction action;
        action.command = EditorCommand::SWITCH_BUFFER;
        action.index = i;
        editor.apply(action);
        CHECK_EQ(contents_of(*editor.buffer()), "buffer " + std::to_string(i));
    }

    TEST_CASE("switching buffers preserves each point");
    EditorAction to_first;
    to_first.command = EditorCommand::SWITCH_BUFFER;
    to_first.index = 0;
    editor.apply(to_first);
    editor.buffer()->set_point(3);
    EditorAction to_second;
    to_second.command = EditorCommand::SWITCH_BUFFER;
    to_second.index = 1;
    editor.apply(to_second);
    editor.apply(to_first);
    CHECK_EQ(editor.buffer()->get_point(), 3);

    TEST_CASE("the clipboard carries text between buffers");
    editor.apply(command_action(EditorCommand::SELECT_ALL));
    editor.apply(command_action(EditorCommand::COPY));
    editor.apply(to_second);
    editor.buffer()->move_point_to_buffer_end();
    editor.apply(command_action(EditorCommand::PASTE));
    CHECK_EQ(contents_of(*editor.buffer()), "buffer 1buffer 0");

    TEST_CASE("an out-of-range buffer index is ignored");
    EditorAction bad;
    bad.command = EditorCommand::SWITCH_BUFFER;
    bad.index = BUFFER_COUNT;
    editor.apply(bad);
    CHECK_EQ(editor.document.get_active_index(), 1);
}

static void test_buffer_full_is_reported() {
    TEST_CASE("a full buffer says so instead of dropping keystrokes silently");
    // insert_character() used to return a bool that TextBuffer checked and then
    // discarded, so a full buffer looked exactly like a working one.
    Editor editor;
    int capacity = editor.buffer()->content.capacity();
    for (int i = 0; i < capacity; i++) { editor.apply(insert_action('x')); }
    CHECK_EQ(editor.buffer()->content_size(), capacity);
    CHECK_EQ(editor.has_status(), false);
    editor.apply(insert_action('y'));
    CHECK_EQ(std::string(editor.status_message), "BUFFER FULL");
    CHECK_EQ(editor.buffer()->content_size(), capacity);
}

// Persistence

static void test_persistence_round_trip() {
    TEST_CASE("a saved buffer comes back byte for byte");
    // This is the record that goes to flash. The on-device TextStore only adds
    // the file I/O around it.
    TextBuffer original;
    type(original, "The quick brown fox\njumps over the lazy dog");
    original.set_point(10);
    // Move the gap to the point, the way an edit there would.
    original.insert_character('!');
    original.delete_before_point(1);

    std::vector<uint8_t> saved(TextBuffer::raw_size());
    const uint8_t *bytes = original.raw_bytes();
    for (int i = 0; i < TextBuffer::raw_size(); i++) { saved[i] = bytes[i]; }

    TextBuffer restored;
    CHECK(restored.load_raw(saved.data(), (int)saved.size()));
    CHECK_EQ(contents_of(restored), contents_of(original));
    CHECK_EQ(restored.content_size(), original.content_size());

    TEST_CASE("the restored point is where the writer left off");
    CHECK_EQ(restored.get_point(), 10);

    TEST_CASE("a restored buffer starts unmodified, so it is not re-saved");
    CHECK_EQ(restored.is_modified, false);

    TEST_CASE("a record from a different format version is refused, not shown");
    // Reading an incompatible record as text would put convincing garbage on
    // screen and then save it back over the real thing.
    std::vector<uint8_t> wrong_version = saved;
    wrong_version[0] = (uint8_t)(GAP_BUFFER_VERSION + 7);
    TextBuffer rejected;
    CHECK_EQ(rejected.load_raw(wrong_version.data(), (int)wrong_version.size()), false);
    CHECK_EQ(rejected.content_size(), 0);

    TEST_CASE("a truncated record is refused");
    TextBuffer truncated;
    CHECK_EQ(truncated.load_raw(saved.data(), (int)saved.size() - 1), false);
    CHECK_EQ(truncated.content_size(), 0);

    TEST_CASE("a record with impossible gap bounds is refused");
    std::vector<uint8_t> corrupt = saved;
    corrupt[4] = 0xFF;  // gap_start low byte
    corrupt[5] = 0xFF;  // gap_start high byte
    TextBuffer bad_gap;
    CHECK_EQ(bad_gap.load_raw(corrupt.data(), (int)corrupt.size()), false);
    CHECK_EQ(bad_gap.content_size(), 0);

    TEST_CASE("an empty buffer round-trips as empty");
    TextBuffer blank;
    std::vector<uint8_t> blank_bytes(TextBuffer::raw_size());
    const uint8_t *blank_raw = blank.raw_bytes();
    for (int i = 0; i < TextBuffer::raw_size(); i++) { blank_bytes[i] = blank_raw[i]; }
    TextBuffer blank_restored;
    type(blank_restored, "leftovers");
    CHECK(blank_restored.load_raw(blank_bytes.data(), (int)blank_bytes.size()));
    CHECK_EQ(blank_restored.content_size(), 0);
}

// Send

// Stands in for a transport so the send path can be tested with no hardware.
class RecordingTextOutput : public TextOutput {
public:
    std::string sent;
    bool began = false;
    bool ended = false;
    bool allow_begin = true;

    bool begin() override {
        began = true;
        return allow_begin;
    }
    void send_character(char character) override { sent += character; }
    void end() override { ended = true; }
};

static void test_send_buffer() {
    TEST_CASE("sending walks the whole buffer through the transport");
    TextBuffer buffer;
    type(buffer, "hello\nworld");
    RecordingTextOutput output;
    int sent = send_text(output, buffer);
    CHECK_EQ(sent, 11);
    CHECK_EQ(output.sent, "hello\nworld");
    CHECK(output.began);
    CHECK(output.ended);

    TEST_CASE("sending reads across the gap, not just the first run");
    // The gap sits wherever the cursor is, so a send from mid-document is the
    // case where a naive memcpy of the backing array would emit the gap.
    TextBuffer split;
    type(split, "abcdef");
    split.set_point(3);
    split.insert_character('-');
    RecordingTextOutput split_output;
    send_text(split_output, split);
    CHECK_EQ(split_output.sent, "abc-def");

    TEST_CASE("a transport that cannot start sends nothing");
    RecordingTextOutput refusing;
    refusing.allow_begin = false;
    CHECK_EQ(send_text(refusing, buffer), 0);
    CHECK_EQ(refusing.sent, "");
    CHECK_EQ(refusing.ended, false);

    TEST_CASE("sending an empty buffer is a no-op that still closes cleanly");
    TextBuffer empty;
    RecordingTextOutput empty_output;
    CHECK_EQ(send_text(empty_output, empty), 0);
    CHECK(empty_output.ended);
}

int main() {
    std::printf("DeNada editor core tests\n\n");
    test_gap_buffer();
    test_text_buffer_editing();
    test_text_buffer_lines();
    test_tab_columns();
    test_word_wrap();
    test_wrap_edge_cases();
    test_vertical_movement();
    test_navigation_edges();
    test_scrolling();
    test_tab_rendering();
    test_keyboard_translation();
    test_keyboard_reports();
    test_selection();
    test_buffers();
    test_buffer_full_is_reported();
    test_persistence_round_trip();
    test_send_buffer();
    return testing::summarize();
}
