/*
 * mm-naive.c
 * 
 * This implementation uses segregated explicit free lists. That is, there 
 * is an array that contains pointers to the heads of free lists. The array
 * is FREE_LIST_SIZE(32) long, and each entry points to free lists with memory
 * [n^2, n^(2+1)) relative to the previous entry.
 *
 * Each block requires at least 4 words. One word is used for the header,
 * one word for the next pointer, one word for the prev pointer, and one
 * word for the footer. The header and footer are identical and use the first
 * three bits to indicate if the memory is free or allocated (1 is allocated)
 * with the remaining bits indicating the size of the block. Each next and prev
 * pointers point to the headers of the next and prev blocks, respectively.
 * If a block is allocated, then there is no next and prev pointer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

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
#define FREE_LIST_SIZE 32
#define REALLOCATION_SIZE (1 << 9)

//Max and Min of two numbers
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

//Address that stores the pointers to next and prev respectively
#define GET_NEXTP(ptr) ((char *) ptr + WSIZE)
#define GET_PREVP(ptr) ((char *) ptr + 2*WSIZE)

static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void remove_node_references(void *ptr);
void find_and_place(void * ptr);
int mm_check(void);
int in_free_list(void*);
int get_list_index(uint size);
void* find_fit(size_t size);
void** is_head(void* ptr);

void* heap_listp; //Pointer to the very first position. Used for mm_check
void **free_list; //Array of pointers to the different lists

/* 
 * mm_init - Initialize the prologue header and footer as well as the epilogue header.
 *           Additionally, Grab the memory required for the array.
 */
int mm_init(void)
{
    //Allocate the memory for the free list
    free_list = mem_sbrk(FREE_LIST_SIZE * WSIZE);

    //Create the initial empty heap
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0); //padding so that we have offset for 8-byte alignment
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); //header setting allocated bit to true
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); //footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); //epilogue header

    int i;
    //Set every head to NULL initially
    for(i = 0; i < FREE_LIST_SIZE; i++) { free_list[i] = NULL; }

    //extend the empty heap with a free block of CHUNKSIZE bytes
    if((extend_heap(CHUNKSIZE) ) == NULL)
        return -1;

    //Set up the heap_listp so that we can successfully check memory in mm_check
    heap_listp += WSIZE;

    return 0;
}

/* 
 * mm_malloc - Size 0 mallocs are ignored. Allocates a block with at least size bytes and returns
 *             a pointer to the first byte. Simply determines the actual needed size to account for 
 *             a header and footer, then finds the best fit in the currently free memory by going through
 *             the free lists. If we don't have free space, then we extend the heap.
 */
void *mm_malloc(size_t size)
{
    // if(mm_check())
    // {
    //     exit(1);
    // }

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

    //Find the best fit
    dest = find_fit(asize);

    //If NULL is returned, we need to extend the heap
    if(dest == NULL)
    {
        extendsize = MAX(asize, CHUNKSIZE);
        if((dest = extend_heap(extendsize/WSIZE)) == NULL)
            return NULL;
    }

    //Updates the pointers
    place(dest, asize);

    return dest + WSIZE; //offset back from header
}

/*
 * mm_free - Frees the pointer at that address simply by updating the header and footer,
 *           coalescing, and placing in its corresponding free list
 */
void mm_free(void *ptr)
{
    // if(mm_check())
    // {
    //     exit(1);
    // }

    size_t size = GET_SIZE(HDRP(ptr));

    //Update to reflect that this block is now free
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    //See if we can combine with the surrounding memory
    void* newPtr = coalesce(HDRP(ptr));

    //Place in a free list
    find_and_place(newPtr);
}

