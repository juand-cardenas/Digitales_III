
// =================================================
// JUEGO DE MEMORIA - RASPBERRY PI PICO
// Arduino IDE
// =================================================

// =================================================
// CONFIGURACIÓN
// =================================================

// LEDs
const int leds[5] = {
  16,   // LED 0
  17,   // LED 1
  18,   // LED 2
  19,   // LED 3
  20    // LED 4 - parpadeo
};

// Botones
// Botón 11 -> LED 16 -> número 0
// Botón 12 -> LED 17 -> número 1
// Botón 13 -> LED 18 -> número 2
// Botón 14 -> LED 19 -> número 3
// Botón 15 -> INICIO / REINICIO

const int botones[5] = {
  11,
  12,
  13,
  14,
  15
};

// Tiempo necesario para reiniciar
const unsigned long TIEMPO_REINICIO = 2000; // 2 segundos


// =================================================
// VARIABLES
// =================================================

float tiempo = 0;
float time_resp = 0;

int vidas = 3;
float tiempo_trans = 0;

// Secuencia aleatoria
int lista[10] = {0};

// Guarda desde cuándo se está presionando
// el botón 15
unsigned long boton15_desde = 0;

// Indica si el botón 15 está siendo medido
bool boton15_midiendo = false;


// =================================================
// SETUP
// =================================================

void setup() {

  Serial.begin(115200);

  // Configurar LEDs como salida
  for (int i = 0; i < 5; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }

  // Configurar botones como entrada con resistencia
  // interna PULL-DOWN
  for (int i = 0; i < 5; i++) {
    pinMode(botones[i], INPUT_PULLDOWN);
  }

  randomSeed(micros());

  Serial.println("======================");
  Serial.println("JUEGO DE MEMORIA");
  Serial.println("======================");
}


// =================================================
// APAGAR TODOS LOS LEDS
// =================================================

void apagar_leds() {

  for (int i = 0; i < 5; i++) {
    digitalWrite(leds[i], LOW);
  }
}


// =================================================
// REVISAR BOTÓN 15
//
// Devuelve:
// true  -> se solicitó reinicio
// false -> no se solicitó reinicio
// =================================================

bool revisar_boton_reinicio() {

  unsigned long ahora = millis();

  // =============================================
  // BOTÓN PRESIONADO
  // =============================================

  if (digitalRead(botones[4]) == HIGH) {

    // Primera vez que detectamos la pulsación
    if (!boton15_midiendo) {

      boton15_desde = ahora;
      boton15_midiendo = true;
    }

    // Ya estaba presionado
    else if (ahora - boton15_desde >= TIEMPO_REINICIO) {

      Serial.println("REINICIO SOLICITADO");

      apagar_leds();

      // Esperar a que el usuario SUELTE
      // el botón
      while (digitalRead(botones[4]) == HIGH) {
        delay(10);
      }

      boton15_midiendo = false;

      return true;
    }
  }

  else {

    // El botón está suelto
    boton15_midiendo = false;
  }

  return false;
}


// =================================================
// ESPERAR UN TIEMPO
//
// Mientras espera, revisa continuamente
// el botón 15.
//
// Devuelve:
// true  -> se pidió reinicio
// false -> terminó normalmente
// =================================================

bool esperar_tiempo(float tiempo_total) {

  unsigned long inicio = millis();

  unsigned long duracion =
    (unsigned long)(tiempo_total * 1000.0);

  while (millis() - inicio < duracion) {

    // Revisar botón 15
    if (revisar_boton_reinicio()) {
      return true;
    }

    // Pequeña espera
    delay(5);
  }

  return false;
}


// =================================================
// ESPERAR PARA INICIAR EL JUEGO
//
// Pulsación corta:
//     inicia el juego
//
// Pulsación larga:
//     NO inicia
//     espera una nueva pulsación
// =================================================

