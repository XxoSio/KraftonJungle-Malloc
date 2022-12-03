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

/* 
 * mm_init - initialize the malloc package.
 */
// malloc 초기화
int mm_init(void)
{
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// brk 포인터를 증가하여 블록을 할당
// 크기가 항상 정렬의 배수인 블록을 할당함
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
// 블록의 할당을 해제함
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














