#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "context.h"
/*
 *
 *
 * TODO TODO TODO NOTE
 * TODO TODO TODO NOTE
 *  
 * HEY!
 * DON"T FUCKING USE THIS FILE UNLESS YOU
 * HAVE INITIALIZED THE PRNG BY RUNNING
 * PRNG_INIT().
 */

/******************************************************************************
 * PRNG for count management
 ******************************************************************************/

static uint32_t prng_table[64];
static int      prng_i;
static int      prng_is_init = 0;

void prng_init(void)
{
        int j;

        prng_table[0] = 123456789;
        prng_table[1] = 987654321;

        for (j=0; j<62; j++) {
                prng_table[j+2] = prng_table[j+1]*11 + prng_table[j]*23/16;
                prng_i = 0;
        }
}

/* 32-bit pseudo random number generator */
uint32_t prng(void)
{
        if (prng_is_init == 0) {
                prng_init();
                prng_is_init = 1;
        }
        prng_i++;
        
        return prng_table[prng_i&63] = prng_table[prng_i-24&63]^prng_table[prng_i-55&63];
}

/******************************************************************************
 * STATE TABLE 
 ******************************************************************************/

// State table:
//   nex(state, 0) = next state if bit y is 0, 0 <= state < 256
//   nex(state, 1) = next state if bit y is 1
//   nex(state, 2) = number of zeros in bit history represented by state
//   nex(state, 3) = number of ones represented
//
// States represent a bit history within some context.
// State 0 is the starting state (no bits seen).
// States 1-30 represent all possible sequences of 1-4 bits.
// States 31-252 represent a pair of counts, (n0,n1), the number
//   of 0 and 1 bits respectively.  If n0+n1 < 16 then there are
//   two states for each pair, depending on if a 0 or 1 was the last
//   bit seen.
// If n0 and n1 are too large, then there is no state to represent this
// pair, so another state with about the same ratio of n0/n1 is substituted.
// Also, when a bit is observed and the count of the opposite bit is large,
// then part of this count is discarded to favor newer data over old.

