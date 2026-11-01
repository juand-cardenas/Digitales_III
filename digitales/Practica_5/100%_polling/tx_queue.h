/**
 * @file tx_queue.h
 * @brief Cola de transmisión por software y envío por polling sin bloqueo.
 * @note Contexto: solo ciclo principal.
 */
#ifndef TX_QUEUE_H
#define TX_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** @brief Bytes libres en la cola. */
uint32_t tx_free(void);

/**
 * @brief Encola una línea completa (todo o nada).
 * @return false si no cabe; no se escribe nada.
 */
bool tx_push(const char *data, size_t len);

/** @brief Entrega al FIFO de la UART lo que quepa, sin esperar. */
void tx_pump(void);

#endif
