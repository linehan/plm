#ifndef _ENCODER_H
#define _ENCODER_H

#include "util.h"

/*
 * The range is initially [0,1). Range values are scaled by 2^32, 
 * i.e., represented as 32-bit fixed-point values (uint32_t).
 *
 * In this world, 1 == 0xFFFFFFFF == UINT32_MAX.
 */

struct ac_t {
        uint32_t lo;    /* Lower-bound of range */
        uint32_t hi;    /* Upper-bound of range */
        uint32_t word;  /* Last 4 input bytes (used for decoding) */
};


void     ac_init                (struct ac_t *ac, FILE *file);
void     ac_encoder_flush       (struct ac_t *ac, FILE *file);

int      ac_encoder_add_bit     (struct ac_t *ac, int p, int bit);
uint32_t ac_encoder_try_get_byte(struct ac_t *ac);

int      ac_decoder_get_bit     (struct ac_t *ac, int p);
int      ac_decoder_try_add_byte(struct ac_t *ac, uint8_t byte);

#endif