/*
 * mm_realloc - If the pointer is null, we simply do an allocation. If the size is 0, then we free.
 *              Otherwise, we check to see if the current block is big enough for the request. If it is,
 *              we can simply return the same pointer. If it isn't, we check to see if the next block is free
 *              and if so, combined if they could form enough memory. If so, we coalesce the two blocks and return the
 *              original pointer. If we can't do any of these things, then as a last resort we allocate new memory,
 *              copy the old memory into it, and free the old ptr. When we allocate the new memory we allocate the
 *              requested with an additional buffer. This is because memory that is reallocated once is likely to be
 *              reallocated again.
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t asize; //adjusted size to include header and footer
    void *oldptr = ptr;
    void *newptr;
    void *ptrH = HDRP(ptr); //pointer to the header

    //If ptr is null, we simply allocate
    if(ptr == NULL) { return mm_malloc(size); }
    
    //Free the pointer if the size is 0
    if(size == 0)
    {
        mm_free(ptr);
        return ptr;
    }

    //adjust block size to include overhead and alignment requirements
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    size_t new_size;
    size_t old_size = GET_SIZE(ptrH);
    void* next = HDRP(NEXT_BLKP(ptr));

    //Check to see if we can fit in the current block
    if(old_size >= asize)
    {
        return ptr;
    }

    //If the next block is empty, check and see if it's big enough 
    if(!GET_ALLOC(next))
    {
        new_size = old_size + GET_SIZE(next);
        if(new_size >= asize) //Can simply use the next block for allocation
        {
            //Set the next pointer to allocated and take out of free list
            remove_node_references(next);

            PUT(ptrH, PACK(new_size, 1));
            PUT(FTRP(next + WSIZE), PACK(new_size, 1));

            return ptr;
        }
    }

    //Otherwise we have to allocate new memory
    new_size = asize + REALLOCATION_SIZE;
    newptr = mm_malloc(new_size);
    if(newptr == NULL) { return NULL; }

    //Copy the new memory
    memcpy(newptr, oldptr, MIN(GET_SIZE(HDRP(oldptr)), size));

    //Free the old memory
    mm_free(oldptr);
    return newptr;
}

/*
 * find_fit - Function to search over the free lists to determine if there is
 *            a free block with at least the required size of memory in bytes.
 *            Begins search at the beginning of the list that corresponds to its
 *            memory bracket and moves up. If no block is found, returns NULL
 */
void* find_fit(size_t size)
{
    //Start at its memory bracket
    int listIndex = get_list_index(size);
    void *bp;

    //Search over list to find 
    int i;
    for(i = listIndex; i < FREE_LIST_SIZE; i++)
    {
        //get head of current list
        bp = free_list[i];

        //Go through list until we find a block
        while(bp != NULL)
        {
            if(GET_SIZE(bp) >= size)
            {
                return bp;
            }

            bp = GET_NEXT(bp);
        }
    }

    return NULL;
}

/*
 * place - Takes a pointer pointing to the free block to be allocated and updates
 *         its header, footer, and pointers in its free list. If the block is big
 *         enough to be split, then we split it and re-place the new free block
 */
static void place(void *ptr, size_t asize)
{
    size_t csize = GET_SIZE(ptr);

    //Remove from its free list
    remove_node_references(ptr);

    //If we can split
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
        PUT(nextB, (uint)NULL); //next
        PUT(nextB + WSIZE, (uint)NULL); //prev

        void* nextBH = HDRP(nextB);

        find_and_place(nextBH);
    }
    else
    {
        //Change current header
        PUT(ptr, PACK(csize, 1));
        PUT(FTRP(ptr + WSIZE), PACK(csize, 1));
    }
}

/*
 * coalesce - Function takes a pointer and combines free blocks if it is surrounded in them. Falls into four cases:
 *              1. Block on left and right are both allocated - no changes required
 *              2. Block on right is free while right is allocated - combine right block with current and return the same pointer
 *              3. Block on left is free while right is allocated - combine left block with current and return left block
 *              4. Block is surround in free blocks - combine all three and return left block
 */
static void *coalesce(void *ptr)
{
    //Get the headers of next and previous
    void* nextH = HDRP(NEXT_BLKP(ptr + WSIZE));
    void* prevH = HDRP(PREV_BLKP(ptr + WSIZE));

    //Get teh sizes of all three
    size_t prev_alloc = GET_ALLOC(prevH);
    size_t next_alloc = GET_ALLOC(nextH);
    size_t size = GET_SIZE(ptr);

    if(prev_alloc && next_alloc) { /*Case 1*/ }
    else if(prev_alloc && !next_alloc) 
    { //Case 2
        remove_node_references(nextH);

        size += GET_SIZE(nextH);
        PUT(ptr, PACK(size, 0));
        PUT(FTRP(ptr + WSIZE), PACK(size, 0));
    } 
    else if(!prev_alloc && next_alloc) 
    { //Case 3
        remove_node_references(prevH);
        
        size += GET_SIZE(prevH);

        PUT(HDRP(PREV_BLKP(ptr + WSIZE)), PACK(size, 0));
        PUT(FTRP(ptr + WSIZE), PACK(size, 0));

        ptr = HDRP(PREV_BLKP(ptr + WSIZE));
    }
    else 
    { //Case 4
        remove_node_references(prevH);
        remove_node_references(nextH);

        size += GET_SIZE(prevH) + GET_SIZE(nextH);

        PUT(HDRP(PREV_BLKP(ptr + WSIZE)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr + WSIZE)), PACK(size, 0));

        ptr = HDRP(PREV_BLKP(ptr + WSIZE));
    }

    PUT(GET_NEXTP(ptr), (uint)NULL); //place null for next
    PUT(GET_PREVP(ptr), (uint)NULL); //place null for prev

    return ptr;
}

void** is_head(void* ptr)
{
    void* head;
    int i;
    for(i = 0; i < FREE_LIST_SIZE; i++)
    {
        head = free_list[i];
        if(ptr == head) { return &free_list[i]; }
    }

    return NULL;
}

