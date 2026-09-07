"""
@file simon_game.py
@brief Juego tipo "Simon Says" para Raspberry Pi Pico (MicroPython).

Controla 5 LEDs, 5 botones y un display de 7 segmentos multiplexado
de 4 dígitos para mostrar nivel, vidas y tiempo de respuesta acumulado.

@author (autor original)
"""

from machine import Pin
import time
import random

# =================================================
# CONFIGURACIÓN DE HARDWARE
# =================================================

## @brief LEDs del juego. leds[0..3] son los LEDs de secuencia/respuesta,
##        leds[4] (pin 16) es el LED de aviso/parpadeo.
leds = [
    Pin(20, Pin.OUT),
    Pin(19, Pin.OUT),
    Pin(18, Pin.OUT),
    Pin(17, Pin.OUT),
    Pin(16, Pin.OUT)
]

## @brief Botones de entrada del jugador y de control.
##
## - botones[0] -> LED 20 -> número 0
## - botones[1] -> LED 19 -> número 1
## - botones[2] -> LED 18 -> número 2
## - botones[3] -> LED 17 -> número 3
## - botones[4] -> INICIO / REINICIO
botones = [
    Pin(0, Pin.IN, Pin.PULL_DOWN),
    Pin(1, Pin.IN, Pin.PULL_DOWN),
    Pin(2, Pin.IN, Pin.PULL_DOWN),
    Pin(3, Pin.IN, Pin.PULL_DOWN),
    Pin(4, Pin.IN, Pin.PULL_DOWN)
]

# =================================================
# DISPLAY DE 7 SEGMENTOS (4 DÍGITOS MULTIPLEXADOS)
# =================================================

## @brief Pines de los 7 segmentos (a, b, c, d, e, f, g).
seg = [Pin(i, Pin.OUT) for i in range(6, 13)]

## @brief Pines que habilitan cada uno de los 4 dígitos del display
##        (común activo en bajo).
dig = [Pin(13, Pin.OUT), Pin(14, Pin.OUT), Pin(15, Pin.OUT), Pin(21, Pin.OUT)]

## @brief Tabla de patrones de segmentos para cada dígito (0-9).
##
## La clave 10 se usa como "dígito en blanco" para lograr el efecto
## de parpadeo en el display (por ejemplo al perder una vida).
tabla_7seg = {
    0: (1, 1, 1, 1, 1, 1, 0),
    1: (0, 1, 1, 0, 0, 0, 0),
    2: (1, 1, 0, 1, 1, 0, 1),
    3: (1, 1, 1, 1, 0, 0, 1),
    4: (0, 1, 1, 0, 0, 1, 1),
    5: (1, 0, 1, 1, 0, 1, 1),
    6: (1, 0, 1, 1, 1, 1, 1),
    7: (1, 1, 1, 0, 0, 0, 0),
    8: (1, 1, 1, 1, 1, 1, 1),
    9: (1, 1, 1, 1, 0, 1, 1),
    10: (0, 0, 0, 0, 0, 0, 0),  # dígito en blanco (para parpadeo)
}

## @brief Valores actualmente mostrados en el display.
##
## - numero[0]: nivel actual
## - numero[1]: vidas restantes
## - numero[2]: unidades del tiempo transcurrido
## - numero[3]: décimas del tiempo transcurrido
numero = [0, 0, 0, 0]

## @brief Índice del dígito (0-3) que se está refrescando actualmente
##        en la multiplexación del display.
posicion_display = 0

## @brief Marca de tiempo (ms) del último cambio de dígito multiplexado.
ultimo_cambio_display = time.ticks_ms()

## @brief Tiempo (ms) que permanece encendido cada dígito durante
##        la multiplexación.
INTERVALO_DISPLAY = 2

## @brief Tiempo (ms) que debe mantenerse presionado el botón de
##        inicio/reinicio para que la acción se considere una
##        "pulsación larga".
TIEMPO_REINICIO = 2000

## @brief Valor máximo permitido para el tiempo acumulado de
##        respuesta (en segundos). Limita a 2 dígitos (X.X).
TIEMPO_ACUMULADO_MAX = 9.9

# =================================================
# VARIABLES DE ESTADO DEL JUEGO
# =================================================

## @brief Duración (s) con la que se muestra cada LED de la secuencia,
##        depende del nivel actual.
tiempo = 0

## @brief Tiempo total (s) que tiene el jugador para responder un nivel.
time_resp = 0

