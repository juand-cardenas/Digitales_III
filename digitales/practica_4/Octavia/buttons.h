#pragma once

#include <stdint.h>
#include <stdbool.h>

void buttons_init(void);
bool buttons_update(void);
float buttons_get_note(void);
static int8_t buttons_read_note_raw(void);