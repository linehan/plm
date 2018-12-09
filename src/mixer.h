#ifndef _MIXER_H
#define _MIXER_H

#include "util.h"

struct nn_t {
        int N;        /* Number of inputs to each neural network */
        int M;        /* Number of neural networks */
        int S;        /* Number of selectable inputs */
        short *tx;    /* Input vector */
        short *wx;    /* Weight vector */
        int *pr;      /* Probability vector */
        int *ctx;     /* Context hashes (index) */
        int head;     /* Write head (index) for input arrays */ 
        int base;     /* Value related to maximum value of input datatypes */
        int nx;       /* Length of @tx, @wx, and @pr vectors */
        struct nn_t *nn_ptr; /* Another neural network mixer */
};

void nn_init   (struct nn_t *nn, int n, int m, int s, int w);
void nn_train  (struct nn_t *nn, int bit);
void nn_input  (struct nn_t *nn, int input);
void nn_set    (struct nn_t *nn, int context, int range);
int  nn_predict(struct nn_t *nn, int bit);

#endif
