/**
 * @file main.c
 * @brief Juego estilo "Simon" para Raspberry Pi Pico (RP2040) en C con el Pico SDK.
 *
 * Núcleo 0 (main):
 *   - Toda la lógica del juego: LEDs de patrón, botones, vidas, niveles,
 *     animaciones y botón de reinicio.
 *
 * Núcleo 1 (core1_main):
 *   - SOLO multiplexa el display de 7 segmentos (4 dígitos) de forma continua.
 *
 * Acceso a GPIO:
 *   Todos los pines que forman un mismo patrón lógico (LEDs, segmentos,
 *   dígitos, botones) se leen/escriben con UNA sola operación sobre el
 *   registro SIO, usando máscaras de bits:
 *
 *     gpio_put_masked(mask, value)  -> escribe 'value' SOLO en los pines de 'mask'
 *     gpio_set_mask(mask)           -> pone a 1 los pines de 'mask'
 *     gpio_clr_mask(mask)           -> pone a 0 los pines de 'mask'
 *     gpio_xor_mask(mask)           -> invierte los pines de 'mask'
 *     gpio_get_all()                -> lee los 30 GPIO en una sola lectura
 *
 *   Así no existen estados intermedios entre pines del mismo patrón y se
 *   elimina la sobrecarga de llamar a digitalWrite() pin por pin.
 *
 * Comunicación entre núcleos:
 *   El arreglo numero[4] es volatile: el núcleo 0 escribe y el núcleo 1 lee.
 *   En el RP2040 la escritura de un int alineado de 32 bits es atómica, por
 *   lo que no se necesita mutex para este caso.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <stdlib.h>
#include "hardware/gpio.h"

/* =================================================
 * MAPA DE GPIO
 *
 * Los pines están agrupados en bloques contiguos, así
 * cada grupo es una máscara de bits simple:
 *
 *   GPIO  0..4   Botones 0..3 + botón 4 (inicio/reinicio)
 *   GPIO  5..11  Segmentos a..g   (activos en BAJO)
 *   GPIO 12..15  Dígitos 0..3     (activos en BAJO)
 *   GPIO 16..20  LEDs: GPIO20=led0 ... GPIO17=led3, GPIO16=led4 (respuesta)
 * ================================================= */

#define BOTONES_BASE     0
#define SEG_BASE         5
#define DIG_BASE         12
#define LEDS_BASE        16

#define MASK_BOTONES     (0x1Fu << BOTONES_BASE)
#define MASK_SEG         (0x7Fu << SEG_BASE)
#define MASK_DIG         (0x0Fu << DIG_BASE)
#define MASK_LEDS        (0x1Fu << LEDS_BASE)

/** Máscara del LED i (0..4). led0 -> GPIO20 ... led4 -> GPIO16. */
#define LED_MASK(i)      (1u << (LEDS_BASE + 4 - (i)))

/** LED de respuesta (leds[4], GPIO16). */
#define MASK_LED_RESP    LED_MASK(4)

/** LEDs de patrón (leds[0..3], GPIO17..20). */
#define MASK_LEDS_PATRON (MASK_LEDS & ~MASK_LED_RESP)

/** Bit del botón de inicio/reinicio dentro de botones_leer(). */
#define BOTON_INICIO_BIT (1u << 4)

/* =================================================
 * DISPLAY DE 7 SEGMENTOS
 *
 * Código de cada número (bit0=a ... bit6=g). Como los
 * segmentos son activos en BAJO, la tabla guarda ya el
 * valor listo para escribir en los GPIO 5..11.
 * ================================================= */

#define SEG_PINS(code)   (((~(uint32_t)(code)) & 0x7Fu) << SEG_BASE)

static const uint32_t TABLA_7SEG[11] = {
    SEG_PINS(0x3F), /* 0 */
    SEG_PINS(0x06), /* 1 */
    SEG_PINS(0x5B), /* 2 */
    SEG_PINS(0x4F), /* 3 */
    SEG_PINS(0x66), /* 4 */
    SEG_PINS(0x6D), /* 5 */
    SEG_PINS(0x7D), /* 6 */
    SEG_PINS(0x07), /* 7 */
    SEG_PINS(0x7F), /* 8 */
    SEG_PINS(0x6F), /* 9 */
    SEG_PINS(0x00)  /* 10 = blanco */
};

