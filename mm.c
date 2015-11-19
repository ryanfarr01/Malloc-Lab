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


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//Basic constants and macros
#define WSIZE 4
#define DSIZE 8
#define BLOCK 16 //Block size required to fit header (1 word), two pointers (1 word each), and footer (1 word)
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

//Packs a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))

//Read and write a word at address p
#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

//Read the size and allocated fields from address p
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

//Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp) ((char *) (bp) + GET_SIZE(((char *) (bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *) (bp) - GET_SIZE(((char *) (bp) - DSIZE)))

//Address of .next and .prev, respectively. ptr must be header address
#define GET_NEXT(ptr) (*(char **) (ptr + WSIZE))
#define GET_PREV(ptr) (*(char **) (ptr + 2*WSIZE))
#define GET_NEXTP(ptr) ((char *) ptr + WSIZE)
#define GET_PREVP(ptr) ((char *) ptr + 2*WSIZE)


// static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
int mm_check(void);


void* heap_listp;
void* head;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //Create the initial empty heap
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0); //padding so that we have offset for 8-byte alignment
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); //header setting free bit to true
    PUT(heap_listp + (2*WSIZE), (unsigned int)NULL); //next
    PUT(heap_listp + (3*WSIZE), (unsigned int)NULL); //prev
    PUT(heap_listp + BLOCK, PACK(DSIZE, 1)); //footer
    PUT(heap_listp + WSIZE + BLOCK, PACK(0, 1)); //tail

    // heap_listp = heap_listp + WSIZE;
    head = heap_listp + WSIZE; //head points to header

    //extend the empty heap with a free block of CHUNKSIZE bytes
    if(extend_heap(CHUNKSIZE) == NULL)
        return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; //adjusted block size
    size_t extendsize; //amount to extend heap if no fit
    void *bp = head;
    void *dest = NULL;

    void* printPtr = head;
    printf("\nhead -> ");
    while(printPtr != NULL)
    {
        printf("%p(%d) -> " printPtr, GET_SIZE(printPtr));
        printPtr = GET_NEXT(printPtr);
    }
    printf("\n\n");

    //Ignore 0 mallocs
    if(size == 0) { return NULL; }

    //adjust block size to include overhead and alignment requirements
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);


    //Search over list to find 
    while(GET_SIZE(bp) < asize && GET_NEXT(bp) != NULL)
    {
        bp = GET_NEXT(bp);
    }

    if(GET_SIZE(bp) >= asize) { dest = bp + WSIZE; } //bp is currently a header

    if(dest == NULL)
    {
        extendsize = MAX(asize, CHUNKSIZE);
        if((dest = extend_heap(extendsize)) == NULL)
            return NULL;
    }

    //Search the free list for a fit
    // if((bp = find_fit(asize)) != NULL) {
    //     place(bp, asize);
    //     return bp;
    // }

    //No fit found. Get some more memory and place the block
    

    place(dest, asize);
    return dest + WSIZE; //offset back from header
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    void* newPtr = coalesce(HDRP(ptr));

    //place around closest memory
    if(newPtr != head)
    {
        // if(GET_NEXT(head) != NULL) 
        // { 
        //     PUT(GET_PREVP(GET_NEXT(head)), newPtr); 
        //     PUT(GET_NEXTP(newPtr), GET_NEXT(head));
        // }
        // else
        // {
        //     PUT(GET_NEXTP(newPtr), (uint)NULL);
        // }

        // head = newPtr;
    }

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
        mm_free(ptr);
        return ptr;
    }

    newptr = mm_malloc(size);
    if(newptr == NULL) { return NULL; }

    memcpy(newptr, oldptr, MIN(GET_SIZE(HDRP(oldptr)), size));
    mm_free(oldptr);
    return newptr;
}

// static void *find_fit(size_t asize)
// {
//     //First-fit search
//     void *bp;

//     for(bp = head; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
//     {

//     }

//     // void *bp;

//     // for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
//     //     if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
//     //         return bp;
//     //     }
//     // }

//     return NULL;
// }

