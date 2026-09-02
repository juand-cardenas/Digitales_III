/*
  =================================================
  JUEGO ESTILO "SIMON" - Raspberry Pi Pico
  Puerto a Arduino IDE (arduino-pico core)
  =================================================

  Núcleo 0 (setup / loop):
      - Toda la lógica del juego (LEDs de patrón,
        botones, vidas, niveles, animaciones,
        botón de reinicio).

  Núcleo 1 (setup1 / loop1):
      - SOLO se encarga de multiplexar el display
        de 7 segmentos (4 dígitos), de forma
        continua e independiente.
      - Así el display NUNCA parpadea ni se
        congela, sin necesidad de llamar a
        multiplexar() manualmente desde el resto
        del código (como se hacía en la versión
        MicroPython).

  Comunicación entre núcleos:
      - El arreglo "numero[4]" (nivel, vidas,
        unidades de tiempo, décimas de tiempo)
        se declara "volatile" porque el núcleo 0
        lo escribe y el núcleo 1 lo lee
        constantemente. En el RP2040 la escritura
        de una palabra de 32 bits (un int) es
        atómica, así que no hace falta un mutex
        para este caso de uso.

  Requiere el core "Raspberry Pi Pico/RP2040" de
  Earle Philhower (arduino-pico) instalado en el
  Arduino IDE, que es el que provee setup1()/loop1().
*/

// =================================================
// CONFIGURACIÓN DE PINES
// =================================================

// LEDs (patrón + LED de respuesta)
// leds[0..3] -> botones 0..3 -> números 0..3
// leds[4]    -> LED de "respuesta" (pin 20)
const uint8_t PIN_LEDS[5] = {20, 19, 18, 17, 16};

// Botones
// Botón 0 -> LED 17 -> número 0   (leds[1])
// Botón 1 -> LED 18 -> número 1   (leds[2])
// Botón 2 -> LED 19 -> número 2   (leds[3])
// Botón 3 -> LED 20 -> número 3   (leds[0])  (ver nota abajo)
// Botón 4 -> INICIO / REINICIO
//
// NOTA: se conserva exactamente la misma
// correspondencia índice-a-índice que en el
// original MicroPython: botones[i] <-> leds[i],
// y es leds[lista[i]] el que se enciende al
// mostrar la secuencia, igual que antes.
const uint8_t PIN_BOTONES[5] = {0, 1, 2, 3, 4};

// Display de 7 segmentos: segmentos (a,b,c,d,e,f,g)
const uint8_t PIN_SEG[7] = {6, 7, 8, 9, 10, 11, 12};

// Display de 7 segmentos: dígitos (común de cada display)
const uint8_t PIN_DIG[4] = {13, 14, 15, 21};

// Tabla de patrones para cada número 0-9
// (10 = dígito en blanco, todos los segmentos apagados)
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

// -------------------------------------------------
// numero[0] -> Nivel actual
// numero[1] -> Vidas restantes
// numero[2] -> Unidades del tiempo transcurrido
// numero[3] -> Décimas del tiempo transcurrido
//
// Compartido entre núcleos: lo escribe el núcleo 0,
// lo lee continuamente el núcleo 1.
// -------------------------------------------------
volatile int numero[4] = {0, 0, 0, 0};

// Tiempo que permanece encendido cada dígito (ms)
const unsigned long INTERVALO_DISPLAY_MS = 2;

// Tiempo necesario para reiniciar (pulsación larga)
const unsigned long TIEMPO_REINICIO_MS = 2000;

// Límite máximo del tiempo acumulado de respuesta
const float TIEMPO_ACUMULADO_MAX = 9.9f;

// =================================================
// VARIABLES DEL JUEGO (usadas SOLO en el núcleo 0)
// =================================================

float tiempo = 0;
float time_resp = 0;
int vidas = 3;

int lista[10] = {0};

