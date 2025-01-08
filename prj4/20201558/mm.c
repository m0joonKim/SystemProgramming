#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


#define WSIZE 4
#define DSIZE 8
#define MAXSEGLIST 20
#define SMALLESTSIZE 16
#define CHUNKSIZE  (1<<12)
#define MAX(x, y) ((x) > (y)? (x) : (y))  
#define MIN(x, y) ((x) < (y)? (x) : (y))  

#define PACK(size, alloc)  ((size) | (alloc))
//Prev Alloc & Next Alloc 여부 저장.
// #define NPPACK(pack, npAlloc) ((pack)|(npAlloc<<1)) //이전/다음블럭 alloc여부를 땡겨와서, shift left 1 -> lsb 2번째에 저장(header/footer)


#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
// #define GET_NPALLOC(p) (GET(p) & 0x2)//next나 prev의 alloc 여부

#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


#define PREV_FN(bp) (*(char **)((bp)))
#define NEXT_FN(bp) (*(char **)((bp) + WSIZE))

// static char *heap_listp = 0;

static void **segFList;

team_t team = {
    /* Your student ID */
    "20201558",
    /* Your full name*/
    "MyoungJoon Kim",
    /* Your email address */
    "audwns0820@sogang.ac.kr",
};

#define ALIGNMENT 8

static void *extend_heap(size_t words);
static void *place(void *bp, size_t asize);
static void *coalesce(void *bp);
// static void printblock(void *bp);
// static void checkheap(int verbose);
// static void checkblock(void *bp);
static void isrtSList(void* bp, size_t size);
static void delList(void* bp);
void *findSuitablePtr(size_t size);
void initSegFList();

// static void checkList(){
//     for(int i=0;i<MAXSEGLIST;i++){
//         void* now=segFList[i];
//         while(now!=NULL){
//             checkblock(now);
//             now = NEXT_FN(now);
//         }
//     }
// }//테스트코드

static void isrtSList(void* bp, size_t size){

    //free List에 넣는거니까, 
    //할당되었던걸 풀면서 올바른 자리에 넣는거임
    int Idx = 0;
    ssize_t tmpSize=SMALLESTSIZE;
    //해당 인덱스에는, 최대 tmpSize까지 저장되는거지
    //즉, size가 tmpSize보다 크다면 다음걸 가야해
    //작거나 같아지면 거기에 넣으면됨
    for(;Idx<MAXSEGLIST-1&&size>tmpSize;Idx++, tmpSize*=2);//원하는 인덱스 찾기
    void* nxNode = segFList[Idx];
    void* pvNode = NULL;
    //old와 new 사이에 넣겠다.

    //while(((nxNode!=NULL)&&((unsigned long)bp>(unsigned long)nxNode))){//얘는 주소순인데 쓰루풋 차이없길래 걍 크기순
    while((nxNode!=NULL)&&(GET_SIZE(HDRP(bp))>(GET_SIZE(HDRP(nxNode))))){
        pvNode = nxNode;
        nxNode = NEXT_FN(nxNode);//크기순으로 넣으면, first fit이 best fit->쓰루풋굿
    }

    if(pvNode==NULL&&nxNode!=NULL){
        //앞은 없고 뒤는 있다
        //맨 앞에 넣어라
        segFList[Idx] = bp;
        PREV_FN(bp) = NULL;
        NEXT_FN(bp) = nxNode;
        PREV_FN(nxNode) = bp;
    }else if(pvNode!=NULL&&nxNode==NULL){
        //앞은 있고 뒤는 있다
        //맨 뒤에 넣어라
        NEXT_FN(pvNode) = bp;
        PREV_FN(bp) = pvNode;
        NEXT_FN(bp) = NULL;
    }else if(pvNode!=NULL&&nxNode!=NULL){
        //prev - 나 - next 순서
        PREV_FN(nxNode) = bp;
        NEXT_FN(pvNode) = bp;
        PREV_FN(bp) = pvNode;
        NEXT_FN(bp) = nxNode;
    }else if(pvNode==NULL&&nxNode==NULL){
        segFList[Idx] = bp;
        PREV_FN(bp) = NULL;
        NEXT_FN(bp) = NULL;
    }
    return ;
}
static void delList(void* bp){
    int Idx = 0;
    ssize_t nowSize = GET_SIZE(HDRP(bp));
    ssize_t tmpSize = SMALLESTSIZE;//시작 사이즈
    void* nxBp = NEXT_FN(bp);
    void* pvBp = PREV_FN(bp);
    for(;Idx<MAXSEGLIST-1&&nowSize>tmpSize;Idx++, tmpSize*=2);//원하는 인덱스 찾기

    if(pvBp==NULL){
        if(nxBp==NULL){
            segFList[Idx] = NULL;
        }else{
            PREV_FN(NEXT_FN(bp)) = NULL;
            segFList[Idx] = NEXT_FN(bp);
        }
    }else{
        if(nxBp==NULL){
            NEXT_FN(PREV_FN(bp)) = NULL;
        }else{
            NEXT_FN(PREV_FN(bp)) = NEXT_FN(bp);
            PREV_FN(NEXT_FN(bp)) = PREV_FN(bp);
        }
    }


}

