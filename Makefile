CC ?= clang
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Wno-deprecated-declarations -g -Iinclude -D_XOPEN_SOURCE=700 -D_GNU_SOURCE

BIN_DIR = bin
BUILD_DIR = build
TEST_DIR = tests
SRC_DIR = src
DEMO_DIR = demos

LIB_NAME = libgt.a
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME)
LIB_OBJS = $(BUILD_DIR)/thread.o

TEST_BINS = $(BIN_DIR)/test_pingpong $(BIN_DIR)/test_basic_mini $(BIN_DIR)/test_basic $(BIN_DIR)/test_join
DEMO_BINS = $(BIN_DIR)/demo_multitask $(BIN_DIR)/tcp_server_gt $(BIN_DIR)/tcp_server_pthread $(BIN_DIR)/tcp_load_test $(BIN_DIR)/demo_showdown
DOCKER_IMAGE = libgt-dev

.PHONY: all clean test demo demo-tcp showdown docker-test docker-valgrind docker-shell

all: $(STATIC_LIB) $(TEST_BINS) $(DEMO_BINS)

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

$(BIN_DIR)/test_join: $(TEST_DIR)/test_join.c $(STATIC_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(STATIC_LIB) -o $@

# Demos
$(BIN_DIR)/demo_multitask: $(DEMO_DIR)/demo_multitask.c $(STATIC_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(STATIC_LIB) -o $@

$(BIN_DIR)/tcp_server_gt: $(DEMO_DIR)/tcp_server.c $(STATIC_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(STATIC_LIB) -o $@

$(BIN_DIR)/tcp_server_pthread: $(DEMO_DIR)/tcp_server.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -DUSE_PTHREAD $< -pthread -o $@

$(BIN_DIR)/tcp_load_test: $(DEMO_DIR)/tcp_load_test.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -pthread -o $@

$(BIN_DIR)/demo_showdown: $(DEMO_DIR)/demo_showdown.c $(STATIC_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(STATIC_LIB) -pthread -o $@

demo: $(BIN_DIR)/demo_multitask
	./$(BIN_DIR)/demo_multitask

showdown: $(BIN_DIR)/demo_showdown
	./$(BIN_DIR)/demo_showdown

demo-tcp: $(BIN_DIR)/tcp_server_gt $(BIN_DIR)/tcp_server_pthread $(BIN_DIR)/tcp_load_test
	@echo "\n=== Running TCP Server under libgt Green Threads (Port 9091) ==="
	./$(BIN_DIR)/tcp_server_gt 9091 100 & SERVER_PID=$$!; \
	sleep 0.2; \
	./$(BIN_DIR)/tcp_load_test 9091 100; \
	wait $$SERVER_PID 2>/dev/null || true
	@echo "\n=== Running TCP Server under POSIX Kernel Threads (pthreads) (Port 9092) ==="
	./$(BIN_DIR)/tcp_server_pthread 9092 100 & SERVER_PID=$$!; \
	sleep 0.2; \
	./$(BIN_DIR)/tcp_load_test 9092 100; \
	wait $$SERVER_PID 2>/dev/null || true

test: $(TEST_BINS)
	./$(BIN_DIR)/test_pingpong
	./$(BIN_DIR)/test_basic
	./$(BIN_DIR)/test_join

docker-test:
	docker run --rm -v "$$(pwd)":/workspace $(DOCKER_IMAGE) bash -c "make clean && make test"

docker-valgrind: $(BIN_DIR)/test_join
	docker run --rm -v "$$(pwd)":/workspace $(DOCKER_IMAGE) valgrind --leak-check=full ./$(BIN_DIR)/test_join

docker-shell:
	docker run --rm -it -v "$$(pwd)":/workspace $(DOCKER_IMAGE) bash

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)
