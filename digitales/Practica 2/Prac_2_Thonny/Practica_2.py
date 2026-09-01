from machine import Pin
import time
import random

# =================================================
# CONFIGURACIÓN
# =================================================

# LEDs
leds = [
    Pin(20, Pin.OUT),
    Pin(19, Pin.OUT),
    Pin(18, Pin.OUT),
    Pin(17, Pin.OUT),
    Pin(16, Pin.OUT)
]

# Botones
# Botón 0 -> LED 16 -> número 0
# Botón 1 -> LED 17 -> número 1
# Botón 2 -> LED 18 -> número 2
# Botón 3 -> LED 19 -> número 3
# Botón 4 -> INICIO / REINICIO
botones = [
    Pin(0, Pin.IN, Pin.PULL_DOWN),
    Pin(1, Pin.IN, Pin.PULL_DOWN),
    Pin(2, Pin.IN, Pin.PULL_DOWN),
    Pin(3, Pin.IN, Pin.PULL_DOWN),
    Pin(4, Pin.IN, Pin.PULL_DOWN)
]

# Tiempo necesario para reiniciar
TIEMPO_REINICIO = 2000       # 2 segundos

# Límite máximo del tiempo acumulado de respuesta
TIEMPO_ACUMULADO_MAX = 9.9    # segundos


# =================================================
# VARIABLES
# =================================================

tiempo = 0
time_resp = 0
vidas = 3
tiempo_trans = 0

lista = [0] * 10

# -------------------------------------------------
# Tiempo acumulado que el jugador tarda en
# COMPLETAR CORRECTAMENTE cada nivel (mientras
# el LED 16 parpadea, ingresando la secuencia).
#
# Se va sumando nivel a nivel y deja de acumular
# al llegar a TIEMPO_ACUMULADO_MAX (9.9 s).
# -------------------------------------------------

tiempo_acumulado = 0


# -------------------------------------------------
# IMPORTANTE:
# Guarda desde cuándo se está presionando
# el botón 15.
#
# Esta variable NO pertenece a una función,
# por lo que conserva su valor entre llamadas.
# -------------------------------------------------

boton15_desde = None


# =================================================
# APAGAR TODOS LOS LEDS
# =================================================

def apagar_leds():

    for led in leds:
        led.value(0)


# =================================================
# ACUMULAR TIEMPO DE RESPUESTA
#
# Suma "segundos" al acumulador global, sin
# sobrepasar TIEMPO_ACUMULADO_MAX.
#
# Se usa sin importar si el intento fue correcto,
# incorrecto, o si se agotó el tiempo.
# =================================================

def acumular_tiempo(segundos):

    global tiempo_acumulado

    if tiempo_acumulado < TIEMPO_ACUMULADO_MAX:

        tiempo_acumulado += segundos

        if tiempo_acumulado > TIEMPO_ACUMULADO_MAX:

            tiempo_acumulado = TIEMPO_ACUMULADO_MAX

    print("Tiempo acumulado:", tiempo_acumulado)


# =================================================
# REVISAR BOTÓN 15
#
# Devuelve:
#
# True  -> se solicitó reinicio
# False -> no se solicitó reinicio
#
# Esta función se llama continuamente durante
# todo el juego.
# =================================================

def revisar_boton_reinicio():

    global boton15_desde

    ahora = time.ticks_ms()

    # =============================================
    # BOTÓN PRESIONADO
    # =============================================

    if botones[4].value() == 1:

        # -----------------------------------------
        # Primera vez que detectamos la pulsación
        # -----------------------------------------

        if boton15_desde is None:

            boton15_desde = ahora

        # -----------------------------------------
        # Ya estaba presionado
        # -----------------------------------------

        elif time.ticks_diff(ahora, boton15_desde) >= TIEMPO_REINICIO:

            print("REINICIO SOLICITADO")

            apagar_leds()

            # -------------------------------------
            # Esperar a que el usuario SUELTE
            # el botón.
            #
            # Esto evita que después del reinicio
            # el mismo botón vuelva a iniciar
            # automáticamente.
            # -------------------------------------

            while botones[4].value() == 1:

                time.sleep_ms(10)

            boton15_desde = None

            return True

    else:

        # -----------------------------------------
        # El botón está suelto
        # -----------------------------------------

        boton15_desde = None

    return False