/**
 * numero[0] -> Nivel actual
 * numero[1] -> Vidas restantes
 * numero[2] -> Decenas de segundo del tiempo acumulado
 * numero[3] -> Unidades de segundo del tiempo acumulado
 * (10 = dígito en blanco)
 */
static volatile int numero[4] = {0, 0, 0, 0};

/** Tiempo que permanece encendido cada dígito (µs). */
#define INTERVALO_DISPLAY_US   2000u

/** Duración de la pulsación larga para reiniciar (ms). */
#define TIEMPO_REINICIO_MS     2000u

/** Límite máximo del tiempo acumulado (s). */
#define TIEMPO_ACUMULADO_MAX   99.0f

/* =================================================
 * VARIABLES DEL JUEGO (solo núcleo 0)
 * ================================================= */

static int   lista[9] = {0};
static float tiempo_acumulado = 0.0f;

static bool     boton4_presionado = false;
static uint32_t boton4_desde = 0;

typedef enum {
    REINICIO_SOLICITADO = -1,
    NIVEL_ERROR         = 0,
    NIVEL_OK            = 1
} resultado_nivel_t;

/* =================================================
 * UTILIDADES DE TIEMPO
 * ================================================= */

/** Milisegundos desde el arranque (equivalente a millis()). */
static inline uint32_t ahora_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

/** Diferencia segura ante desbordamiento (equivalente a ticks_diff). */
static inline uint32_t diff_ms(uint32_t ahora, uint32_t antes) {
    return ahora - antes;
}

/* =================================================
 * ACCESO A GPIO POR MÁSCARAS
 * ================================================= */

/** Configura todos los GPIO del juego sin estados intermedios. */
static void gpio_configurar(void) {
    const uint32_t mask_salidas = MASK_LEDS | MASK_SEG | MASK_DIG;

    /* Deja los pines en modo SIO, como entradas y en nivel bajo. */
    gpio_init_mask(mask_salidas | MASK_BOTONES);

    /* Fija el nivel inicial ANTES de habilitar las salidas (sin glitches):
     *   LEDs apagados (0), segmentos apagados (1), dígitos apagados (1). */
    gpio_put_masked(mask_salidas, MASK_SEG | MASK_DIG);

    /* Direcciones: una sola escritura por grupo. */
    gpio_set_dir_out_masked(mask_salidas);
    gpio_set_dir_in_masked(MASK_BOTONES);

    /* Pull-down por pin (el SDK no ofrece versión con máscara). */
    for (uint8_t i = 0; i < 5; i++) {
        gpio_pull_down(BOTONES_BASE + i);
    }
}

/** Escribe el estado completo de los 5 LEDs de una vez. */
static inline void leds_escribir(uint32_t mask) {
    gpio_put_masked(MASK_LEDS, mask);
}

/** Escribe el estado de los 4 LEDs de patrón sin tocar el LED de respuesta. */
static inline void patron_escribir(uint32_t mask) {
    gpio_put_masked(MASK_LEDS_PATRON, mask);
}

/** Apaga todos los LEDs en una sola operación. */
static inline void apagar_leds(void) {
    gpio_clr_mask(MASK_LEDS);
}

/**
 * Lee TODOS los botones en una sola lectura del bus.
 * @return bits 0..3 = botones 0..3, bit 4 = botón inicio/reinicio.
 */
static inline uint32_t botones_leer(void) {
    return (gpio_get_all() & MASK_BOTONES) >> BOTONES_BASE;
}

static inline bool boton_inicio_activo(void) {
    return (botones_leer() & BOTON_INICIO_BIT) != 0;
}

/* =================================================
 * DISPLAY: MULTIPLEXADO (núcleo 1)
 * ================================================= */

/**
 * Muestra el dígito 'pos' (0..3).
 * Segmentos y selección de dígito cambian en UNA sola escritura de 11 bits
 * (GPIO 5..15), por lo que nunca hay un instante con el número de un dígito
 * sobre el dígito anterior (sin "ghosting" por estados intermedios).
 */
