/** @file timebase.h @brief Referencia temporal de 1 us. */
#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>
#include "hardware/timer.h"

/**
 * @brief Lee el contador de 1 us (32 bits, vuelve a 0 cada ~71 min).
 * @return Tiempo [us]. Restar siempre con aritmética sin signo.
 */
static inline uint32_t now_us(void)
{
    return timer_hw->timerawl;
}

#endif
