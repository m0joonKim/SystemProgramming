/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#include<string.h>

#define MAXARGS  128
#define MAXJOBS  128
#define MAXQUEUE MAXARGS*2
/* Function prototypes */
int eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 

typedef struct {
    pid_t jobPid;
    int jobIndex;
    char comName[MAXLINE];
    int state; // 1 : FOREGROUND | 2 : BACKGROUND | <<앞에 두개는 도는중인거 | STOP은 멈춰있 > |  3 : STOPPED | 0 : 없는거
}Jobs;
Jobs jobList[MAXJOBS+1];
char currentCommandLine[MAXLINE];
volatile int lastJobListIndex;
volatile sig_atomic_t volpid;
volatile int isBackGround;

void jobListInit(){
    memset(jobList,0,sizeof(jobList));
}
void findLastJobIndx(){
    for(int i=MAXJOBS;i>=0;i--){
        if(i==0){
            lastJobListIndex = 0;
            break;
        }
        if(jobList[i].state!=0){
            lastJobListIndex = i;
            break;
        }
    }
}
void makeJobElement (char** newJobName, int newJobPid, int newJobState){
    //last찾기

    int nowJobIdx = lastJobListIndex+1;
    strcpy(jobList[nowJobIdx].comName,"");
    // int j=0;
    // while(newJobName[j]!=NULL){
    //     strcat(jobList[nowJobIdx].comName,newJobName[j++]);
    //     if(newJobName[j]!=NULL){
    //         strcat(jobList[nowJobIdx].comName," ");
    //     }
    // }
    strcat(jobList[nowJobIdx].comName,currentCommandLine);
    //printf("%s\n",jobList[nowJobIdx].comName); //debug
    jobList[nowJobIdx].state = newJobState;
    jobList[nowJobIdx].jobPid = newJobPid;
    jobList[nowJobIdx].jobIndex = nowJobIdx;
    //printf("New Jobs is [%d] %s\n",jobList[nowJobIdx].jobIndex,jobList[nowJobIdx].comName );
}
void delJobElement(pid_t pid){
    for(int i=0;i<MAXJOBS;i++){
        if(jobList[i].jobPid==pid){
            memset(&(jobList[i]),0,sizeof(Jobs));
            break;
        }
    }
}
void jobsPrint(){
    findLastJobIndx();
    for(int i=1;i<=lastJobListIndex;i++){
        if(jobList[i].state){
            printf("[%d] %s %s\n",i, (jobList[i].state==3) ? "suspended" : "running", jobList[i].comName);
           // printf("%d \n",jobList[i].state);
        }
    }
}
Jobs* callJobbyPid(pid_t pid){
    for(int i=1;i<=MAXJOBS;i++){
        if(jobList[i].jobPid==pid){
            return &(jobList[i]);
        }
    }
    return NULL;
}
void sigstp_handler(int sg){//Foground에 돌던놈을 멈춰야함. -> Job중에서 포그인놈 스탑으로 바꾸고 백그로 넘겨주기
    //printf("ctrlZ!!\n");ebug
    // printf("debugstop\n");
    // jobsPrint();
    
    findLastJobIndx();
    for(int i=1;i<=lastJobListIndex;i++){
        //printf("%d is pid and %d is IDX %d is state\n",jobList[i].jobPid,jobList[i].jobIndex,jobList[i].state);

        if(jobList[i].state == 1){

            jobList[i].state = 3;
            Kill(jobList[i].jobPid,SIGSTOP);

            break;
        }
    }

    return ;
}
void sigchld_handler(int sg){//Foground에 돌던놈을 멈춰야함. -> Job중에서 포그인놈 스탑으로 바꾸고 백그로 넘겨주기
    // printf("debugchld\n");
    // jobsPrint();
    int child_status;
    Jobs *chjob;
    pid_t tmppid;
    // WUNTRACED | WCONTINUED | WNOHANG
    while((tmppid=waitpid(-1,&child_status,WUNTRACED | WCONTINUED | WNOHANG))>0){
        chjob = callJobbyPid(tmppid);
        volpid = tmppid;
        if(chjob!=NULL){//pid잡 찾았으면

            if(WIFCONTINUED(child_status)){
                continue;
            }else if(WIFSTOPPED(child_status)){
                break;
            }else if(WIFSIGNALED(child_status)){
                delJobElement(volpid);
            }else if(WIFEXITED(child_status)){
                delJobElement(volpid);
            }
        }
    }
    return ;
}
void sigterm_handler(int sg)
{
    // printf("debugterm\n");
    // jobsPrint();
    _exit(0);
}

