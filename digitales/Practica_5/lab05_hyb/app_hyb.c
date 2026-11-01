/**
 * @file app_hyb.c
 * @brief Versión polling + IRQs.
 *
 * - IRQ (IO_IRQ_BANK0, PRIO_EDGE): marca el flanco y actualiza la medición.
 * - IRQ (UART, PRIO_COMM): driver propio de TX en uart_tx.c.
 * - Polling (bucle principal): vigila el reloj de 1 us y, cuando vence el
 *   periodo, genera y encola el reporte. No usa alarma de hardware.
 */
#include <stdbool.h>
#include "app_hyb.h"
#include "config.h"
#include "timebase.h"
#include "meas.h"
#include "report.h"
#include "uart_tx.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

static uint32_t next_report;   /**< Próximo instante de reporte [us]. Solo el bucle principal. */

/**
 * @brief ISR de flancos de GP2 (igual que en la versión IRQs puras).
 * @param gpio   Pin que interrumpió.
 * @param events Flancos detectados (GPIO_IRQ_EDGE_*).
 * @note Se toma la marca de tiempo antes de cualquier otra cosa.
 */
static void edge_isr(uint gpio, uint32_t events)
{
    const uint32_t t = now_us(); //se toma la marca de tiempo
    (void)gpio;

    const bool rise = (events & GPIO_IRQ_EDGE_RISE) != 0u;
    const bool fall = (events & GPIO_IRQ_EDGE_FALL) != 0u;

    if (rise && fall) {
        /* Dos flancos pendientes (señal muy rápida): el orden lo da el nivel actual. */
        if (gpio_get(PIN_SIGNAL_IN)) {
            meas_on_edge(false, t);
            meas_on_edge(true,  t);
        } else {
            meas_on_edge(true,  t);
            meas_on_edge(false, t);
        }
    } else if (rise) {
        meas_on_edge(true, t);
    } else if (fall) {
        meas_on_edge(false, t);
    }
}

void app_hyb_start(void)
{
    const uint32_t t_start = now_us();
    meas_init(t_start);
    uart_tx_init();

    gpio_set_irq_enabled_with_callback(PIN_SIGNAL_IN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &edge_isr);
    irq_set_priority(IO_IRQ_BANK0, PRIO_EDGE);

    next_report = t_start + REPORT_PERIOD_US;   /* primer reporte a los 100 ms */
}

void app_hyb_poll(void)
{
    const uint32_t t = now_us();

    /* Comparación con signo: correcta aunque el contador de 32 bits desborde. */
    if ((int32_t)(t - next_report) >= 0) {
        next_report += REPORT_PERIOD_US;   /* sin deriva: se suma al instante programado, no a t */
        report_task(t);
    }
}
