/** @file uart_tx.c @brief Driver UART TX por interrupciones (buffer circular). */
#include "uart_tx.h"
#include "config.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

static uint8_t           buf[TX_BUF_SIZE]; /**< Datos pendientes. */
static volatile uint32_t head;             /**< Escritura: solo uart_tx_push(). */
static volatile uint32_t tail;             /**< Lectura: ISR y uart_tx_push() (arranque), nunca a la vez. */

/**
 * @brief Pasa al FIFO de hardware lo que quepa del buffer de software.
 * @note Solo se llama desde contextos de prioridad PRIO_COMM (la ISR de la UART
 *       y la ISR del reporte), que no se interrumpen entre sí.
 */
static void tx_fill_fifo(void)
{
    uart_hw_t *hw = uart_get_hw(UART_ID); //puntero a los registros de la UART 
    while (head != tail && uart_is_writable(UART_ID)) {
        /**head != tail: todavía hay datos pendientes en el buffer circular. head es donde escribe el programa (productor) y tail desde donde se lee/saca (consumidor). Si son iguales, el buffer está vacío.
            uart_is_writable(UART_ID): el FIFO de TX del hardware tiene espacio (no está lleno). */
        hw->dr = buf[tail & TX_BUF_MASK]; //escribe el byte en el registro de datos y lo tranmite por 
        //pin TX
        tail++;
    }
}

/** @brief ISR: pasa al FIFO lo que quepa; si no queda nada, apaga TXIM. */
static void uart_tx_isr(void)
{
    tx_fill_fifo();
    //desactiva la IQR  del TX.
    if (head == tail) { // Si no queda nada, apaga la interrupcion TX
        hw_clear_bits(&uart_get_hw(UART_ID)->imsc, UART_UARTIMSC_TXIM_BITS);
    }
}

void uart_tx_init(void)
{
    hw_clear_bits(&uart_get_hw(UART_ID)->imsc, UART_UARTIMSC_TXIM_BITS);
    irq_set_exclusive_handler(UART_IRQ, uart_tx_isr); //registra UART_tx_isr como unico manejador
    irq_set_priority(UART_IRQ, PRIO_COMM); //prioridad 0x80
    irq_set_enabled(UART_IRQ, true);
}

uint32_t uart_tx_free(void)
{
    return TX_BUF_SIZE - (head - tail); //para saber qie esta libre o no para le escritura
}

bool uart_tx_push(const char *data, size_t len)
{
    if (len > uart_tx_free()) { //si no hay esapcio para escribir no se estrbe, para evitar trucamiento.
        return false;
    }
    uint32_t h = head;
    for (size_t i = 0; i < len; i++) {
        buf[h & TX_BUF_MASK] = (uint8_t)data[i];
        h++;
    }
    __dmb(); //asegura que los datos ya esten escritos antes dede publicar
    head = h;

    /* La interrupcion TX del PL011 se dispara por TRANSICION (cuando el FIFO
     * baja del umbral), no por nivel. Con el FIFO vacio y TXIM recien activado
     * nunca se genera. Por eso se escribe primero al FIFO a mano: al vaciarse
     * ese contenido, el hardware si dispara la IRQ para el resto. */
    tx_fill_fifo();
    if (head != tail) { //queda algo por enviar: la ISR se encarga
        hw_set_bits(&uart_get_hw(UART_ID)->imsc, UART_UARTIMSC_TXIM_BITS);
    }
    return true;
}