#pragma once

#include <stdint.h>

// Inicializar la interfaz con el DAC0830.
void dac_init(void);

// Enviar una muestra de 8 bits al DAC0830.
void dac_write(uint8_t sample);