/**
 * @file uart_tx.h
 * @brief Driver propio de transmisión UART por interrupciones.
 *
 * Productor: uart_tx_push() (ISR del reporte). Consumidor: ISR de la UART.
 * Ambas tienen la misma prioridad (PRIO_COMM), por lo que no se interrumpen
 * entre sí y el buffer no necesita secciones críticas.
 */
#ifndef UART_TX_H
#define UART_TX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** @brief Instala la ISR de la UART (TX desactivada hasta que haya datos). */
void uart_tx_init(void); //instala la ISR de la UART. El TX queda apagada hasta que haya datos.

/** @brief Bytes libres en el buffer de software. */
uint32_t uart_tx_free(void); //defino la cantidad de Bytes en el buffer de software.

/**
 * @brief Encola @p len bytes (todo o nada) y activa la interrupción TX.
 * @return false si no hay espacio; no se escribe nada. Nunca espera.
 * @note Llamar solo desde contexto de prioridad PRIO_COMM o inferior.
 */
bool uart_tx_push(const char *data, size_t len); //me dice si cabe los print o no

#endif
