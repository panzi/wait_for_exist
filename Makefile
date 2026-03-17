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

TEST_OBJ = build/$(BUILD_TYPE)/tests/test_main.o
TEST_BIN = build/$(BUILD_TYPE)/tests/test

.PHONY: all clean test

all: $(BIN)

test: $(BIN) $(TEST_BIN)
	$(TEST_BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

$(TEST_BIN): $(TEST_OBJ)
	$(CC) $(CFLAGS) $(TEST_OBJ) -o $@

build/$(BUILD_TYPE)/%.o: src/%.c src/normpath.h src/wait_for_exist.h
	$(CC) $(CFLAGS) $< -c -o $@

build/$(BUILD_TYPE)/tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -lpthread $< -c -o $@ -DBINARY_PATH=\"$(BIN)\"

clean:
	rm $(OBJ) $(BIN)
