/**
 * @file simon_pico.ino
 * @brief Juego estilo "Simon" para Raspberry Pi Pico (RP2040), portado a
 *        Arduino IDE usando el core arduino-pico.
 *
 * @details
 * El programa utiliza los dos núcleos del RP2040:
 *  - Núcleo 0 (setup()/loop()): ejecuta toda la lógica del juego
 *    (secuencia de LEDs, lectura de botones, vidas, niveles,
 *    animaciones y botón de reinicio).
 *  - Núcleo 1 (setup1()/loop1()): se dedica exclusivamente a
 *    multiplexar el display de 7 segmentos de 4 dígitos de forma
 *    continua e independiente, de modo que el display nunca
 *    parpadea ni se congela, sin necesidad de invocar una función
 *    de multiplexado manualmente desde el resto del código.
 *
 * La comunicación entre núcleos se realiza mediante el arreglo
 * numero[4] (nivel, vidas, unidades de tiempo, décimas de tiempo),
 * declarado `volatile` porque el núcleo 0 lo escribe y el núcleo 1
 * lo lee de forma continua. En el RP2040 la escritura de una
 * palabra de 32 bits (un `int`) es atómica, por lo que no se
 * requiere un mutex para este caso de uso.
 *
 * @note Requiere el core "Raspberry Pi Pico/RP2040" de Earle
 *       Philhower (arduino-pico) instalado en el Arduino IDE, ya
 *       que es el que provee las funciones setup1()/loop1().
 */

/**
 * @name Configuración de pines
 * @{
 */

/**
 * @brief Pines de los LEDs del juego.
 *
 * leds[0..3] corresponden a los botones 0..3 (números 0..3) y
 * leds[4] es el LED de "respuesta" (pin 20).
 */
const uint8_t PIN_LEDS[5] = {20, 19, 18, 17, 16};

/**
 * @brief Pines de los botones del juego.
 *
 * Correspondencia:
 *  - Botón 0 -> LED 17 -> número 0 (leds[1])
 *  - Botón 1 -> LED 18 -> número 1 (leds[2])
 *  - Botón 2 -> LED 19 -> número 2 (leds[3])
 *  - Botón 3 -> LED 20 -> número 3 (leds[0])
 *  - Botón 4 -> inicio / reinicio
 *
 * Se conserva la misma correspondencia índice a índice que en el
 * original en MicroPython: botones[i] <-> leds[i], y es
 * leds[lista[i]] el que se enciende al mostrar la secuencia.
 */
const uint8_t PIN_BOTONES[5] = {0, 1, 2, 3, 4};

/** @brief Pines de los segmentos (a, b, c, d, e, f, g) del display de 7 segmentos. */
const uint8_t PIN_SEG[7] = {6, 7, 8, 9, 10, 11, 12};

/** @brief Pines de los dígitos (común de cada display) del display de 7 segmentos. */
const uint8_t PIN_DIG[4] = {13, 14, 15, 21};

/**
 * @brief Tabla de patrones de segmentos para cada número del 0 al 9.
 *
 * El índice 10 representa un dígito en blanco (todos los segmentos
 * apagados), usado en las animaciones de parpadeo del display.
 */
const uint8_t TABLA_7SEG[11][7] = {
    {1,1,1,1,1,1,0}, // 0
    {0,1,1,0,0,0,0}, // 1
    {1,1,0,1,1,0,1}, // 2
    {1,1,1,1,0,0,1}, // 3
    {0,1,1,0,0,1,1}, // 4
    {1,0,1,1,0,1,1}, // 5
    {1,0,1,1,1,1,1}, // 6
    {1,1,1,0,0,0,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,0,1,1}, // 9
    {0,0,0,0,0,0,0}  // 10 = blanco
};

/** @} */ // fin del grupo "Configuración de pines"