static const uint8_t State_table[256][4] = {
  {  1,  2, 0, 0},{  3,  5, 1, 0},{  4,  6, 0, 1},{  7, 10, 2, 0}, // 0-3
  {  8, 12, 1, 1},{  9, 13, 1, 1},{ 11, 14, 0, 2},{ 15, 19, 3, 0}, // 4-7
  { 16, 23, 2, 1},{ 17, 24, 2, 1},{ 18, 25, 2, 1},{ 20, 27, 1, 2}, // 8-11
  { 21, 28, 1, 2},{ 22, 29, 1, 2},{ 26, 30, 0, 3},{ 31, 33, 4, 0}, // 12-15
  { 32, 35, 3, 1},{ 32, 35, 3, 1},{ 32, 35, 3, 1},{ 32, 35, 3, 1}, // 16-19
  { 34, 37, 2, 2},{ 34, 37, 2, 2},{ 34, 37, 2, 2},{ 34, 37, 2, 2}, // 20-23
  { 34, 37, 2, 2},{ 34, 37, 2, 2},{ 36, 39, 1, 3},{ 36, 39, 1, 3}, // 24-27
  { 36, 39, 1, 3},{ 36, 39, 1, 3},{ 38, 40, 0, 4},{ 41, 43, 5, 0}, // 28-31
  { 42, 45, 4, 1},{ 42, 45, 4, 1},{ 44, 47, 3, 2},{ 44, 47, 3, 2}, // 32-35
  { 46, 49, 2, 3},{ 46, 49, 2, 3},{ 48, 51, 1, 4},{ 48, 51, 1, 4}, // 36-39
  { 50, 52, 0, 5},{ 53, 43, 6, 0},{ 54, 57, 5, 1},{ 54, 57, 5, 1}, // 40-43
  { 56, 59, 4, 2},{ 56, 59, 4, 2},{ 58, 61, 3, 3},{ 58, 61, 3, 3}, // 44-47
  { 60, 63, 2, 4},{ 60, 63, 2, 4},{ 62, 65, 1, 5},{ 62, 65, 1, 5}, // 48-51
  { 50, 66, 0, 6},{ 67, 55, 7, 0},{ 68, 57, 6, 1},{ 68, 57, 6, 1}, // 52-55
  { 70, 73, 5, 2},{ 70, 73, 5, 2},{ 72, 75, 4, 3},{ 72, 75, 4, 3}, // 56-59
  { 74, 77, 3, 4},{ 74, 77, 3, 4},{ 76, 79, 2, 5},{ 76, 79, 2, 5}, // 60-63
  { 62, 81, 1, 6},{ 62, 81, 1, 6},{ 64, 82, 0, 7},{ 83, 69, 8, 0}, // 64-67
  { 84, 71, 7, 1},{ 84, 71, 7, 1},{ 86, 73, 6, 2},{ 86, 73, 6, 2}, // 68-71
  { 44, 59, 5, 3},{ 44, 59, 5, 3},{ 58, 61, 4, 4},{ 58, 61, 4, 4}, // 72-75
  { 60, 49, 3, 5},{ 60, 49, 3, 5},{ 76, 89, 2, 6},{ 76, 89, 2, 6}, // 76-79
  { 78, 91, 1, 7},{ 78, 91, 1, 7},{ 80, 92, 0, 8},{ 93, 69, 9, 0}, // 80-83
  { 94, 87, 8, 1},{ 94, 87, 8, 1},{ 96, 45, 7, 2},{ 96, 45, 7, 2}, // 84-87
  { 48, 99, 2, 7},{ 48, 99, 2, 7},{ 88,101, 1, 8},{ 88,101, 1, 8}, // 88-91
  { 80,102, 0, 9},{103, 69,10, 0},{104, 87, 9, 1},{104, 87, 9, 1}, // 92-95
  {106, 57, 8, 2},{106, 57, 8, 2},{ 62,109, 2, 8},{ 62,109, 2, 8}, // 96-99
  { 88,111, 1, 9},{ 88,111, 1, 9},{ 80,112, 0,10},{113, 85,11, 0}, // 100-103
  {114, 87,10, 1},{114, 87,10, 1},{116, 57, 9, 2},{116, 57, 9, 2}, // 104-107
  { 62,119, 2, 9},{ 62,119, 2, 9},{ 88,121, 1,10},{ 88,121, 1,10}, // 108-111
  { 90,122, 0,11},{123, 85,12, 0},{124, 97,11, 1},{124, 97,11, 1}, // 112-115
  {126, 57,10, 2},{126, 57,10, 2},{ 62,129, 2,10},{ 62,129, 2,10}, // 116-119
  { 98,131, 1,11},{ 98,131, 1,11},{ 90,132, 0,12},{133, 85,13, 0}, // 120-123
  {134, 97,12, 1},{134, 97,12, 1},{136, 57,11, 2},{136, 57,11, 2}, // 124-127
  { 62,139, 2,11},{ 62,139, 2,11},{ 98,141, 1,12},{ 98,141, 1,12}, // 128-131
  { 90,142, 0,13},{143, 95,14, 0},{144, 97,13, 1},{144, 97,13, 1}, // 132-135
  { 68, 57,12, 2},{ 68, 57,12, 2},{ 62, 81, 2,12},{ 62, 81, 2,12}, // 136-139
  { 98,147, 1,13},{ 98,147, 1,13},{100,148, 0,14},{149, 95,15, 0}, // 140-143
  {150,107,14, 1},{150,107,14, 1},{108,151, 1,14},{108,151, 1,14}, // 144-147
  {100,152, 0,15},{153, 95,16, 0},{154,107,15, 1},{108,155, 1,15}, // 148-151
  {100,156, 0,16},{157, 95,17, 0},{158,107,16, 1},{108,159, 1,16}, // 152-155
  {100,160, 0,17},{161,105,18, 0},{162,107,17, 1},{108,163, 1,17}, // 156-159
  {110,164, 0,18},{165,105,19, 0},{166,117,18, 1},{118,167, 1,18}, // 160-163
  {110,168, 0,19},{169,105,20, 0},{170,117,19, 1},{118,171, 1,19}, // 164-167
  {110,172, 0,20},{173,105,21, 0},{174,117,20, 1},{118,175, 1,20}, // 168-171
  {110,176, 0,21},{177,105,22, 0},{178,117,21, 1},{118,179, 1,21}, // 172-175
  {110,180, 0,22},{181,115,23, 0},{182,117,22, 1},{118,183, 1,22}, // 176-179
  {120,184, 0,23},{185,115,24, 0},{186,127,23, 1},{128,187, 1,23}, // 180-183
  {120,188, 0,24},{189,115,25, 0},{190,127,24, 1},{128,191, 1,24}, // 184-187
  {120,192, 0,25},{193,115,26, 0},{194,127,25, 1},{128,195, 1,25}, // 188-191
  {120,196, 0,26},{197,115,27, 0},{198,127,26, 1},{128,199, 1,26}, // 192-195
  {120,200, 0,27},{201,115,28, 0},{202,127,27, 1},{128,203, 1,27}, // 196-199
  {120,204, 0,28},{205,115,29, 0},{206,127,28, 1},{128,207, 1,28}, // 200-203
  {120,208, 0,29},{209,125,30, 0},{210,127,29, 1},{128,211, 1,29}, // 204-207
  {130,212, 0,30},{213,125,31, 0},{214,137,30, 1},{138,215, 1,30}, // 208-211
  {130,216, 0,31},{217,125,32, 0},{218,137,31, 1},{138,219, 1,31}, // 212-215
  {130,220, 0,32},{221,125,33, 0},{222,137,32, 1},{138,223, 1,32}, // 216-219
  {130,224, 0,33},{225,125,34, 0},{226,137,33, 1},{138,227, 1,33}, // 220-223
  {130,228, 0,34},{229,125,35, 0},{230,137,34, 1},{138,231, 1,34}, // 224-227
  {130,232, 0,35},{233,125,36, 0},{234,137,35, 1},{138,235, 1,35}, // 228-231
  {130,236, 0,36},{237,125,37, 0},{238,137,36, 1},{138,239, 1,36}, // 232-235
  {130,240, 0,37},{241,125,38, 0},{242,137,37, 1},{138,243, 1,37}, // 236-239
  {130,244, 0,38},{245,135,39, 0},{246,137,38, 1},{138,247, 1,38}, // 240-243
  {140,248, 0,39},{249,135,40, 0},{250, 69,39, 1},{ 80,251, 1,39}, // 244-247
  {140,252, 0,40},{249,135,41, 0},{250, 69,40, 1},{ 80,251, 1,40}, // 248-251
  {140,252, 0,41}
};  // 252, 253-255 are reserved

