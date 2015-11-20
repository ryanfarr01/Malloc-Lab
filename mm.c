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
#define free_list_size 32

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
static void remove_node_references(void *ptr);
void find_and_place(void * ptr);
int mm_check(void);
int in_free_list(void*);
void print_list(int listIndex, int initial);
int get_list_index(uint size);

void* heap_listp;
// void* head;
void* free_list[free_list_size];

int mallocCalls = 1;
int freeCalls = 1;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //Create the initial empty heap
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0); //padding so that we have offset for 8-byte alignment
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); //header setting free bit to false
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); //footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); //tail

    int i;
    for(i = 0; i < free_list_size; i++) { free_list[i] = NULL; }

    //extend the empty heap with a free block of CHUNKSIZE bytes
    if((extend_heap(CHUNKSIZE) ) == NULL)
        return -1;

    heap_listp += 4*WSIZE;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    printf("\nIn malloc %d with size: %d\n", mallocCalls++, size);
    if(mm_check())
    {
        printf("Broken\n");
        exit(1);
    }

    size_t asize; //adjusted block size
    size_t extendsize; //amount to extend heap if no fit
    void *dest = NULL;

    //Ignore 0 mallocs
    if(size == 0) { return NULL; }

    //adjust block size to include overhead and alignment requirements
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    int listIndex = get_list_index(asize);
    void *bp;// = free_list[get_list_index(asize)]; //head;
    print_list(listIndex, 1);

    //Search over list to find 
    int i;
    for(i = listIndex; i < free_list_size; i++)
    {
        bp = free_list[i];
        while(bp != NULL)
        {
            if(GET_SIZE(bp) >= asize)
            {
                dest = bp;
                break;
            }

            bp = GET_NEXT(bp);
        }
    }

    if(dest == NULL)
    {
        extendsize = MAX(asize, CHUNKSIZE);
        if((dest = extend_heap(extendsize/WSIZE)) == NULL)
            return NULL;
    }

    place(dest, asize);

    print_list(get_list_index(asize), 0);

    return dest + WSIZE; //offset back from header
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    printf("\nIn free %d with pointer: %p\n", freeCalls++, ptr);
    if(mm_check())
    {
        printf("Broken\n");
        exit(1);
    }

    print_list(get_list_index(GET_SIZE(ptr)), 1);

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    void* newPtr = coalesce(HDRP(ptr));

    find_and_place(newPtr);

    print_list(get_list_index(GET_SIZE(ptr)), 0);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    printf("\nIn realloc\n");

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

//ptr should be pointing to a header, not block
static void place(void *ptr, size_t asize)
{
    printf("In place\n");

    int listIndex = get_list_index(asize);
    size_t csize = GET_SIZE(ptr);

    if((csize - asize) >= BLOCK) 
    {
        printf("Splitting\n");
        //split
        //Change current header
        PUT(ptr, PACK(asize, 1));
        PUT(FTRP(ptr + WSIZE), PACK(asize, 1));

        //Set up next header
        void* nextB = NEXT_BLKP(ptr + WSIZE); //Returns a block pointer
        PUT(HDRP(nextB), PACK(csize-asize, 0));
        PUT(FTRP(nextB), PACK(csize-asize, 0));
        PUT(nextB, (uint)NULL); //next
        PUT(nextB + WSIZE, (uint)NULL); //prev

        void* nextBH = HDRP(nextB);

        PUT(GET_PREVP(nextBH), (uint)GET_PREV(ptr)); //n_1.prev = n.prev;
        PUT(GET_NEXTP(nextBH), (uint)GET_NEXT(ptr)); //n_1.next = n.next

        //Fix doubly linked list
        if(GET_NEXT(ptr) != NULL) 
        { 
            printf("Have a next\n");
            PUT(GET_PREVP(GET_NEXT(ptr)), (uint)nextBH); //n.next.prev = n_1;
        }
        if(GET_PREV(ptr) != NULL) 
        { 
            printf("Have a prev\n");
            PUT(GET_NEXTP(GET_PREV(ptr)), (uint)nextBH); //n.prev.next = n_1;
        }
        else //no prev implies it's the head
        {
            printf("Setting head as right side of split\n");
            free_list[listIndex] = nextBH;
        }
        printf("Ended split\n");
    }
    else
    {
        //Change current header
        PUT(ptr, PACK(csize, 1));
        PUT(FTRP(ptr + WSIZE), PACK(csize, 1));

        //Alter doubly linked list
        if(GET_NEXT(ptr) != NULL) { PUT(GET_PREVP(GET_NEXT(ptr)), (uint)GET_PREV(ptr)); }
        if(GET_PREV(ptr) != NULL) { PUT(GET_NEXTP(GET_PREV(ptr)), (uint)GET_NEXT(ptr)); }
        else
        {
            free_list[listIndex] = GET_NEXT(ptr);
        }
    }
}

