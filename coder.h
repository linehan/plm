#ifndef _ENCODER_H
#define _ENCODER_H

#include "util.h"

struct ac_t {
        uint32_t x0;                 /* Range start, initially [0,1), scaled by 2^32 */
        uint32_t x1;                 /* Range end */
        uint32_t word;               /* DECOMPRESS: Last 4 input bytes of archive */
};

void     ac_init               (struct ac_t *e, FILE *file);
int      ac_encode_bit         (struct ac_t *e, int p, int bit);
uint32_t ac_encode_flush       (struct ac_t *e);
void     ac_encode_finish      (struct ac_t *e, FILE *file);
int      ac_decode_bit         (struct ac_t *e, int p);
int      ac_decode_try_add_byte(struct ac_t *e, int byte);


#endif
