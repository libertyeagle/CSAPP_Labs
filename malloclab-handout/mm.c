/*
 * CSAPP Malloc Lab
 * by libertyeagle
 * Implementation:
 *  - using linux's buddy system like solution (segregated free list)
 *  - free list size classes are power of 2
 *  - 32; 64; 128; 256; 512; 1024; 2048
 *  - blocks within each free list are sorted in ascending order
 *  - maintain an explicit free list for each size class
 *  - 4 words are required for each free block, so min size class is 32
 *  - for each allocated block:
 *      header, payload, (optional) padding, footer
 *  - for each free block:
 *      header, predecessor, successor, footer
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "libertyeagle",
    /* First member's full name */
    "libertyeagle",
    /* First member's email address */
    "wuyongji317@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define BLOCK_FREE 0
#define BLOCK_ALLOCATED 1

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define FREE_BLOCK_PRED(bp) ((char *)(bp))
#define FREE_BLOCK_SUCC(bp) ((char *)(bp) + WSIZE)

static void *extend_heap(size_t double_words);
static void *get_segregated_free_list_index(size_t size);
static void insert_to_free_list(void *bp);
static void remove_from_free_list(void *bp);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t asize);
static void place_realloc(void *bp, size_t asize);
static void *coalesce_realloc(void *bp, size_t new_size);

static char *heap_listp;
static char *free_list_pointer;

/*
 * extend_heap - extends the heap with a new free block
 * alignment requirement: 16 bytes (4 words)
 *  since 4 words are required in a free block ()
 * parameter: how many double words to allocate?
 */
static void *extend_heap(size_t double_words)
{
    char *bp;
    size_t size;

    size = (double_words % 2) ? (double_words + 1) * DSIZE : double_words * DSIZE;
    
    if ((long)(bp = mem_sbrk(size)) == (void *) -1) return NULL;

    PUT(HDRP(bp), PACK(size, BLOCK_FREE));         // fill header (location at bp - WSIZE)
    PUT(FTRP(bp), PACK(size, BLOCK_FREE));         // fill footer

    PUT(FREE_BLOCK_PRED(bp), NULL);       // set `pred` field in the new free block
    PUT(FREE_BLOCK_SUCC(bp), NULL);       // set `succ` field in the new free block

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, BLOCK_ALLOCATED)); // new epilogue header

    return coalesce(bp);
}

/*
 * get_segregated_free_list_index - given `size`, return apporatiate free list index
 */
static void *get_segregated_free_list_index(size_t size)
{
    unsigned int i;
    if (size <= 32) i = 0;
    else if (size <= 64) i = 1;
    else if (size <= 128) i = 2;
    else if (size <= 256) i = 3;
    else if (size <= 512) i = 4;
    else if (size <= 1024) i = 5;
    else if (size <= 2048) i = 6;
    else if (size <= 4096) i = 7;
    else i = 8;

    return free_list_pointer + i * WSIZE;
}

/*
 * insert_to_free_list - insert given block to free list
 */
static void insert_to_free_list(void *bp)
{
    char *root_pointer = get_segregated_free_list_index(GET_SIZE(HDRP(bp)));
    char *prev_free_block = root_pointer;
    char *next_free_block = GET(prev_free_block);

    while (next_free_block != NULL) {
        // find the correct position to insert, right before the first free block whose size is larger than bp.
        // ensure that blocks are sorted by size for each free list
        if (GET_SIZE(HDRP(next_free_block)) >= GET_SIZE(HDRP(bp))) break;
        prev_free_block = next_free_block;
        next_free_block = GET(FREE_BLOCK_SUCC(next_free_block));
    }

    if (prev_free_block == root_pointer) {
        // insert to head
        PUT(root_pointer, bp);
        PUT(FREE_BLOCK_PRED(bp), NULL);
        PUT(FREE_BLOCK_SUCC(bp), next_free_block);
        if (next_free_block != NULL) PUT(FREE_BLOCK_PRED(next_free_block), bp);
    }
    else {
        // otherwise
        PUT(FREE_BLOCK_SUCC(prev_free_block), bp);
        PUT(FREE_BLOCK_PRED(bp), prev_free_block);
        PUT(FREE_BLOCK_SUCC(bp), next_free_block);
        if (next_free_block != NULL) PUT(FREE_BLOCK_PRED(next_free_block), bp);
    }
}

/*
 * remove_from_free_list - remove given free block from free list
 */