// Tiempo acumulado que el jugador tarda en
// completar correctamente cada nivel. Se reinicia
// en cada partida nueva.
float tiempo_acumulado = 0;

// Guarda desde cuándo se está presionando el botón
// de inicio/reinicio (botón 4). -1 = no presionado.
long boton4_desde = -1;

// Resultados posibles de esperar_con_parpadeo()
enum ResultadoNivel { REINICIO_SOLICITADO = -1, NIVEL_ERROR = 0, NIVEL_OK = 1 };

// =================================================
// UTILIDADES DE TIEMPO (equivalentes a time.ticks_diff)
//
// La resta de dos unsigned long funciona
// correctamente incluso si millis() da la vuelta
// (overflow), por eso no hace falta nada especial.
// =================================================

inline unsigned long diffMs(unsigned long ahora, unsigned long antes) {
    return ahora - antes;
}

// =================================================
// APAGAR TODOS LOS LEDS (núcleo 0)
// =================================================

void apagarLeds() {
    for (uint8_t i = 0; i < 5; i++) {
        digitalWrite(PIN_LEDS[i], LOW);
    }
}

// =================================================
// DISPLAY: APAGAR TODOS LOS DÍGITOS (núcleo 1)
// =================================================

void apagarDigitos() {
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(PIN_DIG[i], HIGH);
    }
}

// =================================================
// DISPLAY: ESCRIBIR SEGMENTOS DE UN NÚMERO (0-10)
// (núcleo 1)
// =================================================

void escribirSegmentos(int n) {
    for (uint8_t i = 0; i < 7; i++) {
        digitalWrite(PIN_SEG[i], TABLA_7SEG[n][i] ? LOW : HIGH);
    }
}

// =================================================
// DISPLAY: MULTIPLEXAR (SOLO se llama desde loop1,
// núcleo 1). Se ejecuta en bucle continuo, así que
// no necesita ser invocada manualmente desde el
// resto del código como en la versión MicroPython.
// =================================================

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

// =================================================
// DISPLAY: ACTUALIZAR NIVEL / VIDAS / TIEMPO
// (núcleo 0 -> escribe en "numero", que lee núcleo 1)
// =================================================

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

// =================================================
// ESPERAR X MILISEGUNDOS
//
// En la versión MicroPython esta función también
// llamaba a multiplexar() para no congelar el
// display. Aquí el display se refresca solo, en el
// núcleo 1, así que basta con un delay() normal.
// =================================================

void esperarMsConDisplay(unsigned long ms) {
    delay(ms);
}

// =================================================
// ANIMACIÓN: SECUENCIA CORRECTA (NIVEL SUPERADO)
// Barrido rápido, una sola pasada, LED 0 -> LED 3.
// =================================================

void animacionNivelSuperado() {
    apagarLeds();
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(PIN_LEDS[i], HIGH);
        esperarMsConDisplay(60);
        digitalWrite(PIN_LEDS[i], LOW);
    }
}

// =================================================
// ANIMACIÓN: ENTRADA INCORRECTA
// Los 4 LEDs (0-3) encienden juntos y parpadean 2 veces.
// =================================================

void animacionEntradaIncorrecta() {
    apagarLeds();
    for (uint8_t r = 0; r < 2; r++) {
        for (uint8_t i = 0; i < 4; i++) digitalWrite(PIN_LEDS[i], HIGH);
        esperarMsConDisplay(150);
        apagarLeds();
        esperarMsConDisplay(100);
    }
}

// =================================================
// ANIMACIÓN: SE AGOTÓ EL TIEMPO
// El LED de respuesta (leds[4]) se queda ENCENDIDO
// fijo, y son los dígitos de TIEMPO (unidades y
// décimas) los que parpadean en el 7 segmentos.
// =================================================

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

