/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
typedef struct{
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];//1024 정의되어있네
    rio_t clientrio[FD_SETSIZE];
}pool;
struct item{
    int ID;
    int left_stock;
    int price;
    int readcnt;//혼자 읽고있을때만 변경하게끔?
    //이미 누가 읽고있으면 기다리다가 바꾸게 
    struct item* left;
    struct item* right;
};
struct item* stockTree;
void echo(int connfd);
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_client(pool *p);
void analysisCommand(char* buffer);
struct item* makeStockStruct(char* openFile);
struct item* makeItem(int id, int left_stock, int price);
struct item* findItem(struct item* now, int findId);
struct item* insertItem(struct item* root, struct item* isrt);
void freeItem(struct item* now);

void printStock(struct item* root, char* buf);
int buyStock(struct item* root, int id, int num);
void sellStock(struct item* root, int id, int num);
void writeStocktxt(char* openFile, struct item* root);

int noConnection(pool* p);
void sigintHandler(int sig){
    writeStocktxt("stock.txt",stockTree);
    freeItem(stockTree);
    exit(0);
}
int main(int argc, char **argv) 
{
    Signal(SIGINT, sigintHandler);
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }
    static pool pool;
    stockTree = makeStockStruct("stock.txt");
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);//초기는 리슨
    while (1) {
	    // echo(connfd);
	    // Close(connfd);
        pool.ready_set = pool.read_set; //바뀌니까 저장
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
        if(FD_ISSET(listenfd, &pool.ready_set)){
	        clientlen = sizeof(struct sockaddr_storage); 
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE,
                client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
            add_client(connfd, &pool);
        }
        check_client(&pool);
        //연결중인놈 없으면 업데이트해주기 끝날수도잇는거니까
        if(noConnection(&pool)){
            writeStocktxt("stock.txt",stockTree);
        }
    }
    freeItem(stockTree);
    exit(0);
}
/* $end echoserverimain */
int noConnection(pool *p){
    for(int i=0;i<p->maxi;i++){
        if(p->clientfd[i]!=-1){
            return 0;//하나라도 비어있지 않은 게 있으면 업데이트 안함
        }
    }
    return 1;
}
void init_pool(int listenfd, pool *p){
    int i;
    p->maxi = -1;
    for(i=0;i<FD_SETSIZE;i++){
        p->clientfd[i] = -1;
    }
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_ZERO(&p->ready_set);
    FD_SET(listenfd, &p->read_set);
}
void add_client(int connfd, pool *p){
    int i;
    p -> nready--;
    for(i=0;i<FD_SETSIZE;i++){
        if(p->clientfd[i]<0){//빈자리겠지
            p->clientfd[i] = connfd;
            Rio_readinitb(&(p->clientrio[i]),connfd);
            FD_SET(connfd, &p->read_set);
            if(connfd > p->maxfd){
                p->maxfd = connfd;
            }
            if(i > p->maxi){
                p->maxi = i;
            }
            break;
        }
    }
    if(i==FD_SETSIZE){
        app_error("add_client error : Too many clients");
    }
}
void check_client(pool *p){
    int i, connfd, n;
    char buf[MAXBUF];
    rio_t *rio;
    for(i=0;(i <= p->maxi)&&(p->nready > 0);i++){//최대까지 안봣고 레디 남아있으면
        buf[0] = '\0';
        connfd = p->clientfd[i];
        rio = &p->clientrio[i];
        if((connfd>0)&&(FD_ISSET(connfd, &p->ready_set))){
            p->nready--;
            n=Rio_readlineb(rio, buf, MAXLINE);
            printf("server received %d bytes\n",(int)n);
            if(n<=0||!strncmp(buf,"exit",4)){
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
                //printf("close\n");
            }else{
                analysisCommand(buf);
                Rio_writen(connfd, buf, MAXLINE);
            }
        }
    }
}
void analysisCommand(char* buffer){
    int id, num;
    char order[10];
    if(!strncmp(buffer,"show",4)){
        buffer[0] = '\0';
        printStock(stockTree, buffer);
        buffer[strlen(buffer)-1] = '\n';
        //printf("%s\n",buffer);
    }else if(!strncmp(buffer,"buy",3)){
        sscanf(buffer, "%s %d %d\n",order, &id, &num);
        if(buyStock(stockTree,id,num)){//1이라는건 성공했다는거
            strcpy(buffer, "[buy] success\n");
        }else{
            strcpy(buffer, "Not enough left stock\n");
        }
    }else if(!strncmp(buffer,"sell",4)){
        sscanf(buffer, "%s %d %d\n",order, &id, &num);
        sellStock(stockTree, id, num);
        strcpy(buffer, "[sell] success\n");
    }//exit은 위에서 처리했음
    else{
        strcpy(buffer,"Wrong Command!\n");
    }
}
struct item* makeStockStruct(char* openFile){
    FILE *f = fopen(openFile, "r");
    if(f==NULL){
        printf("%s open Error\n",openFile);
        return NULL;
    }
    struct item* root = NULL;
    int id, price, left_stock;
    while(fscanf(f,"%d %d %d", &id, &left_stock, &price) != EOF){
        struct item* tmp = makeItem(id,left_stock,price);
        root = insertItem(root, tmp);
    }
    fclose(f);
    return root;
}
void writeStocktxt(char* openFile, struct item* root){
    FILE *f = fopen(openFile, "w");
    if(f==NULL){
        printf("%s open Error\n",openFile);
        return ;
    }
    char buffer[MAXBUF];
    buffer[0] = '\0';
    printStock(root, buffer);
    fprintf(f,"%s",buffer);
    //printf("new file %s\n",buffer);
    fclose(f);
    return ;
}

