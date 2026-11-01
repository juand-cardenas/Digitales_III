/** @file meas.c @brief Implementación de la medición (sin divisiones). */
#include "meas.h"
#include "config.h"

typedef struct {
    bool     started, fell;
    uint32_t win_t0, rise_t, fall_t;
    uint32_t periods, th_sum, last_edge_t;
    bool     pub_valid;
    meas_result_t pub;
    uint32_t pub_t;
} meas_t;

static meas_t m;   /**< Propiedad exclusiva del ciclo principal. */

void meas_init(uint32_t t)
{
    m = (meas_t){0};
    m.last_edge_t = t;
}

void meas_on_edge(bool level, uint32_t t)
{
    if ((uint32_t)(t - m.last_edge_t) > TIMEOUT_US) {
        m.started   = false;
        m.pub_valid = false;
    }
    m.last_edge_t = t;

    if (level) {                               /* subida */
        if (!m.started) {
            m.started = true;
            m.win_t0  = t;
            m.periods = 0;
            m.th_sum  = 0;
        } else if (m.fell) {                   /* periodo completo */
            m.th_sum += m.fall_t - m.rise_t;
            m.periods++;
            uint32_t span = t - m.win_t0;
            if (span >= WINDOW_MIN_US) {
                m.pub.periods = m.periods;
                m.pub.th      = m.th_sum;
                m.pub.span    = span;
                m.pub_t       = t;
                m.pub_valid   = true;
                m.win_t0      = t;
                m.periods     = 0;
                m.th_sum      = 0;
            }
        }
        m.rise_t = t;
        m.fell   = false;
    } else {                                   /* bajada */
        if (m.started && !m.fell) {
            m.fall_t = t;
            m.fell   = true;
        }
    }
}

meas_state_t meas_get(uint32_t now, meas_result_t *out)
{
    const bool idle = (uint32_t)(now - m.last_edge_t) > TIMEOUT_US;
    if (idle || (m.pub_valid && (uint32_t)(now - m.pub_t) > TIMEOUT_US)) {
        m.pub_valid = false;
    }
    if (idle)         { return MEAS_NO_TRANSITIONS; }
    if (!m.pub_valid) { return MEAS_WAITING; }
    *out = m.pub;
    return MEAS_OK;
}
