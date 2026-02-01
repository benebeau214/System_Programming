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
#include <string.h>
#include <stdint.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    "20221531",
    "Lina Kim",
    "kimrn142@gmail.com",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE 4 //한 word 크기
#define DSIZE 8 // double word 정렬을 위한 사이즈, 헤더+푸터 = 8
#define CHUNKSIZE (1 << 12) // 기본 단위 메모리 크기,  2¹² = 4096bytes

#define MAX(x, y) ((x) > (y) ? (x) : (y)) // 더 큰 값 반환
#define PACK(size, alloc) ((size) | (alloc)) // 크기와 할당 여부를 하나의 unsigned int 값으로 결합
#define GET(p) (*(unsigned int *)(p)) // p가 가리키는 메모리 위치에서 4바이트 값 읽어오기
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p가 가리키는 메모리 위치에서 4바이트 val 값 저장
#define GET_SIZE(p) (GET(p) & ~0x7) // 블록 크기 추출
#define GET_ALLOC(p) (GET(p) & 0x1) // 할당 비트 추출 1이면 할당됨, 0이면 가용 
#define HDRP(bp) ((char *)(bp) - WSIZE) // 블록 포인터로부터 헤더 주소 계산
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 블록 포인터로부터 푸터 주소 계산
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 다음 블록의 payload 포인터 계산
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 이전 블록의 payload 포인터 계산
#define NEXT_FREE(bp) (*(char **)(bp)) // 가용 블록에서 다음 free block 포인터 읽기
#define PREV_FREE(bp) (*((char **)(bp) + 1)) // 가용 블록에서 이전 free block 포인터 읽기

/* heap_listp: 힙의 시작 위치를 가리키는 포인터
   프로로그 블록 이후 첫 유효 블록부터 힙 전체를 순회할 때 기준이 됨*/
static char *heap_listp = 0;

/* free_listp: 명시적 가용 리스트의 첫 번째 free 블록을 가리키는 포인터
   명시적 가용 리스트는 이 포인터를 시작으로 연결된 free 블록들을 탐색함*/
static char *free_listp = 0;

static void *extend_heap(size_t words); // 힙을 주어진 크기만큼 확장
static void *coalesce(void *bp); // 인접한 가용 블록들과 병합
static void insert_free_block(void *bp); // 가용 블록을 free list에 삽입
static void remove_free_block(void *bp); // 가용 블록을 free list에서 제거
static void *find_fit(size_t asize); // 적절한 크기의 가용 블록 탐색
static void place(void *bp, size_t asize); // 블록을 할당하고 필요한 경우 분할


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    // 힙 영역 6워드(24바이트) 할당 (padding + prologue + epilogue 포함)
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1) return -1;

    PUT(heap_listp, 0); // 정렬용 padding
    PUT(heap_listp + (1 * WSIZE), PACK(2 * DSIZE, 1)); // 프로로그 헤더 (16바이트, 할당됨)
    PUT(heap_listp + (2 * WSIZE), 0); // free list prev 포인터
    PUT(heap_listp + (3 * WSIZE), 0); // free list next 포인터
    PUT(heap_listp + (4 * WSIZE), PACK(2 * DSIZE, 1)); // 프로로그 푸터
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1)); // 에필로그 헤더 (크기 0, 할당됨)

    free_listp = heap_listp + 2 * WSIZE; // 가용 리스트 시작 위치 설정

    // 초기 힙 확장 (CHUNKSIZE만큼) 실패하면 return -1
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;

    return 0; // 성공하면 return 0
}


static void *extend_heap(size_t words) {
    char *bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 더블워드 정렬을 위한 짝수 조정
    if ((bp = mem_sbrk(size)) == (void *)-1) return NULL; // 힙 확장 실패 시 return NULL 

    PUT(HDRP(bp), PACK(size, 0)); // 새 free block의 헤더 설정
    PUT(FTRP(bp), PACK(size, 0)); // 새 free block의 푸터 설정
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새로운 에필로그 블록 설정

    return coalesce(bp); // 인접 free 블록과 병합하여 반환
}


static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 할당 여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 할당 여부 확인
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블록 크기

    if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 양쪽 모두 free인 경우
        remove_free_block(PREV_BLKP(bp));
        remove_free_block(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp);
    } else if (!prev_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); // 이전 블록만 free인 경우
        remove_free_block(PREV_BLKP(bp));
        bp = PREV_BLKP(bp);
    } else if (!next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록만 free인 경우
        remove_free_block(NEXT_BLKP(bp));
    }

    PUT(HDRP(bp), PACK(size, 0)); // 병합된 블록의 헤더 설정
    PUT(FTRP(bp), PACK(size, 0)); // 병합된 블록의 푸터 설정
    insert_free_block(bp); // 병합된 블록을 free list에 삽입
    return bp;
}