## @brief Vidas restantes del jugador (inicia en 3).
vidas = 3

## @brief Tiempo transcurrido auxiliar (no utilizado activamente en
##        la lógica principal, se mantiene por compatibilidad).
tiempo_trans = 0

## @brief Secuencia aleatoria de LEDs (valores 0-3) para el nivel actual.
lista = [0] * 10

## @brief Tiempo acumulado (s) que el jugador tarda en completar
##        correctamente cada nivel.
##
## Se suma nivel a nivel y se satura en TIEMPO_ACUMULADO_MAX (9.9 s).
tiempo_acumulado = 0

## @brief Marca de tiempo (ms) en la que se comenzó a presionar el
##        botón de inicio/reinicio, usada para distinguir pulsación
##        corta de pulsación larga. `None` si el botón no está
##        presionado.
boton15_desde = None


# =================================================
# FUNCIONES DE LEDS
# =================================================

def apagar_leds():
    """
    @brief Apaga los 5 LEDs del juego.
    """
    for led in leds:
        led.value(0)


# =================================================
# FUNCIONES DE DISPLAY
# =================================================

def apagar_digitos():
    """
    @brief Apaga los 4 dígitos del display de 7 segmentos.

    Pone en 1 (apagado, común activo en bajo) los pines de
    habilitación de cada dígito.
    """
    for d in dig:
        d.value(1)


def escribir_segmentos(n):
    """
    @brief Configura los 7 segmentos para mostrar el dígito indicado.

    @param n Dígito a mostrar (clave de tabla_7seg, 0-10).
    """
    patron = tabla_7seg[n]

    for i in range(7):
        if patron[i]:
            seg[i].value(0)
        else:
            seg[i].value(1)


def multiplexar():
    """
    @brief Refresca un dígito del display (multiplexación).

    Debe llamarse muy seguido (idealmente en todo bucle de espera
    del juego) para que los 4 dígitos se vean encendidos
    simultáneamente al ojo humano.

    @global posicion_display Índice del dígito actualmente activo.
    @global ultimo_cambio_display Marca de tiempo del último refresco.
    """
    global posicion_display
    global ultimo_cambio_display

    ahora = time.ticks_ms()

    if time.ticks_diff(ahora, ultimo_cambio_display) >= INTERVALO_DISPLAY:

        apagar_digitos()

        escribir_segmentos(numero[posicion_display])

        dig[posicion_display].value(0)

        posicion_display += 1

        if posicion_display >= 4:
            posicion_display = 0

        ultimo_cambio_display = ahora


def actualizar_numero(n_nivel, n_vidas, n_tiempo):
    """
    @brief Actualiza los valores a mostrar en el display.

    Separa el tiempo en unidades y décimas, ya que
    TIEMPO_ACUMULADO_MAX tiene 2 dígitos (X.X).

    @param n_nivel Nivel actual a mostrar (se acota a [0, 9]).
    @param n_vidas Vidas restantes a mostrar (se acota a [0, 3]).
    @param n_tiempo Tiempo acumulado/transcurrido a mostrar, en
                     segundos (se acota a [0, TIEMPO_ACUMULADO_MAX]).
    """
    if n_tiempo > TIEMPO_ACUMULADO_MAX:
        n_tiempo = TIEMPO_ACUMULADO_MAX

    if n_tiempo < 0:
        n_tiempo = 0

    entero = int(n_tiempo)

    decimal = int(round((n_tiempo - entero) * 10))

    if decimal >= 10:
        decimal = 0
        entero += 1

    if entero > 9:
        entero = 9

    numero[0] = n_nivel if 0 <= n_nivel <= 9 else 9
    numero[1] = n_vidas if 0 <= n_vidas <= 3 else 3
    numero[2] = entero
    numero[3] = decimal


def esperar_ms_con_display(ms):
    """
    @brief Espera una cantidad de milisegundos sin congelar el display.

    Mantiene la multiplexación activa mientras transcurre una
    animación u otro evento de espera.

    @param ms Milisegundos a esperar.
    """
    inicio = time.ticks_ms()

    while time.ticks_diff(time.ticks_ms(), inicio) < ms:
        multiplexar()
        time.sleep_ms(2)


# =================================================
# ANIMACIONES
# =================================================

