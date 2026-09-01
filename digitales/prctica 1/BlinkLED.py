from machine import Pin, Timer

led = Pin("LED", Pin.OUT)

timer = Timer()

frecuencia = 2.0

estado = False


def cambiar_led(timer):
    global estado

    estado = not estado
    led.value(estado)


def configurar_frecuencia(frecuencia):
    periodo_ms = int(1000 / (frecuencia * 2))

    timer.init(
        period=periodo_ms,
        mode=Timer.PERIODIC,
        callback=cambiar_led
    )


print("Control de frecuencia con Timer")

configurar_frecuencia(frecuencia)

while True:

    entrada = input("Frecuencia (Hz): ")

    try:
        frecuencia = float(entrada)

        if frecuencia > 0:

            configurar_frecuencia(frecuencia)

            print("Nueva frecuencia:", frecuencia, "Hz")

        else:
            print("La frecuencia debe ser mayor que 0")

    except ValueError:
        print("Introduce un número válido")