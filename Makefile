CC = gcc
CFLAGS = -Wall -std=c11 -Werror
BUILD_TYPE = debug

ifeq ($(BUILD_TYPE),debug)
CFLAGS += -g
else
ifeq ($(BUILD_TYPE),release)
CFLAGS += -O2 -DNDEBUG
endif
endif

OBJ = build/$(BUILD_TYPE)/main.o build/$(BUILD_TYPE)/normpath.o build/$(BUILD_TYPE)/wait_for_exist.o
BIN = build/$(BUILD_TYPE)/wait_for_exist

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

build/$(BUILD_TYPE)/%.o: src/%.c src/normpath.h src/wait_for_exist.h
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm $(OBJ) $(BIN)
