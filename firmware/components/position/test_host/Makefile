CC      ?= cc
CFLAGS  ?= -std=c11 -O1 -g -Wall -Wextra -Werror -Wconversion -Wno-sign-conversion \
           -fsanitize=address,undefined -fno-omit-frame-pointer
CPPFLAGS = -I../include
BIN      = build

test: $(BIN)/test_position
	./$(BIN)/test_position

$(BIN)/test_position: test_position.c ../position.c
	mkdir -p $(BIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_position.c ../position.c -lm

clean:
	rm -rf $(BIN)

.PHONY: test clean
