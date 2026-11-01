/** @file report.h @brief Generación de líneas de reporte n,dt_ms,f_Hz,D_pct,estado. */
#ifndef REPORT_H
#define REPORT_H

#include <stdint.h>

/** Reportes descartados por falta de espacio (consultable por SWD). */
extern volatile uint32_t g_dropped_reports;

/**
 * @brief Genera y encola un reporte programado. n avanza siempre; si no hay
 *        espacio para una línea completa se descarta antes de escribirla.
 * @param now Instante actual [us].
 */
void report_task(uint32_t now);

#endif
