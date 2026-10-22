CC ?= clang
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Wno-deprecated-declarations -g -Iinclude -D_XOPEN_SOURCE=700 -D_GNU_SOURCE

BIN_DIR = bin
BUILD_DIR = build
TEST_DIR = tests
SRC_DIR = src

LIB_NAME = libgt.a
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME)
LIB_OBJS = $(BUILD_DIR)/thread.o

TEST_BINS = $(BIN_DIR)/test_pingpong $(BIN_DIR)/test_basic_mini $(BIN_DIR)/test_basic
DOCKER_IMAGE = libgt-dev

.PHONY: all clean test docker-test docker-valgrind docker-shell

all: $(STATIC_LIB) $(TEST_BINS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# libgt Static Library
$(BUILD_DIR)/thread.o: $(SRC_DIR)/thread.c $(SRC_DIR)/internal.h include/libgt.h include/gt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(STATIC_LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

# Tests
$(BIN_DIR)/test_pingpong: $(TEST_DIR)/test_pingpong.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/test_basic_mini: $(TEST_DIR)/test_basic_mini.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/test_basic: $(TEST_DIR)/test_basic.c $(STATIC_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(STATIC_LIB) -o $@

test: $(TEST_BINS)
	./$(BIN_DIR)/test_pingpong
	./$(BIN_DIR)/test_basic

docker-test:
	docker run --rm -v "$$(pwd)":/workspace $(DOCKER_IMAGE) bash -c "make clean && make test"

docker-valgrind: $(BIN_DIR)/test_basic
	docker run --rm -v "$$(pwd)":/workspace $(DOCKER_IMAGE) valgrind --leak-check=full ./$(BIN_DIR)/test_basic

docker-shell:
	docker run --rm -it -v "$$(pwd)":/workspace $(DOCKER_IMAGE) bash

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)