#define nex(state,sel) State_table[state][sel]

/******************************************************************************
 * State map 
 * A StateMap maps a nonstationary counter state to a probability.
 * After each mapping, the mapping is adjusted to improve future
 * predictions.  Methods:
 *
 * sm.p(cx) converts state cx (0-255) to a probability (0-4095).
 *
 * Counter state -> probability * 256
 ******************************************************************************/
struct statemap_t {
        int       context;
        int       size;
        uint16_t *table;
};

void statemap_init(struct statemap_t *sm)
{
        int i;
        int n0;
        int n1;

        sm->context = 0;
        sm->table   = (uint16_t *)calloc(1, 256*sizeof(uint16_t));
        sm->size    = 256;

        for (i=0; i<256; i++) {
                n0 = nex(i, 2);
                n1 = nex(i, 3);
    
                if (n0 == 0) {
                        n1 *= 64;
                }
                if (n1 == 0) {
                        n0 *= 64;
                }
    
                sm->table[i] = 65536*(n1+1)/(n0+n1+2);
        }
}

int statemap_predict(struct statemap_t *sm, int context, int bit)
{
        assert(context>=0 && context<sm->size);

        sm->table[sm->context] += (bit<<16)-sm->table[sm->context]+128 >> 8;

        sm->context = context;
        
        return sm->table[sm->context] >> 4;
}




