/*
 * mm.c
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"


/* word, 8 bytes */
#define WSIZE 8
#define DSIZE 16
#define FSIZE 32
/* the thresh to index blocks in BST */
#define THRESH 4
/* extend heap by this amount */
#define CHUNKSIZE (1<<12)

/* pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size)|(alloc))

/* read and write a word at address p */
#define GET(p) (*(unsigned long long *)(p))
#define PUT(p, val) (*(unsigned long long *)(p)=((unsigned long long)(val)))
#define HDRP(p) ((void *)(p) - WSIZE)
#define FTRP(p) ((void *)(p) + GET_SIZE(p) - DSIZE)
#define GET_SIZE(p) ((GET(HDRP(p)))&~0x7)
#define GET_ALLOC(p) ((GET(HDRP(p)))&0x1)
#define GET_NEXT(p) ((void *)(GET(p+WSIZE)))
#define GET_PREV(p) ((void *)(GET(p)))
#define SET_NEXT(p, val) (PUT(p+WSIZE, val))
#define SET_PREV(p, val) (PUT(p, val))
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE((void *)(bp)))
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE((void *)(bp) - WSIZE))



/* max of two number */
#define MAX(x, y) ( (x)>(y)? (x): (y) )

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)


static void *coalesce(void *bp);
static void *extend_heap(size_t size);
static void *find_fit(size_t size);
static void *place(void *bp, size_t size);
static void coalesce_next(void *bp);
void *heap_listp;
void *free_listp;

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    /* create the initial empty heap */

    if((heap_listp = mem_sbrk(7 * WSIZE)) == (void *)-1)
        return -1;
	PUT(heap_listp, 0);   //list starter.prev
    PUT(heap_listp + WSIZE * 1, heap_listp+ WSIZE*2);   //list starter.next
    PUT(heap_listp + 2 * WSIZE, heap_listp);   //list ender.prev
    PUT(heap_listp + (3 * WSIZE), 0);  //list ender.next
	PUT(heap_listp + (4 * WSIZE), PACK(DSIZE, 1));  //prologue hdr
	PUT(heap_listp + (5 * WSIZE), PACK(DSIZE, 1));  //prologue ftr
	PUT(heap_listp + (6 * WSIZE), PACK(0, 1));  //epilogue hdr
	free_listp = heap_listp;
	heap_listp += 5*WSIZE;	
	/* extend the empty heap with a free block of CHUCKSIZE bytes */
    if(extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;
    return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    size_t asize; /* adjusted block size */
    void *bp;
    size_t extendsize;
//printf("malloc, %d\n",(int)size);
    /* ignore spurious requests */
    if(size <= 0)
        return NULL;
if (size<DSIZE){
size = DSIZE;
}
    /* adjust block size to include meta-data in head/tail */
    asize = ALIGN(size + DSIZE);
//mm_checkheap(11);    
/* search the free list for a fit */
	if ((bp = find_fit(asize)) != NULL){
//mm_checkheap(12);
		place(bp, asize);
//mm_checkheap(13);
		return bp;
	}
	/* not fit found, get more memory and place the block */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
//printf("start place\n");
	place(bp, asize);
//mm_checkheap(12);
//printf("finish place: %p\n", bp);
//mm_checkheap(0);	
//printf("hi:%p\n",mem_heap_hi());
return bp;
}

/*
 * extend heap
 */
