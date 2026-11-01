/**
 * @file app_irq.c
 * @brief Contextos de interrupción de la aplicación.
 *
 * - IO_IRQ_BANK0 (PRIO_EDGE): marca el flanco y actualiza la medición.
 * - TIMER alarma 0 (PRIO_COMM): prepara y encola el reporte, sin deriva.
 * - UART (PRIO_COMM): driver propio en uart_tx.c.
 */
#include <stdbool.h>
#include "app_irq.h"
#include "config.h"
#include "timebase.h"
#include "meas.h"
#include "report.h"
#include "uart_tx.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/sync.h"

static uint32_t next_alarm;   /**< Próximo instante de reporte [us]. Solo la ISR del timer. */

/**
 * @brief ISR de flancos de GP2.
 * @param gpio   Pin que interrumpió.
 * @param events Flancos detectados (GPIO_IRQ_EDGE_*).
 * @note Se toma la marca de tiempo antes de cualquier otra cosa.
 */
static void edge_isr(uint gpio, uint32_t events) //funcion del callback, limpoia la flag de la interrupcion y luego llama la funcion.
{
    const uint32_t t = now_us(); //se toma la marca de tiempo
    (void)gpio;

    const bool rise = (events & GPIO_IRQ_EDGE_RISE) != 0u; //verdadero si hubo falnco de subida
    const bool fall = (events & GPIO_IRQ_EDGE_FALL) != 0u; //... de bajada

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

/** @brief ISR de la alarma: reprograma primero (sin deriva) y genera el reporte. */
static void report_isr(void)
{
    timer_hw->intr = 1u << REPORT_ALARM;           /* limpiar flag de la alram cero*/
    const uint32_t t = now_us();
    next_alarm += REPORT_PERIOD_US;
    timer_hw->alarm[REPORT_ALARM] = next_alarm; //reprogram la alarma
    report_task(t); //genera el reporte
}

void app_irq_start(void)
{
    const uint32_t t_start = now_us(); //instante de arranque
    meas_init(t_start);
    uart_tx_init(); //se inicializa la medion y el uart

    /* activa interrpciones para Flancos de subida y bajada de GP2. */
    gpio_set_irq_enabled_with_callback(PIN_SIGNAL_IN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &edge_isr);
    irq_set_priority(IO_IRQ_BANK0, PRIO_EDGE);

    /* Alarma periódica de reporte. */
    next_alarm = t_start + REPORT_PERIOD_US;
    hardware_alarm_claim(REPORT_ALARM);
    irq_set_exclusive_handler(REPORT_ALARM_IRQ, report_isr);
    irq_set_priority(REPORT_ALARM_IRQ, PRIO_COMM);
    hw_set_bits(&timer_hw->inte, 1u << REPORT_ALARM);
    timer_hw->alarm[REPORT_ALARM] = next_alarm;
    irq_set_enabled(REPORT_ALARM_IRQ, true);
}