// Predict to mixer m from bit history state s, using sm to map s to
// a probability.
//
//
//  here it is...
inline int mix2(struct nn_t *mixer, int s, int bit, struct statemap_t *sm) 
{
        int p1 = statemap_predict(sm, s, bit);
        int n0 = nex(s, 2);
        int n1 = nex(s, 3);
        int st = stretch(p1) >> 2;
        
        /*printf("add:%d\n", st);*/
        nn_input(mixer, st);
        
        p1 >>= 4;
        
        int p0 = 255 - p1;

        /*printf("add:%d\n", st*(!n0-!n1));*/
        /*printf("add:%d\n", st*(!n0-!n1));*/
        /*printf("add:%d\n", (p1&-!n0)-(p0&-!n1));*/
        /*printf("add:%d\n", (p1&-!n1)-(p0&-!n0));*/
        
        nn_input(mixer, p1-p0);
        nn_input(mixer, st*(!n0-!n1));
        nn_input(mixer, (p1&-!n0)-(p0&-!n1));
        nn_input(mixer, (p1&-!n1)-(p0&-!n0));

        return (s > 0);
}




/******************************************************************************
 * CONTEXT STORAGE 
 ******************************************************************************/
/*
 * Predicts the next bit of a non-stationary process.
 *
 * Raw source data is sampled, one symbol at a time, and passed to the
 * predictor. The predictor adds the new symbol to its predictive model
 * in a process called INCORPORATION.
 *
 */

/*
 * Each bucket has an 8-bit LRU queue, seven 16-bit
 * checksums, and seven 7-byte history arrays.
 *
 * Each "element" in the bucket gets one checksum, 
 * and one 7-byte history array.
 *
 * An item of the array is a state, simply one of
 * the 256 possible bytes you could see reading
 * from the source. 
 *
 * The queue contains the indices of the two most
 * recently used elements in the bucket. 
 *
 *      queue = 0000 0000
 *               |      \
 *         index of    index of most-recently
 *         next-most   used element in bucket.
 *         recently
 *         used element 
 *         in bucket.   
 *
 * 
 * In the event that a context ends on a byte boundary, 
 * then rather than store 7 distinct recently-seen states,
 * the array will become a run model:
 *
 *         B0       B1       B2       B3       B4       B5       B6
 *      00000000 00000000 00000000 00000000 00000000 00000000 00000000
 *                                 \     /|    |     \               /
 *       state    state    state    count |   last    two bytes seen before 
 *                                 (0-127)|   byte    first appearance of B4.
 *                                        |   seen 
 *                                     flag  (poss.
 *                                            repeated)
 *
 * If the flag is set to 0, then B5 and B6 are not used, because
 * no other bytes have been seen.
 *
 * If the flag is set to 1, then B5 and B6 are the history, and
 * there have been (count-2) repeats of B4.
 *
 * Memory for the bytes B5 and B6 is not zeroed, so if the flag is
 * set to 1:
 *      B6 is valid only if (count >= 3), and 
 *      B5 is valid only if (count >= 2).
 */
/**
 * struct context_hash_item_t
 * ``````````````````````````
 * 64-byte (512 bit) bucket
 *
 * Holds 7 items.
 *
 *      U8  queue     (8 bits)   last accessed queue (2 things, one in each nibble)
 *      U16 chk[7]    (112 bits) byte context checksums
 *      U8   bh[7][7] (392 bits) byte context, 3-bit context -> bit history state
 *
 * NOTE: THE ORDER OF THE MEMBERS OF THIS STRUCT
 *       IS NOT AN ACCIDENT. CHANGING THE ORDER
 *       WILL FOUL THE ALIGNMENT AND PADDING OF
 *       THE DATA STRUCTURE, IMPACTING THE CACHE 
 *       AND HARMING PERFORMANCE
 */
struct nsm_hash_item_t {
        uint8_t  queue;
        uint16_t checksum[7];
        uint8_t  history[7][7];
};


/**
 * context_hash_get()
 * ``````````````````
 * Find or create hash item matching checksum.
 *
 * @item    : Hash bucket reference
 * @checksum: Checksum to either find collision or create new item for
 * Return   : Value for checksum 
 */

 /* 
  * Updating the history:
  *
  *     0. The current context is updated with the new bit.
  *     1. A new bucket is selected based on the updated context.
  *     2. A slot for the new context is selected.
  *             2a. The slot exists
  *             2b. The slot does not exist, so we
  *                 make room using LRU based on the
  *                 queue.
  *
  *     Deciding who goes:
  *
  *             If no match is found, the element with the lowest
  *             priority among the 5 elements NOT in the LRU queue
  *             is replaced. 
  *
  *             The priority is the state number of the first element
  *             (the one with 0 additional bits of context).
  *
  *             The states are sorted by increasing n0+n1 (number of
  *             bits seen), implementing an LFU replacement policy.
  *
  *             After a replacement, the queue is emptied.
  *             This means that consecutive misses favor a
  *             LFU replacement policy.
  *
  *             In all cases, the found/replaced element is put in
  *             the front of the queue.
  */