static void mostrar_digito(uint8_t pos) {
    int n = numero[pos];

    /* Dígitos activos en bajo: todos en 1 salvo el seleccionado. */
    uint32_t dig = MASK_DIG & ~(1u << (DIG_BASE + pos));

    gpio_put_masked(MASK_SEG | MASK_DIG, TABLA_7SEG[n] | dig);
}

/** Punto de entrada del núcleo 1: multiplexa el display para siempre. */
static void core1_main(void) {
    uint8_t  pos = 0;
    uint32_t proximo = time_us_32();

    while (true) {
        mostrar_digito(pos);
        pos = (pos + 1) & 3u;

        proximo += INTERVALO_DISPLAY_US;
        while ((int32_t)(time_us_32() - proximo) < 0) {
            tight_loop_contents();
        }
    }
}

/**
 * Actualiza nivel / vidas / tiempo (núcleo 0 escribe, núcleo 1 lee).
 * El tiempo se muestra como dos dígitos de segundos (00..99).
 */
static void actualizar_numero(int nivel, int vidas, float t) {
    if (t > 99.99f) t = 99.99f;
    if (t < 0.0f)   t = 0.0f;

    int t_x100   = (int)lroundf(t * 100.0f);
    int decenas  = t_x100 / 1000;
    int unidades = (t_x100 / 100) % 10;

    numero[0] = (nivel >= 0 && nivel <= 9) ? nivel : 9;
    numero[1] = (vidas >= 0 && vidas <= 3) ? vidas : 3;
    numero[2] = decenas;
    numero[3] = unidades;
}

/* =================================================
 * ANIMACIONES
 * ================================================= */

/** Nivel superado: barrido rápido LED0 -> LED3. */
static void animacion_nivel_superado(void) {
    apagar_leds();
    for (uint8_t i = 0; i < 4; i++) {
        patron_escribir(LED_MASK(i));
        sleep_ms(60);
    }
    patron_escribir(0);
}

/** Entrada incorrecta: los 4 LEDs juntos, 2 parpadeos. */
static void animacion_entrada_incorrecta(void) {
    apagar_leds();
    for (uint8_t r = 0; r < 2; r++) {
        gpio_set_mask(MASK_LEDS_PATRON);    /* los 4 a la vez */
        sleep_ms(150);
        apagar_leds();
        sleep_ms(100);
    }
}

/** Tiempo agotado: LED de respuesta fijo y parpadean los dígitos de tiempo. */
static void animacion_agotamiento_tiempo(void) {
    gpio_set_mask(MASK_LED_RESP);

    int decenas_actual  = numero[2];
    int unidades_actual = numero[3];

    for (uint8_t r = 0; r < 4; r++) {
        numero[2] = 10;
        numero[3] = 10;
        sleep_ms(150);

        numero[2] = decenas_actual;
        numero[3] = unidades_actual;
        sleep_ms(150);
    }

    gpio_clr_mask(MASK_LED_RESP);
}

/** Pérdida de vida: parpadea solo el dígito de vidas (valor anterior). */
static void animacion_perdida_vida(void) {
    int valor_actual = numero[1];

    for (uint8_t r = 0; r < 3; r++) {
        numero[1] = 10;
        sleep_ms(150);

        numero[1] = valor_actual;
        sleep_ms(150);
    }
}

/** Victoria: barrido ida y vuelta x2 + 3 destellos con los 4 LEDs juntos. */
static void animacion_victoria(void) {
    apagar_leds();

    for (uint8_t r = 0; r < 2; r++) {
        for (int i = 0; i < 4; i++) {
            patron_escribir(LED_MASK(i));
            sleep_ms(50);
        }
        for (int i = 2; i >= 0; i--) {
            patron_escribir(LED_MASK(i));
            sleep_ms(50);
        }
        patron_escribir(0);
    }

    for (uint8_t r = 0; r < 3; r++) {
        gpio_set_mask(MASK_LEDS_PATRON);
        sleep_ms(120);
        apagar_leds();
        sleep_ms(120);
    }
}