static void insert_free_block(void *bp) {
    NEXT_FREE(bp) = free_listp; // 현재 블록의 next 포인터를 기존 free_listp로 설정
    PREV_FREE(bp) = NULL; // 현재 블록은 리스트의 맨 앞이므로 prev는 NULL
    if (free_listp) PREV_FREE(free_listp) = bp; // 기존 첫 블록의 prev 포인터를 현재 블록으로 설정
    free_listp = bp; // free_listp를 현재 블록으로 갱신
}


static void remove_free_block(void *bp) {
    if (PREV_FREE(bp)) NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp); // 이전 블록이 있으면 연결을 끊고 다음 블록과 연결
    else free_listp = NEXT_FREE(bp); // 첫 번째 블록이면 free_listp를 다음 블록으로 갱신
    if (NEXT_FREE(bp)) PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp); // 다음 블록이 있으면 그 블록의 prev를 갱신
}



/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize;        // 조정된 블록 크기 (정렬 및 오버헤드 포함)
    size_t extendsize;   // 힙 확장 시 사용할 크기
    char *bp;            // 할당될 블록 포인터

    if (size == 0) return NULL;  // 요청 크기가 0이면 NULL 반환

    // 최소 블록 크기(16바이트) 보장 + 정렬
    asize = (size <= DSIZE) ? 2 * DSIZE : ALIGN(size + DSIZE);

    // 가용 리스트에서 적절한 블록 탐색
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);       // 할당 및 필요 시 분할
        return bp;
    }

    // 적절한 블록이 없으면 힙 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) return NULL;

    place(bp, asize);   // 새로 확장한 영역에 할당
    return bp;
}


static void *find_fit(size_t asize) {
    void *bp = free_listp;             // 가용 리스트의 시작 지점
    void *best_fit = NULL;             // 현재까지 찾은 가장 적합한 블록
    size_t best_size = (size_t)-1;     // 최소 블록 크기를 저장할 변수

    while (bp != NULL) {
        size_t bsize = GET_SIZE(HDRP(bp));  // 현재 블록의 크기

        // 블록이 할당되지 않았고, 요청 크기 이상인 경우
        if (!GET_ALLOC(HDRP(bp)) && bsize >= asize) {
            if (bsize == asize) return bp;  // 크기가 정확히 일치하면 바로 반환
            if (bsize < best_size) {        // 더 작은 블록이면 best 갱신
                best_fit = bp;
                best_size = bsize;
            }
        }
        bp = NEXT_FREE(bp);  // 다음 free 블록으로 이동
    }
    return best_fit;         // 가장 적합한 블록 반환 (또는 NULL)
}


static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp)); // 현재 블록의 전체 크기
    remove_free_block(bp); // 가용 리스트에서 제거

    if ((csize - asize) >= (2 * DSIZE)) { // 분할 가능한 경우
        PUT(HDRP(bp), PACK(asize, 1)); // 앞쪽 블록 할당
        PUT(FTRP(bp), PACK(asize, 1));
        void *next = NEXT_BLKP(bp); // 나머지 공간을 새로운 free block으로 설정
        PUT(HDRP(next), PACK(csize - asize, 0));
        PUT(FTRP(next), PACK(csize - asize, 0));
        insert_free_block(next); // 분할된 블록을 free list에 삽입
    } else {
        PUT(HDRP(bp), PACK(csize, 1)); // 전체 블록을 할당
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp)); // 블록 크기 추출
    PUT(HDRP(bp), PACK(size, 0)); // 헤더를 free 상태로 설정
    PUT(FTRP(bp), PACK(size, 0)); // 푸터를 free 상태로 설정
    coalesce(bp); // 인접 free 블록과 병합 시도
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return mm_malloc(size); // ptr이 NULL이면 malloc 동작
    if (size == 0) { // size가 0이면 free 동작
        mm_free(ptr);
        return NULL;
    }

    size_t oldsize = GET_SIZE(HDRP(ptr)); // 기존 블록 크기
    size_t asize = ALIGN(size + DSIZE); // 새 요청 크기 + 오버헤드 정렬

    if (asize <= oldsize) return ptr; // 기존 공간이 충분하면 그대로 반환

    void *next = NEXT_BLKP(ptr);
    if (!GET_ALLOC(HDRP(next)) && (oldsize + GET_SIZE(HDRP(next))) >= asize) {
        remove_free_block(next); // 다음 블록이 free이고 확장 가능하면 병합
        size_t total = oldsize + GET_SIZE(HDRP(next));
        PUT(HDRP(ptr), PACK(total, 1));
        PUT(FTRP(ptr), PACK(total, 1));
        return ptr;
    }

    void *newptr = mm_malloc(size); // 새 블록 할당
    if (!newptr) return NULL;
    size_t copySize = oldsize - DSIZE; // 데이터 복사 크기 계산
    if (size < copySize) copySize = size;
    memcpy(newptr, ptr, copySize); // 데이터 복사
    mm_free(ptr); // 기존 블록 해제
    return newptr;
}

