/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
    "ateam",
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
    
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, BLOCK_FREE));         // fill header (location at bp - WSIZE)
    PUT(FTRP(bp), PACK(size, BLOCK_FREE));         // fill footer

    PUT(FREE_BLOCK_PRED(bp), NULL);       // set `pred` field in the new free block
    PUT(FREE_BLOCK_SUCC(bp), NULL);       // set `succ` field in the new free block

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header

    return coalesce(bp);
}

static void *get_segregated_free_list_index(size_t size)
{
    unsigned int i = 0;
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

static void *insert_to_free_list(char *bp)
{
    char *root_pointer = get_segregated_free_list_index(GET_SIZE(HDRP(bp)));
    char *prev_free_block = root_pointer;
    char *next_free_block = GET(prev_free_block);

    while (next_free_block != NULL) {
        // find the correct position to insert, right before the first free block whose size is larger than bp.
        // ensure that blocks are sorted by size for each free list
        if (GET_SIZE(HDRP(next_free_block)) > GET_SIZE(HDRP(bp))) break;
        prev_free_block = next_free_block;
        next_free_block = GET(FREE_BLOCK_SUCC(bp));
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
        PUT(FREE_BLOCK_SUCC(bp), prev_free_block);
        PUT(FREE_BLOCK_SUCC(bp), next_free_block);
        if (next_free_block != NULL) PUT(FREE_BLOCK_PRED(next_free_block), bp);
    }
}

static void *remove_from_free_list(char *bp)
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

static void *coalesce_realloc()
{
    // TODO
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

    if (extend_heap(CHUNKSIZE/DSIZE) == NULL) return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