/*
 * find_and_place - Function that finds the list that the current ptr should be in and places it 
 *                  based on an ascending order of memory addresses. If the head for that list is null,
 *                  we simply place the head as the ptr. Otherwise it will be placed somewhere in the middle
 *                  or at the end.
 */
void find_and_place(void * ptr)
{
    int listIndex = get_list_index(GET_SIZE(ptr));
    void* current = free_list[listIndex];

    if(current == NULL) //Case: empty list
    {
        free_list[listIndex] = ptr;

        return;
    }

    if(ptr < current) //should be first in the list
    {
        PUT(GET_PREVP(current), (uint)ptr); //h.prev = n
        PUT(GET_NEXTP(ptr), (uint)current); //n.next = h;
        free_list[listIndex] = ptr;

        return;
    }

    void* next;
    while((next = GET_NEXT(current)) != NULL) //case: somewhere in the middle
    {
        //Search until the next is greater than the current memory address, indicating it needs to be placed between the current and next
        if(next > ptr)
        {
            //Set the current's next to this ptr
            PUT(GET_NEXTP(current), (uint)ptr);

            //Set the next's prev to this ptr
            PUT(GET_PREVP(next), (uint)ptr);

            //Update this ptr so that its next is next and prev is current. Placing this right between the two blocks
            PUT(GET_PREVP(ptr), (uint)current);
            PUT(GET_NEXTP(ptr), (uint)next);

            return;
        }

        //Otherwise increment next
        current = GET_NEXT(current);
    }

    //Place at the end
    PUT(GET_NEXTP(current), (uint)ptr);
    PUT(GET_PREVP(ptr), (uint)current);
}

static void remove_node_references(void *ptr)
{
    if(GET_PREV(ptr) != NULL)
    {
        PUT(GET_NEXTP(GET_PREV(ptr)), (uint)GET_NEXT(ptr)); //n.prev.next = n.next;
    }
    else
    {
        void** h = is_head(ptr);
        PUT(h, (uint)GET_NEXT(ptr)); //no previous implies it was a head
    }
    if(GET_NEXT(ptr) != NULL)
    {
        PUT(GET_PREVP(GET_NEXT(ptr)), (uint)GET_PREV(ptr)); //n.next.prev = n.prev;
    }

    PUT(GET_NEXTP(ptr), (uint)NULL); //replace next
    PUT(GET_PREVP(ptr), (uint)NULL); //replace prev
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
    find_and_place(ptr);

    return  ptr;
}

/*
 * get_list_index - Determines the index in the free list of the size passed in.
 *                  This is based on the leftmost bit. The index starts at 0 and becomes
 *                  the number of bits before the leftmost 1 value. 
 */
int get_list_index(uint size)
{
    int index = 0; //Start at 0

    //Move the size bit vector to the right until it is only 1
    while((size = size >> 1) > 1)
    {
        //increment the index
        index++;
    }

    return index;
}

/*
 * mm_check - Function that goes through the entire heap, checks that every block that is free
 *            is in a free list, every block that is allocated is not in a free list, and makes sure
 *            that, if it is free, the next and prev pointers are correct by looking at the values 
 *            of the prev and next and making sure they are pointing to the current ptr.
 *
 *            Note: this is only used for debugging. All occurances in the code that is run
 *            should be commented out.
 */
int mm_check(void)
{
    void* ptr = heap_listp;

    //Go until we reach the epilogue header
    while(GET_SIZE(ptr) != 0)
    {
        //IF the current block is unallocated
        if(GET_ALLOC(ptr) == 0) 
        {
            //Make sure it's in the free list
            if(!in_free_list(ptr)) 
            { 
                printf("%p not in free list but is unallocated\n", ptr);
                return 1; 
            }

            //Make sure that its prev is pointing to this ptr
            if(GET_PREV(ptr) != NULL)
            {
                if(GET_NEXT(GET_PREV(ptr)) != ptr)
                {
                    printf("The next's previous isn't this\n");
                    return 1;
                }
            }

            //Make sure that its next is pointing to this ptr
            if(GET_NEXT(ptr) != NULL)
            {
                if(GET_PREV(GET_NEXT(ptr)) != ptr)
                {
                    printf("The prev's next isn't this\n");
                    return 1;
                }
            }
        }

        //Otherwise it's allocated
        else
        {
            //Make sure it isn't in a free list
            if(in_free_list(ptr)) 
            { 
                printf("%p in free list but is allocated\n", ptr);
                return 1; 
            }
        }

        //Increment to the next block
        ptr = HDRP(NEXT_BLKP(ptr + WSIZE));
    }

    return 0;
}

int in_free_list(void* ptr)
{
    void* current;

    int i;
    for(i = 0; i < FREE_LIST_SIZE; i++)
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