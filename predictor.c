#include "predictor.h"
/*
 * SECONDARY SYMBOL ESTIMATION
 *
 * Maps the probability of the next bit being 1 and a context 
 * into a new probability that the next bit will be 1.
 *
 * After each guess, the state is updated to improve future
 * guesses.
 *
 * The input probability (p1) is stretched and divided into 32
 * segments to combine with other contexts. The output is
 * interpolated between two adjacent quantized values of
 * stretch(p1).
 */

/* Determines the learning rate for updates (smaller = faster, default=8) */
#define APM_RATE 8 

struct apm_t {
        int index;
        int n;
        uint16_t *t;
};

/**
 * apm_learn()
 * ```````````
 * Compute adjusted probability in context @context (0 to n-1) 
 *
 * @apm    : Reference to APM structure
 * @p      : Probability that the next bit will be 1
 * @context: Context in which the prediction was made (0 to n-1)
 * @bit    : Actual observed bit.
 * Return  : Adjusted probability that next bit will be 1 scaled to (0-65535)
 */
void apm_learn(struct apm_t *apm, int bit)
{
        int g;

        g = (bit<<16) + (bit<<APM_RATE) - bit - bit;
    
        apm->t[apm->index]   += g - apm->t[apm->index]   >> APM_RATE;
        apm->t[apm->index+1] += g - apm->t[apm->index+1] >> APM_RATE;
}

/**
 * apm_prob()
 * ``````````
 * Compute adjusted probability in context @context (0 to n-1) 
 *
 * @apm    : Reference to APM structure
 * @p      : Probability that the next bit will be 1
 * @context: Context in which the prediction was made (0 to n-1)
 * Return  : Adjusted probability that next bit will be 1 scaled to (0-65535)
 */
int apm_prob(struct apm_t *apm, int p, int context)
{
        int w; /* Interpolation weight (33 points) */

        p = stretch(p);

        w = p & 127;
    
        apm->index = (p+2048>>7)+context*33;

        return apm->t[apm->index]*(128-w) + apm->t[apm->index+1]*w >> 11;
}

/**
 * apm_init()
 * ``````````
 * Initialize an APM structure
 *
 * @apm  : Reference to APM structure
 * @n    : Number of contexts to accomodate
 * Return: Nothing
 */
void apm_init(struct apm_t *apm, int n)
{
        int i;
        int j;

        apm->index = 0;

        if (!apm->t) {
                apm->t = malloc(n*33*sizeof(uint16_t));
        }

        for (i=0; i<n; i++) {
                for (j=0; j<33; j++) {
                        if (i == 0) {
                                apm->t[i*33+j] = squash((j-16)*128)*16;
                        } else {
                                apm->t[i*33+j] = apm->t[j];
                        }
                }
        }
}

/**
 * h2()
 * ````
 * Hash two 32-bit integers into one 32-bit integer
 *
 * @a    : 32-bit integer
 * @b    : 32-bit integer
 * Return: Hash of @a and @b
 */
uint32_t h2(uint32_t a, uint32_t b)
{
        uint32_t c;
        uint32_t d;
        uint32_t e;
        uint32_t h;

        c = 0xFFFFFFFF;
        d = 0xFFFFFFFF;
        e = 0xFFFFFFFF;
        
        h = a*200002979U+b*30005491U+c*50004239U+d*70004807U+e*110002499U;
  
        return h^h>>9^a>>2^b>>3^c>>4^d>>5^e>>6;
}

/**
 * h3()
 * ````
 * Hash three 32-bit integers into one 32-bit integer
 *
 * @a    : 32-bit integer
 * @b    : 32-bit integer
 * @c    : 32-bit integer
 * Return: Hash of @a, @b, and @c
 */
uint32_t h3(uint32_t a, uint32_t b, uint32_t c)
{
        uint32_t d;
        uint32_t e;
        uint32_t h;

        d = 0xFFFFFFFF;
        e = 0xFFFFFFFF;
        
        h = a*200002979U+b*30005491U+c*50004239u+d*70004807U+e*110002499U;
  
        return h^h>>9^a>>2^b>>3^c>>4^d>>5^e>>6;
}


static struct apm_t A1;
static struct apm_t A2;
static struct apm_t A3;
static struct apm_t A4;

static int APM_is_active = 0;


int apm_adjust(int pr, uint32_t cx, uint8_t part, int bit)
{
        uint32_t hash0;
        uint32_t hash1;
        uint32_t hash2;

        int pr2;
        int pr3;
        int pr4;

        /* Initialize these static folks the first time */
        if (APM_is_active == 0) {
                apm_init(&A1, 256);
                apm_init(&A2, 0x10000);
                apm_init(&A3, 0x10000);
                apm_init(&A4, 0x10000);

                /* Prevent this branch from happening again */ 
                APM_is_active = 1;
        }

        /* Train the APMs from the last time */
        apm_learn(&A1, bit);
        apm_learn(&A2, bit);
        apm_learn(&A3, bit);
        apm_learn(&A4, bit);

        /* Hash the order 0, order 1, and order 2 contexts into indices */
        hash0 = part+256*(cx&0x000000FF);
        hash1 = part ^ h2(cx&0x000000FF, cx&0x0000FF00>>8);
        hash2 = part ^ h3(cx&0x000000FF, cx&0x0000FF00>>8, cx&0x00FF0000>>16);

        /*
         * There are 2 APM stages in series:
         *
         *      p1 := (p1 + 3 APM(order 0, p1)) / 4.
         *      p1 := (APM(order 1, p1) + 2 APM(order 2, p1) + APM(order 3, p1)) / 4.
         */
        pr  = (apm_prob(&A1, pr, part) * 3 + pr) >> 2;

        pr2 = apm_prob(&A2, pr, hash0 & 0xFFFF);
        pr3 = apm_prob(&A3, pr, hash1 & 0xFFFF);
        pr4 = apm_prob(&A4, pr, hash2 & 0xFFFF);

        return (pr2 + pr3*2 + pr4+2) >> 2;
}

#define USE_SSE

int SMOOTH(int p, uint32_t context, uint8_t part, int bit)
{
        #ifdef USE_SSE
        return apm_adjust(p, context, part, bit);
        #else
        return p;
        #endif
}