/**
 * @brief Valores mostrados actualmente en el display de 7 segmentos.
 *
 * - numero[0]: nivel actual.
 * - numero[1]: vidas restantes.
 * - numero[2]: unidades del tiempo transcurrido.
 * - numero[3]: décimas del tiempo transcurrido.
 *
 * Compartido entre núcleos: el núcleo 0 lo escribe y el núcleo 1 lo
 * lee de forma continua para refrescar el display. Se declara
 * `volatile` para evitar que el compilador cachee su valor.
 */
volatile int numero[4] = {0, 0, 0, 0};

/** @brief Tiempo (en ms) que permanece encendido cada dígito durante el multiplexado. */
const unsigned long INTERVALO_DISPLAY_MS = 2;

/** @brief Tiempo (en ms) de pulsación larga del botón 4 necesario para solicitar un reinicio. */
const unsigned long TIEMPO_REINICIO_MS = 2000;

/** @brief Límite máximo del tiempo acumulado de respuesta, mostrado en el display. */
const float TIEMPO_ACUMULADO_MAX = 9.9f;

/**
 * @name Variables de estado del juego
 * @brief Utilizadas exclusivamente en el núcleo 0.
 * @{
 */

/** @brief Tiempo (en s) entre LEDs al mostrar la secuencia del nivel actual. */
float tiempo = 0;

/** @brief Tiempo total (en s) disponible para que el jugador responda el nivel actual. */
float time_resp = 0;

/** @brief Vidas restantes del jugador en la partida actual. */
int vidas = 3;

/** @brief Secuencia aleatoria de botones (0-3) generada para la partida actual. */
int lista[10] = {0};

/**
 * @brief Tiempo acumulado (en s) que el jugador tarda en completar
 *        correctamente cada nivel.
 *
 * Se reinicia al comienzo de cada partida nueva.
 */
float tiempo_acumulado = 0;

/**
 * @brief Marca de tiempo (millis()) desde la que se mantiene
 *        presionado el botón de inicio/reinicio (botón 4).
 *
 * Un valor de -1 indica que el botón no está presionado.
 */
long boton4_desde = -1;

/** @} */ // fin del grupo "Variables de estado del juego"

/**
 * @brief Resultados posibles de esperarConParpadeo().
 */
enum ResultadoNivel {
    REINICIO_SOLICITADO = -1, /**< Se solicitó un reinicio durante la espera. */
    NIVEL_ERROR = 0,          /**< Respuesta incorrecta o se agotó el tiempo. */
    NIVEL_OK = 1              /**< El jugador completó el nivel correctamente. */
};

/**
 * @brief Calcula la diferencia entre dos marcas de tiempo en milisegundos.
 *
 * Equivalente a time.ticks_diff() de MicroPython. La resta de dos
 * valores `unsigned long` funciona correctamente incluso si
 * millis() da la vuelta (overflow), por lo que no se requiere
 * ningún manejo especial.
 *
 * @param ahora Marca de tiempo actual, en ms.
 * @param antes Marca de tiempo anterior, en ms.
 * @return Diferencia en milisegundos entre ambas marcas.
 */
inline unsigned long diffMs(unsigned long ahora, unsigned long antes) {
    return ahora - antes;
}

/**
 * @brief Apaga todos los LEDs del juego (incluido el LED de respuesta).
 *
 * Se ejecuta en el núcleo 0.
 */
void apagarLeds() {
    for (uint8_t i = 0; i < 5; i++) {
        digitalWrite(PIN_LEDS[i], LOW);
    }
}

/**
 * @brief Apaga (deshabilita) los cuatro dígitos del display de 7 segmentos.
 *
 * Se ejecuta en el núcleo 1, como parte del ciclo de multiplexado.
 */
void apagarDigitos() {
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(PIN_DIG[i], HIGH);
    }
}

/**
 * @brief Activa en los pines de segmentos el patrón correspondiente a un número.
 *
 * Se ejecuta en el núcleo 1.
 *
 * @param n Número a representar (0-9), o 10 para un dígito en blanco.
 */
