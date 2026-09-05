# Host simulator: coordinator + nodes over a lossy channel, using the real components.
CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Werror -Wno-sign-conversion -fsanitize=address,undefined -fno-omit-frame-pointer
CPPFLAGS = -I../components/rwp/include -I../components/audio_pipeline/include
SRCS     = sim.c ../components/rwp/rwp.c ../components/rwp/floor.c ../components/audio_pipeline/jitter.c ../components/audio_pipeline/adpcm.c
BIN      = build/sim

$(BIN): $(SRCS)
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRCS) -lm

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build

.PHONY: run clean
