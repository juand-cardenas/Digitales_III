/** @file config.h @brief Parámetros de configuración de la versión IRQs. */
#ifndef CONFIG_H
#define CONFIG_H

#include "hardware/irq.h" /** Trae a este las definicones de IRQ del RP2040. como TIMER_IRQ_0 */

#define PIN_SIGNAL_IN      2u          /**< GPIO 2 donde esta la señal a medir. no mas de 3.3V */ 
#define UART_ID            uart0       /**< UART física usada para reportar. */
#define UART_IRQ           UART0_IRQ   /**< Numero de interrupcion de esa Uart en el Nvic */
#define UART_TX_PIN        0u          /**< GPIO 0 que actuara como el TX del Uart*/
#define UART_BAUD          115200u     /**< Velocidad [bits/s]. */
// se acabo lo de los pines

#define REPORT_PERIOD_US   100000u     /**< Periodo de reporte [us]. */
#define WINDOW_MIN_US      50000u      /**< Duración mínima de ventana de medición [us]. */
#define TIMEOUT_US         500000u     /**< Plazo sin transiciones para invalidar [us]. */

#define REPORT_ALARM       0u          /**< Alarma de hardware usada para el reporte. */
#define REPORT_ALARM_IRQ   TIMER_IRQ_0 /**< Asocio el IQR a la alarma*/

/** Prioridades NVIC (menor valor = mayor prioridad). */
#define PRIO_EDGE          0x00u       /**< Flancos: nunca esperan a otra ISR. maxima prioridad*/
#define PRIO_COMM          0x80u       /**< Reporte y UART: misma prioridad, no se interrumpen entre sí. */

#define TX_BUF_SIZE        256u        /**< Buffer de software (potencia de 2). */
#define TX_BUF_MASK        (TX_BUF_SIZE - 1u)
#define MAX_LINE           64u         /**< Máx. bytes por línea (incluye LF). */

#endif