void escribirSegmentos(int n) {
    for (uint8_t i = 0; i < 7; i++) {
        digitalWrite(PIN_SEG[i], TABLA_7SEG[n][i] ? LOW : HIGH);
    }
}

/**
 * @brief Multiplexa el display de 7 segmentos de forma continua.
 *
 * Debe llamarse repetidamente desde loop1() (núcleo 1). En cada
 * invocación, si transcurrió al menos INTERVALO_DISPLAY_MS desde el
 * último cambio, apaga todos los dígitos, escribe los segmentos del
 * dígito actual (numero[posicionDisplay]) y lo enciende, avanzando
 * de forma cíclica al siguiente dígito.
 *
 * @note Al ejecutarse en bucle continuo dentro de loop1(), no es
 *       necesario invocarla manualmente desde el resto del código,
 *       a diferencia de la versión original en MicroPython.
 */
void multiplexar() {
    static uint8_t posicionDisplay = 0;
    static unsigned long ultimoCambio = 0;

    unsigned long ahora = millis();

    if (diffMs(ahora, ultimoCambio) >= INTERVALO_DISPLAY_MS) {
        apagarDigitos();

        escribirSegmentos(numero[posicionDisplay]);

        digitalWrite(PIN_DIG[posicionDisplay], LOW);

        posicionDisplay++;
        if (posicionDisplay >= 4) {
            posicionDisplay = 0;
        }

        ultimoCambio = ahora;
    }
}

/**
 * @brief Actualiza los valores mostrados en el display de 7 segmentos.
 *
 * Escribe en el arreglo compartido `numero` (leído por el núcleo 1)
 * el nivel, las vidas y el tiempo acumulado, separando este último
 * en unidades y décimas y aplicando los límites de despliegue
 * correspondientes.
 *
 * @param nNivel  Nivel actual a mostrar (0-9; se limita a 9 si se excede).
 * @param nVidas  Vidas restantes a mostrar (0-9; se limita a 9 si se excede).
 * @param nTiempo Tiempo acumulado a mostrar, en segundos (se limita
 *                al rango [0, TIEMPO_ACUMULADO_MAX]).
 */
void actualizarNumero(int nNivel, int nVidas, float nTiempo) {
    if (nTiempo > TIEMPO_ACUMULADO_MAX) nTiempo = TIEMPO_ACUMULADO_MAX;
    if (nTiempo < 0) nTiempo = 0;

    int entero = (int)nTiempo;
    int decimal = (int)round((nTiempo - entero) * 10.0f);

    if (decimal >= 10) {
        decimal = 0;
        entero += 1;
    }
    if (entero > 9) entero = 9;

    numero[0] = (nNivel >= 0 && nNivel <= 9) ? nNivel : 9;
    numero[1] = (nVidas >= 0 && nVidas <= 9) ? nVidas : 9;
    numero[2] = entero;
    numero[3] = decimal;
}

/**
 * @brief Espera una cantidad de milisegundos sin bloquear el display.
 *
 * En la versión original en MicroPython, esta función también
 * invocaba la rutina de multiplexado para no congelar el display.
 * En esta versión el display se refresca de forma independiente en
 * el núcleo 1, por lo que basta con un delay() convencional.
 *
 * @param ms Milisegundos a esperar.
 */
void esperarMsConDisplay(unsigned long ms) {
    delay(ms);
}

/**
 * @brief Animación de nivel superado.
 *
 * Realiza un barrido rápido de un solo sentido, encendiendo los
 * LEDs 0 a 3 en secuencia.
 */
void animacionNivelSuperado() {
    apagarLeds();
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(PIN_LEDS[i], HIGH);
        esperarMsConDisplay(60);
        digitalWrite(PIN_LEDS[i], LOW);
    }
}

/**
 * @brief Animación de entrada incorrecta.
 *
 * Enciende simultáneamente los LEDs 0 a 3 y los hace parpadear dos veces.
 */
