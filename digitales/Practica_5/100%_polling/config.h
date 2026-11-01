/** @file config.h @brief Parámetros de configuración de la versión polling. */
#ifndef CONFIG_H
#define CONFIG_H

#define PIN_SIGNAL_IN      2u          /**< GPIO de la señal a medir. */
#define UART_ID            uart0       /**< UART física usada para reportar. */
#define UART_TX_PIN        0u          /**< GPIO TX de UART0. */
#define UART_BAUD          115200u     /**< Velocidad [bits/s]. */

#define REPORT_PERIOD_US   100000u     /**< Periodo de reporte [us]. */
#define WINDOW_MIN_US      50000u      /**< Duración mínima de ventana de medición [us]. */
#define TIMEOUT_US         500000u     /**< Plazo sin transiciones para invalidar [us]. */

#define TX_BUF_SIZE        256u        /**< Buffer de software (potencia de 2). */
#define TX_BUF_MASK        (TX_BUF_SIZE - 1u)
#define MAX_LINE           64u         /**< Máx. bytes por línea (incluye LF). */
#define TX_BYTES_PER_PASS  8u          /**< Máx. bytes entregados al FIFO por pasada. */

#endif
