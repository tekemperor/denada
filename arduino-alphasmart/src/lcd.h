#ifndef LCD_H
#define LCD_H

#include "text_window.h"
#include <SerLCD.h>
void display_text(SerLCD *lcd, TextWindow *window)
{
    lcd->clear();
    window->get_window();
    char character;
    for (int row = 0; row < WINDOW_HEIGHT; row++)
    {
        lcd->setCursor(0,row);
        Serial.print("|");
        for (int col = 0; col < WINDOW_WIDTH; col++)
        {
            character = window->contents[row][col];
            if (character == CHAR_NUL)
                character = CHAR_SPC;
            lcd->write(character);
            Serial.print(character);
        }
    }
    lcd->setCursor(window->cursor_col, window->cursor_row);
    Serial.printf("\n(%d,%d)\n",window->cursor_col, window->cursor_row);
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