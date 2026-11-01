/** @file fmt.c @brief Implementación del formato de texto. */
#include "fmt.h"

char *put_str(char *p, const char *s)
{
    while (*s) { *p++ = *s++; }
    return p;
}

char *put_u32(char *p, uint32_t v)
{
    char tmp[10];
    int  i = 0;
    do { tmp[i++] = (char)('0' + (v % 10u)); v /= 10u; } while (v);
    while (i) { *p++ = tmp[--i]; }
    return p;
}

char *put_fixed3(char *p, uint32_t v)
{
    uint32_t frac = v % 1000u;
    p = put_u32(p, v / 1000u);
    *p++ = '.';
    *p++ = (char)('0' + frac / 100u);
    *p++ = (char)('0' + (frac / 10u) % 10u);
    *p++ = (char)('0' + frac % 10u);
    return p;
}

char *put_fixed1(char *p, uint32_t v)
{
    p = put_u32(p, v / 10u);
    *p++ = '.';
    *p++ = (char)('0' + v % 10u);
    return p;
}