/*
* Combine memory and then free all ties
*/
static void *coalesce(void *ptr)
{
    printf("In coalesce\n");
    void* nextH = HDRP(NEXT_BLKP(ptr + WSIZE));
    void* prevH = HDRP(PREV_BLKP(ptr + WSIZE));

    size_t prev_alloc = GET_ALLOC(prevH);
    size_t next_alloc = GET_ALLOC(nextH);
    size_t size = GET_SIZE(ptr);
    void* head = free_list[get_list_index(size)];

    if(prev_alloc && next_alloc)  
    { //Case 1
        printf("case 1\n");
    }
    else if(prev_alloc && !next_alloc) 
    { //Case 2
        printf("case 2\n");
        size += GET_SIZE(nextH);
        PUT(ptr, PACK(size, 0));
        PUT(FTRP(ptr + WSIZE), PACK(size, 0));

        if(nextH == head) 
        { 
            head = GET_NEXT(nextH); 
        }

        remove_node_references(nextH);
    } 
    else if(!prev_alloc && next_alloc) 
    { //Case 3
        printf("case 3\n");
        size += GET_SIZE(prevH);

        PUT(HDRP(PREV_BLKP(ptr + WSIZE)), PACK(size, 0));
        PUT(FTRP(ptr + WSIZE), PACK(size, 0));
        
        if(prevH == head) 
        { 
            head = GET_NEXT(prevH); 
        }
        
        remove_node_references(prevH);

        ptr = HDRP(PREV_BLKP(ptr + WSIZE));
        printf("end of case 3\n");
    }
    else 
    { //Case 4
        printf("case 4\n");
        size += GET_SIZE(prevH) + GET_SIZE(nextH);

        PUT(HDRP(PREV_BLKP(ptr + WSIZE)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr + WSIZE)), PACK(size, 0));
        
        while(head == prevH || head == nextH)
        {
            head = GET_NEXT(head);
        }

        remove_node_references(prevH);
        remove_node_references(nextH);

        ptr = HDRP(PREV_BLKP(ptr + WSIZE));
    }

    PUT(GET_NEXTP(ptr), (uint)NULL); //place null for next
    PUT(GET_PREVP(ptr), (uint)NULL); //place null for prev

    return ptr;
}

void find_and_place(void * ptr)
{
    printf("In find_and_place\n");
    void* head = free_list[get_list_index(GET_SIZE(ptr))];
    PUT(GET_NEXTP(ptr), (uint)NULL);
    PUT(GET_PREVP(ptr), (uint)NULL);

    if(head != NULL)
    {
        PUT(GET_PREVP(head), (uint)ptr);
    }
    PUT(GET_NEXTP(ptr), (uint)head);
    head = ptr;
}

static void remove_node_references(void *ptr)
{
    printf("In remove_node_references\n");
    int listIndex = GET_SIZE(ptr);
    if(GET_PREV(ptr) != NULL)
    {
        PUT(GET_NEXTP(GET_PREV(ptr)), (uint)GET_NEXT(ptr)); //n.prev.next = n.next;
    }
    else
    {

        free_list[listIndex] = GET_NEXT(ptr); //no previous implies it was the head
    }
    if(GET_NEXT(ptr) != NULL)
    {
        PUT(GET_PREVP(GET_NEXT(ptr)), (uint)GET_PREV(ptr)); //n.next.prev = n.prev;
    }

    PUT(GET_NEXTP(ptr), (uint)NULL); //replace next
    PUT(GET_PREVP(ptr), (uint)NULL); //replace prev
    printf("Leaving remove_node_references\n");
}

static void *extend_heap(size_t words)
{
    printf("In extend_heap\n");

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
    find_and_place(ptr);

    return  ptr;
}

int get_list_index(uint size)
{
    int index = 0;
    while((size = size >> 1) > 1)
    {
        index++;
    }

    return index;
}

void print_list(int listIndex, int initial)
{
    if(initial) { printf("Initial: "); }
    else {printf("final: ");}

    int i = 0;

    void* head = free_list[listIndex];
    void *printPtr = head;
    printf("%d: head -> ", listIndex);
    while(printPtr != NULL)
    {
        if(i++ > 50) { printf("Infinite head\n"); exit(-1); }
        printf("%p(%d) p: %p -> ", printPtr, GET_SIZE(printPtr), GET_PREV(printPtr));
        printPtr = GET_NEXT(printPtr);
    }
    printf("null\n");
}

int mm_check(void)
{
    void* ptr = heap_listp;

    while(GET_SIZE(ptr) != 0)
    {
        if(GET_ALLOC(ptr) == 0) 
        {
            if(!in_free_list(ptr)) 
            { 
                printf("%p not in free list but is unallocated\n", ptr);
                return 1; 
            }
        }
        else
        {
            if(in_free_list(ptr)) 
            { 
                printf("%p in free list but is allocated\n", ptr);
                return 1; 
            }
        }

        ptr = HDRP(NEXT_BLKP(ptr + WSIZE));
    }

    if(GET_SIZE(ptr) != 0) 
    { 
        printf("broken?\n");
        return 1; 
    }

    return 0;
}

int in_free_list(void* ptr)
{
    void* current;

    int i;
    for(i = 0; i < free_list_size; i++)
    {
        current = free_list[i];

        while(current != NULL)
        {
            if(current == ptr) { return 1; }
            current = GET_NEXT(current);
        }
    }

    return 0;
}