# Host-side unit tests for the audio_pipeline component (no ESP-IDF needed).
CC      ?= cc
CFLAGS  ?= -std=c11 -O1 -g -Wall -Wextra -Werror -Wconversion -Wno-sign-conversion \
           -fsanitize=address,undefined -fno-omit-frame-pointer
CPPFLAGS = -I../include
LDLIBS   = -lm
BIN      = build

all: test

$(BIN):
	mkdir -p $(BIN)

$(BIN)/test_adpcm: test_adpcm.c ../adpcm.c | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_adpcm.c ../adpcm.c $(LDLIBS)

$(BIN)/test_jitter: test_jitter.c ../jitter.c | $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_jitter.c ../jitter.c $(LDLIBS)

test: $(BIN)/test_adpcm $(BIN)/test_jitter
	./$(BIN)/test_adpcm
	./$(BIN)/test_jitter

clean:
	rm -rf $(BIN)

.PHONY: all test clean
