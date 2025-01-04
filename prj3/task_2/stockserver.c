/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define SBUFSIZE 1000
#define NTHREADS 20

typedef struct{
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
}sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

struct item{
    int ID;
    int left_stock;
    int price;
    struct item* left;
    struct item* right;
    pthread_rwlock_t rwlockp;
};

sbuf_t sbuf;

struct item* stockTree;
void echo(int connfd);
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

void sigintHandler(int sig){
    writeStocktxt("stock.txt",stockTree);
    freeItem(stockTree);
    sbuf_deinit(&sbuf);
    exit(0);
}
void *thread(void *vargp);

int main(int argc, char **argv) 
{
    Signal(SIGINT, sigintHandler);

    int listenfd;
    int connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    pthread_t tid; 

    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }
    stockTree = makeStockStruct("stock.txt");
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    for(int i=0;i<NTHREADS;i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }
    while (1) {
	    clientlen = sizeof(struct sockaddr_storage); 
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
    }
    writeStocktxt("stock.txt",stockTree);
    freeItem(stockTree);
    sbuf_deinit(&sbuf);
    exit(0);
}
/* $end echoserverimain */

void *thread(void *vargp){
    
    int n; 
    char buf[MAXLINE]; 
    rio_t rio;
    Pthread_detach(pthread_self());
    while(1){
        // static pthread_once_t once = PTHREAD_ONCE_INIT;
        // Pthread_once(&once, init_pt_once);
        int connfd = sbuf_remove(&sbuf);
        Rio_readinitb(&rio, connfd);
        while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
	        printf("server received %d bytes\n", n);
            analysisCommand(buf);
            if(!strncmp(buf,"exit",4)){
                writeStocktxt("stock.txt",stockTree);
                break;
            }
            Rio_writen(connfd, buf, MAXLINE);
        }
        writeStocktxt("stock.txt",stockTree);
        Close(connfd);
    }
}

void task2show(char* buffer){
    buffer[0] = '\0';
    printStock(stockTree, buffer);
    buffer[strlen(buffer)-1] = '\n';
}
void task2buy(char* buffer){
    int id, num;
    char order[10];
    sscanf(buffer, "%s %d %d\n",order, &id, &num);
    if(buyStock(stockTree,id,num)){//1이라는건 성공했다는거
        strcpy(buffer, "[buy] success\n");
    }else{
        strcpy(buffer, "Not enough left stock\n");
    }
}
void task2sell(char* buffer){
    int id, num;
    char order[10];
    sscanf(buffer, "%s %d %d\n",order, &id, &num);
    sellStock(stockTree, id, num);
    strcpy(buffer, "[sell] success\n");

}
void analysisCommand(char* buffer){
    if(!strncmp(buffer,"show",4)){
        task2show(buffer);
    }else if(!strncmp(buffer,"buy",3)){
        task2buy(buffer);
    }else if(!strncmp(buffer,"sell",4)){
        task2sell(buffer);
    }else if(!strncmp(buffer,"exit",4)){
        return ;
    }
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
    struct item* newItem = (struct item*)calloc(1,sizeof(struct item));
    if(newItem==NULL){
        printf("mallocERROR\n");
        return NULL;
    }
    newItem->ID = id;
    newItem->price = price;
    newItem->left_stock = left_stock;
    newItem->left = NULL;
    newItem->right = NULL;
    if(pthread_rwlock_init(&(newItem->rwlockp), NULL)!=0){
        printf("rwlock init error\n");
        exit(0);
    }
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
    pthread_rwlock_destroy(&(now->rwlockp));
    free(now);
    return ;
}
void printStock(struct item* root, char* tmp){
    if(root==NULL)return;
    pthread_rwlock_rdlock(&(root->rwlockp));  
    
    char nowstock[MAXBUF];
    sprintf(nowstock,"%d %d %d\n",root->ID, root->left_stock, root->price);
    strcat(tmp, nowstock);
    pthread_rwlock_unlock(&(root->rwlockp));
    
    printStock(root->left, tmp);
    printStock(root->right, tmp);
    return ;
}
int buyStock(struct item* root, int id, int num){
    //존재않는 주식을 구매할때도 그냥 부족한거로 출력??
    struct item* buyItem = findItem(root, id);
    int ret;

    if(buyItem==NULL){
        ret=0;
    }else{
        pthread_rwlock_wrlock(&(buyItem->rwlockp));
        if(buyItem->left_stock>=num){
            buyItem->left_stock -= num;
            ret =  1;
        }else{
            ret =  0;
        }
        pthread_rwlock_unlock(&(buyItem->rwlockp));
    }
    return ret;
}
void sellStock(struct item* root, int id, int num){
    //존재않는 주식을 구매할때도 그냥 부족한거로 출력??
    struct item* sellItem = findItem(root, id);
    if(sellItem==NULL){
        //sellItem = makeItem(id, num, 10000);//없는거 팔면 걍 만원으로 올리지뭐
        //어차피 이런 인풋은 없다고 가정되어있긴 했음
        //root = insertItem(root, sellItem);
        return ;
    }else{
        pthread_rwlock_wrlock(&(sellItem->rwlockp));
        sellItem->left_stock += num;
        pthread_rwlock_unlock(&(sellItem->rwlockp));
        return ;
    }
}
void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}
void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}
void sbuf_insert(sbuf_t *sp, int item){
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}
int sbuf_remove(sbuf_t *sp){
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front)%(sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}