def animacion_nivel_superado():
    """
    @brief Animación de nivel superado.

    Barrido rápido de una sola pasada, LED 0 -> LED 3.
    """
    apagar_leds()

    for i in range(4):
        leds[i].value(1)
        esperar_ms_con_display(60)
        leds[i].value(0)


def animacion_entrada_incorrecta():
    """
    @brief Animación de entrada incorrecta.

    Los 4 LEDs (0-3) encienden juntos y parpadean 2 veces.
    """
    apagar_leds()

    for _ in range(2):
        for i in range(4):
            leds[i].value(1)

        esperar_ms_con_display(150)

        apagar_leds()

        esperar_ms_con_display(100)


def animacion_agotamiento_tiempo():
    """
    @brief Animación de tiempo agotado.

    El LED 16 (leds[4]) permanece encendido fijo, mientras los
    dígitos de tiempo (unidades y décimas) del 7 segmentos
    parpadean para indicar que se acabó el tiempo.
    """
    leds[4].value(1)

    unidades_actual = numero[2]
    decimas_actual = numero[3]

    for _ in range(4):
        numero[2] = 10  # blanco
        numero[3] = 10  # blanco

        esperar_ms_con_display(150)

        numero[2] = unidades_actual
        numero[3] = decimas_actual

        esperar_ms_con_display(150)

    leds[4].value(0)


def animacion_perdida_vida(veces=3, duracion_ms=150):
    """
    @brief Animación de pérdida de vida.

    Parpadea solo el dígito de vidas en el 7 segmentos, mostrando
    el valor anterior (antes de restar la vida), para avisar al
    jugador que perdió una vida.

    @param veces Número de veces que parpadea el dígito.
    @param duracion_ms Duración (ms) de cada estado (encendido/apagado).
    """
    valor_actual = numero[1]

    for _ in range(veces):
        numero[1] = 10  # blanco

        esperar_ms_con_display(duracion_ms)

        numero[1] = valor_actual

        esperar_ms_con_display(duracion_ms)


def animacion_victoria():
    """
    @brief Animación de finalización exitosa del juego (victoria).

    Realiza un barrido de ida y vuelta (estilo "KITT") por 2 veces,
    seguido de 3 destellos con los 4 LEDs encendidos juntos.
    """
    apagar_leds()

    for _ in range(2):
        for i in range(4):
            leds[i].value(1)
            esperar_ms_con_display(50)
            leds[i].value(0)

        for i in range(2, -1, -1):
            leds[i].value(1)
            esperar_ms_con_display(50)
            leds[i].value(0)

    for _ in range(3):
        for i in range(4):
            leds[i].value(1)

        esperar_ms_con_display(120)

        apagar_leds()

        esperar_ms_con_display(120)


# =================================================
# TIEMPO ACUMULADO
# =================================================

def acumular_tiempo(segundos):
    """
    @brief Suma tiempo al acumulador global de tiempo de respuesta.

    Se utiliza sin importar si el intento fue correcto, incorrecto,
    o si se agotó el tiempo. El acumulador se satura en
    TIEMPO_ACUMULADO_MAX.

    @param segundos Cantidad de segundos a sumar al acumulador.
    @global tiempo_acumulado Acumulador global de tiempo de respuesta.
    """
    global tiempo_acumulado

    if tiempo_acumulado < TIEMPO_ACUMULADO_MAX:
        tiempo_acumulado += segundos

        if tiempo_acumulado > TIEMPO_ACUMULADO_MAX:
            tiempo_acumulado = TIEMPO_ACUMULADO_MAX


# =================================================
# BOTÓN DE INICIO / REINICIO
# =================================================

def revisar_boton_reinicio():
    """
    @brief Revisa si se solicitó un reinicio mediante el botón 4.

    Detecta una pulsación larga (>= TIEMPO_REINICIO) del botón de
    inicio/reinicio. Esta función se llama continuamente durante
    todo el juego y mantiene el display activo mientras se ejecuta.

    @return True si se solicitó reinicio (pulsación larga detectada).
    @return False si no se solicitó reinicio.
    @global boton15_desde Marca de tiempo del inicio de la pulsación.
    """
    global boton15_desde

    # Mantener el display encendido mientras se revisa el botón
    multiplexar()

    ahora = time.ticks_ms()

    # =============================================
    # BOTÓN PRESIONADO
    # =============================================
    if botones[4].value() == 1:

        # Primera vez que se detecta la pulsación
        if boton15_desde is None:
            boton15_desde = ahora

        # Ya estaba presionado: verificar duración
        elif time.ticks_diff(ahora, boton15_desde) >= TIEMPO_REINICIO:

            print("REINICIO SOLICITADO")

            apagar_leds()

            # Esperar a que el usuario suelte el botón, para evitar
            # que tras el reinicio la misma pulsación reinicie el
            # juego automáticamente.
            while botones[4].value() == 1:
                multiplexar()
                time.sleep_ms(10)

            boton15_desde = None

            return True

    else:
        # El botón está suelto, no ha sido presionado
        boton15_desde = None

    return False


