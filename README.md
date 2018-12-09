# plm
PLM-1 is a research data compressor designed specifically for computing the Normalized Compression Distance, an effective approximation of the Normalized Information Distance, which is defined in terms of prefix-free Kolmogorov complexity (which is not computable). In their paper first describing the NCD, Cilibrasi and Vitanyi outlined a group of "normality axioms" that, if satisfied by a data compressor, guarantee that the resulting NCD was a metric. 

They also proposed a number of popular compression algorithms to be used, however did not seriously test their compliance with the axioms. In fact, `gzip`, `bzip2`, `ppmz`, `gencompress`, and others, do **not** satisfy the normality axioms, despite being recommended.

## NCD
The normalized compression distance is a very interesting metric to use for machine learning methods because it is parameter-free and operates on arbitrary binary data. Objects are similar based on the amount of information that they know about eachother.

## Compression
PLM-1 operates on binary streams using arithmetic coding to compress each bit, builds predictive models based on dynamic contexts, and usees a neutral networks to perform context mixing, blending the models to give a better prediction. This approach is heavily inspired by paq7/paq8, which themselves nearly satisfy the axioms. However PLM-1 is written in C, and a good deal simpler. This makes it easier to perform research with.


