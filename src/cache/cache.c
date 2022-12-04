/*
 * cache.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions, both dirty and clean.  The replacement policy is LRU. 
 *     The cache is a writeback cache. 
 * 
 * Updated 2021: M. Hinton
 */
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "cache.h"

#define ADDRESS_LENGTH 64

/* Counters used to record cache statistics in printSummary().
   test-cache uses these numbers to verify correctness of the cache. */

//Increment when a miss occurs
int miss_count = 0;

//Increment when a hit occurs
int hit_count = 0;

//Increment when a dirty eviction occurs
int dirty_eviction_count = 0;

//Increment when a clean eviction occurs
int clean_eviction_count = 0;

/* TODO: add more globals, structs, macros if necessary */
uword_t next_lru;
// bool evicted;

/*
 * Initialize the cache according to specified arguments
 * Called by cache-runner so do not modify the function signature
 *
 * The code provided here shows you how to initialize a cache structure
 * defined above. It's not complete and feel free to modify/add code.
 */
cache_t *create_cache(int s_in, int b_in, int E_in, int d_in) {
    /* see cache-runner for the meaning of each argument */
    cache_t *cache = malloc(sizeof(cache_t));
    cache->s = s_in;
    cache->b = b_in;
    cache->E = E_in;
    cache->d = d_in;
    unsigned int S = (unsigned int) 1 << cache->s;
    unsigned int B = (unsigned int) 1 << cache->b;

    cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++){
        cache->sets[i].lines = (cache_line_t*) calloc(cache->E, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->E; j++){
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag   = 0;
            cache->sets[i].lines[j].lru   = 0;
            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].data  = calloc(B, sizeof(byte_t));
        }
    }

    /* TODO: add more code for initialization */
    next_lru = 0;
    // evicted = 0;
    return cache;
}

cache_t *create_checkpoint(cache_t *cache) {
    unsigned int S = (unsigned int) 1 << cache->s;
    unsigned int B = (unsigned int) 1 << cache->b;
    cache_t *copy_cache = malloc(sizeof(cache_t));
    memcpy(copy_cache, cache, sizeof(cache_t));
    copy_cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++) {
        copy_cache->sets[i].lines = (cache_line_t*) calloc(cache->E, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->E; j++) {
            memcpy(&copy_cache->sets[i].lines[j], &cache->sets[i].lines[j], sizeof(cache_line_t));
            copy_cache->sets[i].lines[j].data = calloc(B, sizeof(byte_t));
            memcpy(copy_cache->sets[i].lines[j].data, cache->sets[i].lines[j].data, sizeof(byte_t));
        }
    }
    
    return copy_cache;
}

void display_set(cache_t *cache, unsigned int set_index) {
    unsigned int S = (unsigned int) 1 << cache->s;
    if (set_index < S) {
        cache_set_t *set = &cache->sets[set_index];
        for (unsigned int i = 0; i < cache->E; i++) {
            printf ("Valid: %d Tag: %llx Lru: %lld Dirty: %d\n", set->lines[i].valid, 
                set->lines[i].tag, set->lines[i].lru, set->lines[i].dirty);
        }
    } else {
        printf ("Invalid Set %d. 0 <= Set < %d\n", set_index, S);
    }
}

/*
 * Free allocated memory. Feel free to modify it
 */
