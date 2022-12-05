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

static char *heap_listp;

// 사용하는  함수들 선언
// int mm_init(void);
static void *extend_heap(size_t words);
// void mm_free(void *bp);
static void *coalesce(void *bp);
// void *mm_malloc(size_t size)
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void *first_fit(size_t asize);
// next-fit을 사용하기 위해 이전 bp를 저장하는 전역변수
static void *last_bp;
static void *next_fit(size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
// malloc 초기화
int mm_init(void)
{
    // 비어있는 초기 힙 생성
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1)
        return -1;
    
    // 정렬 패딩
    PUT(heap_listp, 0);
    // 프롤로그 헤더
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    // 프롤로그 풋터
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    //에필로그 헤더
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));

    // 더블워드 정렬을 사용하기 때문에 더블워드 사이즈만큼 증가
    // heap_listp += (2*WSIZE);
    heap_listp += (DSIZE);

    // CHUNKSIZE 사이즈 만큼 힙을 확장해 초기 가용 블록 생성
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    // 초기화시에 last_bp를 heap_listp의 처음으로 잡아줌
    last_bp = heap_listp;

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
    // size를 짝수 word && byte 형태로 만듦
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    // 더블 워드 정렬에 따라 메모리를 mem_sbrk 함수를 이용해 할당 받음
    // mm_sbrk는 힙 용량을 추가로 받아오는 함수
    // 새로운 메모리의 첫 부분을 bp로 둠
    // 주소값은 int로 못받으니 long으로 type casting
    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    /* Initialize free block header/footer and the epologue header */
    // 가용 블록의 헤더와 풋터 그리고 에필로그 헤더 초기화
    // 가용 블록 헤더
    PUT(HDRP(bp), PACK(size, 0));
    // 가용 블록 풋터
    PUT(FTRP(bp), PACK(size, 0));
    // 새로운 에필로그 블록
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* Coalesce if the previous block was free*/
    // 만약 이전 블록이 가용 블록이라면 연결
    return coalesce(bp);
}

/*
 * mm_free - Freeing a block does nothing.
 */
// 블록의 할당을 해제함
void mm_free(void *bp)
{
    // 해당 블록의 size를 알아내 헤더와 풋터의 정보를 수정
    size_t size = GET_SIZE(HDRP(bp));
    
    // 헤더와 풋터를 설정
    // 헤더 갱신
    PUT(HDRP(bp), PACK(size, 0));
    // 풋터 갱신
    PUT(FTRP(bp), PACK(size, 0));

    // 앞, 뒤의 가용 상태 확인후, 연결
    coalesce(bp);
}

