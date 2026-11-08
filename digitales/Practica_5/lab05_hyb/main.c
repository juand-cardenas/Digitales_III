/**
 * @file main.c
 * @brief Práctica 5 - Versión polling + IRQs.
 *
 * Flancos y TX de la UART se atienden por interrupción; el reporte periódico
 * se atiende por polling en el ciclo principal.
 * Pines: GP2 = entrada de señal, GP0 = UART0 TX (115200 8N1).
 * GP15 = señal de prueba por PWM (unir con GP2 y tierras comunes).
 */
#include "pico/stdlib.h"
#include "hw.h"
#include "app_hyb.h"
#include "sig_gen.h"

int main(void)
{
    hw_init();
    sig_gen_start(15, 50000.0f, 25.0f);
    app_hyb_start();

    for (;;) {
        app_hyb_poll();   /* no duerme: consulta el reloj en cada pasada */
    }
}
