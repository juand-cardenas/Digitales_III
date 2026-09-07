from machine import Pin
import time
import random

# =================================================
# CONFIGURACIÓN
# =================================================

# LEDs son los pines de los leds, el pint 16 es el led de abertencia.
leds = [
    Pin(20, Pin.OUT),
    Pin(19, Pin.OUT),
    Pin(18, Pin.OUT),
    Pin(17, Pin.OUT),
    Pin(16, Pin.OUT)
]

# Botones
# Botón 0 -> LED 20 -> número 0
# Botón 1 -> LED 19 -> número 1
# Botón 2 -> LED 18 -> número 2
# Botón 3 -> LED 17 -> número 3
# Botón 4 -> INICIO / REINICIO
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

# Segmentos (a, b, c, d, e, f, g), se configura los pines de salida para el 7 segemnto
#unsado un for que va desde 6 a 13
seg = [Pin(i, Pin.OUT) for i in range(6, 13)]

# Son los que uso para encender el bloque de los segementos.
dig = [ Pin(13, Pin.OUT), Pin(14, Pin.OUT), Pin(15, Pin.OUT), Pin(21, Pin.OUT)
]

# Tabla (diccionario) de acceso rapido, para saber que leds de los 7 enciendo para cada numero
tabla_7seg = {
    0: (1,1,1,1,1,1,0),
    1: (0,1,1,0,0,0,0),
    2: (1,1,0,1,1,0,1),
    3: (1,1,1,1,0,0,1),
    4: (0,1,1,0,0,1,1),
    5: (1,0,1,1,0,1,1),
    6: (1,0,1,1,1,1,1),
    7: (1,1,1,0,0,0,0),
    8: (1,1,1,1,1,1,1),
    9: (1,1,1,1,0,1,1),
    # se usa para parpadear el número de vidas
    10: (0,0,0,0,0,0,0)
}

# -------------------------------------------------
# numero[0] -> Nivel actual
# numero[1] -> Vidas restantes
# numero[2] -> Unidades del tiempo transcurrido
# numero[3] -> Décimas del tiempo transcurrido
# -------------------------------------------------
#Sirve para saber que se debe ver en los 7 segmentos
numero = [0, 0, 0, 0]

#------------------------------------------------------
# Variables de multiplexación (independientes del
# resto del juego, para no chocar con otras
# variables llamadas "intervalo" o "posicion")
posicion_display = 0#Es una variable que indica qué display de los 4 estamos controlando en este momento.
ultimo_cambio_display = time.ticks_ms()
#controlan el "barrido" — cada 2 ms se apaga todo, se prepara el patrón
#del siguiente dígito, y se enciende ese dígito.
# Tiempo que permanece encendido cada dígito (ms)
INTERVALO_DISPLAY = 2
#-----------------------------------------------------

# Tiempo necesario para reiniciar
TIEMPO_REINICIO = 2000       # 2 segundos

# Límite máximo del tiempo acumulado de respuesta
TIEMPO_ACUMULADO_MAX = 9.9    # segundos


# =================================================
# VARIABLES
# =================================================

tiempo = 0 #duración con la que se muestra cada LED de la secuencia (varía según el nivel).
time_resp = 0 #tiempo total que tiene el jugador para responder un nivel.
vidas = 3 #vidas restantes (empieza en 3).
tiempo_trans = 0

lista = [0] * 10 #arreglo de 10 posiciones con números aleatorios 0-3 que forman la secuencia del nivel actual.

# -------------------------------------------------
# Tiempo acumulado que el jugador tarda en
# COMPLETAR CORRECTAMENTE cada nivel (mientras
# el LED 16 parpadea, ingresando la secuencia).
#
# Se va sumando nivel a nivel y deja de acumular
# al llegar a TIEMPO_ACUMULADO_MAX (9.9 s).
tiempo_acumulado = 0 #suma de los tiempos que el jugador tarda en completar cada nivel correctamente,
# limitado a TIEMPO_ACUMULADO_MAX = 9.9 segundos.


# -------------------------------------------------
# IMPORTANTE:
# el botón 15.
#guarda el instante en que empezó a presionarse el botón de inicio/reinicio
#(para medir si es pulsación larga o corta).
boton15_desde = None #ahora es otro numero.
#---------------------------------------------------

# =================================================
# apaga los 5 LEDs
# =================================================

def apagar_leds():

    for led in leds:
        led.value(0)


# =================================================
# DISPLAY: APAGAR TODOS LOS DÍGITOS, pone en 1
#(apagado, porque el común es activo en bajo) los 4 pines de dígito del display.
# =================================================