int mm_init(void) 
{   
    void* localListp;//어차피 free를 링크드 리스트로 전역변수에 넣을거니까
    //listp를 전역으로 사용할 필요가 없을듯함.

    if ((localListp = mem_sbrk((4+MAXSEGLIST)*WSIZE))==(void *)-1){
        return -1;
    }
    //증가는 했고 Listp가 과거값
    segFList = localListp;
    PUT(localListp + MAXSEGLIST*WSIZE, 0);     // 0 pl LIST0 ~ LIST19 pl epi
    PUT(localListp + ((1+MAXSEGLIST)*WSIZE), PACK(DSIZE, 1)); 
    PUT(localListp + ((2+MAXSEGLIST)*WSIZE), PACK(DSIZE, 1));
    PUT(localListp + ((3+MAXSEGLIST)*WSIZE), PACK(0, 1));
   // mm_checkheap(1);

    initSegFList();
    // localListp += ((2+MAXSEGLIST)*WSIZE);
    if(extend_heap(4)==NULL)return -1;

    if(extend_heap(CHUNKSIZE/(WSIZE/2))==NULL){
        return -1;
    }
  //  mm_checkheap(1);

    return 0;
}
void *findSuitablePtr(size_t size){
    void* fitBlock;
    int Idx=0;
    ssize_t tmpSize = SMALLESTSIZE;
    for(Idx=0;Idx<MAXSEGLIST-1;Idx++,tmpSize*=2){
        if(size<=tmpSize&&segFList[Idx]!=NULL){//충분히 들어갈만한 리스트이고, 내용이 있다면
            fitBlock = segFList[Idx];
            while((fitBlock!=NULL)&&(size>GET_SIZE(HDRP(fitBlock)))){
                fitBlock = NEXT_FN(fitBlock);
            }
            if(fitBlock!=NULL)return fitBlock;
        }
    }
    if(fitBlock==NULL){//아직 못찾았으면 마지막블럭은 무제한급이니까 tmpSize무관하게 넣어도됨
        fitBlock = segFList[MAXSEGLIST-1];
        while((fitBlock!=NULL)&&(size>GET_SIZE(HDRP(fitBlock)))){
            fitBlock = NEXT_FN(fitBlock);
        }
        if(fitBlock!=NULL)return fitBlock;
    }
    return NULL;//그래도 null이면 null -> 넣을자리가없다 ㅇㅇ
}
void *mm_malloc(size_t size) 
{   
    if(size==0)return NULL;//0이면 어차피 널
    ssize_t mSize;
    if(size<=DSIZE){
        mSize = 2*DSIZE;
    }else{
        mSize = DSIZE * ((size + DSIZE + (DSIZE - 1))/DSIZE);
    }//말록사이즈 정해놓기
    void* bp = findSuitablePtr(mSize);
    if(bp==NULL){
        if((bp = extend_heap(mSize))==NULL){
            return NULL;
        }
        bp = findSuitablePtr(mSize);
    }
    bp = place(bp,mSize);
    // mm_checkheap();
    return bp;
}
void initSegFList(){
    int Idx;
    for(Idx = 0; Idx<MAXSEGLIST; Idx++){
        segFList[Idx]=NULL;
    }
}
void mm_free(void *bp)
{
  //  mm_checkheap(1);
    if(bp==0)return;
    ssize_t s = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp),PACK(s,0));
    PUT(FTRP(bp),PACK(s,0));
    isrtSList(bp,s);
    coalesce(bp);
    // mm_checkheap(1);

    return ;
}
static void *coalesce(void *bp) 
{//내가 받은 포인터 봐서, 앞뒤로 합칠수있으면 합쳐야하는거
    ssize_t size = GET_SIZE(HDRP(bp));
    if(GET_ALLOC(HDRP(PREV_BLKP(bp)))){
        if(!GET_ALLOC(HDRP(NEXT_BLKP(bp)))){
            delList(bp);
            delList(NEXT_BLKP(bp));
            //둘다 죽이고
            size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
            PUT(HDRP(bp),PACK(size,0));
            PUT(FTRP(bp),PACK(size,0));
        }else{
            return bp;
        }
    }else{//앞이 있는놈들ㅇㅇ
        if(!GET_ALLOC(HDRP(NEXT_BLKP(bp)))){
            delList(PREV_BLKP(bp));
            delList(bp);
            delList(NEXT_BLKP(bp));
            //둘다 죽이고
            size += (GET_SIZE(HDRP(PREV_BLKP(bp)))+GET_SIZE(HDRP(NEXT_BLKP(bp))));
            PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
            PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        }else{
            delList(bp);
            delList(PREV_BLKP(bp));
            //둘다 죽이고
            size += GET_SIZE(HDRP(PREV_BLKP(bp)));
            PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
            PUT(FTRP(bp),PACK(size,0));
        }
        bp = PREV_BLKP(bp);
    }
    isrtSList(bp, size);

    return bp;
}
void *mm_realloc(void *ptr, size_t size)
{
    if(size==0){
        mm_free(ptr);
        return NULL;
    }
    if(ptr==NULL){
        return mm_malloc(size);
    }
    ssize_t mSize;
    if(size<=DSIZE){
        mSize = 2*DSIZE;
    }else{
        mSize = (DSIZE)*((size+DSIZE+DSIZE+DSIZE-1)/DSIZE);
    }//말록사이즈 정해놓기
    ssize_t cSize = GET_SIZE(HDRP(ptr));
    ssize_t rmSize;
    void* rePtr = ptr;
    int nextAlloc;
    // int prevAlloc;
    ssize_t nextSize;
    // ssize_t prevSize;
    nextAlloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    nextSize = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    // prevAlloc = GET_ALLOC(HDRP(PREV_BLKP(ptr)));
    // prevSize = GET_SIZE(HDRP(PREV_BLKP(ptr)));
    if(mSize>GET_SIZE(HDRP(ptr))){//할당 새로해야되면
        // if(!prevAlloc&&prevSize+cSize>=mSize&&prevSize>8){
        //     //앞에랑 나랑 해서 이미 충분한경우
        //     delList(PREV_BLKP(ptr));
        //     PUT(HDRP(PREV_BLKP(ptr)),PACK(prevSize+cSize,1));
        //     PUT(FTRP(PREV_BLKP(ptr)),PACK(prevSize+cSize,1));
        //     memcpy(PREV_BLKP(ptr), ptr, MIN(cSize, mSize));
        // }
        // else //왜 갑자기 세그폴트뜨는지 ? 
        if(!nextAlloc||nextSize==0){//다음이랑합치기
        //다음이 할당 X || 마지막블럭
            rmSize = mSize - (nextSize + cSize);//쓸거 - 가용
            if((long long)rmSize > 0){//쓸게 쓸수있는거보다 많으면 ㅇㅇ
                if(extend_heap(rmSize)==NULL)return NULL;
                nextSize = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
            }
            delList(NEXT_BLKP(ptr));
            PUT(HDRP(ptr), PACK(nextSize + cSize,1));
            PUT(FTRP(ptr), PACK(nextSize + cSize,1));
            
        }
        else{
            rePtr = mm_malloc(mSize - DSIZE);//여기이미헤더푸터포함이니까
            if(rePtr==NULL)return NULL;
            memcpy(rePtr, ptr, MIN(mSize, cSize));
            mm_free(ptr);
        }
    }//이미 크면 걍 하지말자
    return rePtr;
}


