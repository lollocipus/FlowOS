#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

void keyboard_init(void);
char keyboard_get_char(void);
bool keyboard_has_input(void);

#endif
