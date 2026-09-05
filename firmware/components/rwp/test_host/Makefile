# Host-side unit tests for the rwp component (no ESP-IDF needed).
CC      ?= cc
CFLAGS  ?= -std=c11 -O1 -g -Wall -Wextra -Werror -Wconversion -Wno-sign-conversion \
           -fsanitize=address,undefined -fno-omit-frame-pointer
CPPFLAGS = -I../include
BIN      = build

all: test

$(BIN):
	mkdir -p $(BIN)

$(BIN)/test_rwp: test_rwp.c ../rwp.c | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_rwp.c ../rwp.c

$(BIN)/test_floor: test_floor.c ../floor.c | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_floor.c ../floor.c

test: $(BIN)/test_rwp $(BIN)/test_floor
	./$(BIN)/test_rwp
	./$(BIN)/test_floor

clean:
	rm -rf $(BIN)

.PHONY: all test clean
