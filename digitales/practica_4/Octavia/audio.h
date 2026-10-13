#pragma once
#include <stdint.h>


void audio_init(uint8_t, float, float);

void audio_set_frequency(float);

uint8_t audio_phase(void);

uint8_t audio_get_sample(void);