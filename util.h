#ifndef _UTILS_H
#define _UTILS_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

/******************************************************************************
 * UTILITY/MATH FUNCTIONS 
 ******************************************************************************/

uint8_t read_byte(FILE *file);
uint32_t read_word(FILE *file);
void ilog_init(void);
int ilog(uint16_t x);
int llog(uint32_t x);
int squash(int d);
int stretch_init(void);
int stretch(int p);
int min(int a, int b);
int max(int a, int b);
void print_status(int bytes);
int file_length(FILE *file);
void *calloc_align(int boundary, size_t size);

/******************************************************************************
 * MACROS FOR PRINTING BYTES 
 ******************************************************************************/

#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)  \
  (byte & 0x80 ? 1 : 0), \
  (byte & 0x40 ? 1 : 0), \
  (byte & 0x20 ? 1 : 0), \
  (byte & 0x10 ? 1 : 0), \
  (byte & 0x08 ? 1 : 0), \
  (byte & 0x04 ? 1 : 0), \
  (byte & 0x02 ? 1 : 0), \
  (byte & 0x01 ? 1 : 0) 

#define FMT_U8  "%d%d%d%d%d%d%d%d"
#define FMT_U16 FMT_U8  FMT_U8
#define FMT_U32 FMT_U16 FMT_U16 

#define FMT_U8S  "%d%d%d%d%d%d%d%d "
#define FMT_U16S FMT_U8S  FMT_U8S
#define FMT_U32S FMT_U16S FMT_U16S

#define VAL_U8(u8)   BYTETOBINARY(u8)
#define VAL_U16(u16) BYTETOBINARY(u16>>8),  BYTETOBINARY(u16)
#define VAL_U32(u32) BYTETOBINARY(u32>>24), BYTETOBINARY(u32>>16), VAL_U16(u32) 

/******************************************************************************
 * SOME GLOBAL STUFF 
 ******************************************************************************/

#define MEM (0x10000<<level)
#define MEM_MAX (0x10000<<9)

#ifndef DEFAULT_OPTION
#define DEFAULT_OPTION 5
#endif

#define COMPRESS   1
#define DECOMPRESS 0

extern int MODE;   /* COMPRESS or DECOMPRESS, set in main() (main.c) */
extern int level;  /* Compression level (0-9), set in main() (main.c) */ 

void log_init(void);
void log_msg(FILE *file, const char *fmt, ...);
void log_close(void);

extern FILE *LOG_AC;
extern FILE *LOG_NN;
extern FILE *LOG_SSE;
extern FILE *LOG_MODEL;

#endif
