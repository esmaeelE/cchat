CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -D_DEFAULT_SOURCE -O2
SRC = src
BUILD = build

.PHONY: all clean run-server run-client

all: $(BUILD) server client

$(BUILD):
	mkdir -p $(BUILD)

server: $(SRC)/server.c $(SRC)/common.c $(SRC)/common.h | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $(SRC)/server.c $(SRC)/common.c

client: $(SRC)/client.c $(SRC)/common.c $(SRC)/common.h | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/$@ $(SRC)/client.c $(SRC)/common.c

run-server: server
	./$(BUILD)/server

run-client: client
	./$(BUILD)/client

clean:
	rm -f $(BUILD)/server $(BUILD)/client
