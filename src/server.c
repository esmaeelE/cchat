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

/*
 * Multi-client TCP chat server.
 *
 * Architecture:
 *   - Single-threaded event loop using select() to multiplex the listening
 *     socket and all connected client sockets.
 *   - Supports up to MAX_CLIENTS (32) simultaneous clients.
 *   - Each client must send its nickname as the first message; subsequent
 *     messages are broadcast to every other joined client.
 *   - SIGINT/SIGTERM trigger graceful shutdown: all client sockets and the
 *     listening socket are closed before exit.
 *   - SIGPIPE is ignored so a write to a disconnected client is reported via
 *     the return value of send_message instead of killing the process.
 *
 * Client state is stored in a fixed-size Client table; a slot with fd < 0
 * is free.
 */

typedef struct {
    int fd;             /* connected socket, or -1 when slot is free      */
    char nick[NICK_SIZE];/* client's chosen nickname                       */
    int has_nick;       /* 1 once the nickname handshake is complete      */
} Client;

static volatile sig_atomic_t running = 1;

/* SIGINT/SIGTERM handler: sets running to 0 so the main loop exits and
 * performs a graceful shutdown. */
static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

/* Broadcast data of length len to every joined client except sender.
 * Skips free slots and clients that have not completed the nickname
 * handshake (they are not yet part of the chat). */
static void broadcast(Client *clients, int sender, const char *data, uint32_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0 && clients[i].fd != sender && clients[i].has_nick)
            send_message(clients[i].fd, data, len);
    }
}

int main(int argc, char *argv[]) {
    int port = 9090;
    if (argc > 1)
        port = parse_port(argv[1]);

    /* Install signal handlers for graceful shutdown on SIGINT/SIGTERM. */
    struct sigaction sa = { .sa_handler = handle_sigint };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    /* Ignore SIGPIPE: writes to closed sockets are handled via return codes. */
    signal(SIGPIPE, SIG_IGN);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(port)
    };
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind"); exit(1);
    }
    if (listen(listen_fd, SOMAXCONN) != 0) {
        perror("listen"); exit(1);
    }
    printf("Server listening on port %d\n", port);

    Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].nick[0] = '\0';
        clients[i].has_nick = 0;
    }

    /* Main event loop: multiplex on listen_fd and all active client fds. */
    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        int max_fd = listen_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0) {
                FD_SET(clients[i].fd, &rfds);
                if (clients[i].fd > max_fd)
                    max_fd = clients[i].fd;
            }
        }

        if (select(max_fd + 1, &rfds, NULL, NULL, NULL) < 0) {
            if (!running) break;   /* interrupted by signal: exit loop */
            perror("select");
            break;
        }

        /* New connection: accept and allocate a free client slot. */
        if (FD_ISSET(listen_fd, &rfds)) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int cli_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
            if (cli_fd >= 0) {
                int added = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd < 0) {
                        clients[i].fd = cli_fd;
                        clients[i].nick[0] = '\0';
                        clients[i].has_nick = 0;
                        added = 1;
                        break;
                    }
                }
                if (added)
                    printf("+ %s:%d connected\n", inet_ntoa(cli_addr.sin_addr),
                           ntohs(cli_addr.sin_port));
                else {
                    fprintf(stderr, "Max clients reached\n");
                    close(cli_fd);
                }
            }
        }

        /* Read from ready clients. Each ready slot is handled once per loop. */
        char buf[BUF_SIZE];
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0 && FD_ISSET(clients[i].fd, &rfds)) {
                uint32_t msg_len = 0;
                int rc = recv_message(clients[i].fd, buf, BUF_SIZE - 1, &msg_len);
                if (rc <= 0) {
                    /* Client disconnected or errored: announce and free slot. */
                    if (clients[i].has_nick) {
                        char sys_msg[BUF_SIZE];
                        int n = snprintf(sys_msg, sizeof(sys_msg),
                                         "[System] %s left the chat\n", clients[i].nick);
                        if (n > 0)
                            broadcast(clients, clients[i].fd, sys_msg, (uint32_t)n);
                        printf("- %s disconnected\n", clients[i].nick);
                    } else {
                        printf("- Client fd %d disconnected\n", clients[i].fd);
                    }
                    close(clients[i].fd);
                    clients[i].fd = -1;
                    clients[i].has_nick = 0;
                } else if (!clients[i].has_nick) {
                    /* First message from a client is its nickname. */
                    buf[msg_len] = '\0';
                    char *nl = strchr(buf, '\n');
                    if (nl) *nl = '\0';
                    /* Truncate nickname to fit NICK_SIZE. */
                    if (msg_len >= NICK_SIZE) {
                        buf[NICK_SIZE - 1] = '\0';
                        msg_len = NICK_SIZE - 1;
                    }
                    memcpy(clients[i].nick, buf, msg_len + 1);
                    clients[i].has_nick = 1;
                    char sys_msg[BUF_SIZE];
                    int n = snprintf(sys_msg, sizeof(sys_msg),
                                     "[System] %s joined the chat\n", clients[i].nick);
                    if (n > 0)
                        broadcast(clients, clients[i].fd, sys_msg, (uint32_t)n);
                    printf("+ %s joined\n", clients[i].nick);
                } else {
                    /* Regular chat message: prefix with nickname and broadcast. */
                    buf[msg_len] = '\0';
                    char formatted[BUF_SIZE];
                    int n = snprintf(formatted, sizeof(formatted), "%s> %s",
                                     clients[i].nick, buf);
                    if (n > 0)
                        broadcast(clients, clients[i].fd, formatted, (uint32_t)n);
                    printf("%s> %s", clients[i].nick, buf);
                }
            }
        }
    }

    printf("\nShutting down.\n");
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd >= 0) close(clients[i].fd);
    close(listen_fd);
    return 0;
}