# =================================================
# ESPERAR UN TIEMPO
#
# Mientras espera, revisa continuamente
# el botón 15.
#
# Devuelve:
#
# True  -> se pidió reinicio
# False -> terminó el tiempo normalmente
# =================================================

def esperar_tiempo(tiempo_total):

    inicio = time.ticks_ms()

    while time.ticks_diff(time.ticks_ms(), inicio) < tiempo_total * 1000:

        # Revisar botón 15
        if revisar_boton_reinicio():

            return True

        # Espera pequeña
        time.sleep_ms(5)

    return False


# =================================================
# ESPERAR PARA INICIAR EL JUEGO
#
# Pulsación corta:
#     inicia el juego
#
# Pulsación larga:
#     NO inicia
#     espera una nueva pulsación
#
# Si el botón quedó presionado después de un
# reinicio, primero obliga a soltarlo.
# =================================================

def esperar_inicio():

    print("Esperando inicio...")

    apagar_leds()

    # =============================================
    # Si el botón está presionado al entrar,
    # esperar hasta que sea soltado.
    # =============================================

    while botones[4].value() == 1:

        time.sleep_ms(10)

    print("Botón liberado. Presione para comenzar.")

    # =============================================
    # Esperar una nueva pulsación
    # =============================================

    while True:

        if botones[4].value() == 1:

            inicio_pulsacion = time.ticks_ms()

            # -------------------------------------
            # Esperar mientras esté presionado
            # -------------------------------------

            while botones[4].value() == 1:

                ahora = time.ticks_ms()

                # ---------------------------------
                # ¿Pulsación larga?
                # ---------------------------------

                if time.ticks_diff( ahora,inicio_pulsacion) >= TIEMPO_REINICIO:

                    print("Pulsación larga.")
                    print("No se inicia el juego.")

                    apagar_leds()

                    # -----------------------------
                    # Esperar a que suelte
                    # -----------------------------

                    while botones[4].value() == 1:

                        time.sleep_ms(5)

                    # Volver a esperar una nueva
                    # pulsación
                    break

                time.sleep_ms(5)

            else:

                # ---------------------------------
                # El botón se soltó antes de los
                # 2 segundos.
                #
                # Es una pulsación corta.
                # ---------------------------------

                print("INICIANDO JUEGO")

                return


# =================================================
# ESPERAR MIENTRAS LED 20 PARPADEA
# Y RECIBIR RESPUESTAS
#
# False -> error / tiempo agotado
# True  -> nivel completado
# None  -> reinicio solicitado
# =================================================

