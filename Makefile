CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Wno-deprecated-declarations -g -Iinclude -D_XOPEN_SOURCE=700 -D_GNU_SOURCE

BIN_DIR = bin
BUILD_DIR = build
TEST_DIR = tests
SRC_DIR = src

TEST_BINS = $(BIN_DIR)/test_pingpong $(BIN_DIR)/test_basic_mini
DOCKER_IMAGE = green-threads-dev

.PHONY: all clean test docker-test docker-valgrind docker-shell

all: $(TEST_BINS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR)/test_pingpong: $(TEST_DIR)/test_pingpong.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/test_basic_mini: $(TEST_DIR)/test_basic_mini.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

test: $(TEST_BINS)
	./$(BIN_DIR)/test_pingpong
	./$(BIN_DIR)/test_basic_mini

docker-test:
	docker run --rm -v "$$(pwd)":/workspace $(DOCKER_IMAGE) bash -c "make clean && make test"

docker-valgrind: $(BIN_DIR)/test_pingpong
	docker run --rm -v "$$(pwd)":/workspace $(DOCKER_IMAGE) valgrind --leak-check=full ./$(BIN_DIR)/test_pingpong

docker-shell:
	docker run --rm -it -v "$$(pwd)":/workspace $(DOCKER_IMAGE) bash

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)