def esperar_tiempo(tiempo_total):
    """
    @brief Espera un tiempo determinado sin bloquear el display ni
           dejar de revisar el botón de reinicio.

    Actúa como un "delay" que, mientras "espera", mantiene la
    multiplexación del display activa y revisa continuamente si se
    solicitó un reinicio.

    @param tiempo_total Tiempo a esperar, en segundos.
    @return True si se solicitó un reinicio durante la espera.
    @return False si el tiempo terminó normalmente.
    """
    inicio = time.ticks_ms()

    while time.ticks_diff(time.ticks_ms(), inicio) < tiempo_total * 1000:

        # Mantener el display encendido
        multiplexar()

        # Revisar botón de reinicio
        if revisar_boton_reinicio():
            return True

        # Espera pequeña
        time.sleep_ms(5)

    return False


def esperar_inicio():
    """
    @brief Espera a que el jugador inicie una nueva partida.

    Una pulsación corta del botón de inicio/reinicio comienza el
    juego. Si el botón quedó presionado tras un reinicio previo,
    primero se obliga a soltarlo antes de aceptar una nueva
    pulsación.
    """
    apagar_leds()

    # Si el botón está presionado al entrar, esperar a que se suelte.
    while botones[4].value() == 1:
        multiplexar()
        time.sleep_ms(10)

    print("Botón liberado. Presione para comenzar.")

    while True:

        # Mantener el display encendido mientras se espera la
        # pulsación (de lo contrario el display se congelaría).
        multiplexar()

        if botones[4].value() == 1:

            inicio_pulsacion = time.ticks_ms()

            # Esperar mientras el botón esté presionado
            while botones[4].value() == 1:

                multiplexar()

                ahora = time.ticks_ms()

                # ¿Pulsación larga?
                if time.ticks_diff(ahora, inicio_pulsacion) >= TIEMPO_REINICIO:

                    print("Pulsación larga.")
                    print("No se inicia el juego.")

                    apagar_leds()

                    # Esperar a que suelte el botón
                    while botones[4].value() == 1:
                        multiplexar()
                        time.sleep_ms(5)

                    # Volver a esperar una nueva pulsación
                    break

                time.sleep_ms(5)

            else:
                # El botón se soltó antes de los 2 segundos:
                # pulsación corta -> iniciar juego.
                print("INICIANDO JUEGO")
                return


