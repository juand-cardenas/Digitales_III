/**
 * @file sig_gen.c
 * @brief Generador de señal cuadrada de prueba por PWM de hardware.
 *
 * f_pwm = f_sys / (clkdiv * (wrap + 1))
 * Con wrap = 9999 y f_sys = 125 MHz:
 *   1 kHz  -> clkdiv = 12.5
 *   100 Hz -> clkdiv = 125
 */
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "sig_gen.h"

#define SIG_GEN_WRAP 9999u

void sig_gen_start(unsigned int gpio, float freq_hz, float duty_pct)
{
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(gpio);
    uint chan  = pwm_gpio_to_channel(gpio);

    const float f_sys = (float)clock_get_hz(clk_sys);

    /* Divisor mínimo con el que el wrap cabe en 16 bits (máxima resolución). */
    float clkdiv = f_sys / (freq_hz * 65536.0f);
    if (clkdiv < 1.0f) { clkdiv = 1.0f; }

    uint32_t wrap = (uint32_t)(f_sys / (clkdiv * freq_hz) + 0.5f) - 1u;
    if (wrap > 65535u) { wrap = 65535u; }

    pwm_set_clkdiv(slice, clkdiv);
    pwm_set_wrap(slice, (uint16_t)wrap);
    pwm_set_chan_level(slice, chan, (uint16_t)((duty_pct / 100.0f) * (wrap + 1u)));
    pwm_set_enabled(slice, true);
}
