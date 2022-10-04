#ifndef LCD_H
#define LCD_H

#include "text_window.h"
#include <SerLCD.h>
void display_text(SerLCD *lcd, TextWindow *window)
{
    lcd->clear();
    window->update_window();
    char character;
    Serial.printf("(%d,%d):",window->cursor_row, window->cursor_col);
    for (int row = 0; row < WINDOW_HEIGHT; row++)
    {
        lcd->setCursor(0,row);
        for (int col = 0; col < WINDOW_WIDTH; col++)
        {
            character = window->contents[row][col];
            if (character == CHAR_NUL)
                character = CHAR_SPC;
            lcd->write(character);
            Serial.print(character);
        }
        Serial.print("|");
    }
    lcd->setCursor(window->cursor_col, window->cursor_row);
    Serial.printf("\n");
}
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

#endif // LCD_H