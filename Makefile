CC = gcc
CFLAGS = -Wall -std=c2x -Werror
BUILD_TYPE = debug

ifeq ($(BUILD_TYPE),debug)
CFLAGS += -g
else
ifeq ($(BUILD_TYPE),release)
CFLAGS += -O2 -DNDEBUG
endif
endif

OBJ = build/$(BUILD_TYPE)/main.o \
      build/$(BUILD_TYPE)/normpath.o \
      build/$(BUILD_TYPE)/wait_for_exist.o
BIN = build/$(BUILD_TYPE)/wait_for_exist

TEST_OBJ = build/$(BUILD_TYPE)/tests/main.o \
           build/$(BUILD_TYPE)/tests/test.o \
           build/$(BUILD_TYPE)/tests/test_utils.o \
           build/$(BUILD_TYPE)/tests/tests.o \
           build/$(BUILD_TYPE)/tests/strbuf.o \
           build/$(BUILD_TYPE)/normpath.o
TEST_BIN = build/$(BUILD_TYPE)/tests/test

.PHONY: all clean test valgrind

all: $(BIN)

test: $(BIN) $(TEST_BIN)
	$(TEST_BIN)

valgrind: $(BIN) $(TEST_BIN)
	USE_VALGRIND=1 valgrind --leak-check=yes --show-leak-kinds=all -s $(TEST_BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

$(TEST_BIN): $(TEST_OBJ)
	$(CC) $(CFLAGS) $(TEST_OBJ) -o $@

build/$(BUILD_TYPE)/%.o: src/%.c src/normpath.h src/wait_for_exist.h
	$(CC) $(CFLAGS) $< -c -o $@

build/$(BUILD_TYPE)/tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -Isrc -lpthread $< -c -o $@ -DBINARY_PATH=\"$(BIN)\"

clean:
	rm -f $(OBJ) $(BIN) $(TEST_OBJ) $(TEST_BIN)
