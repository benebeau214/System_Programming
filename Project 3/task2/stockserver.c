#include "csapp.h"
#include <stdbool.h>

#define NTHREADS 10 //사용할 thread 수
#define SBUFSIZE 600 //공유 버퍼 크기
#define FILENAME "stock.txt" 

//주식 정보를 담는 노드 구조체(Binary Tree)
typedef struct item {
    int ID; //주식 ID
    int left_stock; //남은 수량
    int price; //가격
    int readcnt; //reader count
    sem_t mutex; // readcnt 보호용 세마포어
    struct item *left, *right; //좌우 자식 포인터
} Item;

Item *root = NULL;  // Binary Tree root
Item *items[10000]; // Stock.txt 순서 보존하기 위한 배열
int item_cnt = 0;   // 아이템 수 

// 생산자-소비자용 원형 큐 구조체
typedef struct {
    int *buf; // 버퍼 배열
    int n;    // 총 버퍼 크기
    int front; // 제거 위치
    int rear;  // 삽입 위치
    sem_t mutex; // 버퍼 접근 보호용 세마포어
    sem_t slots; // 사용 가능한 슬롯 수
    sem_t items; // 존재하는 아이템 수
} sbuf_t;

sbuf_t sbuf;    // 공유 버퍼 전역 변수
static sem_t sem, w; // 연결 수 & writer 보호용 세마포어
int num_conn = 0; // 현재 연결된 클라이언트 수