static void remove_from_free_list(void *bp)
{
    char *root_pointer = get_segregated_free_list_index(GET_SIZE(HDRP(bp)));
    char *prev_free_block = GET(FREE_BLOCK_PRED(bp));
    char *next_free_block = GET(FREE_BLOCK_SUCC(bp));

    if (prev_free_block != NULL) {
        // has previous free block
        if (next_free_block != NULL) PUT(FREE_BLOCK_PRED(next_free_block), prev_free_block);
        PUT(FREE_BLOCK_SUCC(prev_free_block), next_free_block);
    }
    else {
        if (next_free_block != NULL) PUT(FREE_BLOCK_PRED(next_free_block), NULL);
        PUT(root_pointer, next_free_block);
    }

    PUT(FREE_BLOCK_PRED(bp), NULL);
    PUT(FREE_BLOCK_SUCC(bp), NULL);
}

/*
 * coalesce - coalescing target block to merge it with any adjacent free blocks
 */
static void *coalesce(void *bp)
{
    unsigned int prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    unsigned int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc == BLOCK_ALLOCATED && next_alloc == BLOCK_ALLOCATED) {

    }
    else if (prev_alloc == BLOCK_ALLOCATED && next_alloc == BLOCK_FREE) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_from_free_list(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, BLOCK_FREE));
        PUT(FTRP(bp), PACK(size, BLOCK_FREE));
    }
    else if (prev_alloc == BLOCK_FREE && next_alloc == BLOCK_ALLOCATED) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        remove_from_free_list(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, BLOCK_FREE));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, BLOCK_FREE));
        bp = PREV_BLKP(bp);
    }
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        remove_from_free_list(PREV_BLKP(bp));
        remove_from_free_list(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, BLOCK_FREE));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, BLOCK_FREE));
        bp = PREV_BLKP(bp);
    }

    insert_to_free_list(bp);
    return bp;
}

/*
 * find_fit - find apporatiate free block to place, if none return NULL
 *  using first fit (since the free list are ordered, this is the same as best fit)
 */
static void *find_fit(size_t size)
{
    char *root_pointer;
    char *bp;
    for (root_pointer = get_segregated_free_list_index(size); root_pointer != heap_listp - WSIZE; root_pointer += WSIZE) {
        bp = GET(root_pointer);
        while (bp != NULL)
            if (GET_SIZE(HDRP(bp)) >= size) return bp;
            else bp = GET(FREE_BLOCK_SUCC(bp));
    }
    return NULL;
}

/*
 * place
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    remove_from_free_list(bp);

    // 2 * DSIZE (4 words) are needed for a new free block
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, BLOCK_ALLOCATED));
        PUT(FTRP(bp), PACK(asize, BLOCK_ALLOCATED));
        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize - asize, BLOCK_FREE));
        PUT(FTRP(bp), PACK(csize - asize, BLOCK_FREE));
        PUT(FREE_BLOCK_PRED(bp), NULL);
        PUT(FREE_BLOCK_SUCC(bp), NULL);
        insert_to_free_list(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, BLOCK_ALLOCATED));
        PUT(FTRP(bp), PACK(csize, BLOCK_ALLOCATED));
    }
}

/*
 * place_realloc
 */
static void place_realloc(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    remove_from_free_list(bp);

    // 2 * DSIZE (4 words) are needed for a new free block
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, BLOCK_ALLOCATED));
        PUT(FTRP(bp), PACK(asize, BLOCK_ALLOCATED));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, BLOCK_FREE));
        PUT(FTRP(bp), PACK(csize - asize, BLOCK_FREE));
        PUT(FREE_BLOCK_PRED(bp), NULL);
        PUT(FREE_BLOCK_SUCC(bp), NULL);
        coalesce(bp);                   // there might be adjacent free blocks
    }
    else {
        PUT(HDRP(bp), PACK(csize, BLOCK_ALLOCATED));
        PUT(FTRP(bp), PACK(csize, BLOCK_ALLOCATED));
    }
}

/*
 * coalesce_realloc - if possible, realloc given block using adjacent free blocks
 */
