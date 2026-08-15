#ifndef LCD_H
#define LCD_H

#include "editor.h"
#include <SerLCD.h>

void display_config(SerLCD *lcd);
void display_editor(SerLCD *lcd, Editor *editor);

#endif // LCD_H