// =================================================
// ANIMACIÓN: PÉRDIDA DE VIDA
// Parpadea SOLO el dígito de vidas en el 7 segmentos
// (con el valor ANTERIOR, antes de bajarlo). Los
// otros 3 dígitos (nivel y tiempo) siguen
// mostrándose normalmente porque el núcleo 1 sigue
// multiplexando todo el rato.
// =================================================

void animacionPerdidaVida(uint8_t veces = 3, unsigned long duracionMs = 150) {
    int valorActual = numero[1];

    for (uint8_t r = 0; r < veces; r++) {
        numero[1] = 10; // blanco
        esperarMsConDisplay(duracionMs);

        numero[1] = valorActual;
        esperarMsConDisplay(duracionMs);
    }
}

// =================================================
// ANIMACIÓN: FINALIZACIÓN EXITOSA (GANASTE)
// Barrido ida y vuelta (tipo "KITT") x2, seguido de
// 3 destellos con los 4 LEDs juntos.
// =================================================

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

// =================================================
// ACUMULAR TIEMPO DE RESPUESTA
// Suma "segundos" al acumulador global, sin
// sobrepasar TIEMPO_ACUMULADO_MAX. Se usa sin
// importar si el intento fue correcto, incorrecto,
// o si se agotó el tiempo.
// =================================================

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

// =================================================
// REVISAR BOTÓN DE REINICIO (botón 4)
// Devuelve true si se solicitó reinicio.
// Se llama continuamente durante todo el juego.
// (Ya NO llama a multiplexar(): eso lo hace solo
// el núcleo 1 en segundo plano.)
// =================================================

bool revisarBotonReinicio() {
    unsigned long ahora = millis();

    if (digitalRead(PIN_BOTONES[4]) == HIGH) {

        if (boton4_desde == -1) {
            boton4_desde = ahora;
        } else if (diffMs(ahora, (unsigned long)boton4_desde) >= TIEMPO_REINICIO_MS) {

            Serial.println("REINICIO SOLICITADO");
            apagarLeds();

            // Esperar a que el usuario SUELTE el botón,
            // para que no vuelva a iniciar solo.
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

// =================================================
// ESPERAR UN TIEMPO (en segundos), revisando
// continuamente el botón de reinicio.
// Devuelve true si se pidió reinicio.
// =================================================

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

// =================================================
// ESPERAR PARA INICIAR EL JUEGO
//
// Pulsación corta: inicia el juego.
// Pulsación larga:  NO inicia, espera una nueva.
//
// Si el botón quedó presionado al entrar (p.ej.
// después de un reinicio), primero obliga a soltarlo.
// =================================================

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
            // Si fue pulsación larga, se vuelve a esperar
            // una nueva pulsación (continúa el while(true)).
        }
    }
}

// =================================================
// ESPERAR MIENTRAS EL LED DE RESPUESTA PARPADEA
// Y RECIBIR RESPUESTAS
//
// Devuelve:
//   NIVEL_ERROR         -> error / tiempo agotado
//   NIVEL_OK            -> nivel completado
//   REINICIO_SOLICITADO -> se pidió reinicio
// =================================================

