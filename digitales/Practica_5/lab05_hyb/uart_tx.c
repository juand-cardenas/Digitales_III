/** @file uart_tx.c @brief Driver UART TX por interrupciones (buffer circular). */
#include "uart_tx.h"
#include "config.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

static uint8_t           buf[TX_BUF_SIZE]; /**< Datos pendientes. */
static volatile uint32_t head;             /**< Escritura: solo uart_tx_push(). */
static volatile uint32_t tail;             /**< Lectura: ISR y arranque de uart_tx_push(), nunca a la vez. */

/** @brief Pasa al FIFO de hardware lo que quepa del buffer de software. */
static void tx_fill_fifo(void)
{
    uart_hw_t *hw = uart_get_hw(UART_ID);
    while (head != tail && uart_is_writable(UART_ID)) {
        hw->dr = buf[tail & TX_BUF_MASK];
        tail++;
    }
}

/** @brief ISR: pasa al FIFO lo que quepa; si no queda nada, apaga TXIM. */
static void uart_tx_isr(void)
{
    tx_fill_fifo();
    if (head == tail) {
        hw_clear_bits(&uart_get_hw(UART_ID)->imsc, UART_UARTIMSC_TXIM_BITS);
    }
}

void uart_tx_init(void)
{
    hw_clear_bits(&uart_get_hw(UART_ID)->imsc, UART_UARTIMSC_TXIM_BITS);
    irq_set_exclusive_handler(UART_IRQ, uart_tx_isr);
    irq_set_priority(UART_IRQ, PRIO_COMM);
    irq_set_enabled(UART_IRQ, true);
}

uint32_t uart_tx_free(void)
{
    /* tail solo crece: una lectura desfasada subestima el espacio libre, nunca lo sobrestima. */
    return TX_BUF_SIZE - (head - tail);
}

bool uart_tx_push(const char *data, size_t len)
{
    if (len > uart_tx_free()) {
        return false;
    }
    uint32_t h = head;
    for (size_t i = 0; i < len; i++) {
        buf[h & TX_BUF_MASK] = (uint8_t)data[i];
        h++;
    }
    __dmb(); //los datos deben estar escritos antes de publicar head
    head = h;

    /* Arranque: la IRQ TX del PL011 se dispara por TRANSICION, no por nivel;
     * con el FIFO vacio hay que escribir los primeros bytes a mano.
     *
     * Aqui se enmascara la IRQ de la UART en el NVIC (la ISR de flancos NO se
     * toca) porque esta funcion puede llamarse desde el bucle principal, que
     * la ISR de la UART puede interrumpir. Sin esto, ambos modificarian tail
     * a la vez. Si la IRQ queda pendiente mientras esta enmascarada, se
     * atiende al reactivarla. */
    irq_set_enabled(UART_IRQ, false);
    tx_fill_fifo();
    if (head != tail) {
        hw_set_bits(&uart_get_hw(UART_ID)->imsc, UART_UARTIMSC_TXIM_BITS);
    }
    irq_set_enabled(UART_IRQ, true);
    return true;
}