struct item* insertItem(struct item* root, struct item* isrt){
    if(root == NULL)return isrt;
    if(root->ID < isrt->ID){
        root->right = insertItem(root->right, isrt);
    }else if(root->ID > isrt->ID){
        root->left = insertItem(root->left, isrt);
    }else if(root->ID == isrt->ID){
        root->price = isrt->price;
        root->left_stock = isrt->left_stock;
    }  
    return root;
}
struct item* makeItem(int id, int left_stock, int price){
    struct item* newItem = (struct item*)malloc(sizeof(struct item));
    if(newItem==NULL){
        printf("mallocERROR\n");
        return NULL;
    }
    newItem->ID = id;
    newItem->price = price;
    newItem->left_stock = left_stock;
    newItem->readcnt = 0;
    newItem->left = NULL;
    newItem->right = NULL;
    return newItem;
}
struct item* findItem(struct item* now, int findId){
    if(now==NULL)return NULL;
    if(now->ID == findId)return now;
    if(now->ID > findId){
        return findItem(now->left, findId);
    }else if(now->ID < findId){
        return findItem(now->right, findId);
    }
    return now;
}
void freeItem(struct item* now){
    if(now==NULL)return ;
    freeItem(now->left);
    freeItem(now->right);
    free(now);
    return ;
}
void printStock(struct item* root, char* tmp){
    if(root==NULL)return;
    char nowstock[MAXBUF];
    //이제 이걸 서버에 띄우는게 아니라 클라이언트한테 보내주는거로 바꿔야함
    sprintf(nowstock,"%d %d %d\n",root->ID, root->left_stock, root->price);
    strcat(tmp, nowstock);
    printStock(root->left, tmp);
    printStock(root->right, tmp);
    return ;
}
int buyStock(struct item* root, int id, int num){
    //존재않는 주식을 구매할때도 그냥 부족한거로 출력??
    struct item* buyItem = findItem(root, id);
    if(buyItem==NULL){
        return 0;
    }else{
        if(buyItem->left_stock>=num){
            buyItem->left_stock -= num;
            return 1;
        }else{
            return 0;
        }
    }
}
void sellStock(struct item* root, int id, int num){
    struct item* sellItem = findItem(root, id);
    if(sellItem==NULL){
        //sellItem = makeItem(id, num, 10000);//없는거 팔면 걍 만원으로 올리지뭐
        //어차피 이런 인풋은 없다고 가정되어있긴 했음
        //root = insertItem(root, sellItem);
        return ;
    }else{
        sellItem->left_stock += num;
        return ;
    }
}