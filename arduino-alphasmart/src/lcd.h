#ifndef LCD_H
#define LCD_H

#include "editor.h"
#include <SerLCD.h>

void display_config(SerLCD *lcd);
void display_editor(SerLCD *lcd, Editor *editor);

// Sets all three backlight channels to `level`. Cheap to call repeatedly: it
// returns without touching I2C when the level has not changed, which matters
// because this is driven from the editor loop rather than from an event.
void display_set_backlight(SerLCD *lcd, uint8_t level);

#endif // LCD_H
