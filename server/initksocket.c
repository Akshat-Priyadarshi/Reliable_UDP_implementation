#include <ksocket.h>
#define SELECT_TIMEOUT 100000
#define CLOSE_SOCKET(i) do { \
    FD_CLR(SM[i].sockfd, &master); \
    SM[i].is_free = true; \
    close(SM[i].sockfd); \
    printf("Closed KTP socket %d\n", i); \
} while (0)

fd_set master;

void strip_msg(char buf[], char *type, u_int16_t *seq, u_int16_t *rwnd, char *msg){
    memcpy(type, buf, MSGTYPE);
    *seq = ntohs(*((u_int16_t *)(buf + MSGTYPE)));
    *rwnd = ntohs(*((u_int16_t *)(buf + MSGTYPE + sizeof(u_int16_t))));
    memcpy(msg, buf + HEADERSIZE, MSGSIZE);
}

ssize_t send_ack(usockfd_t sockfd, struct sockaddr_in dest_addr, u_int16_t seq, u_int16_t rwnd){
    char buff[PACKETSIZE];
    char type[MSGTYPE] = "ACK\0";
    u_int16_t nseq = htons(seq), nrwnd = htons(rwnd);
    memcpy(buff, type, MSGTYPE);
    memcpy(buff + MSGTYPE, &nseq, sizeof(u_int16_t));
    memcpy(buff + MSGTYPE + sizeof(u_int16_t), &nrwnd, sizeof(u_int16_t));
    return sendto(sockfd, buff, PACKETSIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}

ssize_t send_data(usockfd_t sockfd, struct sockaddr_in dest_addr, u_int16_t seq, const char *msg){
    char buff[PACKETSIZE];
    char type[MSGTYPE] = "DATA";
    u_int16_t nseq = htons(seq);
    u_int16_t nrwnd = htons(0);
    memcpy(buff, type, MSGTYPE);
    memcpy(buff + MSGTYPE, &nseq, sizeof(u_int16_t));
    memcpy(buff + MSGTYPE + sizeof(u_int16_t), &nrwnd, sizeof(u_int16_t));
    memcpy(buff + MSGTYPE + 2 * sizeof(u_int16_t), msg, MSGSIZE);
    return sendto(sockfd, buff, PACKETSIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}


ssize_t send_fin(usockfd_t sockfd, struct sockaddr_in dest_addr){
    char buff[PACKETSIZE];
    char type[MSGTYPE] = "FIN\0";
    u_int16_t nseq = htons(0), nrwnd = htons(0);
    memcpy(buff, type, MSGTYPE);
    memcpy(buff + MSGTYPE, &nseq, sizeof(u_int16_t));
    memcpy(buff + MSGTYPE + sizeof(u_int16_t), &nrwnd, sizeof(u_int16_t));
    return sendto(sockfd, buff, PACKETSIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}

ssize_t send_fin_ack(usockfd_t sockfd, struct sockaddr_in dest_addr){
    char buff[PACKETSIZE];
    char type[MSGTYPE] = "FAK\0";
    u_int16_t nseq = htons(0), nrwnd = htons(0);
    memcpy(buff, type, MSGTYPE);
    memcpy(buff + MSGTYPE, &nseq, sizeof(u_int16_t));
    memcpy(buff + MSGTYPE + sizeof(u_int16_t), &nrwnd, sizeof(u_int16_t));
    return sendto(sockfd, buff, PACKETSIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}

static inline void close_socket(k_sockinfo *SM, int i, fd_set *master){
    if (SM[i].sockfd != -1) {
        FD_CLR(SM[i].sockfd, master);
        close(SM[i].sockfd);
        SM[i].sockfd = -1;
    }

    SM[i].is_free = true;

    printf("Closed KTP socket %d\n", i);
}

void cleanup(int signo){
    int shmid = k_shmget();
    int semid = k_semget();

    if (shmid != -1){
        shmctl(shmid, IPC_RMID, 0);
        printf("SHM %d removed\n", shmid);
    }

    if (semid != -1){
        semctl(semid, 0, IPC_RMID);
        printf("SEM %d removed\n", semid);
    }

    if (signo == SIGSEGV){
        printf("Segmentation fault\n");
    }
    exit(0);
}

void initk_shm(){
    key_t key = ftok(SHM_PATH, SHM_ID);
    int shmid = shmget(key, N * sizeof(k_sockinfo), IPC_CREAT | 0777 | IPC_EXCL);
    if (shmid == -1){
        perror("initsocket: shmget");
        exit(1);
    }

    k_sockinfo *SM = (k_sockinfo *)shmat(shmid, NULL, 0);
    if (SM == (void *)-1){
        perror("initsocket: shmat");
        exit(1);
    }

    for (int i = 0; i < N; i++){
        SM[i].is_free = true;
    }

    printf("Shared memory initialized with ID: %d\n", shmid);
    shmdt(SM);
}

void initk_sem(){
    key_t key = ftok(SEM_PATH, SEM_ID);
    int semid = semget(key, N, IPC_CREAT | 0777 | IPC_EXCL);
    if (semid == -1){
        perror("initsocket: semget");
        exit(1);
    }

    printf("Semaphore initialized with ID: %d\n", semid);

    union semun arg;
    unsigned short vals[N];

    for (int i = 0; i < N; i++)
        vals[i] = 1;

    arg.array = vals;
    if (semctl(semid, 0, SETALL, arg) == -1){
        perror("initsocket: semctl");
        exit(1);
    }
}
void *threadR(void *arg){
    fd_set rfds;
    usockfd_t maxfd = 0;
    struct timeval tv;

    FD_ZERO(&master);

    int semid = k_semget();
    k_sockinfo *SM = k_shmat();

    char buff[PACKETSIZE];

    for (;;){
        rfds = master;
        tv.tv_sec = 0;
        tv.tv_usec = SELECT_TIMEOUT;

        select(maxfd + 1, &rfds, NULL, NULL, &tv);
        int recvsocket = -1;
        ssize_t numbytes = -1;
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        for (int i = 0; i < N; i++){
            wait_sem(semid, i);
            if (!SM[i].is_free && SM[i].is_bound && FD_ISSET(SM[i].sockfd, &rfds)){
                recvsocket = SM[i].sockfd;
                numbytes = recvfrom(SM[i].sockfd, buff, PACKETSIZE, 0, (struct sockaddr *)&sender_addr, &addr_len);
            }
            signal_sem(semid, i);
            if (recvsocket != -1)
                break;
        }

        if (recvsocket != -1){
            if (numbytes < 0){
                perror("recvfrom");
            }
            else if (numbytes == 0){
                for (int i = 0; i < N; i++){
                    wait_sem(semid, i);
                    if (!SM[i].is_free && SM[i].is_bound && SM[i].sockfd == recvsocket){
                        printf("R: Connection closed by other end\n");
                        SM[i].is_closed = true;
                    }
                    signal_sem(semid, i);
                }
            }
            else{
                for (int i = 0; i < N; i++){
                    wait_sem(semid, i);
                    if (!SM[i].is_free && SM[i].is_bound && SM[i].sockfd == recvsocket && SM[i].dest_addr.sin_addr.s_addr == sender_addr.sin_addr.s_addr && SM[i].dest_addr.sin_port == sender_addr.sin_port){
                        char type[MSGTYPE + 1], msg[MSGSIZE];
                        u_int16_t seq, rwnd;
                        strip_msg(buff, type, &seq, &rwnd, msg);
                        if (dropMessage(P)){
                            printf("Dropped: %s %d coming through ksocket %d\n", type, seq, i);
                            signal_sem(semid, i);
                            continue;
                        }
                        if (!strcmp(type, "DATA")){
                            printf("R: DATA %d through ksocket %d\n", seq, i);
                            SM[i].nospace = false;
                            bool duplicate = true;
                            for (int j = SM[i].rwnd.base, ctr = 0; ctr < SM[i].rwnd.size; j = (j + 1) % WINDOWSIZE, ctr++){
                                if (SM[i].rwnd.msg_seq[j] == seq){
                                    if (!SM[i].rwnd.received[j]){
                                        duplicate = false;
                                        SM[i].rwnd.received[j] = true;
                                        memcpy(SM[i].recv_buff[j], msg, MSGSIZE);

                                        int new_last_ack = -1;
                                        for (int k = SM[i].rwnd.base, ct = 0; ct < SM[i].rwnd.size; k = (k + 1) % WINDOWSIZE, ct++){
                                            if (!SM[i].rwnd.received[k])
                                                break;
                                            new_last_ack = k;
                                        }
                                        if (new_last_ack != -1){
                                            SM[i].rwnd.last_ack = SM[i].rwnd.msg_seq[new_last_ack];
                                            for (int k = SM[i].rwnd.base, ct = 0;; k = (k + 1) % WINDOWSIZE, ct++){
                                                SM[i].rwnd.msg_seq[k] = (SM[i].rwnd.last_seq) % MAXSEQ + 1;
                                                SM[i].rwnd.last_seq = SM[i].rwnd.msg_seq[k];
                                                if (k == new_last_ack)
                                                    break;
                                            }

                                            SM[i].rwnd.size -= (new_last_ack - SM[i].rwnd.base + WINDOWSIZE) % WINDOWSIZE + 1;
                                            SM[i].rwnd.base = (new_last_ack + 1) % WINDOWSIZE;
                                            printf("R: Sent ACK %d %d through ksocket %d\n", SM[i].rwnd.last_ack, SM[i].rwnd.size, i);
                                            int n = send_ack(SM[i].sockfd, SM[i].dest_addr, SM[i].rwnd.last_ack, SM[i].rwnd.size);
                                            if (n < 0){
                                                perror("send_ack");
                                            }
                                        }
                                    }

                                    break;
                                }
                            }
                            if (duplicate){
                                printf("R: Duplicate message received: %u\t Sent ACK %d %d through ksocket %d\n", seq, SM[i].rwnd.last_ack, SM[i].rwnd.size, i);
                                int n = send_ack(SM[i].sockfd, SM[i].dest_addr, SM[i].rwnd.last_ack, SM[i].rwnd.size);
                                if (n < 0){
                                    perror("send_ack");
                                }
                            }

                            if (SM[i].rwnd.size == 0){
                                SM[i].nospace = true;
                            }
                        }
                        else if (!strcmp(type, "ACK")){
                            printf("R: ACK %d %d through ksocket %d\n", seq, rwnd, i);
                            for (int j = SM[i].swnd.base, ctr = 0; ctr < SM[i].swnd.size; j = (j + 1) % WINDOWSIZE, ctr++){
                                if (SM[i].swnd.msg_seq[j] == seq){
                                    for (int k = SM[i].swnd.base;; k = (k + 1) % WINDOWSIZE){
                                        SM[i].swnd.timeout[k] = -1;
                                        SM[i].send_buff_empty[k] = true;
                                        SM[i].swnd.msg_seq[k] = (SM[i].swnd.last_seq) % MAXSEQ + 1;
                                        SM[i].swnd.last_seq = SM[i].swnd.msg_seq[k];
                                        if (k == j)
                                            break;
                                    }
                                    SM[i].swnd.base = (j + 1) % WINDOWSIZE;
                                    break;
                                }
                            }
                            SM[i].swnd.size = rwnd;
                        }
                        else if(!strcmp(type, "FIN")){
                            printf("R: FIN through ksocket %d\n", i);
                            int n = send_fin_ack(SM[i].sockfd, SM[i].dest_addr);
                            if(n < 0){
                                perror("send_fin_ack");
                            }
                            CLOSE_SOCKET(i);
                        }
                        else if(!strcmp(type, "FAK")){
                            printf("R: FAK through ksocket %d\n", i);
                            CLOSE_SOCKET(i);
                        }
                        else{
                            printf("Invalid message type: %s\n", type);
                        }
                    }
                    signal_sem(semid, i);
                }
            }
        }
        else{
            for (int i = 0; i < N; i++){
                wait_sem(semid, i);
                if (!SM[i].is_free){
                    if (!SM[i].is_bound){
                        usockfd_t ksockfd = socket(AF_INET, SOCK_DGRAM, 0);
                        if (ksockfd < 0){
                            perror("socket");
                        }
                        else{
                            if (bind(ksockfd, (struct sockaddr *)&SM[i].src_addr, sizeof(SM[i].src_addr)) < 0){
                                perror("bind");
                                close(ksockfd);
                            }
                            else{
                                SM[i].sockfd = ksockfd;
                                FD_SET(SM[i].sockfd, &master);
                                if (SM[i].sockfd > maxfd)
                                    maxfd = SM[i].sockfd;
                                SM[i].is_bound = true;
                                printf("R: Bound KTP socket %d to UDP socket %d\n", i, SM[i].sockfd);
                            }
                        }
                    }
                    else if (SM[i].nospace && SM[i].rwnd.size > 0){
                        int n = send_ack(SM[i].sockfd, SM[i].dest_addr, SM[i].rwnd.last_ack, SM[i].rwnd.size);
                        if (n < 0){
                            perror("send_ack");
                        }
                    }
                }
                signal_sem(semid, i);
            }
        }
    }
    return NULL;
}
void *threadS(void *arg){
    int semid = k_semget();
    k_sockinfo *SM = k_shmat();

    while (1){
        sleep(T / 2);
        for (int i = 0; i < N; i++){
            wait_sem(semid, i);
            if (!SM[i].is_free && SM[i].is_bound){
                if(SM[i].is_closed){
                    if(SM[i].fin_time==-1){
                        printf("S: FIN through ksocket %d\n", i);
                        int n = send_fin(SM[i].sockfd, SM[i].dest_addr);
                        if(n < 0){
                            perror("send_fin");
                        }
                        SM[i].fin_time = time(NULL);
                    }
                    else if(time(NULL) - SM[i].fin_time >= T){
                        if(SM[i].fin_retries<F){
                            printf("S: FIN through ksocket %d (retry %d)\n", i, ++SM[i].fin_retries);
                            int n = send_fin(SM[i].sockfd, SM[i].dest_addr);
                            if(n < 0){
                                perror("send");
                            }
                            SM[i].fin_time = time(NULL);
                        }
                        else{
                            printf("S: FAK not received; closing ksocket %d\n", i);
                            CLOSE_SOCKET(i);
                        }
                    }
                }
                else{
                    bool timeout = (SM[i].swnd.timeout[SM[i].swnd.base] > 0 && (time(NULL) - SM[i].swnd.timeout[SM[i].swnd.base]) >= T);

                    if (timeout){
                        for (int j = SM[i].swnd.base, ctr = 0; ctr < SM[i].swnd.size; j = (j + 1) % WINDOWSIZE, ctr++){
                            if (SM[i].swnd.timeout[j] == -1)
                                break;
                            printf("S: Timeout; DATA %u through ksocket %d\n", SM[i].swnd.msg_seq[j], i);
                            int n = send_data(SM[i].sockfd, SM[i].dest_addr, SM[i].swnd.msg_seq[j], SM[i].send_buff[j]);
                            if (n < 0){
                                perror("send_data");
                            }
                            SM[i].swnd.timeout[j] = time(NULL) + T;
                        }
                    }
                }
                
            }
            signal_sem(semid, i);
        }

        for (int i = 0; i < N; i++){
            wait_sem(semid, i);
            if (!SM[i].is_free && SM[i].is_bound){
                for (int j = SM[i].swnd.base, ctr = 0; ctr < SM[i].swnd.size; j = (j + 1) % WINDOWSIZE, ctr++){
                    if (SM[i].swnd.timeout[j] == -1){
                        if (!SM[i].send_buff_empty[j]){
                            printf("S: DATA %u through ksocket %d\n", SM[i].swnd.msg_seq[j], i);
                            int n = send_data(SM[i].sockfd, SM[i].dest_addr, SM[i].swnd.msg_seq[j], SM[i].send_buff[j]);
                            if (n < 0){
                                perror("send_data");
                            }
                            SM[i].swnd.timeout[j] = time(NULL) + T;
                        }
                    }
                }
            }
            signal_sem(semid, i);
        }
    }
}
void *threadG(void *arg){
    int semid = k_semget();
    k_sockinfo *SM = k_shmat();

    while (1){
        sleep(T);
        for (int i = 0; i < N; i++){
            wait_sem(semid, i);
            if (!SM[i].is_free && !SM[i].is_closed){
                if (kill(SM[i].pid, 0) == -1){
                    printf("G: Process %d terminated\n", SM[i].pid);
                    SM[i].is_closed = true;
                }
            }
            signal_sem(semid, i);
        }
    }
}

int main(){
    srand(time(NULL));

    initk_shm();
    initk_sem();

    FD_ZERO(&master);

    signal(SIGINT, cleanup);
    signal(SIGSEGV, cleanup);

    pthread_t rid, sid, gid;
    pthread_create(&rid, NULL, threadR, NULL);
    pthread_create(&sid, NULL, threadS, NULL);
    pthread_create(&gid, NULL, threadG, NULL);

    pthread_exit(NULL);
}