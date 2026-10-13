#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "audio.h"

#define PI 3.14159265358979323846 
#define AUDIO_TABLE_SIZE  256

static uint8_t audio_table[AUDIO_TABLE_SIZE];

// Variables globales del módulo
static uint8_t  audio_bits = 8;
static float    audio_current_freq = 440.0; // Frecuencia inicial en Hz
static float    audio_systick_freq = 50000.0; // Frecuencia del SysTick en Hz
static float    i_phase = 0.0;
static float    phase_accumulator = 0.0f;


void audio_init(uint8_t bits, float freq_inicial, float systick_freq) 
{
    // Guardamos los valores que nos mandó el main en las variables globales
    audio_bits         = bits; 
    audio_current_freq = freq_inicial;
    audio_systick_freq = systick_freq;
    phase_accumulator = 0.0f;
    uint8_t  DAC_value = 0;
    float    radianes = 0.0f;

    // Calcular paso inicial
    audio_set_frequency(freq_inicial);

    for (uint32_t i = 0; i < AUDIO_TABLE_SIZE; i++)
        {

            radianes = (2.0f * PI * (float)i) / (float)AUDIO_TABLE_SIZE;

            DAC_value = (uint8_t)(127.5f + 127.5f * sinf(radianes));

            audio_table[i] = DAC_value; 
        }
}

void audio_set_frequency(float nueva_frecuencia) 
{
    audio_current_freq = nueva_frecuencia;

    if (nueva_frecuencia == 0)
     {
        i_phase = 0.0f; // Silencio
        return;
        }

    i_phase = ((float)AUDIO_TABLE_SIZE * nueva_frecuencia) / audio_systick_freq;
}


uint8_t audio_phase(void)
{
    // Variables: declaracion e inicializacion    
    uint8_t indice_actual = (uint8_t)phase_accumulator;  

    phase_accumulator += i_phase; 
    
    // Evitar que sobrepase el rango de muestras
    if (phase_accumulator >= (float)AUDIO_TABLE_SIZE) 
    {
        phase_accumulator -= (float)AUDIO_TABLE_SIZE;
    }

    // Extraer la parte entera
    return indice_actual;    
}

uint8_t audio_get_sample(void)
{   
    uint8_t  indice_actual = 0;
    uint8_t  DAC_value = 0;
    
    if (audio_current_freq == 0.0f) 
    {
        return 127; // Valor medio para silencio
    }

    // Obtener el índice actual de la fase
    indice_actual = audio_phase();

    DAC_value = audio_table[indice_actual];

    return DAC_value;
}