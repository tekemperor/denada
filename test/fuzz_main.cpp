// Does dropping the mark layer actually break anything today?
//
// Without marks, TextWindow::win_start is a raw content index that nothing
// adjusts when text is inserted or deleted. The question is whether a real key
// sequence can leave it pointing somewhere that is no longer the start of a
// display line -- which would render every row wrapped from a false origin.
//
// Invariants checked after every render:
//   I1  win_start is a valid display-line start (re-deriving it changes nothing)
//   I2  the point is visible (cursor_row >= 0)
//   I3  win_start is at or before the point's display line
#include "editor.h"
#include <cstdio>
#include <random>

static const char* WORDS[] = {"the ", "quick ", "brown ", "fox ", "a ", "supercalifragilistic ",
                              "jumps ", "over ", "\n", "\t", "lazy ", "dog. ", "\n\n"};

int main() {
    std::mt19937 rng(12345);
    long long checks = 0, i1 = 0, i2 = 0, i3 = 0;

    for (int trial = 0; trial < 400; trial++) {
        Editor editor;
        // Seed with some text so there is something to scroll through.
        for (int i = 0; i < 300; i++) {
            const char* w = WORDS[rng() % 13];
            for (int j = 0; w[j]; j++) {
                EditorAction k;
                k.command = EditorCommand::INSERT_CHARACTER;
                k.character = w[j];
                editor.apply(k);
            }
        }

        for (int step = 0; step < 500; step++) {
            EditorAction action;
            int roll = rng() % 100;
            if (roll < 30) {
                action.command = EditorCommand::INSERT_CHARACTER;
                const char* w = WORDS[rng() % 13];
                action.character = w[rng() % 2 == 0 ? 0 : 0];
            } else if (roll < 40) {
                action.command = EditorCommand::DELETE_BACKWARD;
            } else if (roll < 45) {
                action.command = EditorCommand::DELETE_FORWARD;
            } else if (roll < 50) {
                action.command = EditorCommand::PASTE;
            } else if (roll < 55) {
                action.command = (rng() % 2) ? EditorCommand::CUT : EditorCommand::COPY;
            } else if (roll < 58) {
                action.command = EditorCommand::SELECT_ALL;
            } else {
                static const EditorCommand MOVES[] = {
                    EditorCommand::MOVE_LEFT, EditorCommand::MOVE_RIGHT,
                    EditorCommand::MOVE_UP, EditorCommand::MOVE_DOWN,
                    EditorCommand::MOVE_LINE_START, EditorCommand::MOVE_LINE_END,
                    EditorCommand::MOVE_PAGE_UP, EditorCommand::MOVE_PAGE_DOWN,
                    EditorCommand::MOVE_BUFFER_START, EditorCommand::MOVE_BUFFER_END};
                action.command = MOVES[rng() % 10];
                action.extend_selection = (rng() % 3 == 0);
            }
            editor.apply(action);
            editor.window.render();

            TextWindow& w = editor.window;
            checks++;
            if (w.line_start_containing(w.win_start) != w.win_start) {
                if (i1++ < 3) {
                    printf("I1 FAIL trial %d step %d: win_start=%d re-derives to %d\n",
                           trial, step, w.win_start, w.line_start_containing(w.win_start));
                }
            }
            if (w.cursor_row < 0) {
                if (i2++ < 3) {
                    printf("I2 FAIL trial %d step %d: point %d off screen, win_start=%d size=%d\n",
                           trial, step, editor.buffer()->get_point(), w.win_start,
                           editor.buffer()->content_size());
                }
            }
            if (w.win_start > w.line_start_containing(editor.buffer()->get_point())) {
                if (i3++ < 3) {
                    printf("I3 FAIL trial %d step %d: win_start=%d > point line %d\n", trial, step,
                           w.win_start, w.line_start_containing(editor.buffer()->get_point()));
                }
            }
        }
    }
    printf("\n%lld renders checked\n", checks);
    printf("I1 win_start is a valid display-line start : %lld failures\n", i1);
    printf("I2 point visible on screen                 : %lld failures\n", i2);
    printf("I3 win_start at or above the point's line   : %lld failures\n", i3);
    return 0;
}
