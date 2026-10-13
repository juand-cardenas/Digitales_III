#pragma once

extern volatile uint32_t systick_counter;

void systick_init(void);
void isr_systick(void);