void animacionEntradaIncorrecta() {
    apagarLeds();
    for (uint8_t r = 0; r < 2; r++) {
        for (uint8_t i = 0; i < 4; i++) digitalWrite(PIN_LEDS[i], HIGH);
        esperarMsConDisplay(150);
        apagarLeds();
        esperarMsConDisplay(100);
    }
}

/**
 * @brief Animación de agotamiento del tiempo de respuesta.
 *
 * Mantiene el LED de respuesta (leds[4]) encendido de forma fija,
 * mientras hace parpadear los dígitos de tiempo (unidades y
 * décimas) del display de 7 segmentos.
 */
void animacionAgotamientoTiempo() {
    digitalWrite(PIN_LEDS[4], HIGH);

    int unidadesActual = numero[2];
    int decimasActual = numero[3];

    for (uint8_t r = 0; r < 4; r++) {
        numero[2] = 10; // blanco
        numero[3] = 10; // blanco
        esperarMsConDisplay(150);

        numero[2] = unidadesActual;
        numero[3] = decimasActual;
        esperarMsConDisplay(150);
    }

    digitalWrite(PIN_LEDS[4], LOW);
}

/**
 * @brief Animación de pérdida de vida.
 *
 * Hace parpadear únicamente el dígito de vidas del display de 7
 * segmentos, mostrando el valor anterior (antes de restar la vida).
 * Los demás dígitos (nivel y tiempo) siguen mostrándose con
 * normalidad, ya que el núcleo 1 continúa multiplexando el display
 * durante toda la animación.
 *
 * @param veces      Número de parpadeos a realizar (por defecto 3).
 * @param duracionMs Duración en milisegundos de cada fase del parpadeo (por defecto 150).
 */
void animacionPerdidaVida(uint8_t veces = 3, unsigned long duracionMs = 150) {
    int valorActual = numero[1];

    for (uint8_t r = 0; r < veces; r++) {
        numero[1] = 10; // blanco
        esperarMsConDisplay(duracionMs);

        numero[1] = valorActual;
        esperarMsConDisplay(duracionMs);
    }
}

/**
 * @brief Animación de finalización exitosa (partida ganada).
 *
 * Realiza un barrido de ida y vuelta (estilo "KITT") en dos
 * repeticiones, seguido de tres destellos con los cuatro LEDs
 * encendidos simultáneamente.
 */
void animacionVictoria() {
    apagarLeds();

    for (uint8_t r = 0; r < 2; r++) {
        for (int i = 0; i < 4; i++) {
            digitalWrite(PIN_LEDS[i], HIGH);
            esperarMsConDisplay(50);
            digitalWrite(PIN_LEDS[i], LOW);
        }
        for (int i = 2; i >= 0; i--) {
            digitalWrite(PIN_LEDS[i], HIGH);
            esperarMsConDisplay(50);
            digitalWrite(PIN_LEDS[i], LOW);
        }
    }

    for (uint8_t r = 0; r < 3; r++) {
        for (uint8_t i = 0; i < 4; i++) digitalWrite(PIN_LEDS[i], HIGH);
        esperarMsConDisplay(120);
        apagarLeds();
        esperarMsConDisplay(120);
    }
}

/**
 * @brief Suma un intervalo de tiempo al acumulador global de tiempo de respuesta.
 *
 * Se invoca sin importar si el intento del jugador fue correcto,
 * incorrecto, o si se agotó el tiempo, respetando siempre el límite
 * TIEMPO_ACUMULADO_MAX.
 *
 * @param segundos Cantidad de segundos a sumar al acumulador.
 */
void acumularTiempo(float segundos) {
    if (tiempo_acumulado < TIEMPO_ACUMULADO_MAX) {
        tiempo_acumulado += segundos;
        if (tiempo_acumulado > TIEMPO_ACUMULADO_MAX) {
            tiempo_acumulado = TIEMPO_ACUMULADO_MAX;
        }
    }
    Serial.print("Tiempo acumulado: ");
    Serial.println(tiempo_acumulado);
}

