#ifndef _HUG_MAP_H 
#define _HUG_MAP_H 

#include "util.h"
#include "mixer.h"

/******************************************************************************
 * NONSTATIONARY MAP
 ******************************************************************************/

extern int Context_count;

struct nsm_t {
        int num_contexts;
        int next_context;
        int size;

        struct statemap_t *statemap;

        /* 
         * Bit histories for bits 0-1, 2-4, 5-7 
         */
        struct nsm_hash_item_t *table;

        /* 
         * Array of pointers to current bit history for each 
         * of the @num_contexts contexts.
         */
        uint8_t **cur;

        /* 
         * Array of pointers to the first element of the 7-item array containing
         * current_bit_history[i] 
         */
        uint8_t **arr;
        uint8_t **run;

        /* Whole byte contexts (hashes) */
        uint32_t *context;
};


void     nsm_init    (struct nsm_t *map, int mem, int count);
void     nsm_set     (struct nsm_t *map, uint32_t cx);
//uint8_t *nsm_select  (struct nsm_t *map, int i, int jump);
uint8_t *nsm_update  (struct nsm_t *map, int i, uint32_t ctx, uint8_t last_byte, int last_byte_bits);
//void     nsm_newbit  (struct nsm_t *map, int i, int bit);
int      nsm_predict (struct nsm_t *map, int i, uint8_t last_byte, int last_byte_bits);
int      nsm_mix     (struct nsm_t *map, struct nn_t *mixer, uint32_t ctx, uint8_t last_byte, int last_byte_bits, int last_bit);


void print_statemap_counts(void);

#endif