/* =================================================
 * LÓGICA DE TIEMPO / REINICIO
 * ================================================= */

/** Suma 'segundos' al acumulador sin sobrepasar TIEMPO_ACUMULADO_MAX. */
static void acumular_tiempo(float segundos) {
    if (tiempo_acumulado < TIEMPO_ACUMULADO_MAX) {
        tiempo_acumulado += segundos;
        if (tiempo_acumulado > TIEMPO_ACUMULADO_MAX) {
            tiempo_acumulado = TIEMPO_ACUMULADO_MAX;
        }
    }
    printf("Tiempo acumulado: %.2f\n", (double)tiempo_acumulado);
}

/**
 * Revisa el botón de reinicio (botón 4). Se llama continuamente.
 * @return true si se completó una pulsación larga.
 */
static bool revisar_boton_reinicio(void) {
    uint32_t ahora = ahora_ms();

    if (boton_inicio_activo()) {
        if (!boton4_presionado) {
            boton4_presionado = true;
            boton4_desde = ahora;
        } else if (diff_ms(ahora, boton4_desde) >= TIEMPO_REINICIO_MS) {
            printf("REINICIO SOLICITADO\n");
            apagar_leds();

            /* Espera a que se suelte para que no reinicie solo. */
            while (boton_inicio_activo()) {
                sleep_ms(10);
            }

            boton4_presionado = false;
            return true;
        }
    } else {
        boton4_presionado = false;
    }

    return false;
}

/** Espera 'tiempo_seg' revisando el botón de reinicio. true = reinicio. */
static bool esperar_tiempo(float tiempo_seg) {
    uint32_t inicio   = ahora_ms();
    uint32_t total_ms = (uint32_t)(tiempo_seg * 1000.0f);

    while (diff_ms(ahora_ms(), inicio) < total_ms) {
        if (revisar_boton_reinicio()) {
            return true;
        }
        sleep_ms(5);
    }
    return false;
}

/**
 * Espera el inicio del juego.
 * Pulsación corta: inicia. Pulsación larga: se ignora y sigue esperando.
 */
static void esperar_inicio(void) {
    printf("Esperando inicio...\n");
    apagar_leds();

    while (boton_inicio_activo()) {
        sleep_ms(10);
    }
    printf("Boton liberado. Presione para comenzar.\n");

    while (true) {
        if (boton_inicio_activo()) {
            uint32_t inicio_pulsacion = ahora_ms();
            bool pulsacion_larga = false;

            while (boton_inicio_activo()) {
                if (diff_ms(ahora_ms(), inicio_pulsacion) >= TIEMPO_REINICIO_MS) {
                    printf("Pulsacion larga.\n");
                    printf("No se inicia el juego.\n");
                    apagar_leds();

                    while (boton_inicio_activo()) {
                        sleep_ms(5);
                    }

                    pulsacion_larga = true;
                    break;
                }
                sleep_ms(5);
            }

            if (!pulsacion_larga) {
                printf("INICIANDO JUEGO\n");
                return;
            }
        }
    }
}

/* =================================================
 * FASE DE RESPUESTA
 * ================================================= */

/**
 * Espera la respuesta del jugador mientras el LED de respuesta parpadea.
 * @return NIVEL_OK, NIVEL_ERROR (error o tiempo agotado) o REINICIO_SOLICITADO.
 */