//ptr should be pointing to a header, not block
static void place(void *ptr, size_t asize)
{
    size_t csize = GET_SIZE(ptr);

    if((csize - asize) >= BLOCK) 
    {
        //split
        //Change current header
        PUT(ptr, PACK(asize, 1));
        PUT(FTRP(ptr + WSIZE), PACK(asize, 1));

        //Set up next header
        void* nextB = NEXT_BLKP(ptr + WSIZE); //Returns a block pointer
        PUT(HDRP(nextB), PACK(csize-asize, 0));
        PUT(FTRP(nextB), PACK(csize-asize, 0));

        //Fix doubly linked list
        void* nextBH = HDRP(nextB);
        if(GET_NEXT(ptr) != NULL) 
        { 
            PUT(GET_PREVP(nextBH), (uint)GET_PREV(ptr)); //n_1.prev = n.prev;
            PUT(GET_NEXTP(GET_PREV(ptr)), (uint)nextBH); //n.prev.next = n_1;
        }
        if(GET_PREV(ptr) != NULL) 
        { 
            PUT(GET_NEXTP(nextBH), (uint)GET_NEXT(ptr)); //n_1.next = n.next
            PUT(GET_PREVP(GET_NEXT(ptr)), (uint)nextBH); //n.next.prev = n_1;
        }
    }
    else
    {
        //Change current header
        PUT(ptr, PACK(csize, 1));
        PUT(FTRP(ptr + WSIZE), PACK(csize, 1));

        //Alter doubly linked list
        if(GET_NEXT(ptr) != NULL) { PUT(GET_PREVP(GET_NEXT(ptr)), (uint)GET_PREV(ptr)); }
        if(GET_PREV(ptr) != NULL) { PUT(GET_NEXTP(GET_PREV(ptr)), (uint)GET_NEXT(ptr)); }
    }

    // size_t csize = GET_SIZE(HDRP(bp));

    // if((csize -asize) >= (2*DSIZE)) {

    //     PUT(HDRP(bp), PACK(asize, 1));
    //     PUT(FTRP(bp), PACK(asize, 1));
    //     bp = NEXT_BLKP(bp);
    //     PUT(HDRP(bp), PACK(csize-asize, 0));
    //     PUT(FTRP(bp), PACK(csize-asize, 0));
    // }
    // else {
    //     PUT(HDRP(bp), PACK(csize, 1));
    //     PUT(FTRP(bp), PACK(csize, 1));
    // }
}

/*
* Combine memory and then free all ties
*/
static void *coalesce(void *ptr)
{
    void* nextH = HDRP(NEXT_BLKP(ptr + WSIZE));
    void* prevH = HDRP(PREV_BLKP(ptr + WSIZE));

    size_t prev_alloc = GET_ALLOC(prevH);
    size_t next_alloc = GET_ALLOC(nextH);
    size_t size = GET_SIZE(ptr);

    if(prev_alloc && next_alloc)  
    { //Case 1
        return ptr; 
    }
    else if(prev_alloc && !next_alloc) 
    { //Case 2
        size += GET_SIZE(nextH);
        PUT(HDRP(ptr + WSIZE), PACK(size, 0));
        PUT(FTRP(ptr + WSIZE), PACK(size, 0));

        remove_node_references(nextH);
    } 
    else if(!prev_alloc && next_alloc) 
    { //Case 3
        size += GET_SIZE(prevH);
        PUT(FTRP(ptr + WSIZE), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr + WSIZE)), PACK(size, 0));
        
        remove_node_references(prevH);

        ptr = PREV_BLKP(ptr + WSIZE);
    }
    else 
    { //Case 4
        size += GET_SIZE(prevH) + GET_SIZE(nextH);
        PUT(HDRP(PREV_BLKP(ptr + WSIZE)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr + WSIZE)), PACK(size, 0));
        
        remove_node_references(prevH);
        remove_node_references(nextH);

        ptr = PREV_BLKP(ptr + WSIZE);
    }

    return ptr;
}

static void remove_node_references(void *ptr)
{
    if(GET_PREV(ptr) != NULL)
    {
        PUT(GET_NEXTP(GET_PREV(ptr)), GET_NEXT(ptr)); //n.prev.next = n.next;
    }
    if(GET_NEXT(ptr) != NULL)
    {
        PUT(GET_PREVP(GET_NEXT(ptr)), GET_PREV(ptr)); //n.next.prev = n.prev;
    }
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    //Allocate an even number of words to maintain alignment
    size = (words%2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp= mem_sbrk(size)) == -1)
        return NULL;

    //Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, 0)); //header
    PUT(bp, (uint)NULL); //next
    PUT(bp + WSIZE, (uint)NULL); //prev
    PUT(FTRP(bp), PACK(size, 0)); //footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); //tail

    void *ptr = HDRP(bp); //Set as header

    //Coalesce if the previous block was free
    ptr = coalesce(ptr);

    //Find place for ptr

    return  coalesce(ptr);
}

int mm_check(void)
{
    return 0;
}