/* 
    coalesce - uses boundary-tag coalescing to merge it with any adjacent free bloacks in constant time
*/
// 해당 가용 블록을 앞뒤 가용 블록과 연결하고 연결된 가용 블록의 주소를 리턴
static void *coalesce(void *bp)
{
    // 직전 블록의 풋터, 직후 블록의 헤더를 보고 가용 블록인지 확인
    // 직전 블록 가용 상태 확인
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    // 직후 블록 가용 상태 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 지금 블록의 헤더 사이즈
    size_t size = GET_SIZE(HDRP(bp));

    // case 1
    // 이전과 다음 블록 모두가 할당되어 있는 경우
    if(prev_alloc && next_alloc)
    {
        // 연결한 가용 블록의 bp로 last_bp 수정
        last_bp = bp;

        // 이미 free에서 가용되었으니 여기서는 따로 free할 필요없음
        // 현재 블록만 반환
        return bp;
    }
    // case 2
    // 이전 블록은 할당, 다음 블록은 가용 상태인 경우
    // 현재 블록과 다음 블록을 연결
    else if(prev_alloc && !next_alloc){
        // 다음 블록의 헤더로 다음 블록의 사이즈를 받아와 현재 블록 사이즈에 더함
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // 헤더 갱신
        PUT(HDRP(bp), PACK(size, 0));
        // 풋터 갱신
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case 3
    // 이전 블록은 가용, 다음 블록은 할당 상태인 경우
    else if(!prev_alloc && next_alloc){
        // 이전 블록의 헤더로 사이즈를 받아와 현재 블록 사이즈에 더함
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        // 풋터 갱신
        PUT(FTRP(bp), PACK(size, 0));
        // 이전 블록의 헤더 생긴
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // 현재 블록 포인터를 이전 블록 포인터로 갱신
        bp = PREV_BLKP(bp);
    }
    // case 4
    // 이전 블록과 현재 블록 모두 가용 상태인 경우
    else {
        // 이전 블록과 다음 블록의 헤더로 사이즈를 받아와 현재 블록 사이즈에 더함
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        // 이전 블록의 헤더 갱신
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // 다음 블록의 풋터 갱신
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        // 현재 블록 포인터는 이전 블록 포인터로 갱신
        bp = PREV_BLKP(bp);
    }

    // 연결한 가용 블록의 bp로 last_bp 수정
    last_bp = bp;

    // 위 4개의 case중 한개를 마치고 블록 포인터 리턴
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
    // 기존 malloc 코드
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    /* Adjusted block size */
    // 블록 사이즈 조정
    size_t asize;
    /* Amount to extend heap if  no fit */
    // 힙에 맞는 fit이 없으면 확장하기 위한 사이즈
    size_t extendsize;
    // 블록 포인터
    char *bp;

    /* Ingnore spurious requests */
    // 잘못된 요청 무시
    // 인자로 받은 사이즈가 0이니까 할당할 필요 없음
    if (size == 0)
        return NULL;
    
    /* Adjusted block size to include overhead abd alignment reqs */
    // overhead, 정렬 요청을 포함한 블록 사이즈 조정
    // 사이즈가 정렬의 크기보다 작을때
    if(size <= DSIZE)
        // 블록의 최소크기를 맞추기 위해 사이즈 조정
        // 헤더(4byte) + 풋터(4byte) + 데이터(1byte 이상) = 9byte 이상
        // 8의 배수로 만들어야 하므로 블록의 최소 크기는 16byte
        asize = 2*DSIZE;
    // // 사이즈가 정렬의 크기보다 클때
    else
        // 블록이 가질 수 있는 크기중 최적화된 크기로 재조정
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    // finsd_fit에서 맞는 free 리스트를 찾음
    if((bp = find_fit(asize)) != NULL){
        // 필요하다면 분할하여 할당
        place(bp, asize);
        // place를 마친 블록의 포인터를 리턴
        return bp;
    }

    /* No fit found. Get more memory and place the block*/
    // 만약 맞는 크기의 가용 블록이 없다면, 힙을 확장하여 메모리 할당
    // asize와 CHUNKSIZE(우리가 처음에 세팅한 사이즈)  중 더 큰 값으로 사이즈를 정함
    extendsize = MAX(asize, CHUNKSIZE);
    // extendsize만큼 힙을 확장해 추가적인 가용블록 생성
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    
    // 분할하여 할당
    place(bp, asize);
    // place를 마친 블록 포인터를 리턴
    return bp;
}

// 요청받은 asize에 맞는 가용 블록 탐색
static void *find_fit(size_t asize)
{
    // return first_fit(asize);
    return next_fit(asize);
}

// 메모리의 처음부터 끝까지 검사해서 크기가 충분한 첫번째 메모리 할당
static void *first_fit(size_t asize){
    // bp 선언
    void *bp;

    // init에서 쓴 heap_listp 사용
    // 처음에서 출발하여 헤더의 사이즈가 0이 될때(에필로그 헤더의 크기 = 0)까지 다음 블록으로 넘거가며 탐색
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        // 찾은 블록이 가용 블록이면서 사이즈가 asize보다 클 경우
        // 요청한 asize를 할당할 수 있음
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            // bp 반환
            return bp;
        }
    }
    // 맞는 가용 블록이 없을 경우 NULL 리턴
    return NULL;
}

// 이전 검색이 종료된 지점부터 검사 시작
static void *next_fit(size_t asize){
    void *bp;

        for(bp = last_bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            // 탐색을 끝낸 bp의 위치로 last_bp 변경
            last_bp = bp;
            return bp;
        }
    }

    // 만약 이전 탐색 위치(last_bp)에서 끝까지중 맞는 가용 블록이 없을경우
    // 처음부터 이전 탐색 위치(last_bp)까지 탐색
    for(bp = heap_listp; bp < last_bp; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            last_bp = bp;
            return bp;
        }
    }    

    return NULL;
}

