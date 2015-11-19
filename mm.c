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
 * provide your identifying information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "The I in Team",
    /* First member's full name */
    "Ryan Farr",
    /* First member's UID */
    "u0771896",
    /* Second member's full name (leave blank if none) */
    "Riley Anderson",
    /* Second member's UID (leave blank if none) */
    "u0618652"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define ALIGN2(size) (size%8 == 0 ? size : size + (8 - size%8))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *) (bp) + GET_SIZE(((char *) (bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *) (bp) - GET_SIZE(((char *) (bp) - DSIZE)))


//Our macros
#define GET_SIZE_SIZEP(p) (*(uint *)p)
#define GET_NEXT(p) (*(int *)(p + 4))

void* head;
void* next;
void* heap_listp; //DELETE

int mm_check(void)
{
    return 0;
}

//Returns pointer before the one we need or head or null
static void *find_fit(size_t asize)
{
    void *p = head;

    if(GET_SIZE_SIZEP(p) >= asize) { return p; } //case: head

    while(1)
    {
        if(GET_NEXT(p) == -1) { break; }
        if(GET_SIZE_SIZEP(GET_NEXT(p)) >= asize) { return p; } //case: pointer before one we need

        p = GET_NEXT(p);
    }

    return NULL;

    // void *bp;

    // for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    //     if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
    //         return bp;
    //     }
    // }

    // return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if((csize -asize) >= (2*DSIZE)) {

        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) { return bp; }
    else if(prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words%2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp= mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}


void *PlaceInfo(void *prev, void *current, size_t neededSize)
{
    printf("In PlaceInfo \n");
    size_t currentSize = GET_SIZE_SIZEP(current);

    if(neededSize >= currentSize + 8)
    {
        void* next = current + 4 + neededSize;

        PUT(prev + 4, (uint)next);
        
        PUT(next, currentSize - neededSize - 4);
        PUT(next + 4, (uint)(GET_NEXT(current)));

        printf("placing at address: %p with size: %d and splitting\n", current, neededSize);
        return current + 4;
    }
    else
    {
        PUT(prev + 4, (uint)(GET_NEXT(current)));

        printf("placing at address: %p with size: %d\n", current, neededSize);
        return current + 4;
    }
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    head = mem_sbrk(4*WSIZE) + 4;

    uint size = 4*WSIZE;

    printf("start is: %p\n", head);
    printf("size is: %d\n", size);

    PUT(head, size-4);
    PUT(head + 4, -1);
    printf("working\n");
    printf("size based on head: %d\n", (*(uint*)head));

    printf("\nValue of head: %p, value of next: %d\n", head, GET_NEXT(head));

    // if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
    //     return -1;

    // PUT(heap_listp, 0);
    // PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    // PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    // PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    // heap_listp += (2*WSIZE);

    // //extend the heap
    // if(extend_heap(CHUNKSIZE/WSIZE), == NULL)
    //     return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    void *test = head;
    printf("\nhead -> %p(%d) -> ", head, GET_SIZE_SIZEP(head));
    while((int)GET_NEXT(test) != -1) 
    { 
        printf("%p(%d) -> ", GET_NEXT(test), GET_SIZE_SIZEP(test));
        test = GET_NEXT(test); 
    }
    printf("%d\n", GET_NEXT(test));

    size_t usableSize = ALIGN2(size);
    printf("\nBeginning malloc with requested size: %d\n", usableSize);

    void *p = head; //find_fit(usableSize);

    uint head_size = GET_SIZE_SIZEP(head);
    if(head_size >= usableSize)
    {
        printf("fits in head\n");
        if(head_size >= usableSize + 8)
        {
            printf("splitting head\n");
            head = head + 4 + usableSize;
            PUT(head, head_size - usableSize - 4);
            PUT(head + 4, (uint)(GET_NEXT(p)));

            printf("success in malloc\n");
            return p + 4;
        }
        else
        {
            printf("Not splitting head");
            if(GET_NEXT(head) != -1)
            {
                head = GET_NEXT(head);
                printf("success in malloc\n");

                return p + 4;
            }
            else
            {
                //expand
                printf("Expanding\n");
                size_t extendSize = MAX(usableSize, CHUNKSIZE);
                void *current;
                if((current = mem_sbrk(extendSize) + 4) == NULL)
                    return NULL;

                PUT(current, extendSize - 4);
                PUT(current + 4, -1);

                printf("FInishing with PlaceInfo\n");
                return PlaceInfo(head, current, usableSize);
            }
        }
    }
    else
    {
        printf("Head doesn't fit, going to next\n");
        while(GET_NEXT(p) != -1)
        {
            uint next_size = GET_SIZE_SIZEP(GET_NEXT(p));
            if(next_size >= usableSize)
            {
                printf("Found a fit\n");
                void *temp = GET_NEXT(p);

                printf("Finishing with PlaceInfo\n");
                return PlaceInfo(p, temp, usableSize);
            }

            p = GET_NEXT(p);
        }

        printf("Nothing fit\n");
        //expand

        printf("Expanding\n");
        size_t extendSize = MAX(usableSize, CHUNKSIZE);
        void *current;
        if((current = mem_sbrk(extendSize) + 4) == NULL)
            return NULL;
        
        PUT(current, extendSize - 4);
        PUT(current + 4, -1);
        printf("value of next for current: %d\n", GET_NEXT(current));

        printf("Finishing with PlaceInfo\n");
        return PlaceInfo(p, current, usableSize);
    }

    // size_t asize;
    // size_t extendsize;
    // char *bp;

    // if(size == 0) { return NULL; }

    // if(size <= DSIZE)
    //     asize = 2*DSIZE;
    // else
    //     asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    // if((bp = find_fit(asize)) != NULL) {
    //     place(bp, asize);
    //     return bp;
    // }

    // extendsize = MAX(asize, CHUNKSIZE);
    // if((bp = extend_heap(extendsize/WSIZE)) == NULL)
    //     return NULL;

    // place(bp, asize);
    // return bp;
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    printf("\nIn free\n");
    size_t size = GET_SIZE_SIZEP(ptr - 4);
    PUT(ptr, GET_NEXT(head));
    head = ptr - 4;

    printf("freed successfully\n\n");
    // size_t size = GET_SIZE(HDRP(ptr));

    // PUT(HDRP(ptr), PACK(size, 0));
    // PUT(FTRP(ptr), PACK(size, 0));
    // coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;

    if(ptr == NULL) { return mm_malloc(size); }
    if(size == 0)
    {
        mm_free(size);
        return ptr;
    }

    newptr = mm_malloc(size);
    if(newptr == NULL) { return NULL; }

    memcpy(newptr, oldptr, MIN(GET_SIZE(HDRP(oldptr)), size));
    mm_free(oldptr);
    return newptr;
}














