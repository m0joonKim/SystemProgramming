/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#include<string.h>

#define MAXARGS  128
#define MAXQUEUE MAXARGS*2
/* Function prototypes */
int eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
typedef struct {
    char* Item[MAXQUEUE];
    int Front;
    int Rear;
    int size;
}Queue;
void initQueue(Queue* q){
    q->size=0;
    q->Front=0;
    q->Rear=-1;
}
void EnQueue(Queue* q, char* data){
    if(q->size>=MAXQUEUE){
        printf("QUEUE IS FULL, CANT ENQUEUE\n");
        return;
    }
    q->Rear = (q->Rear + 1) % MAXQUEUE;
    q->Item[q->Rear] = data;
    q->size++;
    return ;
}
char* DeQueue(Queue* q){
    if(q->size==0){
        printf("QUEUE IS EMPTY, CANT DEQUEUE");
        return NULL;
    }
    char* data;
    data = q->Item[q->Front];
    q->Front = (q->Front + 1)%MAXQUEUE;
    q->size--;
    return data;
}
int ParsingUseStr(Queue* Q, char* command, char* parsing){
    initQueue(Q);
    char *com1;
    char *com2;
    command[strlen(command)-1] = ' ';//마지막 엔터 지우기
    com1 = strtok(command, parsing);//&&로 쪼개는법.
    while(com1 != NULL){
        while(*com1 == ' '){
            com1++;
        }
        EnQueue(Q,com1);
        com2 = strtok(NULL, parsing);//NULL에 대해 하면 이전값에 이어서 쪼갬
        com1 = com2;
    }
    return Q->size;
}
int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    Queue andParsingQueue;
    int andParsingNumber=0;
    int nowCom=0;
    char *cmdAfterAndParsing;

    while (1) {
	/* Read */
	    printf("CSE4100-SP-P2> ");                   
	    fgets(cmdline, MAXLINE, stdin); 
	    if (feof(stdin))
	    exit(0);

	/* Evaluate */
        andParsingNumber = ParsingUseStr(&andParsingQueue, cmdline,"&&");//반환값은 큐 사이즈임.
        nowCom=0;
        while(nowCom++<andParsingNumber){
            if(andParsingQueue.size==0)break;
            cmdAfterAndParsing = DeQueue(&andParsingQueue);
	        if(eval(cmdAfterAndParsing)!=0){
                while(andParsingQueue.size>0){//수행중에 오류잇으면 큐 비우고 끝내
                    DeQueue(&andParsingQueue);
                }
                break;
            }
        }
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */

int eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int child_status;
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    //argv에 대해 따옴표 제거해주기

    if (argv[0] == NULL)  
	return 0;   /* Ignore empty lines */
    int builtdinFlag = builtin_command(argv);
    if (builtdinFlag == 0) { //quit -> exit(0), & -> ignore, other -> run //빌트인 아니면
        if((pid=Fork())==0){
            if(execvp(argv[0],argv)<0){
                printf("%s: command not found.\n", argv[0]);
                exit(EXIT_FAILURE);
            }
        }else{
            Waitpid(pid,&child_status,0);
            //printf("%s WIF is %d\n",argv[0],WIFEXITED(child_status));
            if((WIFEXITED(child_status) && WEXITSTATUS(child_status))!=0){
                return -1;
            }
        } 
    }else if(builtdinFlag==9){//cdError (빌트인에서 오류 뱉어냈으면)
        return -1;
    }
    return 0;
}
/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{   
    int cdResult = 1;
    int argvCnt = 0;
    pid_t pid;
    char binop[1000];

    if(!strcmp(argv[0],"exit")){
        exit(0);
    }
    if (!strcmp(argv[0], "&")){ /* Ignore singleton & */
	    return 1;
    }
    if(!strcmp(argv[0],"cd")){
        if(argv[1]==NULL||strlen(argv[1])==0){
            cdResult = chdir(getenv("HOME"));
            if(cdResult != 0){
                 printf("bash: cd: %s: No such file or directory\n",argv[1]); 
                 return 9;
            }
            return 1;
        }
        if(!strncmp(argv[1],"~",1)){
            cdResult = chdir(getenv("HOME"));
            if(cdResult != 0){
                printf("bash: cd: %s: No such file or directory\n",argv[1]); 
                return 9;
            }
            if(strlen(argv[1])>=3){
                cdResult = chdir(argv[1]+2);            
                if(cdResult != 0){
                    printf("bash: cd: %s: No such file or directory\n",argv[1]); 
                    return 9;
                }
            }
        }else{        
            cdResult = chdir(argv[1]);
            if(cdResult != 0){
                printf("bash: cd: %s: No such file or directory\n",argv[1]); 
                return 9;
            }
        }
        return 1;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) {
    char *delim; /* Points to first space delimiter */
    int argc;    /* Number of args */
    int bg;      /* Background job? */

    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        if(*buf=='"'&&*(delim-1)=='"'&&delim-buf>1){//양끝 쌍따옴표 파싱
            buf++;
            *(delim-1)='\0';
        }
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0) {
        argv[--argc] = NULL;
    }

    return bg;
}
/* $end parseline */

