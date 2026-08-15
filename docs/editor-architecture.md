# Editor architecture: buffer, window, and the missing mark layer

Written 2026-08-15T21:02:37Z, after Brian asked why v0.2.0 had consolidated
buffering with windowing when they had been deliberately separate.

The reference this project follows is Craig Finseth, *The Craft of Text
Editing* — cited in `src/gap_buffer.h` and worth reading before changing
anything here.

## What v0.2.0 moved, in both directions

Two opposite things happened, which is why the layering reads as murky.

**Split further, and correctly.** The 2022 `TextBuffer` carried
`window_move_point_up(int)`, `window_move_point_page_down(int,int)`,
`get_next_window_line_start_index(int)` and private `window_line_offset` /
`window_column_offset` — the buffer knew the screen width. That had to go: it
computed wrapping as column-offset modulo width, which is false once lines wrap
at word boundaries. Logical lines belong to `TextBuffer`, display lines to
`TextWindow`. Keep that.

**Merged, and this is the real answer.** `TextWindow` now calls
`buffer->set_point()`. It is not a view over a buffer; it owns vertical cursor
motion. And `text_mark.{h,cpp}` was deleted.

## Marks were the separation

In Finseth's model the decoupler is not "wrapping lives over there." It is that
every position is a **mark** the buffer maintains: the point is a mark, the
selection anchor is a mark, the window's top-of-screen is a mark. The buffer
adjusts every mark on every insert and delete, so the window never needs to know
that an edit happened and the buffer never needs to know a window exists.

Delete marks and `win_start` becomes a raw `int` that the window must re-derive
by walking the text itself. That is the mechanism by which the two layers got
welded together.

In fairness to the deletion: nothing ever included `text_mark.h` — it was
scaffolding, never wired in. But it was the right scaffolding, and it was
removed without a replacement.

## Two defects that follow from it

Both found by fuzzing 200,000 renders of the shipped source against three
invariants, then narrowed to deterministic reproductions.

### 1. `win_start` goes stale after an edit above the window

Any deletion that begins above the top of the screen shifts every later index.
Nothing adjusts `win_start`, so the top of the screen silently moves to a
position that is no longer a display-line start, and every row renders wrapped
from a false origin:

```
rendered:                re-derived correctly:
|ha bravo charlie    |   |alpha bravo charlie |
|alpha bravo charlie |   |alpha bravo charlie |
```

Rate: 152 bad renders in 200,000. The key-reachable path the fuzzer found is a
selection extending upward past the top of the screen, followed by Paste — Cut
happens to self-correct because the point lands above `win_start` and forces a
re-scroll, while Paste can leave the point back inside the stale window.

A mark makes this failure structurally impossible. The selection `anchor` has
the same missing adjustment; it is latent only because every current edit path
happens to clear the selection first.

### 2. The cursor vanishes at exactly WINDOW_WIDTH characters

Type 20 characters on a line with no newline and `cursor_row` becomes -1, so
`lcd.cpp` calls `noBlink()` and the blinking cursor disappears until the 21st
character, when it reappears at row 1 column 1.

```
19 chars typed -> cursor_row= 0 cursor_col=19
20 chars typed -> cursor_row=-1 cursor_col=-1   <-- cursor not drawn
21 chars typed -> cursor_row= 1 cursor_col= 1
```

Root cause is `is_line_start_valid()` in `src/text_window.cpp`: it treats a
position at end-of-content as a line start only when a newline put it there.
That is right for hard lines and wrong for a soft-wrapped line that is exactly
full — where the position after the last character *is* the start of the next
display line. `last_line_start()` therefore returns the wrong line,
`render()` breaks out of its row loop early, and the point has no row to land
on. The correct output for 20 characters is row 1, column 0.

**This is unfixed as of this writing** and is present on shipped v0.2.0.

## Redisplay cost, measured

Character reads (calls to `GapBuffer::get_character`) per keystroke, counting
`editor.apply()` plus `window.render()`, which is what the firmware does per key:

| doc size | one unbroken paragraph | newline every 400 | every 80 |
|---|---|---|---|
| 200 | 3,608 | 3,608 | 1,732 |
| 2,000 | 25,079 | 1,247 | 1,235 |
| 7,000 | **84,351** | 3,587 | 1,710 |

So the cost is O(paragraph), not O(document) — flat as soon as the writer presses
Enter occasionally. Breakdown at 7,000 characters unbroken: `declare_column_desired`
45,470, `render` 35,190 (of which `last_line_start()` 17,496 and
`scroll_to_point()` 17,559).

Finseth's actual efficiency trick is not the gap buffer — it is **incremental
redisplay**: keep the previous screen and the previous line-start positions,
recompute only what changed, and send only the changed cells. This code does
neither: it re-derives line starts from scratch every frame and repaints all 80
cells over I²C on every keystroke. Cached line starts have to stay valid across
edits, which is another thing marks would provide.

Note that this is a **responsiveness and headroom** concern, not a battery one —
see `power.md` for why doing less work with a CPU that cannot downclock or sleep
saves almost no energy.

## Recommendation

Restore marks as a small fixed array inside `TextBuffer`, not Finseth's
malloc'd linked list. `win_start` is a position that must survive edits it
cannot see, and that is exactly and only what a mark is for.

Smallest version that pays: `int marks[4]` in `TextBuffer`; `insert` and
`delete_range` adjust every mark at or after the edit; `win_start` and `anchor`
become mark ids. Roughly twenty lines.

Do **not** move vertical motion back into `TextBuffer`. That re-imports screen
width into the buffer, which is what broke word wrap the first time. The window
owning point motion is fine; the window owning a raw index is not.

Suggested order: fix the cursor bug first (small, user-visible, already
shipped), then marks, then incremental redisplay only if a measurement says it
matters.
