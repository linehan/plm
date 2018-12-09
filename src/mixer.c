#include <stdlib.h>
#include "mixer.h"

/*
 * MODEL MIXING
 *
 * As in Mahoney's 'paq8l', we use a neural network to combine a large number
 * of models together. The following is excerpted from paq8l:
 *
 * The i'th model independently predicts
 *
 *   p1_i = p(y_j = 1 | y_0..j-1), p0_i = 1 - p1_i.
 *
 * The network computes the next bit probabilty
 *
 *   p1 = squash(SUM_i w_i t_i), p0 = 1 - p1                        (1)
 *
 * where t_i = stretch(p1_i) is the i'th input, p1_i is the prediction of
 * the i'th model, p1 is the output prediction, stretch(p) = ln(p/(1-p)),
 * and squash(s) = 1/(1+exp(-s)).  Note that squash() and stretch() are
 * inverses of each other.
 *
 * After bit y_j (0 or 1) is received, the network is trained:
 *
 *   w_i := w_i + eta t_i (y_j - p1)                                (2)
 *
 * where eta is an ad-hoc learning rate, t_i is the i'th input, (y_j - p1)
 * is the prediction error for the j'th input but, and w_i is the i'th
 * weight.  Note that this differs from back propagation:
 *
 *   w_i := w_i + eta t_i (y_j - p1) p0 p1                          (3)
 *
 * which is a gradient descent in weight space to minimize root mean square
 * error.  Rather, the goal in compression is to minimize coding cost,
 * which is -log(p0) if y = 1 or -log(p1) if y = 0.  Taking
 * the partial derivative of cost with respect to w_i yields (2).
 */

/******************************************************************************
 * NN STUFF
 ******************************************************************************/

/**
 * dot_product()
 * `````````````
 * Take the dot product of two n-tuples @t, @w.
 *
 * @t    : Vector
 * @w    : Vector
 * @n    : Length of @t and @w
 * Return: Dot product of @t and @w
 *
 * NOTE
 * Returns the dot product of n elements. @n is rounded
 * up to a multiple of 8. The result is scaled down by
 * 8 bits.
 *
 * NOTE
 * Uses MMX and is about 8 times faster than a C implementation
 */
extern int dot_product(short *t, short *w, int n); /* in NASM */

/**
 * train()
 * ```````
 * Train the neural network
 *
 * @t    : Input array
 * @w    : Weight array
 * @n    : Length of @t and @w
 * @err  : Error
 * Return: Nothing
 *
 * NOTE:
 * We are training the neural network by adjusting weights w[n]
 * given inputs t[n] and an error.
 *
 *      w[i] += t[i] * err, i=0..n-1
 *
 * t, w, and err are signed 16-bit values (+- 32K)
 *
 * @err  is a scaled 16-bit value (representing +- 1/2).
 * @w[i] is clamped to +- 32K and rounded.
 * @n    is rounded up to a multiple of 8.
 */
extern void train(short *t, short *w, int n, int err); /* in NASM */


/******************************************************************************
 * NN INTERFACE
 ******************************************************************************/

/**
 * nn_init()
 * `````````
 * Initialize the neural network structure.
 *
 * @nn   : Reference to neural network mixer.
 * @n    : Number of inputs to each neural network
 * @m    : Number of neural networks
 * @s    : Number of selectable inputs (is this state levels?)
 * @w    : Initial weight
 * Return: Nothing
 */
void nn_init(struct nn_t *nn, int n, int m, int s, int w)
{
        char *mem;
        int   i;

        if (s == -1) {
                s = 1;
        }

        if (w == -1) {
                w = 0;
        }

        /* Round up to a multiple of 8. */
        nn->N = ((n+7) & -8);
        nn->M = m;
        nn->S = s;

        nn->tx  = calloc_align(16, nn->N *         sizeof(short));
        nn->wx  = calloc_align(16, nn->N * nn->M * sizeof(short));
        nn->pr  = (int *)calloc(nn->S*sizeof(short), 1);
        nn->ctx = (int *)calloc(nn->S*sizeof(short), 1);

        nn->head   = 0;
        nn->base   = 0;
        nn->nx     = 0;
        nn->nn_ptr = 0;

        assert(n>0 && nn->N>0 && (nn->N&7)==0 && nn->M>0);

        for (i=0; i<nn->S; i++) {
                nn->pr[i] = 2048;
        }

        for (i=0; i<nn->N*nn->M; i++) {
                nn->wx[i] = w;
        }

        if (nn->S > 1) {
                nn->nn_ptr = calloc(sizeof(struct nn_t), 1);

                 /*They can operate in a chain, so we recurse here. */
                nn_init(nn->nn_ptr, nn->S, 1, 1, 0x7FFF);
        }
}