void sigint_handler(int sg){
    // printf("debugint\n");
    // jobsPrint();

    Jobs* fgJob = NULL;
    for(int i=0;i<=MAXJOBS;i++){
        if(jobList[i].state==1){
            fgJob = &(jobList[i]);
            Kill(fgJob->jobPid,SIGINT);
            break;
        }
    }
    if(fgJob==NULL)return ;
    //printf("%d\n",fgJob->jobPid);
}
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
int isBackGroundCommand(char* cmdline){
    int ok=0;
    int i = strlen(cmdline)-1;
    cmdline[i]=' ';

    while(i>=0&&*(cmdline+i)==' '){
        i--;
    }
    if(i<0)return 0;
    if(*(cmdline+i)=='&'){
        i--;
        if(*(cmdline+i)!='&'){
            return 1;
        }
        return 0;
    }
    return 0;
}
int evalForPipe(char* cmdline, int nowComIdx, int maxComIdx, int* currentOutput);
int evalMakerForPipe(Queue* Q);
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
        if(!strcmp(com1, "")){
            //printf("empty stirng!\n");
            break;
        }
        EnQueue(Q,com1);
        com2 = strtok(NULL, parsing);//NULL에 대해 하면 이전값에 이어서 쪼갬
        com1 = com2;
    }
    return Q->size;
}
int main() 
{
    jobListInit();
    char cmdline[MAXLINE]; /* Command line */

    // for(int i=0; environ[i];i++)
    // {
    //     printf("<%2d>: %s\n", i, environ[i]);
    // }
    Signal(SIGTERM,sigterm_handler);
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP,sigstp_handler);
    Signal(SIGCHLD,sigchld_handler);
    Queue andParsingQueue;
    int andParsingNumber=0;
    int nowCom=0;
    char *cmdAfterAndParsing;

    int pipeParsingNumber=0;
    Queue pipeParsingQueue;
    char* deQeuePipeCom;

    while (1) {
	/* Read */
    
        isBackGround=0;//기본은 foreground
	    printf("CSE4100-SP-P2> ");                   
	    fgets(cmdline, MAXLINE, stdin);
        isBackGround = isBackGroundCommand(cmdline);
        //strcpy(currentCommandLine,cmdline);
        
	    if (feof(stdin))
	    exit(0);

	/* Evaluate */
        andParsingNumber = ParsingUseStr(&andParsingQueue, cmdline,"&&");//반환값은 큐 사이즈임.
        nowCom=0;
        //printf("%d\n",andParsingNumber);
        char * tmpCommad;
        
        while(nowCom++<andParsingNumber){
            if(andParsingQueue.size==0)break;
            cmdAfterAndParsing = DeQueue(&andParsingQueue);
            pipeParsingNumber = ParsingUseStr(&pipeParsingQueue, cmdAfterAndParsing, "|");
            

            /*job 추가 위해서 명령어와 번호 저장*/
            strcpy(currentCommandLine,"");
            for(int i=0;i<pipeParsingQueue.size;i++){
                tmpCommad = DeQueue(&pipeParsingQueue);
                strcat(currentCommandLine, tmpCommad);
                EnQueue(&pipeParsingQueue,tmpCommad);
                if(i==(pipeParsingQueue.size)-1||!strcmp(tmpCommad,"")){//tㅅㅂ왜안돼

                    break;
                }
                strcat(currentCommandLine, "&& ");
            }
            if(pipeParsingQueue.size!=pipeParsingNumber){
                printf("Queue ERROR\n");
            }
            findLastJobIndx();
            /*job 추가 위해서 명령어와 번호 저장*/




            if(pipeParsingNumber <= 1){//파이프로 잘랐는데 명령이 한개거나 더 적으면 -> 파이프 없었다
	            if(eval(cmdAfterAndParsing)!=0){
                    while(andParsingQueue.size>0){//수행중에 오류잇으면 큐 비우고 끝내
                        DeQueue(&andParsingQueue);
                    }
                    break;
                }
            }else{
                if(evalMakerForPipe(&pipeParsingQueue)!=0){
                //파이프있으면 큐를 보내서 eval을 각각 하게함
                    while(andParsingQueue.size>0){//수행중 오류있으면 큐 비우고 끝내
                        DeQueue(&andParsingQueue);
                    }
                    break;
                }
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
    pid_t pid;           /* Process id */
    int child_status;
    sigset_t mask,prev;
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGCHLD);

    strcpy(buf, cmdline);
    int bg = parseline(buf, argv);
    //argv에 대해 따옴표 제거해주기

    if (argv[0] == NULL)  
	return 0;   /* Ignore empty lines */
    int builtdinFlag = builtin_command(argv);
    if (builtdinFlag == 0) { //quit -> exit(0), & -> ignore, other -> run //빌트인 아니면
        Sigprocmask(SIG_BLOCK, &mask, &prev);
        if((pid=Fork())==0){
	    if(isBackGround){
		setpgid(0,0);
	    }		
            if(execvp(argv[0],argv)<0){
                printf("%s: command not found.\n", argv[0]);
                exit(EXIT_FAILURE);
            }
        }
        
        makeJobElement(argv,pid,isBackGround?2:1);
        //1포 2백 3스탑 0없
        volpid=0;
	    if (!isBackGround){ //백그 아니면 안기다리면 된다ㅇㅇ
            while(!volpid){
                Sigsuspend(&prev);
            }
            //pause();
	    }else{
            usleep(100000);
        }
        Sigprocmask(SIG_SETMASK, &prev, NULL);

    }else if(builtdinFlag==9){//cdError (빌트인에서 오류 뱉어냈으면)
        return -1;
    }
    return 0;
}
int evalForPipe(char* cmdline, int nowComIdx, int maxComIdx, int* currentOutput){
    // 0 : stdin | 1 : stdout | 2 : stder | f[0] : pipe in | f[1] : pipe out
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int child_status;
    int saveStdin = STDIN_FILENO;
    int saveStdout = STDOUT_FILENO;
    int fd[2];
    int remainingCommand = maxComIdx-nowComIdx+1;//지금 명령 포함 남은 개수
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    sigset_t mask,prev;
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGCHLD);


    if (argv[0] == NULL) return 0;   /* Ignore empty lines */
    int builtdinFlag = builtin_command(argv);
    if (builtdinFlag == 0) { //quit -> exit(0), & -> ignore, other -> run //빌트인 아니면
        //setpgid(0,0);
	if(isBackGround){
	    setpgid(0,0);
	}		
	int ercode = pipe(fd);
        if(ercode!=0){
            printf("pipe error\n");
            return -1;
        }
        Sigprocmask(SIG_BLOCK, &mask, &prev);
        if((pid=Fork())==0){
            if(remainingCommand>1){//마지막이 아니라면
                Dup2(fd[1], STDOUT_FILENO);//뱉는다 파이프로
            }
            if(nowComIdx!=0){//시작이 아니라면
                Dup2(*currentOutput,STDIN_FILENO);//먹는다 이전아웃풋을
            }
            if(execvp(argv[0],argv)<0){
                printf("%s: command not found.\n", argv[0]);
                exit(EXIT_FAILURE);
            }
        }

        makeJobElement(argv,pid,isBackGround?2:1);
        volpid=0;
	    if (!isBackGround){
            while(!volpid){
                Sigsuspend(&prev);
            }
            // pause();
	    }else{
            usleep(100000);
        }
        Sigprocmask(SIG_SETMASK, &prev, NULL);

        if(remainingCommand!=0){//마지막이 아니라면
            *currentOutput = fd[0];
            close(fd[1]);
        }
    }else if(builtdinFlag==9){//cdError (빌트인에서 오류 뱉어냈으면)
        return -1;
    }

    return 0;
}