static void *extend_heap(size_t words) 
{   
    char *bp;
    ssize_t size;
    size = ( words % 2 ) ? ((words+1) * WSIZE) : (words * WSIZE);
    if((long)(bp = mem_sbrk(size)) == -1 )return NULL;
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));
    isrtSList(bp, size);
    void *ret = coalesce(bp);
    return ret;
}
static void *place(void *bp, size_t asize)
{
    ssize_t freeSize = GET_SIZE(HDRP(bp));
    delList(bp);
    //free사이즈는 프리블럭사이즈, asize는 할당시킬 사이즈
    if(freeSize - asize <= 2*DSIZE){//남는게 최소블럭보다 작다면 그냥 써버리기
    //printf("0   %u - %u\n",freeSize,asize);
        PUT(HDRP(bp),PACK(freeSize, 1));
        PUT(FTRP(bp),PACK(freeSize, 1));
    }else if(freeSize - asize > 16 &&freeSize - asize <= 240){//78번 저격?코드.
    //8번이 16 112만 할당 프리 할당 프리 하는데 유독 util낮게나옴..
    //이거 넣으면 7~두 문항에 대한 평균 util이 50퍼대에서 80퍼대로 오름
        PUT(HDRP(bp),PACK(freeSize-asize,0));
        PUT(FTRP(bp),PACK(freeSize-asize,0));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
        isrtSList(bp, freeSize-asize);
        return NEXT_BLKP(bp);
    }        
    else{ 
   // printf("1   %u - %u\n",freeSize,asize);
        PUT(HDRP(bp),PACK(asize, 1));
        PUT(FTRP(bp),PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(freeSize-asize,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(freeSize-asize,0));
        isrtSList(NEXT_BLKP(bp), freeSize-asize);
        
    }

    return bp;
}

// /*디버깅용 코드*/

// void mm_checkheap(int verbose)  
// { 
//     checkheap(verbose);
// }
// static void printblock(void *bp) 
// {
//     size_t hsize, halloc, fsize, falloc;

//     checkheap(0);
//     hsize = GET_SIZE(HDRP(bp));
//     halloc = GET_ALLOC(HDRP(bp));  
//     fsize = GET_SIZE(FTRP(bp));
//     falloc = GET_ALLOC(FTRP(bp));  

//     if (hsize == 0) {
//         printf("%p: EOL\n", bp);
//         return;
//     }

//     printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp, 
//            hsize, (halloc ? 'a' : 'f'), 
//            fsize, (falloc ? 'a' : 'f')); 
// }

// static void checkblock(void *bp) 
// {
//     if ((size_t)bp % 8)
//         printf("Error: %p is not doubleword aligned\n", bp);
//     if (GET(HDRP(bp)) != GET(FTRP(bp)))
//         printf("Error: header does not match footer\n");
// }

/* 
 * checkheap - Minimal check of the heap for consistency 
//  */
// void checkheap(int verbose) 
// {
//     char *bp = heap_listp;

//     if (verbose)
//         printf("Heap (%p):\n", heap_listp);

//     if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
//         printf("Bad prologue header\n");
//     checkblock(heap_listp);

//     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
//         if (verbose) 
//             printblock(bp);
//         checkblock(bp);
//     }

//     if (verbose)
//         printblock(bp);
//     if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
//         printf("Bad epilogue header\n");
// }