uint8_t* hash_get(struct nsm_hash_item_t *item, uint16_t ch) 
{
  if (item->checksum[item->queue&15]==ch) {
          /*printf("match:%d\n", ch);*/
          return &item->history[item->queue&15][0];
  }
  int b=0xffff, bi=0;
  int i;
  for (i=0; i<7; ++i) {
    if (item->checksum[i]==ch) {
                /*printf("on %d: match:%d\n", i, ch);*/
            return item->queue=item->queue<<4|i, &item->history[i][0];
    }
    int pri=item->history[i][0];
    if ((item->queue&15)!=i && item->queue>>4!=i && pri<b) b=pri, bi=i;
  }
   /*printf("no match:%d\n", ch);*/
  return item->queue=0xf0|bi, item->checksum[bi]=ch, (uint8_t*)memset(&item->history[bi][0], 0, 7);
}


uint8_t *nsm_hash_get(struct nsm_hash_item_t *item, uint16_t checksum)
{
        int lowest   = 0xFFFF; /* Lowest element priority seen */
        int priority = 0;      /* Priority of an element */
        int index    = 0;      /* Index of the element to get returned */
        int i        = 0;      /* Index used during linear probe */

        /* 
         * If it's the most recently-used, we don't need
         * to update the table, just return the same item.
         * 
         * Note the use of BITWISE AND to isolate the lower 
         * nibble 
         */
        if (item->checksum[item->queue & 0x0F] == checksum) { 
                /*printf("match:%d\n", checksum);*/
                return &item->history[item->queue & 0x0F][0];
        }

        /* 
         * Run a linear probe on the table, searching for a 
         * matching checksum. If none is found, select one
         * of the elements and replace it with ours. 
         */ 
        for (i=0; i<7; i++) {
                if (item->checksum[i] == checksum) { 
                        /* 
                         * Left-shift by 4 bits pushes the upper nibble 
                         * into oblivion, the lower nibble into the upper 
                         * nibble, and shifts 4 fresh 0 bits into the lower
                         * nibble, which we BITWISE OR the new index into. 
                         */
                        item->queue = (item->queue << 4) | i;

                        /*printf("on %d: match:%d\n", i, checksum);*/
                        return &item->history[i][0];
                }

                /* 
                 * The priority is simply the value of the first state 
                 * in the history array under that index. 
                 */
                priority = item->history[i][0];

                if (i!=(item->queue & 0x0F) /* Don't replace the LRU items */
                &&  i!=(item->queue & 0xF0) 
                &&  priority<lowest) {
                        lowest = priority;
                        index  = i;
                }
        }
  
        /* Place the decided-upon index into the LRU queue */
        item->queue           = 0xF0 | index;
        
        /* Set the checksum in the checksum array */
        item->checksum[index] = checksum;

        /*printf("no match:%d\n", checksum);*/
        
        /* 
         * Zero the 7-bytes of that element's history array,
         * and return the address of the array. 
         */
        return (uint8_t *)memset(&item->history[index][0], 0, 7);
}


/******************************************************************************
 * "NSM" thing 
 ******************************************************************************/

/**
 * nsm_init()
 * ``````````
 * Initialize a context map.
 *
 * @map  : Reference to context map
 * @mem  : Number of bytes of memory to use
 * @count: Maximum number of contexts to use
 * Return: Nothing
 */