/**
 * @brief Revisa el estado del botón de reinicio (botón 4).
 *
 * Debe llamarse de forma continua durante todo el juego. Si el
 * botón permanece presionado durante al menos TIEMPO_REINICIO_MS,
 * se considera solicitado un reinicio: se apagan los LEDs, se
 * espera a que el usuario suelte el botón (para evitar reinicios en
 * cadena) y se retorna `true`.
 *
 * @note No invoca multiplexar(): el refresco del display lo realiza
 *       de forma autónoma el núcleo 1.
 *
 * @return `true` si se solicitó un reinicio, `false` en caso contrario.
 */
bool revisarBotonReinicio() {
    unsigned long ahora = millis();

    if (digitalRead(PIN_BOTONES[4]) == HIGH) {

        if (boton4_desde == -1) {
            boton4_desde = ahora;
        } else if (diffMs(ahora, (unsigned long)boton4_desde) >= TIEMPO_REINICIO_MS) {

            Serial.println("REINICIO SOLICITADO");
            apagarLeds();

            // Esperar a que el usuario suelte el botón, para que no
            // vuelva a iniciar solo.
            while (digitalRead(PIN_BOTONES[4]) == HIGH) {
                delay(10);
            }

            boton4_desde = -1;
            return true;
        }
    } else {
        boton4_desde = -1;
    }

    return false;
}

/**
 * @brief Espera un tiempo determinado, revisando continuamente el botón de reinicio.
 *
 * @param tiempoTotalSeg Tiempo total a esperar, en segundos.
 * @return `true` si durante la espera se solicitó un reinicio, `false` en caso contrario.
 */
bool esperarTiempo(float tiempoTotalSeg) {
    unsigned long inicio = millis();
    unsigned long totalMs = (unsigned long)(tiempoTotalSeg * 1000.0f);

    while (diffMs(millis(), inicio) < totalMs) {
        if (revisarBotonReinicio()) {
            return true;
        }
        delay(5);
    }
    return false;
}

/**
 * @brief Espera a que el jugador inicie una nueva partida.
 *
 * Una pulsación corta del botón 4 inicia el juego; una pulsación
 * larga (>= TIEMPO_REINICIO_MS) no lo inicia y vuelve a esperar una
 * nueva pulsación. Si al entrar el botón ya está presionado (por
 * ejemplo, justo después de un reinicio), primero se obliga a
 * soltarlo antes de aceptar nuevas pulsaciones.
 *
 * Esta función bloquea hasta que se detecta una pulsación corta
 * válida.
 */
void esperarInicio() {
    Serial.println("Esperando inicio...");
    apagarLeds();

    while (digitalRead(PIN_BOTONES[4]) == HIGH) {
        delay(10);
    }
    Serial.println("Boton liberado. Presione para comenzar.");

    while (true) {
        if (digitalRead(PIN_BOTONES[4]) == HIGH) {

            unsigned long inicioPulsacion = millis();
            bool pulsacionLarga = false;

            while (digitalRead(PIN_BOTONES[4]) == HIGH) {
                unsigned long ahora = millis();

                if (diffMs(ahora, inicioPulsacion) >= TIEMPO_REINICIO_MS) {
                    Serial.println("Pulsacion larga.");
                    Serial.println("No se inicia el juego.");

                    apagarLeds();

                    while (digitalRead(PIN_BOTONES[4]) == HIGH) {
                        delay(5);
                    }

                    pulsacionLarga = true;
                    break;
                }
                delay(5);
            }

            if (!pulsacionLarga) {
                // El botón se soltó antes de los 2 segundos:
                // pulsación corta -> iniciar juego.
                Serial.println("INICIANDO JUEGO");
                return;
            }
            // Si fue pulsación larga, se vuelve a esperar una nueva
            // pulsación (continúa el while(true)).
        }
    }
}