def esperar_con_parpadeo(tiempo_total, nivel):

    global tiempo_acumulado

    inicio = time.ticks_ms()
    ultimo_cambio = inicio

    # Estado LED 20
    #
    # Arranca ENCENDIDO para que el jugador vea
    # de inmediato que puede empezar a ingresar
    # el patrón, sin esperar el primer intervalo.

    estado_led = 1

    leds[4].value(1)

    # Estado anterior de los botones 0-3
    estado_anterior = [0, 0, 0, 0]

    # Posición de la secuencia
    posicion = 0

    # Última pulsación
    ultima_pulsacion = time.ticks_ms()

    # LED correspondiente al botón pulsado
    led_pulsado = -1

    # Momento en que se encendió
    tiempo_led = 0

    # =============================================
    # BUCLE PRINCIPAL
    # =============================================

    while time.ticks_diff(time.ticks_ms(),inicio) < tiempo_total * 1000:

        ahora = time.ticks_ms()

        # =========================================
        # REVISAR BOTÓN 15
        # =========================================

        if revisar_boton_reinicio():

            return None

        # =========================================
        # TIEMPO TRANSCURRIDO
        # =========================================

        transcurrido = time.ticks_diff(ahora, inicio) / 1000

        progreso = transcurrido / tiempo_total

        # =========================================
        # INTERVALO DINÁMICO DEL LED 20
        # =========================================

        intervalo = (0.5 - (0.45 * progreso)) / 2

        # =========================================
        # PARPADEO LED 20
        # =========================================

        if time.ticks_diff(ahora,ultimo_cambio) >= intervalo * 1000:

            if estado_led == 0:

                estado_led = 1

            else:

                estado_led = 0

            leds[4].value(estado_led)

            ultimo_cambio = ahora

        # =========================================
        # APAGAR LED DE LA PULSACIÓN
        # =========================================

        if led_pulsado != -1:

            if time.ticks_diff(ahora,tiempo_led) >= 350:

                leds[led_pulsado].value(0)

                led_pulsado = -1

        # =========================================
        # LEER BOTONES 0-3
        # =========================================

        for i in range(4):

            estado_actual = botones[i].value()

            # -------------------------------------
            # Detectar flanco 0 -> 1
            # -------------------------------------

            if (estado_actual == 1 and estado_anterior[i] == 0):

                # ---------------------------------
                # Debounce
                # ---------------------------------

                if time.ticks_diff(ahora, ultima_pulsacion) >= 40:

                    ultima_pulsacion = ahora

                    # -----------------------------
                    # Encender LED correspondiente
                    # -----------------------------

                    leds[i].value(1)

                    led_pulsado = i

                    tiempo_led = ahora

                    # -----------------------------
                    # Número correspondiente
                    # -----------------------------

                    respuesta = i

                    # -----------------------------
                    # Comprobar respuesta
                    # -----------------------------

                    if respuesta != lista[posicion]:

                        print("ERROR")

                        leds[4].value(0)

                        # -------------------------
                        # ACUMULAR TIEMPO
                        # (intento incorrecto)
                        # -------------------------

                        tiempo_nivel = time.ticks_diff(ahora, inicio) / 1000

                        acumular_tiempo(tiempo_nivel)

                        return False

                    # -----------------------------
                    # Respuesta correcta
                    # -----------------------------

                    posicion += 1

                    print("Correcto")

                    # -----------------------------
                    # ¿Terminó el nivel?
                    # -----------------------------

                    if posicion == nivel:

                        leds[4].value(0)

                        # -------------------------
                        # ACUMULAR TIEMPO DE
                        # RESPUESTA DE ESTE NIVEL
                        #
                        # "ahora" es el momento en
                        # que se presionó el último
                        # botón correcto, es decir,
                        # el instante en que se
                        # completó el nivel.
                        # -------------------------

                        tiempo_nivel = time.ticks_diff(ahora, inicio) / 1000

                        acumular_tiempo(tiempo_nivel)

                        return True

            # -------------------------------------
            # Guardar estado actual
            # -------------------------------------

            estado_anterior[i] = estado_actual

    # =============================================
    # SE ACABÓ EL TIEMPO
    # =============================================

    leds[4].value(0)

    # ---------------------------------------------
    # ACUMULAR TIEMPO
    # (se agotó el tiempo -> se usó todo tiempo_total)
    # ---------------------------------------------

    acumular_tiempo(tiempo_total)

    return False


# =================================================
# PROGRAMA PRINCIPAL
# =================================================

