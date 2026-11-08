/** @file fmt.c @brief Implementación del formato de texto. */
#include "fmt.h"

char *put_str(char *p, const char *s) //se copia una cadena al buffer
{
    while (*s) { *p++ = *s++; }// se hace una copa de s a P
    return p;
}

char *put_u32(char *p, uint32_t v)
{
    char tmp[10];
    int  i = 0;
    do { tmp[i++] = (char)('0' + (v % 10u)); v /= 10u; } while (v);/* se le de deracha izquierda
    y se traduce a ascii*/
    while (i) { *p++ = tmp[--i]; } //se aregla el orden, arriba queda al reves
    return p;
}

char *put_fixed3(char *p, uint32_t v)
// *P apunta a un a lista, la cunal tiene lo que se va a imprimir, hace de cursos
{
    uint32_t frac = v % 1000u; //v esta en milihertz
    p = put_u32(p, v / 1000u); //parte entera de los HZ
    *p++ = '.'; //el separador decimal
    *p++ = (char)('0' + frac / 100u); //primer decimal
    *p++ = (char)('0' + (frac / 10u) % 10u); //segundo decimal
    *p++ = (char)('0' + frac % 10u);  //tercer decimal
    return p;
}

char *put_fixed1(char *p, uint32_t v)
{
    p = put_u32(p, v / 10u); //parte enterea del porcentaje
    *p++ = '.'; //punto decimal
    *p++ = (char)('0' + v % 10u);//parte decimal
    return p;
}
