#define NAME      "plm"
#define EXTENSION "plm"
// #define NO_LOGGING_PLEASE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>

#include "util.h"
#include "mixer.h"
#include "coder.h"
#include "predictor.h"
#include "model.h"
#include "context.h"

/******************************************************************************
 * GLOBAL STATE
 ******************************************************************************/

/* Compression level (0-9) */
int level = DEFAULT_OPTION;

/* COMPRESS OR DECOMPRESS (defined in util.h) */
int MODE;

/* BUFFER and state-keeping structure. */
struct io_t {
        int      bit;            /* Last bit */
        int      byte;           /* Last byte */
        int      part;           /* Currently-assembling byte */
        int      bit_count;      /* Number of bits seen */
        int      byte_count;     /* Number of bytes seen */
        int      part_count;     /* Number of bits in partially-read byte */
        uint32_t word;           /* Last 4 bytes */
        int      pr;             /* Probability (0-4096) of next bit being 1 */
        int      buf_size;       /* Size of buffer (memory) */
        uint8_t  buf[MEM_MAX*8]; /* Buffer (record of past bytes) */
        /* for logging */
        uint32_t avg_bit_raw[8];
        uint32_t avg_bit_smooth[8];
        uint32_t err_bit_raw[8];
        uint32_t err_bit_smooth[8];
        uint32_t err_byte_raw_tmp;
        uint32_t avg_byte_raw_tmp;
        uint32_t err_byte_smooth_tmp;
        uint32_t avg_byte_smooth_tmp;
        uint32_t err_byte_raw;
        uint32_t err_byte_smooth;
        uint32_t avg_byte_raw;
        uint32_t avg_byte_smooth;
        /*int      avg_byte_raw;*/
        /*int      err_byte_raw;*/
        /*int      avg_byte_smooth;*/
        /*int      err_byte_smooth;*/

        /* TODO: This buffer makes the static allocation and exe size of
         *       the program absolutely huge!
         */
};