int evalMakerForPipe(Queue* Q){
    int maxComIdx = (Q->size)-1;
    int currentOutput = 0;
    char* command;
    for(int i=0;i<=maxComIdx;i++){
        command = DeQueue(Q);
        if(evalForPipe(command, i, maxComIdx, &currentOutput)!=0){
            while(Q->size>0){
                DeQueue(Q);
            }
            return -1;//각 파이프에서 문제가 생겼으면 이것도 중지하고 보내버려
        }
    }
    return 0;
}
int correctPhase3type(char* ag1){
    if(ag1[0]!='%'){
        return -1;
    }
    int len = strlen(ag1);
    for(int i=1;i<len;i++){
        if(ag1[i]<'0'||ag1[i]>'9'){
            return -1;
        }
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
    int jobIdx;
    int jobFindOk;
    if(!strcmp(argv[0],"exit")){
        exit(0);
    }
    if (!strcmp(argv[0], "&")){ /* Ignore singleton & */
	    return 1;
    }
    if (!strcmp(argv[0],"jobs")){
        jobsPrint();
        return 1;
    }
    if (!strcmp(argv[0],"bg")){
        jobFindOk=0;
        if(argv[1]==NULL||correctPhase3type(argv[1])==-1){
            printf("bg: usage: bg %%<job number>\n");
            return 1;
        }
        jobIdx = atoi(argv[1]+1);
        if(jobIdx<1||jobIdx>MAXJOBS){
            printf("bg: %%%d: no such job\n",jobIdx);
            return 1;
        }else if(jobList[jobIdx].state==0){
            printf("bg: %%%d: no such job\n",jobIdx);
            return 1;
        }else if(jobList[jobIdx].state==2){
            printf("bg: job %d already in background\n",jobIdx);
            return 1;
        }//fore에 있는애한테는 차피 이 명령을 쓸수가없음

	setpgid(jobList[jobIdx].jobPid,jobList[jobIdx].jobPid);
        Kill(jobList[jobIdx].jobPid, SIGCONT);
	
        jobList[jobIdx].state = 2;
        printf("[%d] %s %s\n",jobIdx, (jobList[jobIdx].state==3) ? "suspended" : "running", jobList[jobIdx].comName);
        return 1;
    }
    if (!strcmp(argv[0],"fg")){


        if(argv[1]==NULL||correctPhase3type(argv[1])==-1){
            printf("fg: usage: fg %%<job number>\n");
            return 1;
        }
        jobIdx = atoi(argv[1]+1);
        if(jobIdx<1||jobIdx>MAXJOBS){
            printf("fg: %%%d: no such job\n",jobIdx);
            return 1;
        }else if(jobList[jobIdx].state==0){
            printf("fg: %%%d: no such job\n",jobIdx);
            return 1;
        }

        Kill(jobList[jobIdx].jobPid, SIGCONT);
        jobList[jobIdx].state = 1;
        printf("%s\n",jobList[jobIdx].comName);
        pause();
       

        return 1;
    }
    if (!strcmp(argv[0],"kill")){
        jobFindOk=0;
        if(argv[1]==NULL||correctPhase3type(argv[1])==-1){
            printf("kill: usage: kill %%<job number>\n");
            return 1;
        }

        jobIdx = atoi(argv[1]+1);
        if(jobIdx<1||jobIdx>MAXJOBS){
            printf("kill: %%%d: no such job\n",jobIdx);
            return 1;
        }else if(jobList[jobIdx].state==0){
            printf("kill: %%%d: no such job\n",jobIdx);
            return 1;
        }

        Kill(jobList[jobIdx].jobPid, SIGKILL);
        printf("[%d] Killed %s\n",jobIdx, jobList[jobIdx].comName);
        delJobElement(jobList[jobIdx].jobPid);

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
    int k=0;
    // while(argv[k]!=NULL){
    //     printf("%s ",argv[k]);
    //     k++;
    // }printf("\n");
    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0) {
        argv[--argc] = NULL;
    }

    return bg;
}
/* $end parseline */

