# Chat

A simple TCP chat application in C. Server and client communicate over length-prefixed messages using `select()`-based I/O multiplexing.

## Requirements

- GCC
- Linux (POSIX sockets, `select`)

## Build

```sh
make
```

Or build individually:

```sh
make server
make client
```

## Usage

Start the server, then connect one or more clients from separate terminals.

**Server** (default port `9090`):

```sh
./build/server [port]
```

**Client** (default `127.0.0.1:9090`):

```sh
./build/client [ip] [port]
```

Type messages in any client terminal. All other connected clients receive them. Press `Ctrl+C` to disconnect gracefully.

## Project Structure

```
├── src/
│   ├── common.h    # Shared declarations, protocol constants
│   ├── common.c    # Shared implementations (toInt, sendMsg, recvMsg)
│   ├── server.c    # Multi-client server (select-based)
│   └── client.c    # Client (select-based)
├── build/          # Compiled binaries (gitignored)
├── Makefile        # Build system
├── LICENSE         # GPL v3
└── README.md
```

## How It Works

- Server uses `select()` to multiplex across up to 32 simultaneous clients.
- Messages use a 4-byte network-order length prefix (TCP framing).
- Clients also use `select()` on stdin and the socket for non-blocking bidirectional I/O.
- `SIGINT`/`SIGTERM` trigger graceful shutdown on both server and client.
- `SIGPIPE` is ignored; write errors are handled via return values.

## License

[GNU General Public License v3.0](LICENSE)