void esperar_inicio() {

  Serial.println("Esperando inicio...");

  apagar_leds();

  // =============================================
  // Si el botón está presionado al entrar,
  // esperar hasta que sea soltado.
  // =============================================

  while (digitalRead(botones[4]) == HIGH) {
    delay(10);
  }

  Serial.println("Botón liberado. Presione para comenzar.");


  // =============================================
  // Esperar una nueva pulsación
  // =============================================

  while (true) {

    if (digitalRead(botones[4]) == HIGH) {

      unsigned long inicio_pulsacion = millis();

      // Esperar mientras esté presionado
      while (digitalRead(botones[4]) == HIGH) {

        unsigned long ahora = millis();

        // =======================================
        // ¿Pulsación larga?
        // =======================================

        if (ahora - inicio_pulsacion >= TIEMPO_REINICIO) {

          Serial.println("Pulsación larga.");
          Serial.println("No se inicia el juego.");

          apagar_leds();

          // Esperar a que suelte
          while (digitalRead(botones[4]) == HIGH) {
            delay(5);
          }

          // Volver a esperar una nueva pulsación
          break;
        }

        delay(5);
      }

      // =========================================
      // Si el botón se soltó antes de los 2 s
      // =========================================

      if (digitalRead(botones[4]) == LOW) {

        unsigned long duracion =
          millis() - inicio_pulsacion;

        // Fue una pulsación corta
        if (duracion < TIEMPO_REINICIO) {

          Serial.println("INICIANDO JUEGO");

          return;
        }
      }
    }

    delay(5);
  }
}


// =================================================
// ESPERAR MIENTRAS LED 20 PARPADEA
// Y RECIBIR RESPUESTAS
//
// Devuelve:
// 0 -> error / tiempo agotado
// 1 -> nivel completado
// 2 -> reinicio solicitado
// =================================================

int esperar_con_parpadeo(float tiempo_total, int nivel) {

  unsigned long inicio = millis();

  unsigned long ultimo_cambio = inicio;

  // Estado LED 20
  bool estado_led = false;

  // Estado anterior de los botones 0-3
  int estado_anterior[4] = {
    LOW, LOW, LOW, LOW
  };

  // Posición de la secuencia
  int posicion = 0;

  // Última pulsación
  unsigned long ultima_pulsacion = millis();

  // LED correspondiente al botón pulsado
  int led_pulsado = -1;

  // Momento en que se encendió
  unsigned long tiempo_led = 0;


  // =============================================
  // BUCLE PRINCIPAL
  // =============================================

  unsigned long duracion_total =
    (unsigned long)(tiempo_total * 1000.0);

  while (millis() - inicio < duracion_total) {

    unsigned long ahora = millis();


    // =========================================
    // REVISAR BOTÓN 15
    // =========================================

    if (revisar_boton_reinicio()) {

      return 2;
    }


    // =========================================
    // TIEMPO TRANSCURRIDO
    // =========================================

    float transcurrido =
      (millis() - inicio) / 1000.0;

    float progreso =
      transcurrido / tiempo_total;


    // =========================================
    // INTERVALO DINÁMICO DEL LED 20
    // =========================================

    float intervalo =
      (0.5 - (0.45 * progreso)) / 2.0;


    // =========================================
    // PARPADEO LED 20
    // =========================================

    if (ahora - ultimo_cambio >=
        (unsigned long)(intervalo * 1000.0)) {

      if (estado_led == false) {
        estado_led = true;
      }
      else {
        estado_led = false;
      }

      digitalWrite(leds[4], estado_led);

      ultimo_cambio = ahora;
    }


    // =========================================
    // APAGAR LED DE LA PULSACIÓN
    // =========================================

    if (led_pulsado != -1) {

      if (ahora - tiempo_led >= 350) {

        digitalWrite(leds[led_pulsado], LOW);

        led_pulsado = -1;
      }
    }


    // =========================================
    // LEER BOTONES 0-3
    // =========================================

    for (int i = 0; i < 4; i++) {

      int estado_actual =
        digitalRead(botones[i]);


      // =======================================
      // Detectar flanco 0 -> 1
      // =======================================

      if (estado_actual == HIGH &&
          estado_anterior[i] == LOW) {


        // =====================================
        // Debounce
        // =====================================

        if (ahora - ultima_pulsacion >= 40) {

          ultima_pulsacion = ahora;


          // ===================================
          // Encender LED correspondiente
          // ===================================

          digitalWrite(leds[i], HIGH);

          led_pulsado = i;

          tiempo_led = ahora;


          // ===================================
          // Número correspondiente
          // ===================================

          int respuesta = i;


          // ===================================
          // Comprobar respuesta
          // ===================================

          if (respuesta != lista[posicion]) {

            Serial.println("ERROR");

            digitalWrite(leds[4], LOW);

            return 0;
          }


          // ===================================
          // Respuesta correcta
          // ===================================

          posicion++;

          Serial.println("Correcto");


          // ===================================
          // ¿Terminó el nivel?
          // ===================================

          if (posicion == nivel) {

            digitalWrite(leds[4], LOW);

            return 1;
          }
        }
      }


      // =======================================
      // Guardar estado actual
      // =======================================

      estado_anterior[i] = estado_actual;
    }
  }


  // =============================================
  // SE ACABÓ EL TIEMPO
  // =============================================

  digitalWrite(leds[4], LOW);

  return 0;
}


