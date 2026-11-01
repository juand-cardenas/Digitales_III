/**
 * @file meas.h
 * @brief Medición de frecuencia y ciclo de trabajo por marcas de tiempo.
 * @note Contexto: solo ciclo principal.
 */
#ifndef MEAS_H
#define MEAS_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Estado de la medición. */
typedef enum {
    MEAS_WAITING,          /**< Aún no hay ventana completa. */
    MEAS_OK,               /**< Resultado vigente. */
    MEAS_NO_TRANSITIONS    /**< Sin transiciones durante TIMEOUT_US. */
} meas_state_t;

/** @brief Resultado publicado (enteros). */
typedef struct {
    uint32_t periods;      /**< Periodos completos en la ventana. */
    uint32_t th;           /**< Suma de tiempos en alto [us]. */
    uint32_t span;         /**< Duración de la ventana [us]. */
} meas_result_t;

/** @brief Inicializa; el plazo sin transiciones cuenta desde @p t [us]. */
void meas_init(uint32_t t);

/** @brief Procesa un flanco. @param level true = subida. @param t instante [us]. */
void meas_on_edge(bool level, uint32_t t);

/**
 * @brief Consulta el estado y el resultado vigente.
 * @param now Instante actual [us].
 * @param out Se llena solo si devuelve MEAS_OK.
 * @return Estado de la medición.
 */
meas_state_t meas_get(uint32_t now, meas_result_t *out);

#endif