struct io_t io = {
        0,
        0,
        1,              /* Start with trailing 1 (see below) */
        0,
        0,
        0,
        0,
        2048,           /* Range is 0-4096, so 2048 is pr=1/2 */
        MEM_MAX*8,
        {0},
        {0},
        {0},
        {0},
        {0},
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/**
 * compress()
 * ``````````
 * Compress a file.
 *
 * @filename: Path of source file
 * @filesize: Size of source file
 * @enc     : Encoder object
 * Return   : Nothing
 */
void compress(FILE *dst, FILE *src, long src_size, struct ac_t *ac)
{
        int i;
        int j;
        uint32_t code;

        /* For each byte in the file */
        for (i=0; i<src_size; i++) {

                /* Read a byte from the source stream */
                io.byte = getc(src);

                /* For each bit in the byte */
                for (j=7; j>=0; j--) {

                        io.bit = (io.byte >> j) & 1;

                        /*
                         * Add the bit to the partially-read
                         * byte.
                         */
                        io.part = (io.part << 1) | io.bit;

                        /*
                         * Increment the bit position in the
                         * current byte.
                         */
                        io.part_count++;

                        /*
                         * Increment the total bit counter.
                         */
                        io.bit_count++;

                        if (io.part > 256) {
                                /*
                                 * Add the full byte (sans trailing '1')
                                 * to the word history
                                 */
                                io.word = (io.word << 8) + io.byte;

                                /*
                                 * Add the full byte (sans trailing '1')
                                 * to the byte history buffer.
                                 */
                                io.buf[io.byte_count & io.buf_size-1] = io.byte;

                                /*
                                 * Increment the byte counter, which
                                 * doubles as the index into io.buf.
                                 */
                                io.byte_count++;

                                /*
                                 * Use a trailing '1' bit here to ensure
                                 * that consecutive zeroes are hashed to
                                 * something different as this '1' gets
                                 * left-shifted.
                                 *
                                 * Also, this allows us to detect when
                                 * to enter this branch, since we keep
                                 * shifting io.part left, and that 1 will
                                 * eventually cause it to become greater
                                 * than 256.
                                 *
                                 * TODO: Is this necessary?
                                 */
                                io.part = 1;

                                /*
                                 * Reset the bit position in the
                                 * current byte to 0.
                                 */
                                io.part_count = 0;


                                /*
                                 *
                                 * output the averages
                                 *
                                 */
                                #ifndef NO_LOGGING_PLEASE
                                int k;
                                for (k=0; k<8; k++) {
                                        log_msg(LOG_AVG_PRAW[k], "%d %g\n", io.byte_count, (float)io.avg_bit_raw[k]/(float)io.byte_count);
                                        log_msg(LOG_AVG_PSMOOTH[k], "%d %g\n", io.byte_count, (float)io.avg_bit_smooth[k]/(float)io.byte_count);
                                        log_msg(LOG_ERR_PSMOOTH[k], "%d %g\n", io.byte_count, (float)io.err_bit_smooth[k]/(float)io.byte_count);
                                        log_msg(LOG_ERR_PRAW[k], "%d %g\n", io.byte_count, (float)io.err_bit_raw[k]/(float)io.byte_count);
                                }

                                io.err_byte_smooth += io.err_byte_smooth_tmp;
                                io.err_byte_raw    += io.err_byte_raw_tmp;

                                log_msg(LOG_ERR_BYTE_PSMOOTH, "%d %g\n", io.byte_count, (float)io.err_byte_smooth/(float)io.byte_count);
                                log_msg(LOG_ERR_BYTE_PRAW, "%d %g\n", io.byte_count, (float)io.err_byte_raw/(float)io.byte_count);

                                io.avg_byte_smooth += abs((io.avg_byte_smooth_tmp-2048));
                                io.avg_byte_raw    += abs((io.avg_byte_raw_tmp-2048));

                                log_msg(LOG_AVG_BYTE_PSMOOTH, "%d %g\n", io.byte_count, (float)io.avg_byte_smooth/(float)io.byte_count);
                                log_msg(LOG_AVG_BYTE_PRAW, "%d %g\n", io.byte_count, (float)io.avg_byte_raw/(float)io.byte_count);

                                #endif
                        }

                        /* Pass the actual bit and prediction to the encoder */
                        ac_encoder_add_bit(ac, io.pr, io.bit);

                        while ((code=ac_encoder_try_get_byte(ac))!=UINT32_MAX) {
                                /* Write any ready bytes to the archive */
                                putc(code, dst);
                        }

                        /* Predict the next bit using the model */
                        io.pr = SIMPLE(io.word, io.part, io.bit, io.part_count);

                        #ifndef NO_LOGGING_PLEASE
                        if (io.byte_count % 2 != 0) {
                                log_msg(LOG_PRAW[7-j], "%d %d\n", io.bit_count, io.pr);
                                if (io.bit == 0 && io.pr >= 2048) {
                                        log_msg(LOG_LOSS_PRAW[7-j], "%d %d\n", io.bit_count, io.pr);
                                } else {
                                        log_msg(LOG_WIN_PRAW[7-j], "%d %d\n", io.bit_count, io.pr);
                                }
                        }
                        #endif

                        /* Smooth the prediction with SSE */
                        io.pr = SMOOTH(io.pr, io.word, io.part, io.bit);

                        #ifndef NO_LOGGING_PLEASE
                        if (io.byte_count % 2 != 0) {
                                log_msg(LOG_PSMOOTH[7-j], "%d %d\n", io.bit_count, io.pr);
                                if (io.bit == 0 && io.pr >= 2048) {
                                        log_msg(LOG_LOSS_PSMOOTH[7-j], "%d %d\n", io.bit_count, io.pr);
                                } else {
                                        log_msg(LOG_WIN_PSMOOTH[7-j], "%d %d\n", io.bit_count, io.pr);
                                }
                        }
                        #endif
                }

        }

        ac_encoder_flush(ac, dst);
}


/**
 * decompress()
 * ````````````
 * Decompress a file.
 *
 * @path : Intended path of target (decompressed) file.
 * @size : Intended size of target (decomrpessed) file (recorded in header)
 * @enc  : Encoder object
 * Return: Nothing
 */
void decompress(FILE *dst, FILE *src, long dst_size, struct ac_t *ac)
{
        int i;
        int j;
        int next_byte;

        /* For each byte of the eventual (decompressed) target */
        for (i=0; i<dst_size; i++) {

                /* For each bit of the byte */
                for (j=0; j<8; j++) {

                        /* Decode a bit */
                        io.bit = ac_decoder_get_bit(ac, io.pr);

                        /*
                         * Add the bit to the partially-read
                         * byte.
                         */
                        io.part = (io.part << 1) | io.bit;

                        /*
                         * Keep building up the "real" byte,
                         * without the leading 1.
                         */
                        io.byte = (io.part & 0x000000FF);

                        /*
                         * Increment the bit position in the
                         * current byte.
                         */
                        io.part_count++;

                        /*
                         * Increment the total bit counter.
                         */
                        io.bit_count++;

                        if (io.part > 256) {
                                /*
                                 * Add the full byte (sans trailing '1')
                                 * to the word history
                                 */
                                io.word = (io.word << 8) + io.byte;

                                /*
                                 * Add the full byte (sans trailing '1')
                                 * to the byte history buffer.
                                 */
                                io.buf[io.byte_count & io.buf_size-1] = io.byte;

                                /*
                                 * Increment the byte counter, which
                                 * doubles as the index into io.buf.
                                 */
                                io.byte_count++;

                                /*
                                 * Use a trailing '1' bit here to ensure
                                 * that consecutive zeroes are hashed to
                                 * something different as this '1' gets
                                 * left-shifted.
                                 *
                                 * Also, this allows us to detect when
                                 * to enter this branch, since we keep
                                 * shifting io.part left, and that 1 will
                                 * eventually cause it to become greater
                                 * than 256.
                                 *
                                 * TODO: Is this necessary?
                                 */
                                io.part = 1;

                                /*
                                 * Reset the bit position in the
                                 * current byte to 0.
                                 */
                                io.part_count = 0;
                        }

                        for (;;) {
                                /* Get the next byte from the source */
                                next_byte = getc(src);

                                /* Try to add it to the encoder */
                                if (!ac_decoder_try_add_byte(ac, next_byte)) {
                                        /* Push it back if unsuccessful */
                                        ungetc(next_byte, src);
                                        break;
                                }
                        }

                        /* Predict the next bit using the model */
                        io.pr = SIMPLE(io.word, io.part, io.bit, io.part_count);

                        /* Smooth the prediction with SSE */
                        io.pr = SMOOTH(io.pr, io.word, io.part, io.bit);
                }

                /* Write the decoded byte to the destination. */
                putc(io.byte, dst);
        }
}

FILE *stream_to_file(FILE *stream)
{
        FILE *file;
        int   ch;

        /* Will be destroyed when program ends or file closed */
        file = tmpfile();

        /* Copy the stream to the temp file */
        while ((ch = fgetc(stream)) != EOF) {
                fputc(ch, file);
        }

        rewind(file);

        return file;
}


void print_usage_and_exit(char *bad_part)
{
        if (bad_part != NULL) {
                fprintf(stderr, "I don't know '%s'.\n", bad_part);
        }

        fprintf(stderr, "USAGE: "NAME" -c|-d [<input>] [-o <output>]\nMEM_MAX:%d\n",MEM_MAX);

        exit(0);
}

/******************************************************************************
 * MAIN
 ******************************************************************************/

int main(int argc, char** argv)
{
        level = DEFAULT_OPTION;

        struct ac_t ac;

        char header[255];     /* Will hold the header line */
        char compressor[255];

        char *source_name;
        char *source_path;
        FILE *source_file = 0;  // compressed file
        long  source_size;

        char  target_name[4096];
        char *target_path;
        FILE *target_file = 0;
        long  target_size;
        long  start;

        if (argc == 1) {
                print_usage_and_exit(NULL);
        }

        if (!strcmp(argv[1], "-c")) {
                MODE = COMPRESS;
        } else if (!strcmp(argv[1], "-d")) {
                MODE = DECOMPRESS;
        } else {
                print_usage_and_exit(NULL);
        }

        switch (argc) {
        case 2:
                source_file = stream_to_file(stdin);
                source_path = "STDIN";
                target_path = "STDOUT";
                target_file = stdout;
                break;
        case 3:
                if (!strcmp(argv[2], "-o")) {
                        print_usage_and_exit(NULL);
                } else if (argv[2][0] == '-') {
                        print_usage_and_exit(argv[2]);
                } else {
                        source_path = argv[2];
                        target_file = stdout;
                        target_path = "STDOUT";
                }
                break;
        case 4:
                if (!strcmp(argv[2], "-o")) {
                        if (argv[3][0] == '-') {
                                print_usage_and_exit(argv[3]);
                        } else {
                                source_path = "STDIN";
                                source_file = stream_to_file(stdin);
                                target_path = argv[3];
                        }
                } else {
                        print_usage_and_exit(NULL);
                }
                break;
        case 5:
                if (!strcmp(argv[2], "-o")) {
                        print_usage_and_exit(NULL);
                } else if (argv[2][0] == '-') {
                        print_usage_and_exit(argv[2]);
                } else {
                        source_path = argv[2];
                }

                if (!strcmp(argv[3], "-o")) {
                        if (argv[4][0] == '-') {
                                print_usage_and_exit(argv[4]);
                        } else {
                                target_path = argv[4];
                        }
                } else {
                        print_usage_and_exit(NULL);
                }

                break;
        default:
                print_usage_and_exit(NULL);
                break;
        }

        ilog_init();    // TODO: fix this shit!
        stretch_init(); // TODO: fix this shit!
        log_init();

        if (source_file == NULL) {
                source_file = fopen(source_path, "rb");
        }

        if (target_file == NULL) {
                /*
                 * If an output file already exists, then we will
                 * compare the result of this decompression with
                 * its contents.
                 */
                if ((target_file = fopen(target_path, "rb"))) {
                        fprintf(stderr, "File exists.\n");
                        fclose(target_file);
                        return 1;
                }

                /* Create a new file */
                if (!(target_file = fopen(target_path, "wb"))) {
                        fprintf(stderr, "Could not open file.\n");
                        exit(1);
                }
        }

        if (!target_file) {
                fprintf(stderr, "Error creating %s\n", target_path);
                exit(1);
        }
        if (!source_file) {
                fprintf(stderr, "Error opening %s\n", source_path);
                exit(1);
        }


        if (MODE == COMPRESS) {

                /*sprintf(target_name, "%s."EXTENSION, basename(strdup(source_path)));*/

                source_size = file_length(source_file);

                /*
                 * Header format:
                 *      program_name:level:source_size
                 */
                fprintf(target_file, "%s:%d:%ld\r\n\x1A",
                        NAME,
                        level,
                        source_size
                );

                /*printf("Creating archive %s...\n", target_name);*/
        }

        if (MODE == DECOMPRESS) {

                /*source_file = fopen(source_path, "rb+");*/
                /*source_size = file_length(source_file);*/

                /*if (!source_file) { */
                        /*printf("Error opening %s\n", source_path);*/
                        /*exit(1);*/
                /*}*/

                /*
                 * Read header and get options
                 */
                fscanf(source_file, "%255[^:]:%d:%ld\r\n\x1A",
                        &compressor,
                        &level,
                        &target_size
                );

                /*fprintf(stderr, "compressor:%s\nlevel:%d\ntarget-size:%ld\n", compressor, level, target_size);*/

                if (strcmp(compressor, NAME) != 0) {
                        fprintf(stderr, "%s: not a "NAME" file\n", source_path);
                        exit(1);
                }

                if (level<0 || level>9) {
                        level = DEFAULT_OPTION;
                }

                /*fprintf(stderr, "Inflating %s at level %d\n", */
                        /*source_path, */
                        /*level*/
                /*);*/
        }

        if (MODE == COMPRESS) {

                ac_init(&ac, target_file);

                /*if (!(source_file = fopen(source_path, "rb"))) {*/
                        /*perror(source_path);*/
                        /*exit(1);*/
                /*}*/

                start = ftell(target_file);

                compress(target_file, source_file, source_size, &ac);

                /*fprintf(stderr, "%ld -> %ld\n", source_size, ftell(target_file)-start);*/

        } else {
                ac_init(&ac, source_file);

                /*
                 * If an output file already exists, then we will
                 * compare the result of this decompression with
                 * its contents.
                 */
                /*if ((target_file = fopen(target_path, "rb"))) {*/
                        /*printf("File exists.\n");*/
                        /*fclose(target_file);*/
                        /*return 1;*/
                /*}*/

                /*[> Create a new file <]*/
                /*if (!(target_file = fopen(target_path, "wb"))) {*/
                        /*printf("Could not open file.\n");*/
                        /*exit(1);*/
                /*}*/

                decompress(target_file, source_file, target_size, &ac);

                /*fprintf(stderr, "%ld -> %ld\n", source_size, ftell(target_file));*/
        }

        if (!strcmp(target_path, "STDOUT")) {
                fflush(stdout);
        }

        /*REPORT();*/

        print_statemap_counts();
        log_close();

        printf("Context_count:%d\n", Context_count);

        return 0;
}

