/**
 * @file sig_gen.h
 * @brief Generador de señal cuadrada de prueba por PWM de hardware.
 */
#ifndef SIG_GEN_H
#define SIG_GEN_H

#include <stdint.h>

/**
 * @brief Arranca una onda cuadrada en un pin usando PWM.
 *
 * El PWM corre solo en hardware: no consume CPU ni genera IRQs,
 * así que no interfiere con las ISR de la aplicación.
 *
 * Rango útil con wrap fijo de 10000 cuentas: ~49 Hz a 12.5 kHz.
 *
 * @param gpio     Pin de salida (p. ej. 15).
 * @param freq_hz  Frecuencia deseada en Hz.
 * @param duty_pct Ciclo útil en % (0..100).
 */
void sig_gen_start(unsigned int gpio, float freq_hz, float duty_pct);

#endif /* SIG_GEN_H */