while True:

    # =============================================
    # ESPERAR AL BOTÓN 15 PARA INICIAR
    # =============================================

    esperar_inicio()

    # =============================================
    # GENERAR SECUENCIA ALEATORIA
    # =============================================

    for i in range(9):

        lista[i] = random.randint(0, 3)

    # =============================================
    # REINICIAR VIDAS
    # =============================================

    vidas = 3

    # =============================================
    # REINICIAR TIEMPO ACUMULADO
    # (nueva partida = nuevo conteo)
    # =============================================

    tiempo_acumulado = 0

    # =============================================
    # COMENZAR NIVEL 1
    # =============================================

    nivel = 1

    reiniciar_juego = False

    # =============================================
    # BUCLE DE NIVELES
    # =============================================

    while nivel <= 9 and vidas > 0:

        print("Nivel:", nivel)
        print("Vidas:", vidas)

        # =========================================
        # DETERMINAR TIEMPO
        # =========================================

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

        # =========================================
        # TIEMPO DE RESPUESTA
        # =========================================

        time_resp = 1.25 * (2 * tiempo * nivel)

        # =========================================
        # MOSTRAR SECUENCIA
        # =========================================

        for i in range(nivel):

            # -------------------------------------
            # Encender LED correspondiente
            # -------------------------------------

            leds[lista[i]].value(1)

            # -------------------------------------
            # Esperar mientras se vigila botón 15
            # -------------------------------------

            if esperar_tiempo(tiempo):

                reiniciar_juego = True

                break

            # -------------------------------------
            # Apagar LEDs
            # -------------------------------------

            apagar_leds()

            # -------------------------------------
            # Esperar mientras se vigila botón 15
            #
            # Esta pausa es el "espacio" entre un
            # LED y el siguiente. NO se aplica
            # después del ÚLTIMO LED, para que la
            # fase de respuesta (LED 16) comience
            # de inmediato.
            # -------------------------------------

            if i < nivel - 1:

                if esperar_tiempo(tiempo):

                    reiniciar_juego = True

                    break

        # =========================================
        # ¿SE SOLICITÓ REINICIO?
        # =========================================

        if reiniciar_juego:

            break

        # =========================================
        # TIEMPO DE RESPUESTA
        # =========================================

        resultado = esperar_con_parpadeo(time_resp,nivel)

        # =========================================
        # ¿SE SOLICITÓ REINICIO?
        # =========================================

        if resultado is None:

            reiniciar_juego = True

            break

        # =========================================
        # COMPROBAR RESULTADO
        # =========================================

        if resultado:

            print("Nivel superado")

            nivel += 1

        else:

            vidas -= 1

            print("Vidas restantes:", vidas)

            if vidas > 0:

                print("Repitiendo nivel...")

            else:

                print("GAME OVER")

        # =========================================
        # APAGAR TODOS LOS LEDS
        # =========================================

        apagar_leds()

        # =========================================
        # PEQUEÑA ESPERA
        # TAMBIÉN VIGILANDO BOTÓN 15
        # =========================================

        if esperar_tiempo(0.4):

            reiniciar_juego = True

            break

    # =================================================
    # JUEGO REINICIADO
    # =================================================

    if reiniciar_juego:

        print("======================")
        print("JUEGO REINICIADO")
        print("======================")

        apagar_leds()

        # ---------------------------------------------
        # El while True vuelve a llamar:
        #
        # esperar_inicio()
        #
        # Por lo tanto el juego queda detenido
        # hasta una NUEVA pulsación corta.
        # ---------------------------------------------

    # =================================================
    # GANÓ
    # =================================================

    elif nivel > 9:

        print("======================")
        print("¡GANASTE!")
        print("======================")

        print("Tiempo acumulado final:", tiempo_acumulado)

        apagar_leds()

        # Esperar 2 segundos vigilando botón 15
        if esperar_tiempo(2):

            print("Reinicio después de ganar.")

    # =================================================
    # GAME OVER
    # =================================================

    else:

        print("======================")
        print("GAME OVER")
        print("======================")

        print("Tiempo acumulado final:", tiempo_acumulado)

        apagar_leds()

        # Esperar 1 segundo vigilando botón 15
        if esperar_tiempo(1):

            print("Reinicio después de GAME OVER.")
