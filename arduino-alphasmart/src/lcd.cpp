#include "lcd.h"
#include <Arduino.h>

void display_config(SerLCD *lcd)
{
    lcd->disableSystemMessages();      // Remove vendor branding
    lcd->setContrast(128);             // 0-255 0 is highest contrast
    lcd->setFastBacklight(64, 64, 64); // 0-255 R,G,B
    lcd->clear();
    lcd->setCursor(0, 1);
    lcd->print(" LibreSmart DeNada");
    lcd->blink();
}

// Fills a row with spaces and copies in as much of `text` as fits.
static void pad_row(char *row_text, const char *text)
{
    for (int column = 0; column < WINDOW_WIDTH; column++)
    {
        row_text[column] = CHAR_SPC;
    }
    for (int i = 0; i < WINDOW_WIDTH && text[i] != CHAR_NUL; i++)
    {
        row_text[i] = text[i];
    }
    row_text[WINDOW_WIDTH] = CHAR_NUL;
}

void display_editor(SerLCD *lcd, Editor *editor)
{
    editor->window.render();

    // Built a row at a time and sent as one string: the SerLCD library batches
    // a buffer into chunked I2C writes, where per-character writes would be 80
    // separate transactions per frame. The old code also called clear() every
    // frame, which blanked the screen between redraws and read as flicker.
    char row_text[WINDOW_WIDTH + 1];
    for (int row = 0; row < WINDOW_HEIGHT; row++)
    {
        for (int column = 0; column < WINDOW_WIDTH; column++)
        {
            char character = editor->window.contents[row][column];
            row_text[column] = (character == CHAR_NUL) ? CHAR_SPC : character;
        }
        row_text[WINDOW_WIDTH] = CHAR_NUL;
        lcd->setCursor(0, row);
        lcd->print(row_text);
    }

    // A 20x4 character LCD has no way to highlight a range, so a selection
    // cannot be shown in place. Reporting its size on the bottom row is the
    // honest substitute, and it only costs a row while a selection exists.
    if (editor->has_status())
    {
        pad_row(row_text, editor->status_message);
        lcd->setCursor(0, WINDOW_HEIGHT - 1);
        lcd->print(row_text);
    }
    else if (editor->buffer()->has_selection())
    {
        char selection_text[WINDOW_WIDTH + 1];
        int selected = editor->buffer()->selection_end() - editor->buffer()->selection_start();
        snprintf(selection_text, sizeof(selection_text), "[%s %d selected]",
                 editor->document.active()->buffer_name, selected);
        pad_row(row_text, selection_text);
        lcd->setCursor(0, WINDOW_HEIGHT - 1);
        lcd->print(row_text);
    }

    if (editor->window.cursor_row >= 0 && editor->window.cursor_col >= 0)
    {
        lcd->setCursor(editor->window.cursor_col, editor->window.cursor_row);
        lcd->blink();
    }
    else
    {
        lcd->noBlink();
    }
}