def apagar_digitos():

    for d in dig:
        d.value(1)


# =================================================
# DISPLAY: ESCRIBIR SEGMENTOS DE UN NÚMERO (0-9)
# =================================================

def escribir_segmentos(n):

    patron = tabla_7seg[n] #se obtiene la secuencia de (1,1,1,1,0,0,1) dependiendo de n

    for i in range(7):

        if patron[i]:

            seg[i].value(0)

        else:

            seg[i].value(1)


# =================================================
# DISPLAY: MULTIPLEXAR
#
# Debe llamarse muy seguido (idealmente en TODO
# bucle de espera del juego) para que los 4
# dígitos se vean encendidos a la vez.
# =================================================

def multiplexar():

    global posicion_display #Es una variable que indica qué display de los 4 estamos controlando en este momento.
    global ultimo_cambio_display

    ahora = time.ticks_ms()

    if time.ticks_diff(ahora, ultimo_cambio_display) >= INTERVALO_DISPLAY: #reviso si han pasado dos ms desde la variable ahora fue ejecutada

        apagar_digitos()#Apaga todos los dígitos.

        escribir_segmentos(numero[posicion_display]) #me dice que cosa tomar de numero y convertirlo en 7 segemto.

        dig[posicion_display].value(0) #Enciende solo ese bloque.

        posicion_display += 1

        if posicion_display >= 4:

            posicion_display = 0

        ultimo_cambio_display = ahora #esta variable sirve como referencia para medir cuánto tiempo ha pasado desde el último cambio.


# =================================================
# DISPLAY: ACTUALIZAR NIVEL / VIDAS / TIEMPO
#
# n_tiempo se separa en unidades y décimas,
# ya que TIEMPO_ACUMULADO_MAX es 9.9 (2 dígitos).
# =================================================

def actualizar_numero(n_nivel, n_vidas, n_tiempo):

    if n_tiempo > TIEMPO_ACUMULADO_MAX:

        n_tiempo = TIEMPO_ACUMULADO_MAX

    if n_tiempo < 0:

        n_tiempo = 0

    entero = int(n_tiempo)

    decimal = int(round((n_tiempo - entero) * 10))#me da la parte decimal.
    

    if decimal >= 10:

        decimal = 0

        entero += 1

    if entero > 9:

        entero = 9

    numero[0] = n_nivel if 0 <= n_nivel <= 9 else 9
    numero[1] = n_vidas if 0 <= n_vidas <= 3 else 3
    numero[2] = entero
    numero[3] = decimal 


# =================================================
# ESPERAR X MILISEGUNDOS SIN CONGELAR EL DISPLAY
# =================================================
#es para mantner los 7 segmentos funcioanndo mientar corurre una animacion de evento
def esperar_ms_con_display(ms):

    inicio = time.ticks_ms()

    while time.ticks_diff(time.ticks_ms(), inicio) < ms:

        multiplexar()

        time.sleep_ms(2)


# =================================================
# ANIMACIÓN: SECUENCIA CORRECTA (NIVEL SUPERADO)
#
# Barrido rápido, una sola pasada, LED 0 -> LED 3.
# =================================================

def animacion_nivel_superado():

    apagar_leds()

    for i in range(4):

        leds[i].value(1)

        esperar_ms_con_display(60)

        leds[i].value(0)


# =================================================
# ANIMACIÓN: ENTRADA INCORRECTA
#
# Los 4 LEDs (0-3) encienden juntos y parpadean
# 2 veces.
# =================================================

def animacion_entrada_incorrecta():

    apagar_leds()

    for _ in range(2):

        for i in range(4):

            leds[i].value(1)

        esperar_ms_con_display(150)

        apagar_leds()

        esperar_ms_con_display(100)

# =================================================
# ANIMACIÓN: SE AGOTÓ EL TIEMPO
#
# El LED 16 (leds[4]) se queda ENCENDIDO fijo,
# y son los dígitos de TIEMPO (unidades y
# décimas) los que parpadean en el 7 segmentos.
# =================================================

def animacion_agotamiento_tiempo():

    leds[4].value(1)

    unidades_actual = numero[2]
    decimas_actual = numero[3]

    for _ in range(4):

        numero[2] = 10   # blanco
        numero[3] = 10   # blanco

        esperar_ms_con_display(150)

        numero[2] = unidades_actual
        numero[3] = decimas_actual

        esperar_ms_con_display(150)

    leds[4].value(0)

