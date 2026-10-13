#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h" 
#include "systick.h"
#include "audio.h"
#include "dac.h"
#include "buttons.h"


int main(void)
{
    // 1. Inicializar la salida USB para mostrar resultados.
    stdio_init_all();

    // El programa se quedará en pausa aquí hasta que detecte una conexión USB activa
    while (!stdio_usb_connected()) {
        sleep_ms(50);
    }
    sleep_ms(2000);

    // Inicializar valores
    buttons_init();
    audio_init(8, 0.0f, 50000.0f);
    dac_init();
    systick_init();

    printf("Prueba de Octavia, esperando notas: \n");

    while (1)
    {
        if (buttons_update())
        {
            float frecuencia = buttons_get_note();
            audio_set_frequency(frecuencia);
            // printf("Cambio detectado: Nueva frecuencia = %.2f Hz\n", frecuencia);
        }
    }
}
