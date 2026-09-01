#include "pico/stdlib.h"
#include <stdio.h>

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

// --------------------------------------------------
// Frecuencia inicial
// --------------------------------------------------

float frecuencia = 15.0f;


// --------------------------------------------------
// Buffer para recibir la frecuencia por USB
// --------------------------------------------------

char buffer[32];
int buffer_pos = 0;


// --------------------------------------------------
// Inicializar LED
// --------------------------------------------------

int pico_led_init(void) {

#if defined(PICO_DEFAULT_LED_PIN)

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    return PICO_OK;

#elif defined(CYW43_WL_GPIO_LED_PIN)

    return cyw43_arch_init();

#endif
}


// --------------------------------------------------
// Encender o apagar LED
// --------------------------------------------------

void pico_set_led(bool led_on) {

#if defined(PICO_DEFAULT_LED_PIN)

    gpio_put(PICO_DEFAULT_LED_PIN, led_on);

#elif defined(CYW43_WL_GPIO_LED_PIN)

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);

#endif
}


// --------------------------------------------------
// PROGRAMA PRINCIPAL
// --------------------------------------------------

int main() {

    // --------------------------------------------------
    // Inicializar USB / Serial
    // --------------------------------------------------

    stdio_init_all();

    // Esperar a que el USB se inicialice
    sleep_ms(3000);


    // --------------------------------------------------
    // Mensaje inicial
    // --------------------------------------------------

    printf("\n");
    printf("=================================\n");
    printf("      CONTROL DE FRECUENCIA\n");
    printf("=================================\n");

    printf("Frecuencia inicial: %.2f Hz\n", frecuencia);

    printf("Escriba una nueva frecuencia\n");
    printf("y presione ENTER.\n");

    printf("=================================\n");


    // --------------------------------------------------
    // Inicializar LED
    // --------------------------------------------------

    int rc = pico_led_init();

    hard_assert(rc == PICO_OK);


    // Estado inicial del LED
    bool estado_led = false;


    // Guardar momento del último cambio
    uint64_t ultimo_cambio = time_us_64();


    // --------------------------------------------------
    // BUCLE PRINCIPAL
    // --------------------------------------------------

    while (true) {


        // ==================================================
        // 1. REVISAR SI LLEGÓ UN CARÁCTER POR USB
        // ==================================================

        int c = getchar_timeout_us(0);


        if (c != PICO_ERROR_TIMEOUT) {


            // --------------------------------------------------
            // Si llegó ENTER
            // --------------------------------------------------

            if (c == '\r' || c == '\n') {


                // Comprobar que haya algo escrito
                if (buffer_pos > 0) {


                    // --------------------------------------------------
                    // Terminar la cadena
                    // --------------------------------------------------

                    buffer[buffer_pos] = '\0';


                    // Variable donde guardaremos la frecuencia
                    float nueva_frecuencia;


                    // --------------------------------------------------
                    // Convertir el texto recibido a número
                    // --------------------------------------------------

                    if (sscanf(buffer, "%f", &nueva_frecuencia) == 1) {


                        // --------------------------------------------------
                        // Comprobar que la frecuencia sea válida
                        // --------------------------------------------------

                        if (nueva_frecuencia > 0) {


                            // Actualizar frecuencia
                            frecuencia = nueva_frecuencia;


                            // Mostrar nueva frecuencia
                            printf(
                                "Nueva frecuencia: %.2f Hz\n",
                                frecuencia
                            );


                            // Reiniciar temporizador
                            ultimo_cambio = time_us_64();


                        } else {


                            printf(
                                "La frecuencia debe ser mayor que 0\n"
                            );
                        }


                    } else {


                        // El usuario escribió algo que no es un número
                        printf(
                            "Entrada no válida: %s\n",
                            buffer
                        );
                    }


                    // --------------------------------------------------
                    // Limpiar el buffer
                    // --------------------------------------------------

                    buffer_pos = 0;
                }
            }


            // --------------------------------------------------
            // Si NO es ENTER
            // Guardamos el carácter en el buffer
            // --------------------------------------------------

            else {


                // Comprobar que todavía haya espacio
                if (buffer_pos < sizeof(buffer) - 1) {


                    buffer[buffer_pos] = (char)c;

                    buffer_pos++;
                }
            }
        }


        // ==================================================
        // 2. CALCULAR MEDIO PERÍODO
        // ==================================================

        uint64_t mitad_periodo_us =
            (uint64_t)(
                1000000.0f /
                (2.0f * frecuencia)
            );


        // ==================================================
        // 3. COMPROBAR SI DEBEMOS CAMBIAR EL LED
        // ==================================================

        uint64_t ahora = time_us_64();


        if ((ahora - ultimo_cambio) >= mitad_periodo_us) {


            // Cambiar estado del LED
            estado_led = !estado_led;


            // Encender/apagar LED
            pico_set_led(estado_led);


            // Guardar momento del cambio
            ultimo_cambio = ahora;
        }
    }
}