#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <memory.h>
#include <arpa/inet.h>
#include <zconf.h>

#define SUCCESS 0
#define ERROR 1

int get_file_size(FILE *file) { //credit for stack overflow
    long size;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return (int)size;
}

void handle_error(int  sockfd) {
    if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
        fprintf(stderr, "Error: %s \n", strerror(errno));
        close(sockfd);
        exit(ERROR);
    }
}

void run_client(char *ip, uint16_t port, int N, char *data) {
    int connfd, N_to_send, number_of_printable_chars, number_read;
    struct sockaddr_in serv_addr;

    if((connfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket\n");
        exit(ERROR);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip);

    if(connect(connfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error connecting: %s\n", strerror(errno));
        exit(ERROR);
    }

    N_to_send = htonl(N);
    write(connfd, &N_to_send, sizeof(int));
    handle_error(connfd);

    write(connfd, data, N);
    handle_error(connfd);

    while (1) {
        number_read = read(connfd, &number_of_printable_chars, sizeof(int));
        if (number_read <= 0) {
            break;
        }
        handle_error(connfd);
    }

    printf("# of printable characters: %u\n", ntohl(number_of_printable_chars));

    close(connfd);
    exit(SUCCESS);
}

int main(int argc, char *argv[]) {
    char *ip, *path, *data;
    uint16_t port;
    int N;

    if (argc != 4) {
        fprintf(stderr, "Wrong arguments number\n");
        exit(ERROR);
    }
    ip = argv[1];

    port = (uint16_t)strtol(argv[2], NULL, 10);
    if (port < 1024 || port > 365535) {
        fprintf(stderr, "Wrong port number\n");
        exit(ERROR);
    }

    path = argv[3];
    FILE *file;
    if ((file = fopen(path, "r")) == NULL) {
        fprintf(stderr, "Error open file: %s\n", strerror(errno));
        exit(ERROR);
    }

    N = get_file_size(file);
    data = malloc(sizeof(char) * N);
    if (data == NULL) {
        fprintf(stderr, "Error allocating memory\n");
        exit(ERROR);
    }
    fread(data, sizeof(char), N, file);
    run_client(ip, port, N, data);

    return SUCCESS;
}