# =================================================
# ANIMACIÓN: PÉRDIDA DE VIDA
#
# Parpadea SOLO el dígito de vidas en el 7
# segmentos (con el valor ANTERIOR, antes de
# bajarlo), para avisar que se perdió una vida.
# =================================================

def animacion_perdida_vida(veces=3, duracion_ms=150):

    valor_actual = numero[1]

    for _ in range(veces):

        numero[1] = 10   # blanco

        esperar_ms_con_display(duracion_ms)

        numero[1] = valor_actual

        esperar_ms_con_display(duracion_ms)


# =================================================
# ANIMACIÓN: FINALIZACIÓN EXITOSA (GANASTE)
#
# Barrido ida y vuelta (tipo "KITT") x2,
# seguido de 3 destellos con los 4 LEDs juntos.
# =================================================

def animacion_victoria():

    apagar_leds()

    for _ in range(2):

        for i in range(4):

            leds[i].value(1)

            esperar_ms_con_display(50)

            leds[i].value(0)

        for i in range(2, -1, -1): #range(inicio, final, paso)

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

   


# =================================================
# REVISAR BOTÓN 16
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
    # Mantener el display encendido mientras se revisa el botón de reinicio
    multiplexar()
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
            # Esperar a que el usuario SUELTE el botón.
            # Esto evita que después del reinicio
            # el mismo botón vuelva a iniciar automáticamente.

            while botones[4].value() == 1:

                multiplexar()

                time.sleep_ms(10)

            boton15_desde = None

            return True

    else:

        # El botón está suelto, no ha sido precionado

        boton15_desde = None

    return False


# =================================================
# ESPERAR UN TIEMPO, una funcion echa como delay, como no se puede dejar de multiplxar ni de reviar el
#boton de reinicio, esto lo hace minetra se "espera"
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

        # Mantener el display encendido
        multiplexar()

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
# Si el botón quedó presionado después de un
# reinicio, primero obliga a soltarlo.
# =================================================

def esperar_inicio():

    apagar_leds()
    # Si el botón está presionado al entrar, esperar hasta que sea soltado.
    while botones[4].value() == 1:

        multiplexar()

        time.sleep_ms(10)

    print("Botón liberado. Presione para comenzar.")

    while True:

        # Mantener el display encendido MIENTRAS
        # se espera la pulsación (antes esto no se llamaba y el display se congelaba)
        multiplexar()

        if botones[4].value() == 1:

            inicio_pulsacion = time.ticks_ms()

            # -------------------------------------
            # Esperar mientras esté presionado
            # -------------------------------------

            while botones[4].value() == 1:

                multiplexar()

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

                        multiplexar()

                        time.sleep_ms(5)

                    # Volver a esperar una nueva
                    # pulsación
                    break

                time.sleep_ms(5)

            else:

                # ---------------------------------
                # El botón se soltó antes de los
                # 2 segundos.Es una pulsación corta.
                # ---------------------------------

                print("INICIANDO JUEGO")

                return



