/**
 * @file uart_tx.h
 * @brief Driver propio de transmisión UART por interrupciones.
 *
 * Productor: uart_tx_push() (ISR del reporte o bucle principal).
 * Consumidor: ISR de la UART.
 * uart_tx_push() enmascara brevemente la IRQ de la UART al arrancar el envío,
 * así que es segura tanto desde una ISR de prioridad PRIO_COMM como desde el
 * bucle principal. Debe llamarla un único productor.
 */
#ifndef UART_TX_H
#define UART_TX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** @brief Instala la ISR de la UART (TX desactivada hasta que haya datos). */
void uart_tx_init(void);

/** @brief Bytes libres en el buffer de software. */
uint32_t uart_tx_free(void);

/**
 * @brief Encola @p len bytes (todo o nada) y arranca la transmisión.
 * @return false si no hay espacio; no se escribe nada. Nunca espera.
 * @note No llamar desde una ISR de prioridad mayor que PRIO_COMM.
 */
bool uart_tx_push(const char *data, size_t len);

#endif
