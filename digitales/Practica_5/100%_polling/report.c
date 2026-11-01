/** @file report.c @brief Preparación de reportes. */
#include "report.h"
#include "config.h"
#include "meas.h"
#include "fmt.h"
#include "tx_queue.h"

static uint32_t report_n;          /**< Número del próximo reporte. */
static uint32_t prev_report_t;     /**< Instante del reporte anterior [us]. */
volatile uint32_t g_dropped_reports;

void report_task(uint32_t now)
{
    const uint32_t n  = report_n;
    const uint32_t dt = (n == 0u) ? 0u : (uint32_t)((now - prev_report_t + 500u) / 1000u);
    report_n++;
    prev_report_t = now;

    meas_result_t r = {0};
    const meas_state_t st = meas_get(now, &r);

    if (tx_free() < MAX_LINE) {
        g_dropped_reports++;
        return;
    }

    char  line[MAX_LINE];
    char *p = line;
    p = put_u32(p, n);  *p++ = ',';
    p = put_u32(p, dt); *p++ = ',';

    if (st == MEAS_OK) {
        uint64_t f_mhz = ((uint64_t)r.periods * 1000000000ull + r.span / 2u) / r.span;
        uint64_t d_x10 = ((uint64_t)r.th * 1000ull + r.span / 2u) / r.span;
        if (f_mhz > UINT32_MAX) { f_mhz = UINT32_MAX; }
        p = put_fixed3(p, (uint32_t)f_mhz); *p++ = ',';
        p = put_fixed1(p, (uint32_t)d_x10);
    } else {
        p = put_str(p, "NA,NA");
    }
    *p++ = ',';
    p = put_str(p, st == MEAS_OK ? "OK" :
                   st == MEAS_WAITING ? "ESPERANDO" : "SIN_TRANSICIONES");
    *p++ = '\n';

    tx_push(line, (size_t)(p - line));
}
