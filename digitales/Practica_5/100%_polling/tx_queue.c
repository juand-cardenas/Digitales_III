/** @file tx_queue.c @brief Cola circular y bombeo a la UART por polling. */
#include "tx_queue.h"
#include "config.h"
#include "hardware/uart.h"

static uint8_t  tx_buf[TX_BUF_SIZE];  /**< Propiedad del ciclo principal. */
static uint32_t tx_head;              /**< Contador de escritura (sin límite, se enmascara). */
static uint32_t tx_tail;              /**< Contador de lectura. */

uint32_t tx_free(void)
{
    return TX_BUF_SIZE - (tx_head - tx_tail);
}

bool tx_push(const char *data, size_t len)
{
    if (len > tx_free()) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        tx_buf[tx_head & TX_BUF_MASK] = (uint8_t)data[i];
        tx_head++;
    }
    return true;
}

void tx_pump(void)
{
    for (uint32_t i = 0; i < TX_BYTES_PER_PASS; i++) {
        if (tx_head == tx_tail || !uart_is_writable(UART_ID)) {
            return;
        }
        uart_get_hw(UART_ID)->dr = tx_buf[tx_tail & TX_BUF_MASK];
        tx_tail++;
    }
}
