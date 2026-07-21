#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common.h"

int parse_port(char *str) {
    char *p;
    long arg = strtol(str, &p, 10);
    if (*p != '\0' || arg < 1 || arg > 65535) {
        fprintf(stderr, "Invalid port: %s (must be 1-65535)\n", str);
        exit(1);
    }
    return (int)arg;
}

static int write_all(int fd, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = write(fd, p + sent, n - sent);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0)
            return -1;
        sent += (size_t)w;
    }
    return 0;
}

static int read_exact(int fd, void *buf, size_t n) {
    char *p = (char *)buf;
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, p + got, n - got);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0)
            return 0;
        got += (size_t)r;
    }
    return 1;
}

int send_message(int fd, const void *data, uint32_t len) {
    uint32_t net_len = htonl(len);
    if (write_all(fd, &net_len, sizeof(net_len)) < 0)
        return -1;
    if (len > 0 && write_all(fd, data, len) < 0)
        return -1;
    return 0;
}

int recv_message(int fd, void *buf, uint32_t bufsize, uint32_t *out_len) {
    uint32_t net_len;
    int rc = read_exact(fd, &net_len, sizeof(net_len));
    if (rc <= 0) return rc;
    uint32_t len = ntohl(net_len);
    if (len > bufsize) {
        fprintf(stderr, "Message too large: %u\n", len);
        return -1;
    }
    rc = read_exact(fd, buf, len);
    if (rc <= 0) return rc;
    if (out_len) *out_len = len;
    return 1;
}