void nsm_init(struct nsm_t *map, int mem, int count)
{
        /* 
         * mem>>6 == mem/2^6 == mem/64 
         * This is the number of buckets we will have
         * (each bucket is 64 bytes)
         */
        int i;

        map->num_contexts = count;
        map->table    = calloc_align(64, (mem>>6)*sizeof(struct nsm_hash_item_t));
        map->size     = (mem>>6);
        map->statemap = calloc(1, map->num_contexts*sizeof(struct statemap_t));

        for (i=0; i<map->num_contexts; i++) {
                statemap_init(&map->statemap[i]); 
        }

        /* 
         * Why do we not rather have an array of this struct, than
         * arrays of (most of) its members? 
         */
        map->cur          = (uint8_t **)calloc(1, count*sizeof(uint8_t *));
        map->arr          = (uint8_t **)calloc(1, count*sizeof(uint8_t *));
        map->run          = (uint8_t **)calloc(1, count*sizeof(uint8_t *));
        map->context      = (uint32_t *)calloc(1, count*sizeof(uint32_t ));
        map->next_context = 0;

        for (i=0; i<count; i++) {
                map->arr[i] = map->cur[i] = &map->table[0].history[0][0];
                map->run[i] = map->arr[i] + 3;
        }
}


/**
 * nsm_set()
 * `````````
 * Write a value into the next available context.
 *
 * @map  : Reference to context map
 * @cx   : Context value
 * Return: Nothing
 */
void nsm_set(struct nsm_t *map, uint32_t cx)
{
        int i;
       
        i = map->next_context++;
        uint32_t savecx = cx;
  
        assert(i>=0 && i<map->num_contexts);
  
        /* Permute (don't hash) cx to spread the distribution */
        cx = cx*987654323 + i;
  
        cx = cx<<16 | cx>>16;

        /*printf("i:%d got:%d set:%d\n", i, savecx, cx * 123456791 + i);*/
  
        map->context[i] = cx * 123456791 + i;
}


uint8_t *nsm_select(struct nsm_t *map, int i, int jump)
{
        struct nsm_hash_item_t *bucket;
        int                     index;

        /* 
         * TODO: This "jump" thing is not correct. Something
         * is going on so that the index is the 2 low-order
         * bits of the context.
         *
         * Figure it out.
         */
        index  = map->context[i] + jump & (map->size-1);
        bucket = &map->table[index];
        
                                        /* This is where the checksum lives */
        return hash_get(bucket, map->context[i] >> 16);
}


uint8_t *nsm_update(struct nsm_t *map, int i, uint32_t ctx, uint8_t last_byte, int last_byte_bits)
{
        int bpos = last_byte_bits;
        int cc   = last_byte;
        int c1   = (ctx & 0x000000FF);

        if (bpos > 1 && map->run[i][0] == 0) {
                return NULL;

        } else if (bpos == 1 || bpos == 3 || bpos == 6) {

                return &map->arr[i][1+(cc & 1)];

        } else if (bpos == 4 || bpos == 7) {

                return &map->arr[i][3 + (cc & 3)];

        } else if (1 || bpos == 0 || bpos == 2 || bpos == 5) {
                map->arr[i] = nsm_select(map, i, cc);

                /*
                 * Update pending bit histories for bits 2-7
                 */
                if (bpos == 0) {
                        /* 
                         * There has been a repeat.
                         */
                        if (map->arr[i][3] == 2) {
                                /* 
                                 * Let c be the repeated byte plus
                                 * 256.
                                 */
                                const int c = map->arr[i][4] + 256;

                                uint8_t *p = nsm_select(map, i, c>>6);

                                /*
                                 * Form the index from the 2 least
                                 * significant bits in the byte.
                                 */
                                p[0]            = 1 + ((c>>5)&1);
                                p[1+((c>>5)&1)] = 1 + ((c>>4)&1);
                                p[3+((c>>4)&3)] = 1 + ((c>>3)&1);

                                p = nsm_select(map, i, c>>3);

                                /*
                                 * Do it again
                                 */
                                p[0]            = 1 + ((c>>2)&1);
                                p[1+((c>>2)&1)] = 1 + ((c>>1)&1);
                                p[3+((c>>1)&3)] = 1 + (c&1);

                                map->arr[i][6] = 0;
                        }
                        
                        /* No repeats yet */ 
                        if (map->run[i][0] == 0) { 
                                /* 
                                 * NOTE: Setting the count to
                                 * 2 is actually setting it to
                                 * 1, since the lowest-order 
                                 * bit is a flag.
                                 *
                                 *      2 DEC == 00000010 BIN
                                 */
                                map->run[i][0] = 2;

                                /* 
                                 * Set the byte that might
                                 * be repeated
                                 */
                                map->run[i][1] = c1;

                        /* 
                         * A new byte has been seen, so the
                         * run has ended. 
                         */
                        } else if (map->run[i][1] != c1) {
                                /* Set the flag to 1 */
                                map->run[i][0] = 1;
                                /* Set the last-seen byte */
                                map->run[i][1] = c1;

                        /*
                         * The count is less than 254
                         */
                        } else if (map->run[i][0]<254) {
                                /* 
                                 * Increment the count by 2.
                                 * (Rather than 1, so we 
                                 * aren't touching the flag 
                                 * bit)
                                 */
                                map->run[i][0] += 2;
                        }

                        /* 
                         * Update the runpointer to be
                         * 3 items down the head of the
                         * array.
                         */
                        map->run[i] = map->arr[i]+3;
                        return map->arr[i];
                }

                return map->arr[i];
        }

        /* Unreachable */
        return NULL;
}

