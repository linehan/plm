#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include "coder.h"

/******************************************************************************
 * ARITHMETIC CODER 
 ******************************************************************************/

extern int MODE;

#define PR_BITS 12    /* (0-4095) */
#define PR_MAX  4095  /* Maximum probability value */

/* 
 * NOTE
 * To adaptively choose the midpoint of our interval, we 
 * multiply the current interval by a fractional value 
 * (p/PR_MAX), where p is some probability (0-PR_MAX).
 *
 * We are working in fixed-point arithmetic, so to get
 * the integral part, we do:
 *
 *      interval * (p/PR_MAX)
 *
 * To get the fractional part, we compute the remainder of
 * the interval mod PR_MAX, and do: 
 *
 *      remainder * (p/PR_MAX)
 *
 * Then we add the integral and fractional parts, and this 
 * is the new midpoint.
 *
 * ---------------------------------------------------------------
 */
#define MIDPOINT(lo, hi, prob) \
        (lo + ((hi - lo)>>PR_BITS)*prob + ((hi - lo & PR_MAX)*prob>>PR_BITS))
/*
 * ---------------------------------------------------------------
 *
 * Recall that ">> PR_BITS" is arithmetic right-shift by PR_BITS bits,
 * which is equivalent to division by 2^PR_BITS = PR_MAX.
 *
 * Recall that "& PR_MAX" is equivalent to calculating the remainder 
 * of the interval mod PR_MAX.
 *
 * This works because:
 *      
 *      x mod 2^n == x & 2^(n-1)
 *
 * This only works for integer powers of 2, because powers of two 
 * have the unique property of having only one bit set to '1' in 
 * their (unsigned) binary expansion.
 */

/******************************************************************************
 * FUNCTIONS 
 ******************************************************************************/

/**
 * ac_init()
 * `````````
 * Initialize the encoder structure.
 *
 * @ac   : Reference to encoder structure.
 * @file : Output file.
 * Return: Nothing
 */
void ac_init(struct ac_t *ac, FILE *file)
{
        int i;

        ac->lo   = 0x00000000;
        ac->hi   = 0xFFFFFFFF;
        ac->word = 0x00000000;

        if (level > 0 && MODE == DECOMPRESS) {
                /* Read in a word (getc returns a byte) */
                ac->word = (ac->word << 8) | (getc(file) & 0xFF);
                ac->word = (ac->word << 8) | (getc(file) & 0xFF);
                ac->word = (ac->word << 8) | (getc(file) & 0xFF);
                ac->word = (ac->word << 8) | (getc(file) & 0xFF);
        }
}

/**
 * ac_encoder_add_bit()
 * ````````````````````
 * Encode a bit.
 *
 * @ac   : Reference to encoder structure.
 * @p    : A prediction (probability) from (0-4096) 
 * @bit  : Bit to encode.
 * Return: The bit decoded (1 or 0) (should match @bit)
 */
int ac_encoder_add_bit(struct ac_t *ac, int p, int bit)
{
        /* Midpoint of current range */
        uint32_t mid; 

        mid = MIDPOINT(ac->lo, ac->hi, p);

        /*log_msg(LOG_AC, "%g\n", (double)p/(double)4096);*/

        assert(mid >= ac->lo && mid < ac->hi); 
        
        /* Adjust the endpoints depending on the bit value y */
        if (bit == 0) {
                ac->lo = mid + 1;
        } else {
                ac->hi = mid;
        }

        return bit;
}


/**
 * ac_encoder_try_get_byte()
 * `````````````````````````
 * Get any ready bytes from the encoder.
 *
 * @ac   : Reference to arithmetic coder structure.
 * Return: Encoded byte if available, UINT32_MAX if none available.
 */
uint32_t ac_encoder_try_get_byte(struct ac_t *ac)
{
        uint32_t byte = 0x00000000;

        /* The endpoints are equal on their high-order byte */
        if (((ac->lo ^ ac->hi) & 0xFF000000) == 0) {

                /* Gets high-order byte */
                byte = (ac->hi >> 24);

                ac->lo = (ac->lo << 8);
                ac->hi = (ac->hi << 8) + 255;

                return byte;
        } else {
                return UINT32_MAX;
        }
}


/** 
 * ac_encoder_flush()
 * ``````````````````
 * Flush first unequal byte of range
 *
 * @ac   : Reference to arithmetic coder structure.
 * Return: Nothing
 */
void ac_encoder_flush(struct ac_t *ac, FILE *file) 
{
        if (MODE == COMPRESS && level > 0) {
                /* Write high-order byte to file */
                putc((ac->lo >> 24), file);
        }
}


/**
 * ac_decoder_try_add_byte()
 * `````````````````````````
 * Try to add a new input character to the encoder.
 *
 * @ac   : Reference to encoder structure.
 * @byte : New byte from the compressed file.
 * Return: 1 if @byte appended, otherwise 0.
 */
int ac_decoder_try_add_byte(struct ac_t *ac, uint8_t byte)
{
        /* The endpoints are equal on their high-order byte */
        if (((ac->lo ^ ac->hi) & 0xFF000000) == 0) {

                /* Shift endpoints and separate low-order bytes */
                ac->lo = (ac->lo << 8) | 0x00000000;
                ac->hi = (ac->hi << 8) | 0x000000FF;

                /* Shift+Append the new byte to the word */
                ac->word = (ac->word << 8) | (byte & 0xFF);

                return 1;
        } else {
                return 0;
        }
}


/**
 * ac_decoder_get_bit()
 * ````````````````````
 * Decode a bit.
 *
 * @ac   : Reference to encoder structure.
 * @p    : A prediction (probability) from (0-4096) TODO:unsigned? 
 * Return: The bit decoded (1 or 0)
 */
int ac_decoder_get_bit(struct ac_t *ac, int p)
{
        /* Midpoint of current range */
        uint32_t mid; 
        int      bit;

        mid = MIDPOINT(ac->lo, ac->hi, p);

        assert(mid >= ac->lo && mid < ac->hi);
        
        bit = (ac->word <= mid) ? 1 : 0;
        
        /* Adjust the endpoints depending on the bit value y */
        if (bit == 0) {
                ac->lo = mid + 1;
        } else {
                ac->hi = mid;
        }

        return bit;
}