static resultado_nivel_t esperar_con_parpadeo(float tiempo_total_seg,
                                              int nivel,
                                              int vidas_actuales) {
    uint32_t inicio        = ahora_ms();
    uint32_t ultimo_cambio = inicio;
    uint32_t total_ms      = (uint32_t)(tiempo_total_seg * 1000.0f);

    /* El LED de respuesta arranca ENCENDIDO. */
    gpio_set_mask(MASK_LED_RESP);

    bool     estado_anterior[4] = {false, false, false, false};
    bool     led_activo[4]      = {false, false, false, false};
    uint32_t t_led[4]           = {0, 0, 0, 0};
    int      posicion           = 0;
    uint32_t ultima_pulsacion   = ahora_ms();

    while (diff_ms(ahora_ms(), inicio) < total_ms) {

        uint32_t ahora = ahora_ms();

        if (revisar_boton_reinicio()) {
            return REINICIO_SOLICITADO;
        }

        /* Tiempo en vivo: acumulado previo + lo de este intento. */
        float transcurrido = diff_ms(ahora, inicio) / 1000.0f;
        float progreso     = transcurrido / tiempo_total_seg;
        actualizar_numero(nivel, vidas_actuales, tiempo_acumulado + transcurrido);

        /* Parpadeo del LED de respuesta: un solo XOR sobre su bit. */
        float intervalo = (0.5f - (0.45f * progreso)) / 2.0f;
        if (diff_ms(ahora, ultimo_cambio) >= (uint32_t)(intervalo * 1000.0f)) {
            gpio_xor_mask(MASK_LED_RESP);
            ultimo_cambio = ahora;
        }

        /* Apaga los LEDs de pulsación vencidos: acumula la máscara y
         * los apaga TODOS juntos con una sola escritura. */
        uint32_t apagar = 0;
        for (uint8_t j = 0; j < 4; j++) {
            if (led_activo[j] && diff_ms(ahora, t_led[j]) >= 350u) {
                apagar |= LED_MASK(j);
                led_activo[j] = false;
            }
        }
        if (apagar) {
            gpio_clr_mask(apagar);
        }

        /* Una única lectura de los botones por vuelta del bucle. */
        uint32_t botones = botones_leer();

        for (uint8_t i = 0; i < 4; i++) {
            bool actual = ((botones >> i) & 1u) != 0;

            /* Flanco 0 -> 1 */
            if (actual && !estado_anterior[i]) {

                /* Debounce */
                if (diff_ms(ahora, ultima_pulsacion) >= 80u) {
                    ultima_pulsacion = ahora;

                    gpio_set_mask(LED_MASK(i));
                    led_activo[i] = true;
                    t_led[i] = ahora;

                    if ((int)i != lista[posicion]) {
                        printf("ERROR\n");
                        gpio_clr_mask(MASK_LED_RESP);

                        acumular_tiempo(diff_ms(ahora, inicio) / 1000.0f);
                        actualizar_numero(nivel, vidas_actuales, tiempo_acumulado);

                        animacion_entrada_incorrecta();
                        return NIVEL_ERROR;
                    }

                    posicion++;
                    printf("Correcto\n");

                    if (posicion == nivel) {
                        float gastado = diff_ms(ahora_ms(), inicio) / 1000.0f;
                        printf("este es el tiempo gastado por el jugador: %.3f\n",
                               (double)gastado);
                        gpio_clr_mask(MASK_LED_RESP);

                        acumular_tiempo(diff_ms(ahora, inicio) / 1000.0f);
                        actualizar_numero(nivel, vidas_actuales, tiempo_acumulado);

                        animacion_nivel_superado();
                        return NIVEL_OK;
                    }
                }
            }

            estado_anterior[i] = actual;
        }
    }

    /* Se acabó el tiempo. */
    printf("este es el tiempo gastado por el jugador: %.3f\n",
           (double)(diff_ms(ahora_ms(), inicio) / 1000.0f));

    gpio_clr_mask(MASK_LED_RESP);

    acumular_tiempo(tiempo_total_seg);
    actualizar_numero(nivel, vidas_actuales, tiempo_acumulado);

    animacion_agotamiento_tiempo();
    return NIVEL_ERROR;
}

/* =================================================
 * UNA PARTIDA COMPLETA
 * ================================================= */