void *extend_heap(size_t words){
    void *bp;
    size_t size;
//printf("extend\n");	
    /* allocate an even number of words to maintain alignment */
    size = words*WSIZE;
    if((bp = mem_sbrk(size)) == (void *)-1) return NULL;

    
	/* initialize free block hdr/ftr */
    PUT(HDRP(bp), PACK(size, 0)); //Free block hdr
    PUT(FTRP(bp), PACK(size, 0)); //Free block ftr
    SET_NEXT(bp, GET_NEXT(free_listp));
	SET_PREV(bp, free_listp);
	SET_NEXT(free_listp, bp);
	SET_PREV(GET_NEXT(bp), bp);
//mm_checkheap(3);	
	/* re-init epilogue hdr */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
//mm_checkheap(3);
    /* coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * find fit in explicit list
 */
void *find_fit(size_t size){
	void *bp = GET_NEXT(free_listp);
	while(1){
		if (GET_NEXT(bp) == 0){
			return NULL;
		}
		if (GET_SIZE(bp) >= size){
			break;
		}
		bp = GET_NEXT(bp);
	}
	return bp;
}

/*
 * coalesce
 */
void *coalesce(void *bp){
    size_t prev_alloc = GET(FTRP(PREV_BLKP(bp))) & 0x1;
    size_t next_alloc = GET_ALLOC(NEXT_BLKP(bp));
//printf("%p %p %p %d\n", bp, PREV_BLKP(bp),NEXT_BLKP(bp), (int)(prev_alloc+next_alloc));
    if(prev_alloc && next_alloc){
        /* prev/next blocks are not free */
    }else if(prev_alloc && !next_alloc){
		/* next block is free */
        coalesce_next(bp);
    }else if(!prev_alloc && next_alloc){
		/* prev block is free */
		bp = PREV_BLKP(bp);
//printf("@@@@@@@@@@@@@@%p\n", bp);
		coalesce_next(bp);
    }else{
        coalesce_next(bp);
		bp = PREV_BLKP(bp);
		coalesce_next(bp);
    }
//printf("coal finish\n");
//mm_checkheap(20);
    return bp;
}

/*
 * coalesce next block with current block
 */
void coalesce_next(void *bp){
    size_t size = GET_SIZE(bp);
    void *next_bp = NEXT_BLKP(bp);	
    SET_NEXT(GET_PREV(next_bp), GET_NEXT(next_bp));
    SET_PREV(GET_NEXT(next_bp), GET_PREV(next_bp));
    size += GET_SIZE(NEXT_BLKP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
}

/*
 * place block
 */
void *place(void *bp, size_t size){
	void *free_bp;
	size_t full_size = GET_SIZE(bp);
//mm_checkheap(44444);
	if ((full_size - size) < FSIZE || size < FSIZE ){
		/* not split the free block for small allocation or small space left */
		PUT(HDRP(bp), PACK(full_size, 1));
		PUT(FTRP(bp), PACK(full_size, 1));
		SET_NEXT(GET_PREV(bp), GET_NEXT(bp));
		SET_PREV(GET_NEXT(bp), GET_PREV(bp));
		return bp;
	}
	/* split the free block, build the new free block */
	free_bp = bp + size;
	PUT(HDRP(free_bp), PACK(full_size-size, 0));
	PUT(FTRP(free_bp), PACK(full_size-size, 0));
	SET_NEXT(free_bp, GET_NEXT(bp));
	SET_PREV(free_bp, GET_PREV(bp));
	SET_NEXT(GET_PREV(bp), free_bp);
	SET_PREV(GET_NEXT(bp), free_bp);
	PUT(HDRP(bp), PACK(size, 1));
	PUT(FTRP(bp), PACK(size, 1));
	return bp;
}


/*
 * free
 */
void free (void *ptr) {
	size_t size;
//printf("free,%p\n",ptr);
if (ptr == NULL)return;
size = GET_SIZE(ptr);
//printf("free,%p,%d\n",ptr,(int)GET_SIZE(ptr));
//mm_checkheap(33333);
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    SET_NEXT(ptr, GET_NEXT(free_listp));
    SET_PREV(ptr, free_listp);
    SET_PREV(GET_NEXT(ptr), ptr);
    SET_NEXT(free_listp, ptr);    
    coalesce(ptr);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
	size_t asize;
    size_t oldsize;
    void *newptr;
	void *free_bp;
//printf("re\n");
//mm_checkheap(1122);
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
		free(oldptr);
		return 0;
    }
	
	/* If oldptr is NULL, then this is just malloc. */
    if(oldptr == NULL) {
		return malloc(size);
    }
	
	asize = ALIGN(size + 2*WSIZE);
	oldsize = GET_SIZE(oldptr);
	if (oldsize < asize){
		
		newptr = malloc(size);
		memcpy(newptr, oldptr, oldsize - 2*WSIZE);
        free(oldptr);
        return newptr;
		
	}else if (oldsize > asize){
		/* realloc a smaller space */
		if ((oldsize - asize) < FSIZE || asize < FSIZE ){
			/* small space, no changes */
			return oldptr;
		}else{
//mm_checkheap(111);
			free_bp = oldptr + asize;
			PUT(HDRP(free_bp), PACK(oldsize-asize, 0));
			PUT(FTRP(free_bp), PACK(oldsize-asize, 0));
			SET_NEXT(free_bp, GET_NEXT(free_listp));
			SET_PREV(free_bp, free_listp);
			SET_NEXT(free_listp, free_bp);
			SET_PREV(GET_NEXT(free_bp), free_bp);
			
			PUT(HDRP(oldptr), PACK(asize, 1));
			PUT(FTRP(oldptr), PACK(asize, 1));
			return oldptr;
		}
	}else{
		/* same size, no changes */
		return oldptr;
	}
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
	size_t bytes = nmemb * size;
	void *newptr;
//printf("call\n");
	newptr = malloc(bytes);
	memset(newptr, 0, bytes);

	return newptr;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int lineno) {
	
	void *bp = heap_listp;
	int last_free = 0;
	int count = 0;
//	printf("AT line %d we run test!\n", lineno);
	/* check prologue */
	if (GET(bp) != PACK(DSIZE, 1)){
		printf("wrong prologue!%d\n",lineno);
	}
	
	while (1){
		if (GET_SIZE(bp) == 0 && GET_ALLOC(bp) == 1){
			/* reach epilogue */
			//printf("epilogue %p\n", bp);
			//printf("last byte %p\n", mem_heap_hi());
			if (bp-1 != mem_heap_hi()){
				printf("wrong epilogue,%d\n",lineno);
			}
			break;
		}else{
//printf("hdr:%p,ftr:%p,value:%p\n",HDRP(bp),FTRP(bp),(void *)GET(HDRP(bp)));
			/* check aligned */
			if (!aligned(bp)){
				printf("not aligned, bp = %d!\n", lineno);
			}
			/* check ftr == hdr */
			if (GET(HDRP(bp)) != GET(FTRP(bp))){
				printf("hdr != ftr, bp = %d!\n", lineno);
			}
			/* check no consecutive free block */
			if (last_free && GET_ALLOC(bp) == 0){
				printf("consecutive free blocks, bp = %d!\n", lineno);
			}
			last_free = !GET_ALLOC(bp);
			count += last_free;
			bp = NEXT_BLKP(bp);
		}
	}
	
	/* check blocks in explicit free list are all free */
	bp = free_listp;
	while(1){
		bp = GET_NEXT(bp);
		/* check if reach end */
		if (GET_NEXT(bp) == 0){
			break;
		}else{
			/* check free */
			if (GET_ALLOC(bp)){
				printf("allocated block in free list! line = %d!\n", lineno);
			}
			/* check cur.next.prev == cur and cur.prev.next == cur */
			if (GET_PREV(GET_NEXT(bp)) != bp || GET_NEXT(GET_PREV(bp)) != bp){
				printf("inconsistent pointers! bp = %d!\n", lineno);
			}
			if (!in_heap(bp)){
				printf("not in heap!%d\n", lineno);
			}
			count--;
		}
	}
	/* check free block numbers are the same through two traversing methods */
	if (count){
		printf("inconsistent free block numbers in two checks!\n");
	}
//	printf("AT line %d the test finished!\n", lineno);
	
}
