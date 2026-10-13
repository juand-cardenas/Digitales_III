#include <stdint.h>
#include <stdbool.h>
#include "systick.h"
#include "audio.h"
#include "dac.h"

#define SYST_CSR (*(volatile uint32_t *)0xE000E010) // Control and Status Register
#define SYST_RVR (*(volatile uint32_t *)0xE000E014) // Reload Value Register
#define SYST_CVR (*(volatile uint32_t *)0xE000E018) // Current Value Register

// Variables
volatile uint32_t systick_counter = 0; 
static volatile uint8_t ultima_muestra = 127;

// Initialize the SysTick timer
void systick_init(void) 
    {
        // Detener el temporizador SysTick mientras se inicializa
        SYST_CSR = 0;

        // Establecer el valor de recarga; 125 Mhz / 50000 eventos = 2500 ciclos por evento
        SYST_RVR = 2500 - 1; // Se emplea 2500 - 1 porque el contador cuenta desde 0

        // Limpiar el valor actual del contador
        SYST_CVR = 0;

        // Habilitar SysTick counter, habilitar interrupción, usar fuente de reloj del procesador
        SYST_CSR = 0x07; 
    }

void isr_systick(void) 
    {
        ultima_muestra = audio_get_sample();
        dac_write(ultima_muestra);
        systick_counter++;
    }