static void ciclo_juego(void) {

    /* El display conserva lo último mostrado hasta que se pulse inicio. */
    esperar_inicio();
    srand(time_us_32());

    /* Secuencia aleatoria 0..3 (get_rand_32 usa el oscilador en anillo). */
    for (uint8_t i = 0; i < 9; i++) {
        lista[i] = (int)(rand() % 4);
    }

    int  vidas = 3;
    int  nivel = 1;
    bool reiniciar_juego = false;
    tiempo_acumulado = 0.0f;

    actualizar_numero(nivel, vidas, tiempo_acumulado);

    while (nivel <= 9 && vidas > 0) {

        printf("Nivel: %d\n", nivel);
        printf("Vidas: %d\n", vidas);

        actualizar_numero(nivel, vidas, tiempo_acumulado);

        /* Tiempo entre LEDs según el nivel. */
        float tiempo;
        if      (nivel <= 2) tiempo = 0.5f;
        else if (nivel <= 4) tiempo = 0.3f;
        else if (nivel <= 6) tiempo = 0.25f;
        else if (nivel <= 8) tiempo = 0.2f;
        else                 tiempo = 0.166f;

        float time_resp = 1.25f * (2.0f * tiempo * nivel);
        printf("es el tiempo que tiene el jugador para meter el patron: %.3f\n",
               (double)time_resp);
        printf("este es el tiempo esperado de los leds: %.3f\n",
               (double)(tiempo * 2.0f * nivel));

        /* ---- Mostrar secuencia ---- */
        uint32_t inicio_seq = ahora_ms();

        for (int i = 0; i < nivel; i++) {

            /* Estado exacto de los 5 LEDs en una sola escritura. */
            leds_escribir(LED_MASK(lista[i]));

            if (esperar_tiempo(tiempo)) {
                reiniciar_juego = true;
                break;
            }

            apagar_leds();

            /* Pausa entre LEDs (también tras el último, igual que el
             * código Arduino original: la condición i < nivel siempre
             * se cumplía). */
            if (esperar_tiempo(tiempo)) {
                reiniciar_juego = true;
                break;
            }
        }

        printf("tiempo de la presentacion de los leds ejecutando: %.3f\n",
               (double)(diff_ms(ahora_ms(), inicio_seq) / 1000.0f));

        if (reiniciar_juego) break;

        /* ---- Tiempo de respuesta ---- */
        resultado_nivel_t resultado = esperar_con_parpadeo(time_resp, nivel, vidas);

        if (resultado == REINICIO_SOLICITADO) {
            reiniciar_juego = true;
            break;
        }

        if (resultado == NIVEL_OK) {
            printf("Nivel superado\n");
            nivel += 1;
        } else {
            /* Parpadea con el valor ANTERIOR y DESPUÉS se resta la vida. */
            animacion_perdida_vida();
            vidas -= 1;

            printf("Vidas restantes: %d\n", vidas);

            if (vidas > 0) {
                printf("Repitiendo nivel...\n");
            } else {
                printf("GAME OVER\n");
            }
        }

        actualizar_numero(nivel, vidas, tiempo_acumulado);
        apagar_leds();

        if (esperar_tiempo(0.4f)) {
            reiniciar_juego = true;
            break;
        }
    }

    if (reiniciar_juego) {
        printf("======================\n");
        printf("JUEGO REINICIADO\n");
        printf("======================\n");

        apagar_leds();
        actualizar_numero(1, 3, 0.0f);   /* el reinicio sí vuelve al estado inicial */

    } else if (nivel > 9) {
        printf("======================\n");
        printf("GANASTE!\n");
        printf("======================\n");
        printf("Tiempo acumulado final: %.2f\n", (double)tiempo_acumulado);

        animacion_victoria();
        apagar_leds();

        if (esperar_tiempo(2.0f)) {
            printf("Reinicio despues de ganar.\n");
        }

    } else {
        printf("======================\n");
        printf("GAME OVER\n");
        printf("======================\n");
        printf("Tiempo acumulado final: %.2f\n", (double)tiempo_acumulado);

        apagar_leds();

        if (esperar_tiempo(1.0f)) {
            printf("Reinicio despues de GAME OVER.\n");
        }
    }
}

/* =================================================
 * MAIN (núcleo 0)
 * ================================================= */

int main(void) {
    stdio_init_all();       /* USB CDC (UART desactivada en CMake: GPIO0/1 son botones) */

    gpio_configurar();

    /* Estado inicial: Nivel 1, 3 vidas, tiempo 00. */
    actualizar_numero(1, 3, 0.0f);

    /* Arranca el multiplexado del display en el núcleo 1. */
    multicore_launch_core1(core1_main);

    while (true) {
        ciclo_juego();
    }

    return 0;
}