def esperar_con_parpadeo(tiempo_total, nivel, vidas_actuales):
    """
    @brief Espera la respuesta del jugador mientras el LED 16 parpadea.

    Controla la fase de "respuesta" del juego: hace parpadear el LED
    de aviso a una velocidad que aumenta con el progreso del tiempo,
    lee los botones 0-3, valida cada pulsación contra la secuencia
    del nivel y actualiza el display con el tiempo transcurrido.

    @param tiempo_total Tiempo total (s) disponible para responder
                         el nivel completo.
    @param nivel Nivel actual (longitud de la secuencia a ingresar).
    @param vidas_actuales Vidas restantes del jugador, para mostrar
                           en el display.
    @return True si el jugador completó correctamente el nivel.
    @return False si hubo un error en la secuencia o se agotó el
            tiempo.
    @return None si se solicitó un reinicio durante la espera.
    @global tiempo_acumulado Acumulador global de tiempo de respuesta.
    """
    global tiempo_acumulado

    inicio = time.ticks_ms()
    ultimo_cambio = inicio  # última vez que cambió el estado del LED 16

    # El LED de aviso arranca ENCENDIDO para que el jugador vea de
    # inmediato que puede comenzar a ingresar el patrón.
    estado_led = 1
    leds[4].value(1)

    # Estado anterior de los botones 0-3 (para detectar flancos)
    estado_anterior = [0, 0, 0, 0]

    # Posición actual dentro de la secuencia que se debe ingresar
    posicion = 0

    # Marca de tiempo de la última pulsación válida (para debounce)
    ultima_pulsacion = time.ticks_ms()

    # Momento en que se presionó cada botón (0-3). Es un arreglo (uno
    # por botón) para poder apagar varios LEDs de forma independiente
    # aunque se hayan presionado casi al mismo tiempo.
    tiempo_led = [None, None, None, None]

    # =============================================
    # BUCLE PRINCIPAL DE LA FASE DE RESPUESTA
    # =============================================
    while time.ticks_diff(time.ticks_ms(), inicio) < (tiempo_total * 1000):

        ahora = time.ticks_ms()

        multiplexar()

        # Revisar botón de reinicio
        if revisar_boton_reinicio():
            return None

        # Tiempo transcurrido desde que empezó el nivel
        transcurrido = time.ticks_diff(ahora, inicio) / 1000
        progreso = transcurrido / tiempo_total

        # Display: tiempo en vivo = lo acumulado en niveles
        # anteriores + lo que va de este intento.
        actualizar_numero(nivel, vidas_actuales, tiempo_acumulado + transcurrido)

        # Intervalo dinámico de parpadeo del LED 16 (se acelera con
        # el progreso del tiempo disponible)
        intervalo = (0.5 - (0.45 * progreso)) / 2

        # Parpadeo del LED 16
        if time.ticks_diff(ahora, ultimo_cambio) >= intervalo * 1000:

            estado_led = 0 if estado_led == 1 else 1

            leds[4].value(estado_led)

            ultimo_cambio = ahora

        # Apagar los LEDs de respuesta (0-3) tras 350 ms de haberse
        # encendido, cada uno con su propio temporizador, para poder
        # apagar varios a la vez sin perder el rastro de ninguno.
        for j in range(4):
            if tiempo_led[j] is not None:
                if time.ticks_diff(ahora, tiempo_led[j]) >= 350:
                    leds[j].value(0)
                    tiempo_led[j] = None

        # Leer botones 0-3
        for i in range(4):

            estado_actual = botones[i].value()

            # Detectar flanco de subida 0 -> 1
            if estado_actual == 1 and estado_anterior[i] == 0:

                # Debounce
                if time.ticks_diff(ahora, ultima_pulsacion) >= 40:

                    ultima_pulsacion = ahora

                    # Encender LED correspondiente a la pulsación
                    leds[i].value(1)
                    tiempo_led[i] = ahora

                    respuesta = i

                    # Comprobar si la respuesta es correcta
                    if respuesta != lista[posicion]:

                        print("ERROR")

                        leds[4].value(0)

                        # Acumular tiempo del intento incorrecto
                        tiempo_nivel = time.ticks_diff(ahora, inicio) / 1000
                        acumular_tiempo(tiempo_nivel)

                        actualizar_numero(nivel, vidas_actuales, tiempo_acumulado)

                        animacion_entrada_incorrecta()

                        return False

                    # Respuesta correcta
                    posicion += 1

                    print("Correcto")

                    # ¿Se completó el nivel?
                    if posicion == nivel:

                        leds[4].value(0)

                        # Acumular el tiempo de respuesta de este
                        # nivel. "ahora" es el instante en que se
                        # presionó el último botón correcto, es
                        # decir, cuando se completó el nivel.
                        tiempo_nivel = time.ticks_diff(ahora, inicio) / 1000
                        acumular_tiempo(tiempo_nivel)

                        actualizar_numero(nivel, vidas_actuales, tiempo_acumulado)

                        animacion_nivel_superado()

                        return True

            estado_anterior[i] = estado_actual

    # =============================================
    # SE ACABÓ EL TIEMPO
    # =============================================
    leds[4].value(0)

    # Acumular tiempo: se agotó el tiempo -> se usó todo tiempo_total
    acumular_tiempo(tiempo_total)

    actualizar_numero(nivel, vidas_actuales, tiempo_acumulado)

    animacion_agotamiento_tiempo()

    return False


# =================================================
# @brief Programa principal.
#
# Inicializa el display, muestra el estado inicial (Nivel 1,
# 3 vidas, tiempo 0) y ejecuta el bucle principal del juego:
# espera el inicio, genera una secuencia aleatoria, recorre los
# niveles mostrando la secuencia y evaluando la respuesta del
# jugador, y finalmente gestiona el desenlace (victoria, game over
# o reinicio solicitado).
# =================================================

