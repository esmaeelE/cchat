#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

/*
 * Common header for the chat application.
 *
 * Defines shared protocol constants and declarations used by both the
 * server and the client.
 *
 * Protocol:
 *   Every message is length-prefixed: a 4-byte big-endian (network order)
 *   uint32 length followed by exactly that many payload bytes. The length
 *   counts only the payload, not the 4-byte prefix. Framing this way lets
 *   readers recover message boundaries over a stream socket regardless of
 *   how TCP segments the underlying bytes.
 */

#define BUF_SIZE 1000     /* max bytes read into a single receive buffer        */
#define MAX_CLIENTS 32    /* maximum simultaneous clients the server handles    */
#define NICK_SIZE 32      /* maximum nickname length (incl. terminating NUL)    */

/* Parse a decimal port string into an int in the range [1, 65535].
 * Prints an error and exits on invalid input. */
int parse_port(char *str);

/* Send a length-prefixed message over fd.
 * Writes the 4-byte network-order length, then len payload bytes.
 * Returns 0 on success, -1 on error (caller should close fd). */
int send_message(int fd, const void *data, uint32_t len);

/* Receive a length-prefixed message from fd into buf.
 * Reads the 4-byte length, validates it fits in bufsize, then reads the
 * payload. On success stores the payload length in *out_len (if non-NULL)
 * and returns 1. Returns 0 on clean EOF, -1 on error. */
int recv_message(int fd, void *buf, uint32_t bufsize, uint32_t *out_len);

#endif