/** @file fmt.h @brief Formato de texto con enteros (sin printf). */
#ifndef FMT_H
#define FMT_H

#include <stdint.h>

/** @brief Copia cadena terminada en NUL; devuelve puntero siguiente. */
char *put_str(char *p, const char *s);
/** @brief Entero sin signo en decimal. */
char *put_u32(char *p, uint32_t v);
/** @brief v/1000 con 3 decimales (v en milésimas). */
char *put_fixed3(char *p, uint32_t v);
/** @brief v/10 con 1 decimal (v en décimas). */
char *put_fixed1(char *p, uint32_t v);

#endif
