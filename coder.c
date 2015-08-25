#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "predictor.h"
#include "coder.h"

/******************************************************************************
 * ARITHMETIC CODER 
 ******************************************************************************/
#include <stdint.h>
#include <limits.h>

extern int MODE;

/* 
 * APPEND_BYTE()
 * `````````````
 * "Append" a byte to an existing value. 
 *
 * @target: Value to append to. Should be at least 16-bits.
 * @byte  : Byte to be appended. Will be wrapped if @byte>255.
 * Return : @target appended with @byte. @target is not modified.
 *
 * NOTE
 * Here is an explanation of the bitwise operations.
 *
 *      0. Shift the target left by 8 bits, 
 *         to make room for the new byte.
 *
 *      1. Ensure that the byte is actually
 *         a byte, by doing a BITWISE AND
 *         with the value 0xFF == 255 == 2^8, 
 *         the maximum value a byte can have.
 *
 *         This acts as a fast binary modulus,
 *         ensuring the byte value is between
 *         0-255, and will fit within 8 bits.
 *
 *      2. Use BITWISE OR to combine the left
 *         shifted target with the wrapped byte
 *         to obtain the final output.
 *
 *         Instead of BITWISE OR, addition could
 *         also be used, as the two are the same
 *         thing here since the 8 low-order bits
 *         are all 0. 
 */
#define APPEND_BYTE(target, byte)       (((target)<<8) | (byte & 0xFF))

/* 
 * NOTE
 * We want to use our probability value to choose the midpoint
 * of the new interval. 
 *
 * To do this, we simply multiply the current interval by
 * a fractional value (p/4096), where p is the probability
 * ranging from 0-4096.
 *
 *      interval * (p/4096)
 *
 * This gives us the integral part.
 *
 * To get the fractional part, we compute the remainder of
 * the interval mod 4096, and again apply
 *
 *      remainder * (p/4096)
 *
 * Then we add them, and this is the new midpoint.
 *
 * ---------------------------------------------------------------
 *
 * Recall that ">> 12" is arithmetic right-shift by 12 bits,
 * which is equivalent to division by 2^12 = 4096.
 *
 * Recall that "& 4095" is equivalent to calculating the remainder 
 * of the interval mod 4096.
 *
 * ---------------------------------------------------------------
 * This works because of an interesting property of modular 
 * arithmetic:
 *      
 *      x mod 2^n == x & 2^(n-1)
 *
 * This only works for integer powers of 2, because powers of two 
 * have the unique property of having only one bit set to '1' in 
 * their (unsigned) binary expansion.
 *
 * This is probably true in any base, but obviously as '&' is
 * a bitwise operation, we only care about base-2.
 */
#define MIDPOINT(lo, hi, prob) \
        (lo + ((hi - lo)>>12)*prob + ((hi - lo & 4095)*prob>>12))


/**
 * encoder_init()
 * ``````````````
 * Initialize the encoder structure.
 *
 * @enc  : Reference to encoder structure.
 * @file : Output file.
 * Return: Nothing
 */
void ac_init(struct ac_t *e, FILE *file)
{
        int i;

        e->x0      = 0;
        e->x1      = 0xffffffff;
        e->word    = 0;

        if (level > 0 && MODE == DECOMPRESS) {
                for (i=0; i<4; i++) {
                        e->word = APPEND_BYTE(e->word, getc(file));
                }
        }
}

/**
 * ac_encode_flush()
 * `````````````````
 * Get any ready bytes from the encoder.
 *
 * @e    : Reference to encoder structure.
 * Return: Encoded byte if available, UINT32_MAX if none available.
 */
uint32_t ac_encode_flush(struct ac_t *e)
{
        uint32_t ret;

        if (((e->x0 ^ e->x1) & 0xFF000000) == 0) {
                /* 
                 * Get leading 8 bits in low-order position 
                 * by shifting right 24 bits.
                 */
                ret = e->x1 >> 24;

                e->x0 = (e->x0 << 8);
                e->x1 = (e->x1 << 8) + 255;

                return ret;
        } else {
                return UINT32_MAX;
        }
}

/** 
 * ac_encode_finish()
 * ``````````````````
 * Flush first unequal byte of range
 *
 * @enc  : Reference to encoder structure.
 * Return: First unequal byte of range if available, else UINT32_MAX
 */
void ac_encode_finish(struct ac_t *e, FILE *file) 
{
        if (MODE == COMPRESS && level>0) {
                /* Flush first unequal byte of range */
                putc(e->x0 >> 24, file);
        }
}


/**
 * ac_encode_bit()
 * ```````````````
 * Encode a bit.
 *
 * @e    : Reference to encoder structure.
 * @p    : A prediction (probability) from (0-4096) TODO:unsigned? 
 * @bit  : Bit to encode.
 * Return: The bit decoded (1 or 0) (should match @bit)
 */
int ac_encode_bit(struct ac_t *e, int p, int bit)
{
        /* Midpoint of current range */
        uint32_t xmid; 

        /* What is this doing? */
        p += (p < 2048) ? 1 : 0; 

        /* See NOTE at top */
        xmid = MIDPOINT(e->x0, e->x1, p);

        log_msg(LOG_AC, "%g\n", (double)p/(double)4096);

        assert(xmid >= e->x0 && xmid < e->x1); 
        
        /* Adjust the endpoints depending on the bit value y */
        if (bit == 0) {
                e->x0 = xmid+1;
        } else {
                e->x1 = xmid;
        }

        return bit;

        /* Whether we are ready to write output */
        /*return (((e->x0 ^ e->x1) & 0xFF000000) == 0) ? 1 : 0;*/
}


/**
 * ac_decode_try_add_byte()
 * ````````````````````````
 * Try to add a new input character to the encoder.
 *
 * @e    : Reference to encoder structure.
 * @byte : New byte from the compressed file.
 * Return: 1 if @byte appended, otherwise 0.
 */
int ac_decode_try_add_byte(struct ac_t *e, int byte)
{
        if (((e->x0 ^ e->x1) & 0xFF000000) == 0) {

                e->x0 = (e->x0 << 8);
                e->x1 = (e->x1 << 8) + 255;

                e->word = APPEND_BYTE(e->word, byte);

                return 1;
        } else {
                return 0;
        }
}

/**
 * ac_decode_bit()
 * ```````````````
 * Decode a bit.
 *
 * @e    : Reference to encoder structure.
 * @p    : A prediction (probability) from (0-4096) TODO:unsigned? 
 * Return: The bit decoded (1 or 0)
 */
int ac_decode_bit(struct ac_t *e, int p)
{
        /* Midpoint of current range */
        uint32_t xmid; 
        int bit;

        /* What is this doing? */
        p += (p < 2048) ? 1 : 0; 

        /* See NOTE at top */
        xmid = MIDPOINT(e->x0, e->x1, p);

        assert(xmid >= e->x0 && xmid < e->x1);
        
        bit = (e->word <= xmid) ? 1 : 0;
        
        /* Adjust the endpoints depending on the bit value y */
        if (bit == 0) {
                e->x0 = xmid+1;
        } else {
                e->x1 = xmid;
        }

        return bit;
}