static void *coalesce_realloc(void *bp, size_t new_size)
{
    size_t orig_size = GET_SIZE(HDRP(bp));
    size_t max_adj_size;

    int prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    if (prev_alloc == BLOCK_ALLOCATED && next_alloc == BLOCK_ALLOCATED) {

    }
    else if (prev_alloc == BLOCK_ALLOCATED && next_alloc == BLOCK_FREE) {
        max_adj_size = orig_size + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        if (max_adj_size >= new_size + 2 * DSIZE) {
            void *next_bp = NEXT_BLKP(bp);
            remove_from_free_list(next_bp);
            PUT(HDRP(next_bp), PACK(new_size - orig_size, BLOCK_ALLOCATED));
            PUT(FTRP(next_bp), PACK(new_size - orig_size, BLOCK_ALLOCATED));
            PUT(HDRP(NEXT_BLKP(next_bp)), PACK(max_adj_size - new_size, BLOCK_FREE));
            PUT(FTRP(NEXT_BLKP(next_bp)), PACK(max_adj_size - new_size, BLOCK_FREE));
            insert_to_free_list(NEXT_BLKP(next_bp));

            PUT(FTRP(next_bp), PACK(new_size, BLOCK_ALLOCATED));
            PUT(HDRP(bp), PACK(new_size, BLOCK_ALLOCATED));
            return bp;
        }
        else if (max_adj_size >= new_size) {
            void *next_bp = NEXT_BLKP(bp);
            remove_from_free_list(next_bp);
            PUT(FTRP(next_bp), PACK(max_adj_size, BLOCK_ALLOCATED));
            PUT(HDRP(bp), PACK(max_adj_size, BLOCK_ALLOCATED));
            return bp;
        }
    }
    else if (prev_alloc == BLOCK_FREE && next_alloc == BLOCK_ALLOCATED) {
        max_adj_size = orig_size + GET_SIZE(FTRP(PREV_BLKP(bp)));
        if (max_adj_size >= new_size + 2 * DSIZE ) {
            void *prev_bp = PREV_BLKP(bp);
            remove_from_free_list(prev_bp);
            PUT(FTRP(prev_bp), PACK(new_size - orig_size, BLOCK_ALLOCATED));
            prev_bp = PREV_BLKP(bp);
            PUT(HDRP(prev_bp), PACK(new_size - orig_size, BLOCK_ALLOCATED));
            PUT(prev_bp - DSIZE, PACK(max_adj_size - new_size, BLOCK_FREE));
            PUT(HDRP(PREV_BLKP(prev_bp)), PACK(max_adj_size - new_size, BLOCK_FREE));
            // use memmove instead of memcpy to handle the (possible) overlapped area
            insert_to_free_list(PREV_BLKP(prev_bp));

            PUT(HDRP(prev_bp), PACK(new_size, BLOCK_ALLOCATED));
            PUT(FTRP(bp), PACK(new_size, BLOCK_ALLOCATED));

            memmove(prev_bp, bp, orig_size - DSIZE);

            return prev_bp;
        }
        else if (max_adj_size >= new_size) {
            void *prev_bp = PREV_BLKP(bp);
            remove_from_free_list(prev_bp);
            PUT(HDRP(prev_bp), PACK(max_adj_size, BLOCK_ALLOCATED));
            PUT(FTRP(bp), PACK(max_adj_size, BLOCK_ALLOCATED));
            memmove(prev_bp, bp, orig_size - DSIZE);
            return prev_bp;
        }
    }
    else {
        max_adj_size = orig_size + GET_SIZE(FTRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        if (max_adj_size >= new_size) {
            void *next_bp = NEXT_BLKP(bp);
            size_t size_next = GET_SIZE(HDRP(next_bp));
            if (size_next >= new_size - orig_size + 2 * DSIZE) {
                remove_from_free_list(next_bp);
                PUT(HDRP(next_bp), PACK(new_size - orig_size, BLOCK_ALLOCATED));
                PUT(FTRP(next_bp), PACK(new_size - orig_size, BLOCK_ALLOCATED));
                PUT(HDRP(NEXT_BLKP(next_bp)), PACK(size_next - (new_size - orig_size), BLOCK_FREE));
                PUT(FTRP(NEXT_BLKP(next_bp)), PACK(size_next - (new_size - orig_size), BLOCK_FREE));
                PUT(FREE_BLOCK_PRED(NEXT_BLKP(next_bp)), NULL);
                PUT(FREE_BLOCK_SUCC(NEXT_BLKP(next_bp)), NULL);
                insert_to_free_list(NEXT_BLKP(next_bp));

                PUT(FTRP(next_bp), PACK(new_size, BLOCK_ALLOCATED));
                PUT(HDRP(bp), PACK(new_size, BLOCK_ALLOCATED));
                return bp;
            }
            else if (size_next >= new_size - orig_size) {
                remove_from_free_list(next_bp);
                PUT(FTRP(next_bp), PACK(orig_size + size_next, BLOCK_ALLOCATED));
                PUT(HDRP(bp), PACK(orig_size + size_next, BLOCK_ALLOCATED));
                return bp;
            }
            else remove_from_free_list(next_bp);
            
            size_t size_left = new_size - orig_size - size_next;
            void *prev_bp = PREV_BLKP(bp);
            size_t size_prev = GET_SIZE(FTRP(prev_bp));
            if (size_prev >= size_left + 2 * DSIZE) {
                remove_from_free_list(prev_bp);
                PUT(FTRP(prev_bp), PACK(size_left, BLOCK_ALLOCATED));
                prev_bp = PREV_BLKP(bp);
                PUT(HDRP(prev_bp), PACK(size_left, BLOCK_ALLOCATED));
                PUT(prev_bp - DSIZE, PACK(size_prev - size_left, BLOCK_FREE));
                PUT(HDRP(PREV_BLKP(prev_bp)), PACK(size_prev - size_left, BLOCK_FREE));
                insert_to_free_list(PREV_BLKP(prev_bp));

                PUT(FTRP(next_bp), PACK(new_size, BLOCK_ALLOCATED));
                PUT(HDRP(prev_bp), PACK(new_size, BLOCK_ALLOCATED));
                memmove(prev_bp, bp, orig_size - DSIZE);
                return prev_bp;
            }
            else {
                remove_from_free_list(prev_bp);
                PUT(FTRP(next_bp), PACK(max_adj_size, BLOCK_ALLOCATED));
                PUT(HDRP(prev_bp), PACK(max_adj_size, BLOCK_ALLOCATED));
                memmove(prev_bp, bp, orig_size - DSIZE);
                return prev_bp;
            }
        }
    }
    return NULL;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // allocate 12 words for free list pointer, prologue block and epilogue block
    if ((heap_listp = mem_sbrk(12 * WSIZE)) == (void *) -1) return -1;

    PUT(heap_listp, NULL);                  // block size <= 32
    PUT(heap_listp + WSIZE, NULL);          // 32 < block size <= 64
    PUT(heap_listp + 2 * WSIZE, NULL);      // 64 < block size <= 128
    PUT(heap_listp + 3 * WSIZE, NULL);      // 128 < block size <= 256
    PUT(heap_listp + 4 * WSIZE, NULL);      // 256 < block size <= 512
    PUT(heap_listp + 5 * WSIZE, NULL);      // 512 < block size <= 1024
    PUT(heap_listp + 6 * WSIZE, NULL);      // 1024 < block size <= 2048
    PUT(heap_listp + 7 * WSIZE, NULL);      // 2048 < block size <= 4096
    PUT(heap_listp + 8 * WSIZE, NULL);      // block size > 4096
    PUT(heap_listp + 9 * WSIZE, PACK(DSIZE, BLOCK_ALLOCATED));  // prologue block header
    PUT(heap_listp + 10 * WSIZE, PACK(DSIZE, BLOCK_ALLOCATED)); // prologue block footer
    PUT(heap_listp + 11 * WSIZE, PACK(0, BLOCK_ALLOCATED));     // epilogue block

    free_list_pointer = heap_listp;
    heap_listp += 10 * WSIZE;

    if (extend_heap(CHUNKSIZE / DSIZE) == NULL) return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) return NULL;

    // adjust block size to include overhead and alignment requirements
    if (size <= DSIZE) asize = 2 * DSIZE;
    // minimum allocated block size is 4 words (alignment requirement)
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {

        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / DSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, BLOCK_FREE));
    PUT(FTRP(bp), PACK(size, BLOCK_FREE));
    PUT(FREE_BLOCK_PRED(bp), NULL);
    PUT(FREE_BLOCK_SUCC(bp), NULL);
    coalesce(bp);
    // coalesce adjacent free blocks, and insert to free list.
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    // if bp is NULL, equivalent to mm_malloc
    if (bp == NULL) return mm_malloc(size);
    // if size is 0, equivalent to mm_free
    if (size == 0) {
        mm_free(bp);
        return NULL;
    }

    size_t asize;

    // adjust block size to include overhead and alignment requirements
    if (size <= DSIZE) asize = 2 * DSIZE;
        // minimum allocated block size is 4 words (alignment requirement)
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);


    size_t orig_size = GET_SIZE(HDRP(bp));

    if (orig_size == asize) return bp;
    if (orig_size > asize) {
        place_realloc(bp, asize);
        return bp;
    }

    char *new_bp = coalesce_realloc(bp, asize);

    if (new_bp != NULL) return new_bp;
    else {
        new_bp = mm_malloc(size);
        memcpy(new_bp, bp, orig_size - DSIZE);
        mm_free(bp);
        return new_bp;
    }
}














