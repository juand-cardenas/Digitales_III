/** @file app_hyb.h @brief Aplicación híbrida: flancos por IRQ, reporte por polling. */
#ifndef APP_HYB_H
#define APP_HYB_H

/** @brief Inicia medición, driver UART e IRQ de flancos. */
void app_hyb_start(void);

/** @brief Una pasada de polling: si venció el periodo, genera y encola el reporte. */
void app_hyb_poll(void);

#endif