/**
 * nn_train()
 * ``````````
 * Train the network, with the expected output being @bit.
 *
 * @nn   : Reference to neural network mixer
 * @bit  : Bit to train against.
 * Return: Nothing
 *
 * NOTE:
 * The point here is to adjust weights to minimize the
 * coding cost of the last prediction.
 */
void nn_train(struct nn_t *nn, int bit)
{
        int i;
        int err;

        for (i=0; i<nn->head; i++) {
                err = ((bit<<12) - nn->pr[i])*7;

                assert(err >= -32768 && err < 32768);
                /*printf("err:%d\n", err);*/

                /* ASM */
                train(&nn->tx[0], &nn->wx[nn->ctx[i] * nn->N], nn->nx, err);
        }

        /* Reset all of the vectors */
        nn->nx = nn->base = nn->head = 0;
}


/**
 * nn_input()
 * ``````````
 * Add an input to the input array. (Call up to N times)
 *
 * @nn   : Reference to neural network mixer.
 * @input: Input value.
 * Return: Nothing.
 *
 * NOTE
 *  input(stretch(p)) inputs a prediction from one of N models.
 *  The prediction should be positive to predict a 1 bit, and
 *  negative to predict a 0 bit, nominally +-256 to +-2K.
 *
 *  The maximum allowed value is +-32K but, using such large
 *  values may cause overflow if N is large.
 */
void nn_input(struct nn_t *nn, int input)
{
        assert(nn->nx < nn->N);
        nn->tx[nn->nx++] = input;
}


/**
 * nn_set()
 * ````````
 * Load context into the mixer. (Call S times, sum of ranges <= M)
 *
 * @m      : Reference to neural network mixer
 * @context: Context value
 * @range  : Maximum size of context
 * Return  : Nothing
 *
 * NOTE
 * set(ctx, range) selects ctx as one of 'range' neural networks
 * to use. 0 <= ctx < range.  Should be called up to S times such
 * that the total of the ranges is <= M.
 */
void nn_set(struct nn_t *nn, int context, int range)
{
        assert(range >= 0);
        assert(nn->head < nn->S);
        assert(context >= 0);
        assert(nn->base + context < nn->M);

        nn->ctx[nn->head++] = nn->base + context;
        nn->base += range;
}

void nn_print(struct nn_t *nn)
{
        int i;
        for (i=0; i<nn->nx; i++) {
                printf("tx[%d]:%d\n", i, nn->tx[i]);
        }
        for (i=0; i<nn->nx; i++) {
                printf("wx[%d]:%d\n", i, nn->wx[i]);
        }
}

/**
 * nn_predict()
 * ````````````
 * Predict the next bit
 *
 * @nn   : Reference to neural network mixer
 * @bit  : Observed bit
 * Return: Probability value (12 bits) (0-4095)
 *
 * NOTE
 * predict() returns the output prediction that the next bit
 * is 1 as a 12-bit number (0 to 4095).
 */
int nn_predict(struct nn_t *nn, int bit)
{
        int i;

        while (nn->nx & 7) {
                nn->tx[nn->nx++] = 0;  /* pad */
        }

        if (nn->nn_ptr != NULL) {
                /* Combine outputs */
                nn_train(nn->nn_ptr, bit);

                for (i=0; i<nn->head; i++) {
                        nn->pr[i] = squash(dot_product(&nn->tx[0], &nn->wx[nn->ctx[i]*nn->N], nn->nx)>>5);
                        nn_input(nn->nn_ptr, stretch(nn->pr[i]));
                }

                nn_set(nn->nn_ptr, 0, 1);

                /* Recurse down the chain of neural networks */
                return nn_predict(nn->nn_ptr, bit);
        } else {
                /* S=1 context */
                return nn->pr[0] = squash(dot_product(&nn->tx[0], &nn->wx[0], nn->nx)>>8);
        }
}

