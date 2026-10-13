#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "dac.h"

#if __has_include("pico/stdlib.h")
#include "pico/stdlib.h"
#endif
#if __has_include("hardware/gpio.h")
#include "hardware/gpio.h"
#endif

#include "dac.h"


// Pines de salida de datos del DAC0830
#define PIN_B0 15 
#define PIN_B1 14
#define PIN_B2 13
#define PIN_B3 12
#define PIN_B4 19
#define PIN_B5 18
#define PIN_B6 17
#define PIN_B7 16

// Pines de control del DAC0830 
#define PIN_CS   20
#define PIN_WR1  21
#define PIN_ILE  22

// Inicializar la interfaz con el DAC0830.
void dac_init(void){
    // Configurar los pines del DAC0830 como salida
    gpio_init(PIN_B0);
    gpio_init(PIN_B1);
    gpio_init(PIN_B2);
    gpio_init(PIN_B3);
    gpio_init(PIN_B4);
    gpio_init(PIN_B5);
    gpio_init(PIN_B6);
    gpio_init(PIN_B7);
    
    gpio_set_dir(PIN_B0, true);
    gpio_set_dir(PIN_B1, true); 
    gpio_set_dir(PIN_B2, true);
    gpio_set_dir(PIN_B3, true);
    gpio_set_dir(PIN_B4, true);
    gpio_set_dir(PIN_B5, true);
    gpio_set_dir(PIN_B6, true);
    gpio_set_dir(PIN_B7, true);

    // Pines de control del DAC0830
    gpio_init(PIN_CS);
    gpio_init(PIN_WR1);
    gpio_init(PIN_ILE);

    // Configurar pines de control
    gpio_set_dir(PIN_CS,  true);
    gpio_set_dir(PIN_WR1, true);
    gpio_set_dir(PIN_ILE, true);

    // Establecer estado inicial seguro en reposo
    gpio_put(PIN_ILE, 1); // ILE siempre en alto para habilitar el latch de entrada
    gpio_put(PIN_CS, 1);  // Deseleccionado (Alto)
    gpio_put(PIN_WR1, 1); // Escritura desactivada (Alto)
}

// Enviar una muestra de 8 bits al DAC0830.
void dac_write(uint8_t sample){
    // Configurar los pines de datos del DAC0830 según la muestra
    gpio_put(PIN_B0, (sample >> 0) & 0x01);
    gpio_put(PIN_B1, (sample >> 1) & 0x01);
    gpio_put(PIN_B2, (sample >> 2) & 0x01);
    gpio_put(PIN_B3, (sample >> 3) & 0x01);
    gpio_put(PIN_B4, (sample >> 4) & 0x01);
    gpio_put(PIN_B5, (sample >> 5) & 0x01);
    gpio_put(PIN_B6, (sample >> 6) & 0x01);
    gpio_put(PIN_B7, (sample >> 7) & 0x01);

    // Secuencia de tiempos para el DAC0830
    gpio_put(PIN_ILE, 1);
    gpio_put(PIN_CS, 0);

    // Esperar el establecimiento de las señales de control
    busy_wait_us_32(2);

    gpio_put(PIN_WR1, 0);

    // Mantener activo el pulso de escritura
    busy_wait_us_32(2);

    gpio_put(PIN_WR1, 1);
    gpio_put(PIN_CS, 1);
}