# ESPERAR MIENTRAS LED 20 PARPADEA Y RECIBIR RESPUESTAS
# False -> error / tiempo agotado
# True  -> nivel completado
# None  -> reinicio solicitado
def esperar_con_parpadeo(tiempo_total, nivel, vidas_actuales):
    global tiempo_acumulado
    inicio = time.ticks_ms()
    ultimo_cambio = inicio #guarda cuándo fue la última vez que cambió el estado del LED 20.
    # Arranca ENCENDIDO para que el jugador vea
    # de inmediato que puede empezar a ingresar el patrón, sin esperar el primer intervalo.
    estado_led = 1
    leds[4].value(1)
    # Estado anterior de los botones 0-3
    estado_anterior = [0, 0, 0, 0]
    # Posición de la secuencia
    posicion = 0 #Indica qué elemento de la secuencia se debe introducir
    # Última pulsación
    ultima_pulsacion = time.ticks_ms()

    # Momento en que se presionó cada botón (0-3).
    # Es un arreglo (uno por botón) para poder
    # apagar varios LEDs de forma independiente,
    # aunque se hayan presionado casi al mismo
    # tiempo. Antes era una sola variable y por
    # eso una pulsación nueva "perdía" el rastro
    # de la anterior, dejándola encendida para siempre.

    tiempo_led = [None, None, None, None] #arreglo que guarda cuándo se encendió cada LED de respuesta,
    #para poder apagarlo automáticamente pasados 350 ms sin perder el rastro de otros LEDs encendidos simultáneamente
    # =============================================
    # BUCLE PRINCIPAL
    
    
    while time.ticks_diff(time.ticks_ms(),inicio) < (tiempo_total * 1000): #el 1000 es para convertir de s a ms
        ahora = time.ticks_ms()
        multiplexar()
        # =========================================
        # REVISAR BOTÓN 15
        # =========================================
        if revisar_boton_reinicio():

            return None

        # =========================================
        # TIEMPO TRANSCURRIDO
        # =========================================

        transcurrido = time.ticks_diff(ahora, inicio) / 1000 #tiempo trancurrudo desde que empezo el nivel.
        progreso = transcurrido / tiempo_total

        # =========================================
        # DISPLAY: TIEMPO EN VIVO
        #
        # Lo ya acumulado en niveles anteriores
        # + lo que se lleva en este intento.
        # =========================================

        actualizar_numero(nivel, vidas_actuales, tiempo_acumulado + transcurrido)

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

            ultimo_cambio = ahora #despues de esto, empiza a contar el tiempo de parpadeo de nuevo

        # Se revisan los 4 leds para apagar los leds encedidos por los botones, cada uno con su propio
        # tiempo, para poder apagar varios a la vez sin perder el rastro de ninguno.
        # =========================================
        for j in range(4):

            if tiempo_led[j] is not None:

                if time.ticks_diff(ahora, tiempo_led[j]) >= 350:

                    leds[j].value(0)

                    tiempo_led[j] = None

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

                    tiempo_led[i] = ahora

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

                        actualizar_numero(nivel, vidas_actuales, tiempo_acumulado)

                        animacion_entrada_incorrecta()

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

                        actualizar_numero(nivel, vidas_actuales, tiempo_acumulado)

                        animacion_nivel_superado()

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

    actualizar_numero(nivel, vidas_actuales, tiempo_acumulado)

    animacion_agotamiento_tiempo()

    return False


# =================================================
# PROGRAMA PRINCIPAL

apagar_digitos()
# DISPLAY: ESTADO INICIAL AL ENERGIZAR
# Muestra Nivel 1, 3 Vidas y Tiempo 00
# =================================================

actualizar_numero(1, 3, 0)

while True:
    # ESPERAR AL BOTÓN 15 PARA INICIAR
    # El display NO se toca aquí: debe conservar
    # lo último mostrado (ya sea el estado inicial
    # o el resultado del juego anterior) hasta quempresione para iniciar una nueva
    # partida.
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

    tiempo_acumulado = 0.00

    # =============================================
    # COMENZAR NIVEL 1
    # =============================================

    nivel = 1

    reiniciar_juego = False

    # =============================================
    # DISPLAY: NIVEL 1, VIDAS 3, TIEMPO 0
    # =============================================

    actualizar_numero(nivel, vidas, tiempo_acumulado)

    # =============================================
    # BUCLE DE NIVELES
    # =============================================

    while nivel <= 9 and vidas > 0:

        print("Nivel:", nivel)
        print("Vidas:", vidas)

        # =========================================
        # DISPLAY: REFRESCAR NIVEL / VIDAS / TIEMPO
        # =========================================

        actualizar_numero(nivel, vidas, tiempo_acumulado)

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

        resultado = esperar_con_parpadeo(time_resp, nivel, vidas)

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

            # =====================================
            # ANIMACIÓN: PÉRDIDA DE VIDA
            #
            # Se parpadea el dígito con el valor
            # ANTERIOR, y DESPUÉS se resta la vida.
            # =====================================

            animacion_perdida_vida()

            vidas -= 1

            print("Vidas restantes:", vidas)

            if vidas > 0:

                print("Repitiendo nivel...")

            else:

                print("GAME OVER")

        # =========================================
        # DISPLAY: REFRESCAR TRAS EL RESULTADO
        # =========================================

        actualizar_numero(nivel, vidas, tiempo_acumulado)

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
        apagar_leds()
        # DISPLAY: RESETEAR A NIVEL 1, VIDAS 3, TIEMPO 0
        actualizar_numero(1, 3, 0)

    # =================================================
    # GANÓ
    # =================================================

    elif nivel > 9:

        print("======================")
        print("¡GANASTE!")
        print("======================")

        print("Tiempo acumulado final:", tiempo_acumulado)

        animacion_victoria()

        apagar_leds()

        # Esperar 2 segundos vigilando botón 15
        if esperar_tiempo(2):

            print("Reinicio después de ganar.")

    # =================================================
    # GAME OVER
    # =================================================

    else:

        apagar_leds()

        # Esperar 1 segundo vigilando botón 15
        if esperar_tiempo(1):

            print("Reinicio después de GAME OVER.")