/**
 * @brief Espera la respuesta del jugador durante la fase de entrada de un nivel.
 *
 * Hace parpadear el LED de respuesta a una frecuencia que aumenta
 * conforme avanza el tiempo disponible, y va leyendo las
 * pulsaciones de los botones 0-3, comparándolas contra la secuencia
 * objetivo (`lista`). También revisa continuamente el botón de
 * reinicio.
 *
 * @param tiempoTotalSeg Tiempo total disponible para responder, en segundos.
 * @param nivel          Nivel actual (longitud de la secuencia a repetir).
 * @param vidasActuales  Vidas restantes del jugador, usadas para actualizar el display.
 *
 * @return
 *  - NIVEL_ERROR si la respuesta fue incorrecta o se agotó el tiempo.
 *  - NIVEL_OK si el jugador completó correctamente la secuencia del nivel.
 *  - REINICIO_SOLICITADO si se solicitó un reinicio durante la espera.
 */
int esperarConParpadeo(float tiempoTotalSeg, int nivel, int vidasActuales) {
    unsigned long inicio = millis();
    unsigned long ultimoCambio = inicio;
    unsigned long totalMs = (unsigned long)(tiempoTotalSeg * 1000.0f);

    // Estado del LED de respuesta. Arranca ENCENDIDO para que el
    // jugador vea de inmediato que puede empezar a ingresar el
    // patrón.
    int estadoLed = 1;
    digitalWrite(PIN_LEDS[4], HIGH);

    bool estadoAnterior[4] = {false, false, false, false};
    int posicion = 0;
    unsigned long ultimaPulsacion = millis();

    // Momento (millis) en que se presionó cada botón (0-3). -1
    // significa "no está encendido / no aplica". Es un arreglo para
    // poder apagar varios LEDs de forma independiente aunque se
    // hayan presionado casi al mismo tiempo.
    long tiempoLed[4] = {-1, -1, -1, -1};

    while (diffMs(millis(), inicio) < totalMs) {

        unsigned long ahora = millis();

        if (revisarBotonReinicio()) {
            return REINICIO_SOLICITADO;
        }

        float transcurrido = diffMs(ahora, inicio) / 1000.0f;
        float progreso = transcurrido / tiempoTotalSeg;

        // Actualiza el display con el tiempo en vivo: lo ya
        // acumulado en niveles anteriores más lo que lleva este
        // intento.
        actualizarNumero(nivel, vidasActuales, tiempo_acumulado + transcurrido);

        // Intervalo dinámico de parpadeo del LED de respuesta.
        float intervalo = (0.5f - (0.45f * progreso)) / 2.0f;

        if (diffMs(ahora, ultimoCambio) >= (unsigned long)(intervalo * 1000.0f)) {
            estadoLed = (estadoLed == 0) ? 1 : 0;
            digitalWrite(PIN_LEDS[4], estadoLed);
            ultimoCambio = ahora;
        }

        // Apaga los LEDs de pulsación, cada uno con su propio
        // tiempo, sin perder el rastro de ninguno.
        for (uint8_t j = 0; j < 4; j++) {
            if (tiempoLed[j] != -1) {
                if (diffMs(ahora, (unsigned long)tiempoLed[j]) >= 350) {
                    digitalWrite(PIN_LEDS[j], LOW);
                    tiempoLed[j] = -1;
                }
            }
        }

        // Lectura de los botones 0-3.
        for (uint8_t i = 0; i < 4; i++) {

            bool estadoActual = digitalRead(PIN_BOTONES[i]) == HIGH;

            // Detecta flanco 0 -> 1.
            if (estadoActual && !estadoAnterior[i]) {

                // Antirrebote (debounce).
                if (diffMs(ahora, ultimaPulsacion) >= 40) {
                    ultimaPulsacion = ahora;

                    // Enciende el LED correspondiente.
                    digitalWrite(PIN_LEDS[i], HIGH);
                    tiempoLed[i] = ahora;

                    int respuesta = i;

                    // Comprueba la respuesta.
                    if (respuesta != lista[posicion]) {
                        Serial.println("ERROR");
                        digitalWrite(PIN_LEDS[4], LOW);

                        float tiempoNivel = diffMs(ahora, inicio) / 1000.0f;
                        acumularTiempo(tiempoNivel);
                        actualizarNumero(nivel, vidasActuales, tiempo_acumulado);

                        animacionEntradaIncorrecta();
                        return NIVEL_ERROR;
                    }

                    // Respuesta correcta.
                    posicion++;
                    Serial.println("Correcto");

                    // ¿Terminó el nivel?
                    if (posicion == nivel) {
                        digitalWrite(PIN_LEDS[4], LOW);

                        float tiempoNivel = diffMs(ahora, inicio) / 1000.0f;
                        acumularTiempo(tiempoNivel);
                        actualizarNumero(nivel, vidasActuales, tiempo_acumulado);

                        animacionNivelSuperado();
                        return NIVEL_OK;
                    }
                }
            }

            estadoAnterior[i] = estadoActual;
        }
    }

    // Se acabó el tiempo disponible.
    digitalWrite(PIN_LEDS[4], LOW);

    acumularTiempo(tiempoTotalSeg);
    actualizarNumero(nivel, vidasActuales, tiempo_acumulado);

    animacionAgotamientoTiempo();
    return NIVEL_ERROR;
}

