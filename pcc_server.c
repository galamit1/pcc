#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define SUCCESS 0
#define ERROR 1
#define QUEUE_MAX_LENGTH 10
#define PRINTABLE_CHARS_NUMBER (126 - 32 + 1)

int finish_run = 0, listenfd = -1, connfd = -1;
int *pcc_total[PRINTABLE_CHARS_NUMBER];

void signal_handler(int signal) {
    finish_run = 1;
    if (connfd >= 0) {
        return;
    }
    for (int i = 0; i < PRINTABLE_CHARS_NUMBER; i++) {
        printf("char '%c' : %u times\n", i + 32, (unsigned int)pcc_total[i]);
    }
    if (listenfd != -1) {
        close(listenfd);
    }
    exit(SUCCESS);
}

int init_sigint() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    return sigaction(SIGINT, &sa, NULL);
}

int calculate_printable_chars(char *data, int N) {
    int printable_chars = 0;
    char check;

    for (int i = 0; i < N; ++i) {
        check = *(data + i);
        if (check >= 32 && check <= 126) {
            pcc_total[check - 32] ++;
            printable_chars ++;
        }
    }
    return printable_chars;
}

void handle_client() {
    int N_sent, N, number_read, remain_to_read, total_read, printable_chars, printable_chars_to_send;
    char *data;

    remain_to_read = sizeof(int), total_read = 0;
    while (total_read < remain_to_read) {
        number_read = read(connfd, &N_sent, remain_to_read);
        if (number_read <= 0) {
            fprintf(stderr, "Error reading N\n");
            return;
        }
        remain_to_read -= number_read;
        total_read += number_read;
    }

    if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
        fprintf(stderr, "Error reading N\n");
        return;
    }
    N = ntohl(N_sent);

    data = malloc(N * sizeof(char));
    if (data == NULL) {
        fprintf(stderr, "Error allocating memory\n");
        return;
    }

    remain_to_read = N, total_read = 0;
    while (total_read < remain_to_read) {
        number_read = read(connfd, data, remain_to_read);
        if (number_read <= 0) {
            fprintf(stderr, "Error reading data\n");
            return;
        }
        remain_to_read -= number_read;
        total_read += number_read;
    }
    if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
        fprintf(stderr, "Error reading data\n");
        free(data);
        return;
    }
    printable_chars = calculate_printable_chars(data, N);
    printable_chars_to_send = htonl(printable_chars);
    write(connfd, &printable_chars_to_send, sizeof(int));
    if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
        fprintf(stderr, "Error writing number\n");
        free(data);
        return;
    }
}

void run_server(uint16_t port) {
    struct sockaddr_in serv_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd, &serv_addr, addrsize) != 0) {
        fprintf(stderr, "Error binding: %s\n", strerror(errno));
        exit(ERROR);
    }

    if (listen(listenfd, QUEUE_MAX_LENGTH) != 0) {
        fprintf(stderr, "Error listening: %s \n", strerror(errno));
        exit(ERROR);
    }

    while (finish_run == 0) {
        connfd = accept(listenfd, (struct sockaddr*) &peer_addr, &addrsize);
        if (connfd < 0) {
            fprintf(stderr, "Error accepting: %s \n", strerror(errno));
            exit(ERROR);
        }
        handle_client();
        close(connfd);
        connfd = -1;
    }
    // because finish_run != 0 it means there was a sigint
    for (int i = 0; i < PRINTABLE_CHARS_NUMBER; i++) {
        printf("char '%c' : %u times\n", i + 32, (unsigned int)pcc_total[i]);
    }
    close(connfd);
    close(listenfd);
}

int main(int argc, char *argv[]) {
    int port;

    if (argc != 2) {
        fprintf(stderr, "Wrong arguments number\n");
        exit(ERROR);
    }

    port = (int)strtol(argv[1], NULL, 10);
    if (port < 1024 || port > 365535) {
        fprintf(stderr, "Wrong port number\n");
        exit(ERROR);
    }
    init_sigint();
    run_server(port);
    return SUCCESS;
}