// 들어갈 위치를 포인터로 받음(first_fit에서 찾는 bp, 크기는 asize로 받음)
// 요청한 블록을 가용 블록의 시작 부분에 배치
// 나머지 부분의 크기가 최소 블록 크기와 같거나 큰 경우에만 분할하는 함수
static void place(void *bp, size_t asize)
{
    // 현재 블록 사이즈
    size_t csize = GET_SIZE(HDRP(bp));

    // 현재 블록 사이즈 안에 요청받은 asize를 넣어도
    // 2*DSIZE(헤더와 풋터를 감안한 최소 사이즈) 이상 남는 경우
    if((csize - asize) >= (2*DSIZE)){
        // 헤더위치에 asize만큼 넣고 할당 상태를 1(alloced)로 변경
        PUT(HDRP(bp), PACK(asize, 1));
        // 풋터의 size도 asize로 변경
        PUT(FTRP(bp), PACK(asize, 1));

        // 다음 블록으로의 이동을 위해 bp위치 갱신
        bp = NEXT_BLKP(bp);

        // 나머지 블록(size - asize)은 할당 상태를 0(free)으로 표시
        PUT(HDRP(bp), PACK(csize - asize, 0));
        // 풋터의 표시
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    // size가 asize의 크기와 같음 -> asize만 size에 들어갈 수 있음
    else{
        // 헤더에 asize를 넣고 할당 상태를 1(alloced)로 변경
        PUT(HDRP(bp), PACK(csize, 1));
        // 풋터도 변경
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// 기존 코드
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
// void *mm_realloc(void *ptr, size_t size)
// {
//     // 이전의 포인터를 받아옴
//     void *oldptr = ptr;
//     // 새로운 포인터 선언
//     void *newptr;
//     // 복사할 주소 선언
//     size_t copySize;
    
//     // 받아온 size로 새로운 힙 할당
//     newptr = mm_malloc(size);

//     // 잘못된 할당 요청
//     if (newptr == NULL)
//     // NULL 반환
//       return NULL;

//     // 이전 포인터의 사이즈를 복사
//     // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//     copySize = GET_SIZE(HDRP(oldptr));

//     // 새롭게 할당한 사이즈가 기존의 카피사이즈 보다 작으면
//     if (size < copySize)
//         // 카피사이즈를 사이즈로 변경
//         copySize = size;

//     // 이전 포인터를 새로운 포인터에 카피사이즈만큼 복사
//     memcpy(newptr, oldptr, copySize);

//     // 이전 포인터 free
//     mm_free(oldptr);

//     // 새로운 포인터 반환
//     return newptr;
// }

// 기존 malloc으로 동적 할당된 메모리 크기를 변경시켜줌
// 현재 메모리에 bp가 가르키는 사이즈를 할당한만큼  충분하지 않다면
// 메모리의 다른 공간의 기존 크기의 공간 할당 + 기존에 있던 데이터를 복사한 후 추가로 메모리 할당
void *mm_realloc(void *bp, size_t size){
    // 예전 사이즈 받아옴
    size_t old_size = GET_SIZE(HDRP(bp));
    // 새로운 사이즈 할당(DSIZE는 헤더와 풋터)
    size_t new_size = size + (DSIZE);


    // 만약 새로운 사이즈가 이전 사이즈보다 작거나 같은 경우
    if(new_size <= old_size){
        // 기존의 bp 반환
        return bp;
    }
    // 만약 새로운 사이즈가 이전 사이즈보다 큰 경우
    // 사이즈 변경
    else{
        // 다음 블록의 할당 상태를 받아옴
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
        // 다음 블록의 사이즈를 받아와 예전 사이즈와 합침
        size_t current_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(bp)));

        // 다음 블록이 가용 상태이고, 다음 블록의 사이즈의 합이 새로운 사이즈보다 클 경우
        // 바로 합쳐서 할당
        if(!next_alloc && current_size >= new_size){
            // 할당 상태로 헤더 추가
            PUT(HDRP(bp), PACK(current_size, 1));
            // 할당 상태로 풋터 추가
            PUT(FTRP(bp), PACK(current_size, 1));
            
            // 기존의 bp 반환
            return bp;
        }
        // 새로 블록 만들어서 옮기기
        else{
            // 새로운 사이즈로 new bp 할당
            void *new_bp = mm_malloc(new_size);
            // 새로운 bp에 새로운 사이즈로 가용 블록 분항
            place(new_bp, new_size);
            // 메모리의 특정한 부분으로부터 얼마까지의 다른 메모리 영역으로 복사해주는 함수
            // 이전 bp로부터 사이즈만큼의 문자를 새로운 bp에 복사하라
            memcpy(new_bp, bp, new_size);
            // memmove(new_bp, bp, new_size);

            // 이전 bp free
            mm_free(bp);

            // 새로운 bp 반환
            return new_bp;
        }
    }
}