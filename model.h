#ifndef _MODELS_H
#define _MODELS_H

#include "util.h"
#include "mixer.h"

int MODEL(uint32_t context, uint8_t last_byte, int last_bit, int bits_left);
int SIMPLE(uint32_t context, uint8_t last_byte, int last_bit, int bits_left);

#endif
