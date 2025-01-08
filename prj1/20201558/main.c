#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <time.h>
#include"bitmap.h"
#include"debug.h"
#include"hash.h"
#include"hex_dump.h"
#include"limits.h"
#include"list.h"
#include"round.h"
bool _less(const struct list_elem *a, const struct list_elem *b, void *aux) {
  int aData = list_entry(a, struct list_item, elem)->data;
  int bData = list_entry(b, struct list_item, elem)->data;
  return aData < bData;
}

int commandAnalysis(char* input, char command[5][30]){
    int wordidx=0, comidx=0,len = strlen(input);
    for(int k=0;k<len;k++){
        if(input[k]==' '||input[k]=='\n'){
            command[comidx][wordidx]='\0';
            comidx++;
            wordidx=0;
        }else{
            command[comidx][wordidx] = input[k];
            wordidx++;
        }
    }
    return comidx;
}


int main(){
    srand(time(NULL));//랜덤성 위해 time seed로 하는 rand
    char input[100];
    char command[5][30];
    struct list** LIST = (struct list**)calloc(NUM_OF_DATA, sizeof(struct list*));//20개
    struct hash** HASH = (struct hash**)calloc(NUM_OF_DATA, sizeof(struct hash*));//20개
    struct bitmap** BITMAP = (struct bitmap**)calloc(NUM_OF_DATA, sizeof(struct bitmap*));//20개
    if(LIST==NULL || HASH==NULL || BITMAP==NULL){
        printf("MEMORY ALLOCATE ERROR! in main.c\n");
        exit(0);
    }
    int comCnt;
    while(fgets(input,100,stdin)!=NULL){
        comCnt = commandAnalysis(input, command);//입력받은 string을 공백 기준으로 나누어 커맨드에 저장.
        if(!strcmp(command[0],"create")){//create 자료구조 데이터명
            if(!strcmp(command[1],"list")){
                createList(LIST, command[2]);
            }else if(!strcmp(command[1],"bitmap")){
                BITMAP[atoi(command[2]+2)] = bitmap_create((size_t)atoi(command[3]));
            }else if(!strcmp(command[1],"hashtable")){
                HASH[atoi(command[2]+4)] = createHash();
            }
        }
        else if(!strcmp(command[0],"dumpdata")){//dumpdata 데이터명
            if(!strncmp(command[1],"list",4)){//데이터명 앞글자를 보고 자료구조 판단
                dumpList(LIST, command[1]);
            }else if(!strncmp(command[1],"bm",2)){
                //printf("ookok %d\n",atoi(command[1]+2));
                printBitmap(BITMAP, command[1]);
            }else if(!strncmp(command[1],"hash",4)){
                printHash(HASH[atoi(command[2]+4)]);
            }
        }
        else if(!strcmp(command[0],"delete")){//delete 데이터명
            if(!strncmp(command[1],"list",4)){//데이터명 앞글자를 보고 자료구조 판단
                deleteList(LIST, command[1]);
            }else if(!strncmp(command[1],"bm",2)){
                bitmap_destroy(BITMAP[atoi(command[1]+2)]);
            }else if(!strncmp(command[1],"hashtable",9)){
                hash_destroy(HASH[atoi(command[1]+4)],removeHashElem);//해시 내용 다 비우고
                //free(HASH[atoi(command[1]+4)]);//free
            }
        }
        else if(!strcmp(command[0],"quit")){//quit 입력시 입력 종료
            break;
        }/*
            이하 각 개별 함수들.
        */
        else if(!strcmp(command[0],"list_push_front")){
            pushList(LIST, command[1], atoi(command[2]), 0);//마지막 option값에 따라 front, push 구분.
        }
        else if(!strcmp(command[0],"list_push_back")){
            pushList(LIST, command[1], atoi(command[2]), 1);//마지막 option값에 따라 front, push 구분.
        }        
        else if(!strcmp(command[0],"list_pop_front")){
            popList(LIST, command[1], 0);//마지막 option값에 따라 front, push 구분.
        }
        else if(!strcmp(command[0],"list_pop_back")){
            popList(LIST, command[1], 1);//마지막 option값에 따라 front, push 구분.
        }
        else if(!strcmp(command[0],"list_front")){
            printListFrontBack(LIST, command[1], 0);
        }
        else if(!strcmp(command[0],"list_back")){
            printListFrontBack(LIST, command[1], 1);
        }else if(!strcmp(command[0],"list_insert")){
            insertindexList(LIST, command[1], atoi(command[2]), atoi(command[3]));
        }else if(!strcmp(command[0],"list_insert_ordered")){
            insertOrderedList(LIST, command[1], atoi(command[2]));
        }else if(!strcmp(command[0],"list_empty")){
            if(list_empty(LIST[atoi(command[1]+4)])){
              printf("true\n");
            }else{
              printf("false\n");
            }
            //emptyList(LIST, command[1]);
        }else if(!strcmp(command[0],"list_size")){
            printf("%zu\n",list_size(LIST[atoi(command[1]+4)]));
            //printListSize(LIST, command[1]);
        }else if(!strcmp(command[0],"list_min")){
            
            printListItemFun(list_min(LIST[atoi(command[1]+4)], _less, NULL), '\n');//Elem출력
        }else if(!strcmp(command[0],"list_max")){
            printListItemFun(list_max(LIST[atoi(command[1]+4)], _less, NULL), '\n');//Elem출력
        }else if(!strcmp(command[0],"list_remove")){
            removeList(LIST, command[1], atoi(command[2]));
        }else if(!strcmp(command[0],"list_reverse")){
            if(LIST[atoi(command[1]+4)]!=NULL){
                list_reverse(LIST[atoi(command[1]+4)]);
            }
        }else if(!strcmp(command[0],"list_shuffle")){
            if(LIST[atoi(command[1]+4)]!=NULL){
                list_shuffle(LIST[atoi(command[1]+4)]);
            }
        }else if(!strcmp(command[0],"list_sort")){
            list_sort(LIST[atoi(command[1]+4)],_less,NULL);
            //listSort(LIST, command[1]);
        }else if(!strcmp(command[0],"list_splice")){
            listSplice(LIST,command[1],atoi(command[2]),command[3],atoi(command[4]),atoi(command[5]));//list_splice 연장될놈 시작위치 뺏길놈 [시작~끝)
        }else if(!strcmp(command[0],"list_swap")){
            listSwap(LIST, command[1], atoi(command[2]), atoi(command[3]));
        }else if(!strcmp(command[0],"list_unique")){
            if(comCnt==3){//앞에 리스트 중복 제거하고 그걸 뒤에 리스트에 줘버리기
                list_unique(LIST[atoi(command[1]+4)], LIST[atoi(command[2]+4)], _less, NULL);
            }else if(comCnt==2){ //받은 리스트 중복 제거
                list_unique(LIST[atoi(command[1]+4)], NULL, _less, NULL);
            }
        }
        /* LIST 끝 */
        /* BITMAP 시작 */
        else if(!strcmp(command[0],"bitmap_mark")){
            bitmap_mark(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]));
        }else if(!strcmp(command[0],"bitmap_all")){//함수 결과에 따라 true면 true 출력. false면 false출력  아래에도 동일코드 반복
            if(bitmap_all(BITMAP[atoi(command[1]+2)], (size_t)atoi(command[2]), (size_t)atoi(command[3]))){
                printf("true\n");
            }else{
                printf("false\n");
            }
        }else if(!strcmp(command[0],"bitmap_any")){
            if(bitmap_any(BITMAP[atoi(command[1]+2)], (size_t)atoi(command[2]), (size_t)atoi(command[3]))){
                printf("true\n");
            }else{
                printf("false\n");
            }
        }else if(!strcmp(command[0],"bitmap_contains")){
            bool ToF;
            if(!strcmp(command[4],"true")){//com문자열 보고 동일한 bool값으로 ToF를 지정. 아래에도 동일코드 반복
                ToF = true;
            }else{
                ToF = false;
            }
            if(bitmap_contains(BITMAP[atoi(command[1]+2)], (size_t)atoi(command[2]),(size_t)atoi(command[3]), ToF)){
                printf("true\n");
            }else{
                printf("false\n");
            }
        }else if(!strcmp(command[0],"bitmap_count")){
            bool ToF;
            if(!strcmp(command[4],"true")){
                ToF = true;
            }else{
                ToF = false;
            }
            printf("%zu\n",bitmap_count(BITMAP[atoi(command[1]+2)], (size_t)atoi(command[2]),(size_t)atoi(command[3]), ToF));
        }else if(!strcmp(command[0],"bitmap_dump")){
            bitmap_dump(BITMAP[atoi(command[1]+2)]);
        }else if(!strcmp(command[0],"bitmap_expand")){
            BITMAP[atoi(command[1]+2)]=bitmap_expand(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]));
        }else if(!strcmp(command[0],"bitmap_set")){
            bool ToF;
            if(!strcmp(command[3],"true")){
                ToF = true;
            }else{
                ToF = false;
            }
            bitmap_set(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]),ToF);
        }    
        else if(!strcmp(command[0],"bitmap_set_all")){
            bool ToF;
            if(!strcmp(command[2],"true")){
                ToF = true;
            }else{
                ToF = false;
            }
            bitmap_set_all(BITMAP[atoi(command[1]+2)],ToF);
        }else if(!strcmp(command[0],"bitmap_flip")){
            bitmap_flip(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]));
        }else if(!strcmp(command[0],"bitmap_none")){
            if(bitmap_none(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]),(size_t)atoi(command[3]))){
                printf("true\n");
            }else{
                printf("false\n");
            }
        }else if(!strcmp(command[0],"bitmap_reset")){
            bitmap_reset(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]));
        }else if(!strcmp(command[0],"bitmap_scan_and_flip")){
            bool ToF;
            if(!strcmp(command[4],"true")){
                ToF = true;
            }else{
                ToF = false;
            }
            printf("%zu\n",bitmap_scan_and_flip(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]),(size_t)atoi(command[3]),ToF));
        }else if(!strcmp(command[0],"bitmap_scan")){
            bool ToF;
            if(!strcmp(command[4],"true")){
                ToF = true;
            }else{
                ToF = false;
            }
            printf("%zu\n",bitmap_scan(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]),(size_t)atoi(command[3]),ToF));
        }else if(!strcmp(command[0],"bitmap_set_multiple")){
            bool ToF;
            if(!strcmp(command[4],"true")){
                ToF = true;
            }else{
                ToF = false;
            }
            bitmap_set_multiple(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]),(size_t)atoi(command[3]),ToF);
        }else if(!strcmp(command[0],"bitmap_size")){
            printf("%zu\n",bitmap_size(BITMAP[atoi(command[1]+2)]));
        }else if(!strcmp(command[0],"bitmap_test")){
            if(bitmap_test(BITMAP[atoi(command[1]+2)],(size_t)atoi(command[2]))){
                printf("true\n");
            }else{
                printf("false\n");
            }
        }/*  BITMAP 끝    */
        else if(!strcmp(command[0],"hash_insert")){//makeHashElem은 인자로 받은 data를 갖는 Hase_Elem 반환
            hash_insert(HASH[atoi(command[1]+4)],makeHashElem(atoi(command[2])));
        }else if(!strcmp(command[0],"hash_size")){
            printf("%zu\n",hash_size(HASH[atoi(command[1]+4)]));
        }else if(!strcmp(command[0],"hash_apply")){
            applyHash(HASH[atoi(command[1]+4)],command[2]);
        }else if(!strcmp(command[0],"hash_delete")){
            hash_delete(HASH[atoi(command[1]+4)],makeHashElem(atoi(command[2])));
        }else if(!strcmp(command[0],"hash_empty")){
            if(hash_empty(HASH[atoi(command[1]+4)])){
                printf("true\n");
            }else{
                printf("false\n");
            }
        }else if(!strcmp(command[0],"hash_size")){
            printf("%zu\n",hash_size(HASH[atoi(command[1]+4)]));
        }else if(!strcmp(command[0],"hash_clear")){
            hash_clear(HASH[atoi(command[1]+4)],removeHashElem);
        }else if(!strcmp(command[0],"hash_find")){
            struct hash_elem* he = hash_find(HASH[atoi(command[1]+4)], makeHashElem(atoi(command[2])));
            if(NULL != he){
                printf("%d\n",he->data);
            }
        }else if(!strcmp(command[0],"hash_replace")){
            hash_replace(HASH[atoi(command[1]+4)], makeHashElem(atoi(command[2])));
        }
         
    }
    //더블포인터 모두 free.
    free(LIST);
    free(HASH);
    free(BITMAP);
    return 0;
}
