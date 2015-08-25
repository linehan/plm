#include "model.h"

/******************************************************************************
 * SMALL STATIONARY CONTEXT MAP 
 *
 * Maps a context hash to a probability.
 * The probability is looked up directly using the context hash.
 *
 * The size of the table should be a power of 2 in bytes.
 * 
 * The size of the context hash should be < m/512. 
 * High bits are discarded.
 ******************************************************************************/

/*
 * NOTE:
 * For inputs that have many unique contexts, in which case the
 * various contexts and their probabilities will likely only be
 * visited a few times, or only once, then the UPDATE_RATE should
 * be lower, in order to maximize the step in predictions during
 * the byte that context is active. 
 *
 * A range from 2-8 is what I generally use. Remember, the rate is
 * just a bit shift to scale the factor you add or subtract to the
 * probability.
 */
#define UPDATE_RATE 2

struct ssm_t {
        uint16_t *table;     /* Table of bit history */
        int       context;   /* Current context */ 
        int       size;      /* Size of table */ 
        uint16_t *current;   /* Current history item */
};


/**
 * ssm_init()
 * ``````````
 * Initialize a small stationary context map
 *
 * @map   : Reference to the type
 * @memory: Bytes available to allocate
 * Return : Nothing
 */
void ssm_init(struct ssm_t *map, int bytes)
{
        int i;

        map->table   = calloc(bytes/2*sizeof(uint16_t), 1);
        map->size    = bytes/2;
        map->context = 0;

        for (i=0; i<map->size; i++) {
                /* 
                 * The initial probability value is 1/2. 
                 * A 16-bit probability value can range 
                 * from 0-65536, so 1/2 is 65536/2 = 32768.
                 */
                map->table[i] = 32768;
        }

        map->current = &map->table[0];
}

/**
 * ssm_set()
 * `````````
 * Set a context
 *
 * @map    : Reference to the type
 * @context: Context
 * Return  : Nothing
 */
void ssm_set(struct ssm_t *map, uint32_t context)
{
        /* 
         * Mod by (map->size - 256), so we can add the 
         * last_byte (a byte has values (0-255)) to the
         * context later and obtain a value that is still
         * within our table.
         */
        map->context = context*256 & map->size-256;
        map->current = &map->table[map->context];
}

/*
 * A stationary map stores the probability that the next bit
 * will be a 1 as an unsigned 16-bit fixed-point integer. 
 *
 * 16-bit types have values (0-64436).
 *
 * Initially all the probabilities are set to at 1/2, which is 
 * the value 64436/2 = 32768 in our fixed-point representation.
 *
 * Separate "p1" probabilities are stored for each context, by 
 * placing the "p1" values in a table, indexed by a hash of the 
 * context, allowing us to look up, if we find ourself in a 
 * particular context, what the probability is that the next bit 
 * will be a 1.
 *
 * Once the actual bit is observed, the probability "p1" in that
 * context is updated, according to the following rule:
 *
 *      if the bit is a 0:
 *              p = p - Kp
 *
 *      if the bit is a 1:
 *              p = p + K(1-p)
 *
 * Where the '1' in K(1-p) is actually UINT16_MAX, since we are
 * in a fixed-point representation of a real number from (0-1].
 *
 * Roughly, if we see a 0, we decrease the probability of seeing
 * a 1. If we see a 1, we increase the probability of seeing a 1.
 * It's that simple.
 *
 * K is a "learning rate" which adjusts how large the steps in our
 * probability should be.
 */

void ssm_update(struct ssm_t *map, uint8_t last_byte, int last_bit)
{
        /* 
         * UPDATE THE HISTORY FOR THE LAST BIT
         */
        uint16_t prob;

        prob = *map->current;
                
        if (last_bit == 0) {
                prob = prob - (prob >> UPDATE_RATE);
        } else {
                prob = prob + ((UINT16_MAX - prob) >> UPDATE_RATE);
        }

        *map->current = prob;
}

/**
 * ssm_predict()
 * `````````````
 * Write a prediction to a mixer
 *
 * @map  : Reference to the type
 * @mixer: Reference to a mixer 
 * Return: Nothing
 */
int ssm_predict(struct ssm_t *map) 
{
        /* 
         * We have a 16-bit integer, but our probability values
         * go from (0-4096), that is, a 12-bit integer. So we
         * have to scale down by 4 bits in order to get into that
         * range (0-4096).
         */
        return stretch(*map->current >> 4);
}


/******************************************************************************
 * MODEL 
 ******************************************************************************/

int MODEL(uint32_t context, uint8_t last_byte, int last_bit, int do_update) 
{
        static struct ssm_t   cm;
        static struct nn_t    mixer;
        static uint32_t       context_hash;
        static int            first = 1;

        uint8_t  byte0;
        uint8_t  byte1;
        uint8_t  byte2;
        int      i;

        /* 
         * Initialize the context models 
         * and the mixer 
         */
        if (first) {
                ssm_init(&cm, MEM*32);

                /* 
                 * 512  inputs
                 * 1040 neural networks
                 * 4    selectable inputs
                 * 128  default weight 
                 */
                nn_init(&mixer, 512, 1040, 4, 128);

                first = 0;
        }

        nn_train(&mixer, last_bit);
        nn_input(&mixer, 256);

        /* 
         * When we have no bits in buf->last_byte, we
         * are on a new byte boundary, so we should 
         * update the order 0-11 context hashes.
         */
        if (do_update == 1) {

                context_hash = context_hash*257 + (context & 0x000000FF) + 1;
                ssm_set(&cm, context_hash);
        }

        byte0 = (context & 0x000000FF);
        byte1 = (context & 0x0000FF00) >> 8; 
        byte2 = (context & 0x00FF0000) >> 16;

        int p = ssm_predict(&cm);

        /* How did we do? */
        ssm_update(&cm, byte0, last_bit);

        nn_input(&mixer, p);

        nn_set(&mixer, last_byte, 256);
        nn_set(&mixer, byte1, 256);
        nn_set(&mixer, byte2, 256);
  
        return nn_predict(&mixer, last_bit);
}

