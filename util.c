#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "util.h"

int file_length(FILE *file)
{
        int pos;
        int end;

        pos = (int)ftell(file);

        fseek(file, 0, SEEK_END);

        end = (int)ftell(file);

        /* Put it back where you found it */
        fseek(file, pos, SEEK_SET);

        return end;
}

void *calloc_align(int boundary, size_t size)
{
        char *mem;

        mem = (char *)calloc(size, 1);
        return (void *)(mem+boundary-(((long)mem)&(boundary-1)));
}

/**
 * print_status()
 * ``````````````
 * Print the progress of the algorithm.
 *
 * bytes : Number of bytes (de)compressed so far
 * Return: nothing
 */
void print_status(int bytes) 
{
        if (bytes > 0 && !(bytes & 0x3fff)) {
                printf("%12d\b\b\b\b\b\b\b\b\b\b\b\b", bytes);
                fflush(stdout);
        }
}

/* min, max functions */
int min(int a, int b) 
{
        return a<b?a:b;
}

int max(int a, int b) 
{
        return a<b?b:a;
}

static uint8_t ilog_table[65536];

/**
 * ilog_init()
 * ```````````
 * Pre-compute a table used to look up values for ilog()
 *
 * Return: nothing.
 *
 * NOTE
 * The technique we're using to build the table is numerical
 * integration of 1/x, since the result of that integral is
 * ln x.
 */
void ilog_init(void)
{
        int i;

        uint32_t x = 14155776;
  
        for (i=2; i<65536; ++i) {
                /* The numerator is 2^29 / ln 2 */
                x += 774541002 / (i*2-1);  
                ilog_table[i] = x>>24;
        }
}

/**
 * ilog()
 * ``````
 * Compute the inverse logarithm of 1/x
 *
 * @x    : The value to find the inverse logarithm of.
 * Return: ln^(-1) (1/x)
 */
int ilog(uint16_t x)
{
        return ilog_table[x];
}

/**
 * llog()
 * ``````
 * Compute the inverse logarithm of 1/x, where x is 32-bits.
 *
 * @x    :  The value to find the inverse logarithm of.
 * Return: ln^(-1) (1/x)
 */
int llog(uint32_t x) 
{
        if (x >= 0x1000000) {
                return 256 + ilog(x>>16);
        } else if (x >= 0x10000) {
                return 128 + ilog(x>>8);
        } else {
                return ilog(x);
        }
}


/**
 * squash()
 * ````````
 * Quickly compute the logistic function for @d.
 *
 * @d    : The value to be "squashed"
 * Return: 1/(1 + e^(-d)), scaled to 12 bits (0-4096)
 *
 * NOTE
 * The logistic function is a common choice for the activation or 
 * "squashing" function in a neural network and other applications.
 * Its purpose is to clip large magnitudes smoothly (differentiably)
 * in order to keep the response of the neural network or other system
 * bounded.
 *
 * NOTE
 * The input @d is scaled down by 8 bits.
 * The output is scaled by 12 bits (0-4096)
 */
int squash(int d) 
{
        static const int t[33] = {
                1   ,2   ,3   ,6   ,10  ,16  ,27  ,45  ,73  ,120 ,194 ,
                310 ,488 ,747 ,1101,1546,2047,2549,2994,3348,3607,3785,
                3901,3975,4022,4050,4068,4079,4085,4089,4092,4093,4094
        };

        int w;

        if (d > 2047) { 
                return 4095;
        }

        if (d < -2047) {
                return 0;
        }

        /* 
         * 127_dec = 011111111_bin, so we are getting everything but the
         * high-order bit of the low-order byte of @d. 
         */
        w = d & 127;

        d = (d>>7)+16;

        return ((t[d]*(128-w)) + (t[(d+1)]*w+64)) >> 7;
}


static int stretch_table[4096];

/**
 * stretch_init()
 * ``````````````
 * Pre-compute a table used to look up values for stretch().
 * Return: Nothing
 */
int stretch_init()
{
        int pi = 0;
        int x;
        int i;
        int j;

        /* 
         * We're literally going to invert the squash() 
         * function and store the results in a table.
         */ 
        for (x=-2047; x<=2047; x++) {
                i = squash(x);
                for (j=pi; j<=i; j++) {
                        stretch_table[j] = x;
                }
                pi = i+1;
        }
        
        stretch_table[4095] = 2047;
}

/**
 * stretch()
 * `````````
 * Quickly compute the logit function, the inverse of the logistic function. 
 *
 * @p    : Probability to compute
 * Return: ln(p/(1-p))
 *
 * NOTE
 * When @p is a probability, stretch(p) gives the "log-odds", that is,
 * the logarithm of the odds p/(1-p).
 *
 * NOTE
 * The output is scaled by 8 bits and has a range (-2047 to 2047) 
 * representing (-8 to 8).
 *
 * The input @p is scaled by 12 bits, and has a range (0 to 4095) 
 */
int stretch(int p) 
{
        assert(p>=0 && p<4096);

        return stretch_table[p];
}


FILE *LOG_AC;
FILE *LOG_NN;
FILE *LOG_SSE;
FILE *LOG_MODEL;

int log_interval = 10;
int logi = 0;

void log_init(void)
{
        LOG_AC    = fopen("./log/ac.log", "w+");
        LOG_NN    = fopen("./log/nn.log", "w+");
        LOG_SSE   = fopen("./log/sse.log", "w+");
        LOG_MODEL = fopen("./log/model.log", "w+");
}

void log_close(void)
{
        if (LOG_AC != NULL) {
                fclose(LOG_AC);
        }
        if (LOG_NN != NULL) {
                fclose(LOG_NN);
        }
        if (LOG_SSE != NULL) {
                fclose(LOG_SSE);
        }
}

void log_msg(FILE *file, const char *fmt, ...)
{
        if (logi++ > 10) {
                logi = 0;
        } else {
                return;
        }

        va_list ap;

        if (file != NULL) {
                va_start(ap, fmt); 
                vfprintf(file, fmt, ap);
                va_end(ap);
        }
}


