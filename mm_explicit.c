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
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
// 아래 ALIGH(size) 함수에서 할당할 크기인 size를 8의 배수로 맞춰서 할당하기 위한 매크로
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
// 할당할 크기인 size를 보고 8의 배수 크기로 할당하기 위해 size를 다시 align하는 작업을 함
// 만약 size가 4이면 (4+8-1) = 11 = 0000 1011 이고
// 이를 ~0x7 = 1111 1000과 AND 연산하면 0000 1000 = 8이 되어 적당한 8의 배수 크기로 align 할 수 있음
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// 메모리 할당 시 기본적으로 header와 footer를 위해 필요한 더블워드만큼의 메모리 크기
// long형인 size_t의 크기만큼 8을 나타내는 매크로
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


// 기본 상수와 매크로 정의

/*
    기본 단위인 word, double word, 새로 할당받는 힙의 크기 CHUNKSIZE를 정의
*/
// word와 header,footer 사이즈
#define WSIZE 4
// double word size(byte)
#define DSIZE 8
// 힙을 1<<12 만큼 연장 -> 4096byte
#define CHUNKSIZE (1<<12)

// 최댓값을 구하는 함수 매크로
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/*
    header 및 footer 값 (size + allocated) 리턴
*/
#define PACK(size, alloc) ((size) | (alloc))

/*
    주소 p에서의 word를 읽어오거나 쓰는 함수
*/
#define GET(p) (*(unsigned int *)(p))
// 주소 p에 val을 넣어줌
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/*
    header or footer에서 블록의 size, allocated field를 읽어옴
*/
// GET(p)로 읽어오는 주소는 8의 배수와 7을 뒤집은 1111 1000 AND 연산
// 정확히 블록 크기(뒷 세자리는 제외)만 읽어오기 가능
#define GET_SIZE(p) (GET(p) & ~0x7)
// GET(p)로 읽어오는 주소는 8의 배수와 0000 0001과 AND 연산
// 끝자리가 1이면 할당, 0이면 해제
#define GET_ALLOC(p) (GET(p) & 0x1)

/*
    블록 포인터 bp를 인자로 받아 header와 footer의 주소를 반환
*/
// header 포인터 : bp의 주소를 받아서 한 워드 사이즈 앞으로 가면 헤더가 있음
#define HDRP(bp) ((char *)(bp) - WSIZE)
// footer 포인터 : 현재 블록 포인터 주소에서 전체 사이즈만큼 더해주고 맨앞 패딩 + header 만큼 빼줘야 footer를 가리킴
// 전체 사이즈를 알려면 HDRP(bp)로 전체 사이즈를 알아내야함
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*
    블록 포인터 bp를 인자로 받아, 이전 블록의 주소를 리턴
*/
// 현재 블록 포인터에서 전체 블록 사이즈만큼 더하고 헤더 워드 사이즈 하나 빼면 다음 블록 포인터
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
//현재 블록 포인터에서 더블워드만큼 빼면 이전 블록 헤더 -> 다음 블록 footer로 가서 사이즈 읽어내기 가능
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// 사용하는  함수들 선언
// int mm_init(void);
static void *extend_heap(size_t words);
// void mm_free(void *bp);
static void *coalesce(void *bp);
// void *mm_malloc(size_t size)
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static char *heap_listp;

// explicit
// 다음의 가용블록의 위치를 저장하는 포인터를 저장하는 포인터
#define SUCC_P(bp)  (*(void **)(bp))
// 이전의 가용블록의 위치를 저장하는 포인터를 저장하는 포인터
#define PRED_P(bp)  (*(void **)((bp)+WSIZE))
static void list_add(void *bp);
static void list_remove(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */
// malloc 초기화
int mm_init(void)
{
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *) -1)
        return -1;
	
    // Alignment padding
    PUT(heap_listp,0);      
    // Prologue header
    // 헤더,푸터, 가용리스트를 가리키는 포인터 두개가 들어 가야 하므로
    // 총 16byte 할당
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE,1));
    // 이전 가용 리스트 블록을 가리키는 포인터
    PUT(heap_listp + (2*WSIZE), heap_listp+(2*WSIZE));
    // 다음 가용 리스트 블록을 가리키는 포인터
    PUT(heap_listp + (3*WSIZE), heap_listp+(3*WSIZE));
    // Prologue Footer
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE,1));
    // Epilogue header
    PUT(heap_listp + (5*WSIZE), PACK(0,1));
	
    heap_listp += (DSIZE);
	
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

/*
    extends the heap with a free block
*/
// 워드 단위 메모리로 인자를 받아 가용 블록으로 힙 확장
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    /* Initialize free block header/footer and the epologue header */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* Coalesce if the previous block was free*/
    return coalesce(bp);
}

/*
 * mm_free - Freeing a block does nothing.
 */
// 블록의 할당을 해제함
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}

// 루트 바로 뒤(오른쪽)에 새로운 가용 블록을 연결해줌
static void list_add(void *bp){
    SUCC_P(bp) = SUCC_P(heap_listp);
    PRED_P(bp) = heap_listp;
    PRED_P(SUCC_P(heap_listp)) = bp;
    SUCC_P(heap_listp) = bp;
}

// 루트 바로 뒤(오른쪽)에 새로운 가용 블록을 연결해줌
static void list_remove(void *bp){
    PRED_P(SUCC_P(bp)) = PRED_P(bp);
    SUCC_P(PRED_P(bp)) = SUCC_P(bp);
}

/* 
    coalesce - uses boundary-tag coalescing to merge it with any adjacent free bloacks in constant time
*/
// 해당 가용 블록을 앞뒤 가용 블록과 연결하고 연결된 가용 블록의 주소를 리턴
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1
    if(prev_alloc && next_alloc)
    {
        list_add(bp);
        return bp;
    }
    // case 2
    else if(prev_alloc && !next_alloc){
        list_remove(NEXT_BLKP(bp));

	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

	PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case 3
    else if(!prev_alloc && next_alloc){
        list_remove(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	    
        bp = PREV_BLKP(bp);
    }
    // case 4
    else {
	list_remove(PREV_BLKP(bp));
        list_remove(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));

	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

	PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
	    
        bp = PREV_BLKP(bp);
    }

    list_add(bp);

    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// brk 포인터를 증가시켜 블록을 할당함
// 항상 크기가 정렬의 배수로 블록 할당
void *mm_malloc(size_t size)
{
    /* Adjusted block size */
    size_t asize;
    /* Amount to extend heap if  no fit */
    size_t extendsize;
    char *bp;

    /* Ingnore spurious requests */
    if (size == 0)
        return NULL;
    
    /* Adjusted block size to include overhead abd alignment reqs */
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
	    
        return bp;
    }

    /* No fit found. Get more memory and place the block*/
    extendsize = MAX(asize, CHUNKSIZE);

    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    
    place(bp, asize);

    return bp;
}

// Root의 다음 가용블록을 찾아다니면서 인자로 전달받은 사이즈보다 
// 큰 사이즈의 가용블록 사이즈가 존재한다면  해당 주소를 리턴해준다.
static void *find_fit(size_t asize)
{
    char *bp;
	
    for (bp = SUCC_P(heap_listp); !GET_ALLOC(HDRP(bp)); bp = SUCC_P(bp)) 
        if(GET_SIZE(HDRP(bp)) >= asize)
            return bp;
    
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    list_remove(bp);

    if((csize - asize) >= (2*DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));

        list_add(bp);
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
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

    copySize = GET_SIZE(HDRP(oldptr));

    if (size < copySize)
        copySize = size;

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);

    return newptr;
}
