# Compiler and flags.
#   -Wall -Wextra : enable most warnings
#   -std=c11 -pedantic : strict C11 conformance
#   -D_DEFAULT_SOURCE : expose POSIX extensions (needed for SOMAXCONN etc.)
#   -O2 : moderate optimization
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -D_DEFAULT_SOURCE -O2

# Source and output directories.
SRC = src
BUILD = build

.PHONY: all clean run-server run-client

# Default target: ensure build dir exists, then build both binaries.
all: $(BUILD) server client

# Create the build directory (order-only prerequisite of the binaries).
$(BUILD):
	mkdir -p $(BUILD)

# Build the server binary from server.c + common.c + common.h.
# Order-only prerequisite | $(BUILD) ensures the dir exists without
# forcing a rebuild whenever the directory timestamp changes.
server: $(SRC)/server.c $(SRC)/common.c $(SRC)/common.h | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $(SRC)/server.c $(SRC)/common.c

# Build the client binary from client.c + common.c + common.h.
client: $(SRC)/client.c $(SRC)/common.c $(SRC)/common.h | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $(SRC)/client.c $(SRC)/common.c

# Convenience target: start the server on the default port (9090).
run-server: server
	./$(BUILD)/server

# Convenience target: start the client on the default address (127.0.0.1:9090).
run-client: client
	./$(BUILD)/client

# Remove built binaries (leaves the build directory intact).
clean:
	rm -f $(BUILD)/server $(BUILD)/client