// =================================================
// PROGRAMA PRINCIPAL
// =================================================

void loop() {

  // =============================================
  // ESPERAR AL BOTÓN 15 PARA INICIAR
  // =============================================

  esperar_inicio();


  // =============================================
  // GENERAR SECUENCIA ALEATORIA
  // =============================================

  for (int i = 0; i < 9; i++) {

    lista[i] = random(0, 4);
  }


  // =============================================
  // REINICIAR VIDAS
  // =============================================

  vidas = 3;


  // =============================================
  // COMENZAR NIVEL 1
  // =============================================

  int nivel = 1;

  bool reiniciar_juego = false;


  // =============================================
  // BUCLE DE NIVELES
  // =============================================

  while (nivel <= 9 && vidas > 0) {

    Serial.print("Nivel: ");
    Serial.println(nivel);

    Serial.print("Vidas: ");
    Serial.println(vidas);


    // =========================================
    // DETERMINAR TIEMPO
    // =========================================

    if (nivel <= 2) {

      tiempo = 0.5;
    }

    else if (nivel <= 4) {

      tiempo = 0.3;
    }

    else if (nivel <= 6) {

      tiempo = 0.25;
    }

    else if (nivel <= 8) {

      tiempo = 0.2;
    }

    else {

      tiempo = 0.166;
    }


    // =========================================
    // TIEMPO DE RESPUESTA
    // =========================================

    time_resp =
      1.25 * (2.0 * tiempo * nivel);


    // =========================================
    // MOSTRAR SECUENCIA
    // =========================================

    for (int i = 0; i < nivel; i++) {

      // ---------------------------------------
      // Encender LED correspondiente
      // ---------------------------------------

      digitalWrite(leds[lista[i]], HIGH);


      // ---------------------------------------
      // Esperar mientras se vigila botón 15
      // ---------------------------------------

      if (esperar_tiempo(tiempo)) {

        reiniciar_juego = true;

        break;
      }


      // ---------------------------------------
      // Apagar LEDs
      // ---------------------------------------

      apagar_leds();


      // ---------------------------------------
      // Esperar mientras se vigila botón 15
      // ---------------------------------------

      if (esperar_tiempo(tiempo)) {

        reiniciar_juego = true;

        break;
      }
    }


    // =========================================
    // ¿SE SOLICITÓ REINICIO?
    // =========================================

    if (reiniciar_juego) {
      break;
    }


    // =========================================
    // TIEMPO DE RESPUESTA
    // =========================================

    int resultado =
      esperar_con_parpadeo(time_resp, nivel);


    // =========================================
    // ¿SE SOLICITÓ REINICIO?
    // =========================================

    if (resultado == 2) {

      reiniciar_juego = true;

      break;
    }


    // =========================================
    // COMPROBAR RESULTADO
    // =========================================

    if (resultado == 1) {

      Serial.println("Nivel superado");

      nivel++;
    }

    else {

      vidas--;

      Serial.print("Vidas restantes: ");
      Serial.println(vidas);

      if (vidas > 0) {

        Serial.println("Repitiendo nivel...");
      }

      else {

        Serial.println("GAME OVER");
      }
    }


    // =========================================
    // APAGAR TODOS LOS LEDS
    // =========================================

    apagar_leds();


    // =========================================
    // PEQUEÑA ESPERA
    // TAMBIÉN VIGILANDO BOTÓN 15
    // =========================================

    if (esperar_tiempo(0.4)) {

      reiniciar_juego = true;

      break;
    }
  }


  // =================================================
  // JUEGO REINICIADO
  // =================================================

  if (reiniciar_juego) {

    Serial.println("======================");
    Serial.println("JUEGO REINICIADO");
    Serial.println("======================");

    apagar_leds();

    // El loop() vuelve a comenzar y llama
    // nuevamente a esperar_inicio().
  }


  // =================================================
  // GANÓ
  // =================================================

  else if (nivel > 9) {

    Serial.println("======================");
    Serial.println("¡GANASTE!");
    Serial.println("======================");

    apagar_leds();

    // Esperar 2 segundos vigilando botón 15
    if (esperar_tiempo(2)) {

      Serial.println("Reinicio después de ganar.");
    }
  }


  // =================================================
  // GAME OVER
  // =================================================

  else {

    Serial.println("======================");
    Serial.println("GAME OVER");
    Serial.println("======================");

    apagar_leds();

    // Esperar 1 segundo vigilando botón 15
    if (esperar_tiempo(1)) {

      Serial.println("Reinicio después de GAME OVER.");
    }
  }
}