void nsm_newbit(struct nsm_t *map, int i, int bit) 
{
        assert(map->cur[i]>=&map->table[0].history[0][0] && map->cur[i]<=&map->table[map->size-1].history[6][6]);
        if (((long)(map->cur[i])&63) < 15) {
                /*printf("x:%d\n", (long)(map->cur[i])&63);*/
        }
        /*assert(((long)(map->cur[i])&63)>=15);*/
        /* TODO: This last assertion is failing, we must fix that */

        /* Get a new state */
        int ns = nex(*map->cur[i], bit);
        
        /* Probabilistic increment of the 1's and 0's counts */
        if (ns >= 204 && prng() << (452-ns>>3)) { 
                ns -= 4;
        }

        /* Set the history to that index */
        *map->cur[i] = ns;
}

int nsm_predict(struct nsm_t *map, int i, uint8_t last_byte, int last_byte_bits)
{
        /* count*2, +1 if 2 different bytes seen */
        int rc = map->run[i][0];

        /* If we have matched the partial byte of the context */
        if (map->run[i][1]+256>>8 - last_byte_bits == last_byte) {

                /* Predicted bit + for 1, - for 0 */
                /* Will be +-1 */
                int b = (map->run[i][1]>>7-last_byte_bits&1)*2-1;  
                int c = ilog(rc+1)<<2+(~rc&1);

                /*printf("runp[%d][1]:%d \n", i, map->run[i][1]);*/
                /*printf("predicting bp:%d|cc:%d :%d*%d=%d\n", last_byte_bits, last_byte, b, c, b*c);*/

                return b*c;
        } else {
                /*printf("not exactly...\n");*/
                return 0;
        }
}

/**
 * nsm_mix()
 * `````````
 * Update the context model with bit y1, and predict the next bit to mixer m
 *
 * @map  : Context map reference
 * @mixer: Mixer reference
 * @cc   : c0
 * @bp   : bpos
 * @c1   : buf(1)
 * @y1   : y
 * Return: Some result 
 */
int nsm_mix(struct nsm_t *map, struct nn_t *mixer, uint32_t ctx, uint8_t last_byte, int last_byte_bits, int last_bit)
{
        int result=0;
        int i;

        /* 
         * For all current contexts.
         */ 
        /*printf("next_ctx:%d\n", map->next_context);*/
        for (i=0; i<map->next_context; ++i) {
                /* 
                 * Update each of the current contexts
                 * with the new bit. 
                 */
                if (map->cur[i]) {
                        nsm_newbit(map, i, last_bit);
                }

                /* Update the run model */
                map->cur[i] = nsm_update(map, i, ctx, last_byte, last_byte_bits);

                /* 
                 * Predict from last byte in context
                 */
                nn_input(mixer, nsm_predict(map, i, last_byte, last_byte_bits));
                
                /* Predict from bit context */
                result += mix2(mixer, map->cur[i] ? *map->cur[i] : 0, last_bit, &map->statemap[i]);
        }

        if (last_byte_bits == 7) { 
                /* We've read a whole byte. Time to move on. */
                map->next_context = 0;
        }

        return result;
}
