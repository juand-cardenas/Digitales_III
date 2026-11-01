/**
 * @file main.c
 * @brief Práctica 5 - Versión IRQs puras.
 *
 * Todo el trabajo de la aplicación (flancos, cálculo, reporte, UART) corre
 * en ISR. El ciclo principal no despacha nada: duerme hasta la próxima IRQ.
 * Pines: GP2 = entrada de señal, GP0 = UART0 TX (115200 8N1).
 * Recordar poner todas la tierras comunes no sea pelotudo
 */
#include "pico/stdlib.h"
#include "hardware/sync.h"   // <-- nueva: declara __wfi()
#include "hw.h"
#include "app_irq.h"
#include "sig_gen.h"
#include "hardware/uart.h"
#include "config.h"

int main(void)
{
    hw_init();
    sig_gen_start(15, 10000.0f, 55.0f);
    app_irq_start();

    for (;;) {
        //uart_puts(UART_ID, "hola\r\n");
        __wfi(); //sinfica aguardando una interrupcion, el procesador duerme
        /** 
         * Tenemos tre interrupciones, con 2 teniendo la mis prioridad.
         * 0X00 se dispara cada flanco de subida o bajada (edge_isr)
         * 0X80 se dispara cada 100ms y se reprograma (report_isr)
         * 0X80 se diapara con el fifo de Tx  (uart_tx_isr)
         * 
         * * */
    }
}
