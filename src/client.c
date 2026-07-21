//~ ./client 127.0.0.1 9090

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/select.h>

#include "common.h"

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    char *host = "127.0.0.1";
    int port = 9090;

    if (argc > 2) {
        host = argv[1];
        port = parse_port(argv[2]);
    } else if (argc > 1) {
        host = argv[1];
    }

    struct sigaction sa = { .sa_handler = handle_sigint };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(host),
        .sin_port = htons(port)
    };
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }

    /* Send nickname as first message */
    char nick[NICK_SIZE];
    printf("Enter nickname: ");
    fflush(stdout);
    if (fgets(nick, sizeof(nick), stdin) == NULL) {
        close(fd);
        return 1;
    }
    uint32_t nick_len = (uint32_t)strlen(nick);
    if (nick_len == 0 || send_message(fd, nick, nick_len) < 0) {
        fprintf(stderr, "Failed to send nickname.\n");
        close(fd);
        return 1;
    }

    printf("Connected to %s:%d. Type messages, Ctrl+C to quit.\n", host, port);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd, &rfds);
        int max_fd = fd > STDIN_FILENO ? fd : STDIN_FILENO;

        if (select(max_fd + 1, &rfds, NULL, NULL, NULL) < 0) {
            if (!running) break;
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char send_buf[BUF_SIZE];
            if (fgets(send_buf, sizeof(send_buf), stdin) == NULL)
                break;
            uint32_t len = (uint32_t)strlen(send_buf);
            if (len > 0 && send_message(fd, send_buf, len) < 0) {
                fprintf(stderr, "Send failed.\n");
                break;
            }
        }

        if (FD_ISSET(fd, &rfds)) {
            char recv_buf[BUF_SIZE];
            uint32_t msg_len = 0;
            int rc = recv_message(fd, recv_buf, BUF_SIZE - 1, &msg_len);
            if (rc <= 0) {
                printf("\nServer disconnected.\n");
                break;
            }
            recv_buf[msg_len] = '\0';
            printf("\rReceived> %s", recv_buf);
        }
    }

    close(fd);
    return 0;
}