/**
 * @brief Inicialización del núcleo 0.
 *
 * Configura el puerto serie, inicializa la semilla del generador
 * aleatorio, configura los pines de LEDs y botones, y deja el
 * display en el estado inicial (nivel 1, 3 vidas, tiempo 00),
 * listo para que el jugador presione el botón de inicio.
 */
void setup() {
    Serial.begin(115200);

    randomSeed(micros());

    for (uint8_t i = 0; i < 5; i++) {
        pinMode(PIN_LEDS[i], OUTPUT);
        digitalWrite(PIN_LEDS[i], LOW);
    }

    for (uint8_t i = 0; i < 5; i++) {
        pinMode(PIN_BOTONES[i], INPUT_PULLDOWN);
    }

    actualizarNumero(1, 3, 0);
}

/**
 * @brief Bucle principal del juego, ejecutado en el núcleo 0.
 *
 * Cada iteración representa una partida completa: espera el inicio,
 * genera una secuencia aleatoria, recorre los niveles mostrando la
 * secuencia y evaluando la respuesta del jugador, y finalmente
 * gestiona el desenlace de la partida (reinicio manual, victoria o
 * game over).
 */
void loop() {

    // Espera a que el jugador presione el botón de inicio. El
    // display no se modifica aquí: conserva lo último mostrado
    // (estado inicial o resultado de la partida anterior) hasta que
    // el jugador presione para iniciar una nueva partida.
    esperarInicio();

    // Genera la secuencia aleatoria de la partida.
    for (uint8_t i = 0; i < 9; i++) {
        lista[i] = random(0, 4); // 0..3
    }

    // Reinicia vidas y tiempo acumulado (nueva partida).
    vidas = 3;
    tiempo_acumulado = 0;

    int nivel = 1;
    bool reiniciarJuego = false;

    actualizarNumero(nivel, vidas, tiempo_acumulado);

    // Bucle de niveles.
    while (nivel <= 9 && vidas > 0) {

        Serial.print("Nivel: "); Serial.println(nivel);
        Serial.print("Vidas: "); Serial.println(vidas);

        actualizarNumero(nivel, vidas, tiempo_acumulado);

        // Determina el tiempo entre LEDs según el nivel actual.
        if (nivel <= 2)      tiempo = 0.5f;
        else if (nivel <= 4) tiempo = 0.3f;
        else if (nivel <= 6) tiempo = 0.25f;
        else if (nivel <= 8) tiempo = 0.2f;
        else                 tiempo = 0.166f;

        time_resp = 1.25f * (2.0f * tiempo * nivel);

        // Muestra la secuencia del nivel actual.
        for (uint8_t i = 0; i < (uint8_t)nivel; i++) {

            digitalWrite(PIN_LEDS[lista[i]], HIGH);

            if (esperarTiempo(tiempo)) {
                reiniciarJuego = true;
                break;
            }

            apagarLeds();

            // Pausa entre un LED y el siguiente. No se aplica tras
            // el último LED, para que la fase de respuesta comience
            // de inmediato.
            if (i < (uint8_t)(nivel - 1)) {
                if (esperarTiempo(tiempo)) {
                    reiniciarJuego = true;
                    break;
                }
            }
        }

        if (reiniciarJuego) break;

        // Fase de espera de la respuesta del jugador.
        int resultado = esperarConParpadeo(time_resp, nivel, vidas);

        if (resultado == REINICIO_SOLICITADO) {
            reiniciarJuego = true;
            break;
        }

        if (resultado == NIVEL_OK) {
            Serial.println("Nivel superado");
            nivel += 1;
        } else {
            // Animación de pérdida de vida: parpadea con el valor
            // anterior, y después se resta la vida.
            animacionPerdidaVida();
            vidas -= 1;

            Serial.print("Vidas restantes: "); Serial.println(vidas);

            if (vidas > 0) {
                Serial.println("Repitiendo nivel...");
            } else {
                Serial.println("GAME OVER");
            }
        }

        actualizarNumero(nivel, vidas, tiempo_acumulado);
        apagarLeds();

        if (esperarTiempo(0.4f)) {
            reiniciarJuego = true;
            break;
        }
    }

    // Desenlace de la partida.
    if (reiniciarJuego) {
        Serial.println("======================");
        Serial.println("JUEGO REINICIADO");
        Serial.println("======================");

        apagarLeds();

        // A diferencia de GAME OVER / GANASTE, el reinicio manual sí
        // vuelve al estado inicial.
        actualizarNumero(1, 3, 0);

        // loop() volverá a llamar a esperarInicio(), así que el
        // juego queda detenido hasta una nueva pulsación corta.
    }
    else if (nivel > 9) {
        Serial.println("======================");
        Serial.println("GANASTE!");
        Serial.println("======================");
        Serial.print("Tiempo acumulado final: ");
        Serial.println(tiempo_acumulado);

        animacionVictoria();
        apagarLeds();

        if (esperarTiempo(2.0f)) {
            Serial.println("Reinicio despues de ganar.");
        }
    }
    else {
        Serial.println("======================");
        Serial.println("GAME OVER");
        Serial.println("======================");
        Serial.print("Tiempo acumulado final: ");
        Serial.println(tiempo_acumulado);

        apagarLeds();

        if (esperarTiempo(1.0f)) {
            Serial.println("Reinicio despues de GAME OVER.");
        }
    }

    // loop() termina aquí y Arduino lo vuelve a llamar,
    // equivalente a una nueva vuelta del while(True) externo del
    // programa original.
}

/**
 * @brief Inicialización del núcleo 1.
 *
 * Configura como salidas los pines de segmentos y de dígitos del
 * display de 7 segmentos, y apaga inicialmente todos los dígitos.
 */
void setup1() {
    for (uint8_t i = 0; i < 7; i++) {
        pinMode(PIN_SEG[i], OUTPUT);
    }
    for (uint8_t i = 0; i < 4; i++) {
        pinMode(PIN_DIG[i], OUTPUT);
    }
    apagarDigitos();
}

/**
 * @brief Bucle del núcleo 1: multiplexa continuamente el display de 7 segmentos.
 *
 * Es el único responsable del refresco del display; se ejecuta de
 * forma totalmente independiente de la lógica del juego en el
 * núcleo 0.
 */
void loop1() {
    multiplexar();
}
