// Mini Project 1 Submission
// Group Details:
// Member 1 Name: Akshat Priyadarshi
// Member 1 Roll number: 23CS30003
// Member 2 Name: Aman Tudu
// Member 2 Roll number: 23CS30004

#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<stdbool.h>
#include<unistd.h>
#include<signal.h>
#include<sys/select.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/sem.h>
#include<errno.h>
#include<pthread.h>
#include<time.h>

typedef int ksockfd_t;
typedef int usockfd_t;
#define SOCK_KTP 256

#define MSGSIZE 512
#define MSGTYPE 4
#define HEADERSIZE (MSGTYPE + 2 * sizeof(u_int16_t))
#define PACKETSIZE (HEADERSIZE + MSGSIZE)
#define SEQSIZE 5
#define MAXSEQ (1 << SEQSIZE)
#define BUFFSIZE 10
#define WINDOWSIZE BUFFSIZE
#define T 5
#define N 10
#define P 0.3
#define F 5

#define ENOSPACE ENOSPC
#define ENOTBOUND ENOTCONN
#define ENOMESSAGE ENOMSG

#define SHM_PATH "/ktp_shm"
#define SHM_ID 'K'

#define SEM_PATH "/ktp_sem"
#define SEM_ID 'K'

typedef struct window
{
    int base;
    u_int16_t size;
    u_int16_t msg_seq[WINDOWSIZE];
    u_int16_t last_seq;
    u_int16_t last_ack;
    bool received[WINDOWSIZE];
    time_t timeout[WINDOWSIZE];
} window;

typedef struct k_sockinfo
{
    bool is_free;
    pid_t pid;
    usockfd_t sockfd;
    struct sockaddr_in dest_addr;
    struct sockaddr_in src_addr;
    bool is_bound;
    char send_buff[BUFFSIZE][MSGSIZE];
    char recv_buff[BUFFSIZE][MSGSIZE];
    bool send_buff_empty[BUFFSIZE];
    window swnd;
    window rwnd;
    bool nospace;
    bool is_closed;
    time_t fin_time;
    int fin_retries;
} k_sockinfo;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

ksockfd_t k_socket(int domain, int type, int protocol);

int k_bind(ksockfd_t sockfd, const char *src_ip, int src_port, const char *dest_ip, int dest_port);

ssize_t k_sendto(ksockfd_t sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

ssize_t k_recvfrom(ksockfd_t sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

int k_close(ksockfd_t fd);

int k_shmget();

k_sockinfo *k_shmat();

int k_shmdt(k_sockinfo *);

int k_semget();

void wait_sem(int semid, ksockfd_t i);

void signal_sem(int semid, ksockfd_t i);

window init_window();

bool dropMessage(float p);