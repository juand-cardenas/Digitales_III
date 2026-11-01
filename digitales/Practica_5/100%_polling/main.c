/**
 * @file main.c
 * @brief Práctica 5 - Versión POLLING: solo el ciclo principal.
 *
 * Cada pasada: (1) lee entrada y tiempo, (2) procesa flanco,
 * (3) consulta temporizador de reporte, (4) consulta UART sin esperar.
 * Pines: GP2 = entrada de señal, GP0 = UART0 TX (115200 8N1).
 */
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "config.h"
#include "timebase.h"
#include "hw.h"
#include "meas.h"
#include "report.h"
#include "tx_queue.h"
#include "sig_gen.h"

int main(void)
{
    hw_init();
    sig_gen_start(15, 1000.0f, 25.0f);

    const uint32_t t_start = now_us();
    meas_init(t_start);
    uint32_t next_report = t_start + REPORT_PERIOD_US;
    bool     prev_level  = gpio_get(PIN_SIGNAL_IN);

    for (;;) {
        const uint32_t t     = now_us();
        const bool     level = gpio_get(PIN_SIGNAL_IN);

        if (level != prev_level) {
            prev_level = level;
            meas_on_edge(level, t);
        }

        if ((int32_t)(t - next_report) >= 0) {
            next_report += REPORT_PERIOD_US;
            report_task(t);
        }

        tx_pump();
    }
}
