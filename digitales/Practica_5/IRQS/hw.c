/** @file hw.c @brief Inicialización de hardware. */
#include "hw.h"
#include "config.h"
#include "pico/stdlib.h"//funciones genereales del sdk
#include "hardware/gpio.h" //funciones del gpio
#include "hardware/uart.h" //funciones del uart

void hw_init(void)
{
    gpio_init(PIN_SIGNAL_IN); //el pin 2 lo configuro como un SIO
    gpio_set_dir(PIN_SIGNAL_IN, GPIO_IN); //lo configuro como entrada
    gpio_disable_pulls(PIN_SIGNAL_IN); // desactivo los pull para evitar deformar la señal

    uart_init(UART_ID, UART_BAUD);// avilito el reloj del uart y
                                // se hace la divicion para que sea de 11520 bit/s
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART); //conecto el gpio 0 al UART
    uart_set_hw_flow(UART_ID, false, false); //apaga el control de lujos
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE); //8 bits de datos, 1 bit de parada, sin paridad
    uart_set_fifo_enabled(UART_ID, true); //se activa el FIFO de 32 bytes de TX,
                                        // para escibir bytes por intrrupcion.
}