// 공유 버퍼 초기화 함수 
void sbuf_init(sbuf_t *sp, int n) {
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

// 버퍼에 연결 추가(생산자 역할)
void sbuf_insert(sbuf_t *sp, int item) {
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear) % (sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}

// 버퍼에 연결 제거(소비자 역할)
int sbuf_remove(sbuf_t *sp) {
    P(&sp->items);
    P(&sp->mutex);
    int item = sp->buf[(++sp->front) % (sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}

//새로운 주식 아이템 생성 
Item *create_item(int id, int stock, int price) {
    Item *node = Malloc(sizeof(Item));
    node->ID = id;
    node->left_stock = stock;
    node->price = price;
    node->readcnt = 0;
    node->left = node->right = NULL;
    Sem_init(&node->mutex, 0, 1);
    return node;
}

// 트리에 노드(주식) 삽입 
void bst_insert(Item **root, Item *node) {
    if (*root == NULL) {
        *root = node;
        return;
    }
    if (node->ID < (*root)->ID)
        bst_insert(&(*root)->left, node);
    else
        bst_insert(&(*root)->right, node);
}

// 업데이트 된 주식 정보를 파일에 저장
void write_stock_file() {
    FILE *fp = Fopen(FILENAME, "w");
    for (int i = 0; i < item_cnt; i++) {
        fprintf(fp, "%d %d %d\n", items[i]->ID, items[i]->left_stock, items[i]->price);
    }
    Fclose(fp);
}

// 파일로 부터 주식 정보 읽어오기
void load_data(const char *filename) {
    FILE *fp = Fopen(filename, "r");
    int id, stock, price;
    while (fscanf(fp, "%d %d %d", &id, &stock, &price) == 3) {
        Item *node = create_item(id, stock, price);
        bst_insert(&root, node);
        items[item_cnt++] = node;
    }
    Fclose(fp);
}

// show 명령어 처리 (내용 더 살펴보기)
void show(char *result) {
    char buf[MAXLINE] = "";
    for (int i = 0; i < item_cnt; i++) {
        Item *node = items[i];

        // Reader-Writer Lock
        P(&node->mutex);
        node->readcnt++;
        if (node->readcnt == 1) P(&w); // 첫 번째 reader가 writer 차단 
        V(&node->mutex);

        // 읽기 수행 : 주식 정보를 문자열로 포맷팅하여 버퍼에 누적 
        char line[64];
        snprintf(line, sizeof(line), "%d %d %d\n", node->ID, node->left_stock, node->price);
        strcat(buf, line);

        // Reader-Writer Unlock
        P(&node->mutex);
        node->readcnt--;
        if (node->readcnt == 0) V(&w); // 마지막 reader가 writer 해제 
        V(&node->mutex);
    }

    // reply에 결과 복사 
    strcpy(result, buf);
}

// 주식 정보 업데이트 (buy, sell에 따라)
bool update_stock(Item *node, int id, int amount, bool buy) {
    if (!node) return false;
    if (id == node->ID) {
        P(&node->mutex);
        if (buy && node->left_stock < amount) {
            V(&node->mutex);
            return false;
        }
        node->left_stock += buy ? -amount : amount;
        V(&node->mutex);
        return true;
    } else if (id < node->ID) {
        return update_stock(node->left, id, amount, buy);
    } else {
        return update_stock(node->right, id, amount, buy);
    }
}

// 클라이언트 명령 처리 함수 
void handle_client(int connfd) {
    char buf[MAXLINE], reply[MAXLINE];
    rio_t rio;
    int n;
    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0) { // 클라이언트 요청 수신 
        printf("server received %d bytes\n", n);

        int id, amount;
        memset(reply, 0, MAXLINE); // 응답 버퍼 초기화 
        
        if (!strncmp(buf, "show", 4)) { // show 요청 
            show(reply); // show 함수 호출, 주식 정보 출력 
        } else if (!strncmp(buf, "buy", 3)) { // buy 요청 
            sscanf(buf + 4, "%d %d", &id, &amount); // ID와 수량 파싱 
            // 구매 성공하면 주식 재고 감소 [buy] success 출력 , 수량이 부족하면 안내 문구 출력 
            snprintf(reply, MAXLINE, update_stock(root, id, amount, true) ? "[buy] success\n" : "Not enough left stocks\n");
        } else if (!strncmp(buf, "sell", 4)) { // sell 요청 
            sscanf(buf + 5, "%d %d", &id, &amount); // ID와 수량 파싱
            update_stock(root, id, amount, false); // 주식 재고 증가
            snprintf(reply, MAXLINE, "[sell] success\n"); // [sell] success 출력
        } else if (!strncmp(buf, "exit", 4)) { // exit 요청 
            write_stock_file(); // 업데이트 된 주식 정보 파일에 저장  
            Rio_writen(connfd, reply, MAXLINE); // 응답 전송 후 종료 
            break;
        }
        Rio_writen(connfd, reply, MAXLINE);
    }
}

// 워커 thread 함수
void *thread(void *vargp) {
    Pthread_detach(Pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf); // 버퍼에서 연결 가져오기
        handle_client(connfd);           // 클라이언트 처리 
        Close(connfd);

        P(&sem);
        num_conn--; 
        V(&sem);

        // 모든 클라이언트가 종료되었을 경우 stock.txt 저장
        if (num_conn == 0) {
            write_stock_file();
        }

    }
    return NULL;
}

// 서버 종료 시(Ctr+C) stock.txt에 업데이트 된 주식 정보 저장 
void sigint_handler(int signo){
    write_stock_file();
    exit(0);
}

// 메인 함수 : 서버 초기화 및 연결 대기 
int main(int argc, char **argv) {

    Signal(SIGINT, sigint_handler);

    char client_hostname[MAXLINE], client_port[MAXLINE];
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    //세마 포어 초기화(클라이언트 수 보호 & writer 보호)
    Sem_init(&sem, 0, 1);
    Sem_init(&w, 0, 1);

    load_data(FILENAME); // stock.txt에서 주식 정보 읽어오기
    sbuf_init(&sbuf, SBUFSIZE); // 공유 버퍼 초기화 

    int listenfd = Open_listenfd(argv[1]); //서버 소켓 생성 및 리스닝 시작
    pthread_t tid;
    for (int i = 0; i < NTHREADS; i++)
        Pthread_create(&tid, NULL, thread, NULL); // 워커 thread 미리 생성 

    // 클라이언트 연결 수락 및 큐에 추가 무한 루프
    while (1) {
        struct sockaddr_storage clientaddr;
        socklen_t clientlen = sizeof(clientaddr);

        // 연결 수락 
        int connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // 클라이언트 주소 정보 출력 
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        
        // 보호된 영역 진입하여 연결 수 증가 및 큐 삽입 
        P(&sem);
        sbuf_insert(&sbuf, connfd);
        num_conn++;
        V(&sem);
    }
}