void free_cache(cache_t *cache) {
    unsigned int S = (unsigned int) 1 << cache->s;
    for (unsigned int i = 0; i < S; i++){
        for (unsigned int j = 0; j < cache->E; j++) {
            free(cache->sets[i].lines[j].data);
        }
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

/* TODO:
 * Get the line for address contained in the cache
 * On hit, return the cache line holding the address
 * On miss, returns NULL
 */
cache_line_t *get_line(cache_t *cache, uword_t addr) {
    /* your implementation */
    // uword_t tag = addr >> (cache->b + cache->s);
    // unsigned int set_index = 0;
    cache_line_t *line = NULL;
    cache_set_t set = cache->sets[(addr >> cache->b) & ((1 << cache->s) -1)];
    // int i = 0;

    for (int i = 0; i<cache->E; i++)
    {
        if (set.lines[i].valid && set.lines[i].tag == addr >> (cache->b + cache->s))
        {
            line = &(set.lines[i]);
        }
    }
    return line;
}

/* TODO:
 * Select the line to fill with the new cache line
 * Return the cache line selected to filled in by addr
 */
cache_line_t *select_line(cache_t *cache, uword_t addr) {
    /* your implementation */
    // uword_t tag = addr >> (cache->b + cache->s);
    // unsigned int set_index = 0;
    cache_set_t set = cache->sets[(addr >> cache->b) & ((1 << cache->s) -1)];
    cache_line_t *line = &set.lines[0];
    uword_t lru = set.lines[0].lru;
    bool evicted = true; //true
    for (int i = 0; i < cache->E; i++)
    {
        if (!set.lines[i].valid)
        {
            evicted = false; //false
            // printf("not evicted, found line\n");
            return &set.lines[i];
        }

        else if (set.lines[i].lru < lru)
        {
            
            // printf("evicted\n");
            line = &set.lines[i];
            lru = set.lines[i].lru;
            // printf("(inside loop) dirty? %d\n", line->dirty);
        }
    }
    
    if (evicted)
    {
        // printf("(outside loop) dirty? %d\n", line->dirty);
        // printf("(outside loop) evicted? %d\n", evicted);
        if (!line->dirty)
        {
            // printf("clean\n");
            clean_eviction_count++;
        }
        else
        {
            // printf("dirty\n");
            dirty_eviction_count++;
        }
    }
    
    return line;
}

/*  TODO:
 * Check if the address is hit in the cache, updating hit and miss data.
 * Return true if pos hits in the cache.
 */
bool check_hit(cache_t *cache, uword_t addr, operation_t operation) {
    /* your implementation */
    // uword_t tag = addr >> (cache->b + cache->s);
    cache_line_t *line = get_line(cache, addr);
    bool hit = false;
    if (line != NULL)
    {
        hit = true;
    }
    // printf("hit is %d \n", hit);
    
    if (!hit)
    {
        // line->dirty = operation == WRITE;
        // printf("miss\n");
        miss_count++;
    }
    else
    {
        if (operation != READ)
        {
            line->dirty = true;
        }
        // line->dirty = operation == WRITE;
        hit_count++;
        // printf("hit\n");
        line->lru = next_lru;
        next_lru++;
    }
    
    return hit;
}

/*  TODO:
 * Handles Misses, evicting from the cache if necessary.
 * Fill out the evicted_line_t struct with info regarding the evicted line.
 */
evicted_line_t *handle_miss(cache_t *cache, uword_t addr, operation_t operation, byte_t *incoming_data) {
    evicted_line_t *evicted = malloc(sizeof(evicted_line_t));
    cache_line_t *line = select_line(cache, addr);
    unsigned int off = cache->s + cache->b;
    evicted->data = malloc(sizeof(line->data));
    memcpy(evicted->data, line->data, 8);
    evicted->dirty = line->dirty;
    evicted->valid = line->valid;
    evicted->addr = line->tag << (off) | ((addr >> cache->b) & ((1 << cache->s) -1) << cache->b);

    line->lru = next_lru;
    next_lru++;

    if (incoming_data != NULL)
    {
        memcpy(line->data, incoming_data, sizeof(8));
    }
    line->dirty = operation == WRITE;
    line->tag = addr >> (off);
    line->valid = true;

    return evicted;
}

/* TODO:
 * Get 8 bytes from the cache and write it to dest.
 * Preconditon: pos is contained within the cache.
 */
void get_word_cache(cache_t *cache, uword_t addr, word_t *dest) {
    cache_line_t *line;
    word_t *data = 0;
    int byte = 0;
    // word_t to_shift;
    while (byte < 8)
    {
        uword_t spot = addr + byte;
        line = get_line(cache, spot);
        int shift = 8 * byte;
        *data |= ((line->data[spot & ((1 << cache->b) - 1)] & 0XFF) << shift);
        byte++;
    }
    /* your implementation */
    dest = data;
    return;
}

/* TODO:
 * Set 8 bytes in the cache to val at pos.
 * Preconditon: pos is contained within the cache.
 */
void set_word_cache(cache_t *cache, uword_t addr, word_t val) {
    cache_line_t *line;
    int byte = 0;
    // word_t to_shift;
    while (byte < 8)
    {
        uword_t spot = addr + byte;
        line = get_line(cache, spot);
        line->data[spot & ((1 << cache->b) - 1)] = (byte_t) val & 0xFF;
        val = val >> 8;
        byte++;
    }
    /* Your implementation */
    return;
}

/*
 * Access data at memory address addr
 * If it is already in cache, increast hit_count
 * If it is not in cache, bring it in cache, increase miss count
 * Also increase eviction_count if a line is evicted
 *
 * Called by cache-runner; no need to modify it if you implement
 * check_hit() and handle_miss()
 */
void access_data(cache_t *cache, uword_t addr, operation_t operation) {
    if(!check_hit(cache, addr, operation))
        free(handle_miss(cache, addr, operation, NULL));
}