int esperarConParpadeo(float tiempoTotalSeg, int nivel, int vidasActuales) {
    unsigned long inicio = millis();
    unsigned long ultimoCambio = inicio;
    unsigned long totalMs = (unsigned long)(tiempoTotalSeg * 1000.0f);

    // Estado del LED de respuesta.
    // Arranca ENCENDIDO para que el jugador vea de
    // inmediato que puede empezar a ingresar el patrón.
    int estadoLed = 1;
    digitalWrite(PIN_LEDS[4], HIGH);

    bool estadoAnterior[4] = {false, false, false, false};
    int posicion = 0;
    unsigned long ultimaPulsacion = millis();

    // Momento (millis) en que se presionó cada botón
    // (0-3). -1 significa "no está encendido / no
    // aplica". Es un arreglo para poder apagar varios
    // LEDs de forma independiente aunque se hayan
    // presionado casi al mismo tiempo.
    long tiempoLed[4] = {-1, -1, -1, -1};

    while (diffMs(millis(), inicio) < totalMs) {

        unsigned long ahora = millis();

        // =========================================
        // REVISAR BOTÓN DE REINICIO
        // =========================================
        if (revisarBotonReinicio()) {
            return REINICIO_SOLICITADO;
        }

        // =========================================
        // TIEMPO TRANSCURRIDO / PROGRESO
        // =========================================
        float transcurrido = diffMs(ahora, inicio) / 1000.0f;
        float progreso = transcurrido / tiempoTotalSeg;

        // =========================================
        // DISPLAY: TIEMPO EN VIVO
        // (lo ya acumulado en niveles anteriores +
        // lo que se lleva en este intento)
        // =========================================
        actualizarNumero(nivel, vidasActuales, tiempo_acumulado + transcurrido);

        // =========================================
        // INTERVALO DINÁMICO DEL LED DE RESPUESTA
        // =========================================
        float intervalo = (0.5f - (0.45f * progreso)) / 2.0f;

        // =========================================
        // PARPADEO DEL LED DE RESPUESTA
        // =========================================
        if (diffMs(ahora, ultimoCambio) >= (unsigned long)(intervalo * 1000.0f)) {
            estadoLed = (estadoLed == 0) ? 1 : 0;
            digitalWrite(PIN_LEDS[4], estadoLed);
            ultimoCambio = ahora;
        }

        // =========================================
        // APAGAR LEDS DE PULSACIÓN (cada uno con su
        // propio tiempo, sin perder el rastro de
        // ninguno)
        // =========================================
        for (uint8_t j = 0; j < 4; j++) {
            if (tiempoLed[j] != -1) {
                if (diffMs(ahora, (unsigned long)tiempoLed[j]) >= 350) {
                    digitalWrite(PIN_LEDS[j], LOW);
                    tiempoLed[j] = -1;
                }
            }
        }

        // =========================================
        // LEER BOTONES 0-3
        // =========================================
        for (uint8_t i = 0; i < 4; i++) {

            bool estadoActual = digitalRead(PIN_BOTONES[i]) == HIGH;

            // Detectar flanco 0 -> 1
            if (estadoActual && !estadoAnterior[i]) {

                // Debounce
                if (diffMs(ahora, ultimaPulsacion) >= 40) {
                    ultimaPulsacion = ahora;

                    // Encender LED correspondiente
                    digitalWrite(PIN_LEDS[i], HIGH);
                    tiempoLed[i] = ahora;

                    int respuesta = i;

                    // Comprobar respuesta
                    if (respuesta != lista[posicion]) {
                        Serial.println("ERROR");
                        digitalWrite(PIN_LEDS[4], LOW);

                        float tiempoNivel = diffMs(ahora, inicio) / 1000.0f;
                        acumularTiempo(tiempoNivel);
                        actualizarNumero(nivel, vidasActuales, tiempo_acumulado);

                        animacionEntradaIncorrecta();
                        return NIVEL_ERROR;
                    }

                    // Respuesta correcta
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

    // =============================================
    // SE ACABÓ EL TIEMPO
    // =============================================
    digitalWrite(PIN_LEDS[4], LOW);

    acumularTiempo(tiempoTotalSeg);
    actualizarNumero(nivel, vidasActuales, tiempo_acumulado);

    animacionAgotamientoTiempo();
    return NIVEL_ERROR;
}

// =================================================
// NÚCLEO 0: PROGRAMA PRINCIPAL DEL JUEGO
// =================================================

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

    // Estado inicial al energizar: Nivel 1, 3 Vidas,
    // Tiempo 00, listo para que el jugador presione
    // para iniciar.
    actualizarNumero(1, 3, 0);
}

void loop() {

    // =============================================
    // ESPERAR AL BOTÓN 4 PARA INICIAR
    //
    // El display NO se toca aquí: debe conservar lo
    // último mostrado (estado inicial o resultado
    // del juego anterior) hasta que el jugador
    // presione para iniciar una nueva partida.
    // =============================================
    esperarInicio();

    // =============================================
    // GENERAR SECUENCIA ALEATORIA
    // =============================================
    for (uint8_t i = 0; i < 9; i++) {
        lista[i] = random(0, 4); // 0..3
    }

    // Reiniciar vidas y tiempo acumulado (nueva partida)
    vidas = 3;
    tiempo_acumulado = 0;

    int nivel = 1;
    bool reiniciarJuego = false;

    actualizarNumero(nivel, vidas, tiempo_acumulado);

    // =============================================
    // BUCLE DE NIVELES
    // =============================================
    while (nivel <= 9 && vidas > 0) {

        Serial.print("Nivel: "); Serial.println(nivel);
        Serial.print("Vidas: "); Serial.println(vidas);

        actualizarNumero(nivel, vidas, tiempo_acumulado);

        // Determinar tiempo entre LEDs según el nivel
        if (nivel <= 2)      tiempo = 0.5f;
        else if (nivel <= 4) tiempo = 0.3f;
        else if (nivel <= 6) tiempo = 0.25f;
        else if (nivel <= 8) tiempo = 0.2f;
        else                 tiempo = 0.166f;

        time_resp = 1.25f * (2.0f * tiempo * nivel);

        // =========================================
        // MOSTRAR SECUENCIA
        // =========================================
        for (uint8_t i = 0; i < (uint8_t)nivel; i++) {

            digitalWrite(PIN_LEDS[lista[i]], HIGH);

            if (esperarTiempo(tiempo)) {
                reiniciarJuego = true;
                break;
            }

            apagarLeds();

            // Pausa entre un LED y el siguiente. NO se
            // aplica tras el ÚLTIMO LED, para que la
            // fase de respuesta comience de inmediato.
            if (i < (uint8_t)(nivel - 1)) {
                if (esperarTiempo(tiempo)) {
                    reiniciarJuego = true;
                    break;
                }
            }
        }

        if (reiniciarJuego) break;

        // =========================================
        // TIEMPO DE RESPUESTA
        // =========================================
        int resultado = esperarConParpadeo(time_resp, nivel, vidas);

        if (resultado == REINICIO_SOLICITADO) {
            reiniciarJuego = true;
            break;
        }

        if (resultado == NIVEL_OK) {
            Serial.println("Nivel superado");
            nivel += 1;
        } else {
            // Animación de pérdida de vida: parpadea con
            // el valor ANTERIOR, y DESPUÉS se resta la vida.
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

    // =================================================
    // JUEGO REINICIADO
    // =================================================
    if (reiniciarJuego) {
        Serial.println("======================");
        Serial.println("JUEGO REINICIADO");
        Serial.println("======================");

        apagarLeds();

        // A diferencia de GAME OVER / GANASTE, el
        // reinicio manual sí vuelve al estado inicial.
        actualizarNumero(1, 3, 0);

        // loop() volverá a llamar a esperarInicio(),
        // así que el juego queda detenido hasta una
        // nueva pulsación corta.
    }
    // =================================================
    // GANÓ
    // =================================================
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
    // =================================================
    // GAME OVER
    // =================================================
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
    // equivalente a una nueva vuelta del while(True)
    // externo del programa original.
}

// =================================================
// NÚCLEO 1: SOLO MULTIPLEXA EL DISPLAY DE 7 SEGMENTOS
// =================================================

void setup1() {
    for (uint8_t i = 0; i < 7; i++) {
        pinMode(PIN_SEG[i], OUTPUT);
    }
    for (uint8_t i = 0; i < 4; i++) {
        pinMode(PIN_DIG[i], OUTPUT);
    }
    apagarDigitos();
}

void loop1() {
    multiplexar();
}