apagar_digitos()

# Estado inicial al energizar: Nivel 1, 3 Vidas, Tiempo 00
actualizar_numero(1, 3, 0)

while True:

    # Esperar al botón de inicio. El display no se toca aquí: debe
    # conservar lo último mostrado (estado inicial o resultado del
    # juego anterior) hasta que se presione para iniciar una
    # nueva partida.
    esperar_inicio()

    # Generar secuencia aleatoria
    for i in range(9):
        lista[i] = random.randint(0, 3)

    # Reiniciar vidas
    vidas = 3

    # Reiniciar tiempo acumulado (nueva partida = nuevo conteo)
    tiempo_acumulado = 0.00

    # Comenzar en el nivel 1
    nivel = 1

    reiniciar_juego = False

    # Display: Nivel 1, Vidas 3, Tiempo 0
    actualizar_numero(nivel, vidas, tiempo_acumulado)

    # =============================================
    # BUCLE DE NIVELES
    # =============================================
    while nivel <= 9 and vidas > 0:

        print("Nivel:", nivel)
        print("Vidas:", vidas)

        # Refrescar nivel / vidas / tiempo en el display
        actualizar_numero(nivel, vidas, tiempo_acumulado)

        # Determinar la duración de cada LED de la secuencia según
        # el nivel actual (a mayor nivel, más rápido)
        if nivel <= 2:
            tiempo = 0.5
        elif nivel <= 4:
            tiempo = 0.3
        elif nivel <= 6:
            tiempo = 0.25
        elif nivel <= 8:
            tiempo = 0.2
        else:
            tiempo = 0.166

        # Tiempo total de respuesta disponible para el nivel
        time_resp = 1.25 * (2 * tiempo * nivel)

        # =========================================
        # MOSTRAR SECUENCIA
        # =========================================
        for i in range(nivel):

            # Encender LED correspondiente
            leds[lista[i]].value(1)

            # Esperar mientras se vigila el botón de reinicio
            if esperar_tiempo(tiempo):
                reiniciar_juego = True
                break

            # Apagar LEDs
            apagar_leds()

            # Pausa entre un LED y el siguiente (vigilando el botón
            # de reinicio). No se aplica después del último LED,
            # para que la fase de respuesta comience de inmediato.
            if i < nivel - 1:
                if esperar_tiempo(tiempo):
                    reiniciar_juego = True
                    break

        # ¿Se solicitó reinicio durante la secuencia?
        if reiniciar_juego:
            break

        # =========================================
        # FASE DE RESPUESTA DEL JUGADOR
        # =========================================
        resultado = esperar_con_parpadeo(time_resp, nivel, vidas)

        # ¿Se solicitó reinicio durante la respuesta?
        if resultado is None:
            reiniciar_juego = True
            break

        # Comprobar resultado del intento
        if resultado:

            print("Nivel superado")
            nivel += 1

        else:

            # Animación de pérdida de vida: se parpadea el dígito
            # con el valor anterior, y después se resta la vida.
            animacion_perdida_vida()

            vidas -= 1

            print("Vidas restantes:", vidas)

            if vidas > 0:
                print("Repitiendo nivel...")
            else:
                print("GAME OVER")

        # Refrescar el display tras el resultado
        actualizar_numero(nivel, vidas, tiempo_acumulado)

        # Apagar todos los LEDs
        apagar_leds()

        # Pequeña espera, también vigilando el botón de reinicio
        if esperar_tiempo(0.4):
            reiniciar_juego = True
            break

    # =============================================
    # DESENLACE DE LA PARTIDA
    # =============================================

    if reiniciar_juego:
        # Juego reiniciado: volver a Nivel 1, Vidas 3, Tiempo 0
        apagar_leds()
        actualizar_numero(1, 3, 0)

    elif nivel > 9:
        # El jugador ganó
        print("======================")
        print("¡GANASTE!")
        print("======================")
        print("Tiempo acumulado final:", tiempo_acumulado)

        animacion_victoria()

        apagar_leds()

        # Esperar 2 segundos vigilando el botón de reinicio
        if esperar_tiempo(2):
            print("Reinicio después de ganar.")

    else:
        # Game over (se acabaron las vidas)
        apagar_leds()

        # Esperar 1 segundo vigilando el botón de reinicio
        if esperar_tiempo(1):
            print("Reinicio después de GAME OVER.")