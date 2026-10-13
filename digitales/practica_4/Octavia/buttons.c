#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "buttons.h"
#include <math.h>

#if __has_include("pico/stdlib.h")
#include "pico/stdlib.h"
#endif
#if __has_include("hardware/gpio.h")
#include "hardware/gpio.h"
#endif

// Pines de salida de datos del DAC0830
#define PIN_DO 1
#define PIN_RE 2
#define PIN_MI 3
#define PIN_FA 4
#define PIN_SOL 5
#define PIN_LA 6
#define PIN_SI 7

#define PIN_OC1 8
#define PIN_OC2 10
#define PIN_OC3 11

// Constante especial para indicar que no hay ninguna tecla presionada
#define NOTA_NINGUNA -128
#define TIEMPO_DEBOUNCE_MS 10  // Tiempo de espera

static uint8_t octava_referencia     = 0; // Estado anterior de los botones

// Variable para el antirrebote real
static uint32_t tiempo_cambio_nota = 0;

// Variables para almacenar las lecturas 
static int8_t nota_candidata = NOTA_NINGUNA;
static uint8_t octava_candidata = 4;

// Variable para almacenar la nota validada después del antirebote
static int8_t nota_actual = NOTA_NINGUNA;

void buttons_init(void) 
    {
    // Inicializar los pines de los botones
    gpio_init(PIN_DO);  gpio_init(PIN_RE);  gpio_init(PIN_MI);  gpio_init(PIN_FA);
    gpio_init(PIN_SOL); gpio_init(PIN_LA);  gpio_init(PIN_SI);
    gpio_init(PIN_OC1); gpio_init(PIN_OC2); gpio_init(PIN_OC3);
    
    // configurar los pines de los botones como entradas
    gpio_set_dir(PIN_DO,  false); gpio_set_dir(PIN_RE,  false);
    gpio_set_dir(PIN_MI,  false); gpio_set_dir(PIN_FA,  false);
    gpio_set_dir(PIN_SOL, false); gpio_set_dir(PIN_LA,  false);
    gpio_set_dir(PIN_SI,  false);
    gpio_set_dir(PIN_OC1, false); gpio_set_dir(PIN_OC2, false);
    gpio_set_dir(PIN_OC3, false);
    
    // Colocar resistencias pull-up internas para los botones
    gpio_pull_up(PIN_DO);  gpio_pull_up(PIN_RE);  gpio_pull_up(PIN_MI);  gpio_pull_up(PIN_FA);
    gpio_pull_up(PIN_SOL); gpio_pull_up(PIN_LA);  gpio_pull_up(PIN_SI);
    gpio_pull_up(PIN_OC1); gpio_pull_up(PIN_OC2); gpio_pull_up(PIN_OC3);


    octava_referencia = 4;
    octava_candidata = 4;
    }

static int8_t buttons_read_note_raw(void)
{
    // Verificar qué botón de nota está presionado y retornar su desplazamiento en semitonos
    if      (gpio_get(PIN_DO) == 0)  return -9; 
    else if (gpio_get(PIN_RE) == 0)  return -7;
    else if (gpio_get(PIN_MI) == 0)  return -5;
    else if (gpio_get(PIN_FA) == 0)  return -4;
    else if (gpio_get(PIN_SOL) == 0) return -2;
    else if (gpio_get(PIN_LA) == 0)  return  0; // Referencia La4
    else if (gpio_get(PIN_SI) == 0)  return  2;
    
    // Si no se cumple ninguna de las anteriores, ninguna tecla está abajo
    return NOTA_NINGUNA; 
}



bool buttons_update(void)
    {
        // Obtener el tiempo actual del sistema en milisegundos desde que encendió la Pico
        uint32_t tiempo_actual = to_ms_since_boot(get_absolute_time());

        int8_t   nota_leida = buttons_read_note_raw();
        uint8_t  octava_leida = octava_candidata; 

        if (gpio_get(PIN_OC1) == 0)
            octava_leida = 3; // Botón de octava 3 presionado
        else if (gpio_get(PIN_OC2) == 0)
            octava_leida = 4; // Botón de octava 4 presionado
        else if (gpio_get(PIN_OC3) == 0)
            octava_leida = 5; // Botón de octava 5 presionado 

        if (nota_leida != nota_candidata || octava_leida != octava_candidata) {
        nota_candidata = nota_leida;
        octava_candidata = octava_leida;
        tiempo_cambio_nota = tiempo_actual; // Reiniciar temporizador por inestabilidad
        return false; 
        }

        // Verificar tiempo de estabilización
        if ((tiempo_actual - tiempo_cambio_nota) >= TIEMPO_DEBOUNCE_MS){
            bool cambio = false;

            // Si la señal es estable, se verifica si es diferente a la última nota aceptada
            if (nota_actual != nota_candidata){
                nota_actual = nota_candidata;
                cambio = true;
            }

            if (octava_referencia != octava_candidata) {
                octava_referencia = octava_candidata;
                cambio = true;
            }

            // Si el cambio es estable, retornar true
            if (cambio) return true;
        }

        return false; // No hubo cambio en la octava
    }


float buttons_get_note(void)
    {
        // Si la variable del antirebote indica que no hay nota presionada, se retorna silencio
        if (nota_actual == NOTA_NINGUNA) return 0.0f; 

        // n = n_nota_oct4 + 12 * (octava_actual - 4)
        int8_t n = (int8_t)nota_actual + 12 * ((int8_t)octava_referencia - 4);

        // frecuencia basada en La4 (440Hz)
        float frecuencia_hz = 440.0f * exp2f((float)n / 12.0f);

        return frecuencia_hz;
    }