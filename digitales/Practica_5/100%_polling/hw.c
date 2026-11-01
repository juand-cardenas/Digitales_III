/** @file hw.c @brief Inicialización de hardware. */
#include "hw.h"
#include "config.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

void hw_init(void)
{
    gpio_init(PIN_SIGNAL_IN);
    gpio_set_dir(PIN_SIGNAL_IN, GPIO_IN);
    gpio_disable_pulls(PIN_SIGNAL_IN);

    uart_init(UART_ID, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);
}
