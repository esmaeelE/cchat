#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define BUF_SIZE 1000
#define MAX_CLIENTS 32
#define NICK_SIZE 32

int parse_port(char *str);
int send_message(int fd, const void *data, uint32_t len);
int recv_message(int fd, void *buf, uint32_t bufsize, uint32_t *out_len);

#endif
