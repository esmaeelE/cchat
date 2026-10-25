# Improvement Plan — chat application

Priority-ordered suggestions for hardening the TCP chat app (`src/server.c`, `src/client.c`, `src/common.c`).

## P1 — Broadcast error handling (`server.c`)

**Problem:** `broadcast()` ignores the return value of `send_message()`. If a client disconnects mid-broadcast, the failure is silent and the message is dropped; the dead fd is only cleaned up on its next readable event.

**Fix:** Check `send_message()`'s return in `broadcast()`. On failure, close the dead fd and clear its slot so it is not retried.

```c
static void broadcast(Client *clients, int sender, const char *data, uint32_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0 && clients[i].fd != sender && clients[i].has_nick) {
            if (send_message(clients[i].fd, data, len) < 0) {
                close(clients[i].fd);
                clients[i].fd = -1;
                clients[i].has_nick = 0;
            }
        }
    }
}
```

## P2 — Client quit notification (`client.c`)

**Problem:** When the user presses `Ctrl+C` or stdin hits EOF, the client exits without informing the server. Other clients never receive a "[System] X left the chat" message; they only learn of the departure when the server's `select` next reports the fd as dead.

**Fix:** Before `close(fd); return 0;`, attempt to send a quit/sysmsg to the server (best effort — ignore the send result since the socket may already be dead). On `SIGINT`, the signal handler already sets `running = 0`; send the quit in the main loop after the `while (running)` exits.

## P3 — Nickname validation (`server.c`)

**Problem:** The server strips `\n` from the nickname but accepts any other bytes, including empty strings (after stripping a lone `\n`), control characters, or names that fill the buffer without a terminator.

**Fix:** Reject empty nicknames and control characters; ensure the stored nickname is always NUL-terminated within `NICK_SIZE`.

```c
/* After stripping newline and truncating: */
if (msg_len == 0 || buf[0] == '\0') {
    /* Reject empty nickname */
    close(clients[i].fd);
    clients[i].fd = -1;
    clients[i].has_nick = 0;
    continue;
}
```

## P4 — `select()` timeout (`server.c`)

**Problem:** `select()` blocks indefinitely with `NULL` timeout, so the server cannot do periodic housekeeping or react promptly to external shutdown conditions.

**Fix:** Pass a small `struct timeval` (e.g., 1 second) so the loop wakes periodically for maintenance tasks. Note `select()` modifies the timeout to reflect remaining time, so re-initialize it each iteration.

## P5 — `parse_port` exits the process (`common.c`)

**Problem:** `parse_port()` calls `exit(1)` on invalid input, making it unsuitable as a reusable library function.

**Fix:** Change signature to `int parse_port(char *str, int *out)` returning 0 on success / -1 on failure, and let `main()` decide to exit.

## P6 — Client terminal display (`client.c`)

**Problem:** Received messages use `\rReceived> ` to overwrite the current input line, which can garble the screen when a message arrives while the user is typing.

**Fix:** Print a leading newline before the received message, or use `readline`/`ncurses` for a proper split input/output display.

---

### Suggested implementation order

1. P1 — Broadcast error handling (prevents silent client loss)
2. P2 — Client quit notification (improves UX for all users)
3. P3 — Nickname validation (prevents degenerate entries)
4. P4 — `select()` timeout (enables periodic housekeeping)
5. P5 — `parse_port` error return (library usability)
6. P6 — Client terminal display (polish)