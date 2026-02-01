#include "csapp.h"

// select 기반 동시성 관리를 위한 client pool 구조체
typedef struct {
    int maxfd;                  // 가장 큰 파일 디스크립터 번호
    fd_set read_set;            // 전체 read set
    fd_set ready_set;           // select로 감지된 ready set
    int nready;                 // 준비된 파일 디스크립터 수
    int maxi;                   // client 배열 내 최대 인덱스
    int clientfd[FD_SETSIZE];   // 클라이언트 연결 소켓 FD 배열
    rio_t clientrio[FD_SETSIZE];// 각 클라이언트에 대한 rio 버퍼
} pool;

int byte_cnt = 0;

// 주식 정보를 나타내는 Binary Tree 노드 구조체
typedef struct __item {
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;        //task1에선 미사용
    struct __item *left;
    struct __item *right;
} item;

item *root = NULL;
item *ordered_items[10000]; // 주식 순서 보존을 위한 배열
int item_cnt = 0;

//새로운 주식(아이템) 생성 함수
item *create_item(int ID, int left_stock, int price) {
    item *node = (item *)Malloc(sizeof(item));
    node->ID = ID;
    node->left_stock = left_stock;
    node->price = price;
    node->readcnt = 0;
    Sem_init(&node->mutex, 0, 1);
    node->left = node->right = NULL;
    return node;
}

// 트리에 노드(주식) 삽입 
item *insert(item *root, item *node) {
    if (root == NULL) return node;
    if (node->ID < root->ID)
        root->left = insert(root->left, node);
    else if (node->ID > root->ID)
        root->right = insert(root->right, node);
    return root;
}


// ID를 이용하여 주식 find
item *find_id(item *root, int id) {
    item *node = root;
    while (node) {
        if (id == node->ID) return node;
        else if (id < node->ID) node = node->left;
        else node = node->right;
    }
    return NULL;
}

// 서버 종료 시 stock.txt에 업데이트 반영
void strsave(item *root, FILE *fp) {
    if (root == NULL) return;
    fprintf(fp, "%d %d %d\n", root->ID, root->left_stock, root->price);
    strsave(root->left, fp);
    strsave(root->right, fp);
}

// select 기반 pool 초기화
void init_pool(int listenfd, pool *p) {
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++) p->clientfd[i] = -1;
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

// 새로운 클라이언트를 pool에 추가 
void add_client(int connfd, pool *p) {
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++) {
        if (p->clientfd[i] < 0) {
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);
            FD_SET(connfd, &p->read_set);
            if (connfd > p->maxfd) p->maxfd = connfd;
            if (i > p->maxi) p->maxi = i;
            break;
        }
    }
}

// 클라이언트 응답 메시지 padding 후 전송 
void pad_and_send(int connfd, char *msg) {
    char padded[MAXLINE];
    memset(padded, ' ', MAXLINE);
    strncpy(padded, msg, MAXLINE - 1);
    padded[MAXLINE - 1] = '\n';
    Rio_writen(connfd, padded, MAXLINE);
}

//클라이언트 요청 처리
void check_clients(pool *p) {
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
                byte_cnt += n;

                printf("server received %d bytes\n", n);
                
                // show 명령어 처리
                if (!strcmp(buf, "show\n")) {
                    char tmp[MAXLINE];
                    char padded[MAXLINE];
                    size_t offset = 0;

                    memset(tmp, 0, sizeof(tmp));
                    memset(padded, 0, sizeof(padded));

                    for (int j = 0; j < item_cnt; ++j) {
                        item *it = ordered_items[j]; // stock.txt 순서대로 그대로 출력
                        int written = snprintf(tmp + offset, sizeof(tmp) - offset,
                                               "%d %d %d\n", it->ID, it->left_stock, it->price);
                        if (written < 0 || written >= sizeof(tmp) - offset) break;
                        offset += written;
                    }

                    strncpy(padded, tmp, MAXLINE);
                    for (int j = strlen(padded); j < MAXLINE - 1; j++) padded[j] = '\0';

                    Rio_writen(connfd, padded, MAXLINE);
                }
                // buy 명령어 처리
                else if (!strncmp(buf, "buy", 3)) {
                    int buy_id, buyN;
                    buf[strlen(buf) - 1] = '\0';
                    sscanf(buf + 4, "%d %d", &buy_id, &buyN);
                    item *snode = find_id(root, buy_id);
                    if (snode->left_stock < buyN)
                        strcpy(buf, "Not enough left stock\n");
                    else {
                        snode->left_stock -= buyN; // 주식 재고 수 감소 
                        strcpy(buf, "[buy] success\n");
                    }
                    pad_and_send(connfd, buf);
                }
                // sell 명령어 처리 
                else if (!strncmp(buf, "sell", 4)) {
                    int sell_id, sellN;
                    buf[strlen(buf) - 1] = '\0';
                    sscanf(buf + 4, "%d %d", &sell_id, &sellN);
                    item *snode = find_id(root, sell_id);
                    snode->left_stock += sellN; // 주식 재고 수 증가 
                    strcpy(buf, "[sell] success\n");
                    pad_and_send(connfd, buf);
                }
                // exit 명령어 처리 
                else if (!strcmp(buf, "exit\n")) {
                    FILE *fp = fopen("stock.txt", "w");
                    if (fp != NULL) {
                        strsave(root, fp);
                        fclose(fp);
                    }
                    Close(connfd);
                    FD_CLR(connfd, &p->read_set);
                    p->clientfd[i] = -1;
                }
                else {
                    Rio_writen(connfd, buf, n);
                }
            } else { // 클라이언트 종료 처리
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }

    // 모든 클라이언트가 종료되었는지 확인
    int all_disconnected = 1;
    for (int k = 0; k <= p->maxi; k++) {
        if (p->clientfd[k] != -1) {
            all_disconnected = 0;
            break;
        }
    }

    // 모든 클라이언트가 종료되었으면 stock.txt 저장
    if (all_disconnected) {
        FILE *fp = fopen("stock.txt", "w");
        if (fp != NULL) {
            strsave(root, fp);
            fclose(fp);
        }
    }
}

// SIGINT(Ctrl+C) 시 서버 종료되면서 stock.txt 저장 후 종료 
void sigint_handler(int signo) {
    FILE *fp = fopen("stock.txt", "w");
    if (fp != NULL) {
        strsave(root, fp);
        fclose(fp);
    }
    exit(1);
}

int main(int argc, char **argv) {
    Signal(SIGINT, sigint_handler);
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    FILE *fp = fopen("stock.txt", "r");
    if (fp == NULL) {
        printf("Error: stock.txt file does not exist.\n");
        return 0;
    } else {
        int stid, leftst, stprice;
        while (EOF != fscanf(fp, "%d %d %d\n", &stid, &leftst, &stprice)) {
            item *node = create_item(stid, leftst, stprice);
            root = insert(root, node); // 동일 노드 삽입
            ordered_items[item_cnt++] = node; // 동일 노드 저장
        }
        fclose(fp);
    }

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
            add_client(connfd, &pool);
        }
        check_clients(&pool);
    }
    exit(0);
}