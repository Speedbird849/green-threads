#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int target_port = 8080;
static int total_requests = 500;
static int concurrency = 20;
static volatile int req_counter = 0;
static volatile int successful_requests = 0;
static volatile int failed_requests = 0;

static const char req[] = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";

static void *client_worker(void *arg) {
    (void)arg;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(target_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (1) {
        int idx = __sync_fetch_and_add(&req_counter, 1);
        if (idx >= total_requests) {
            break;
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            __sync_fetch_and_add(&failed_requests, 1);
            continue;
        }

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            __sync_fetch_and_add(&failed_requests, 1);
            close(sock);
            continue;
        }

        if (send(sock, req, sizeof(req) - 1, 0) < 0) {
            __sync_fetch_and_add(&failed_requests, 1);
            close(sock);
            continue;
        }

        char buf[256];
        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n > 0 && strstr(buf, "200 OK") != NULL) {
            __sync_fetch_and_add(&successful_requests, 1);
        } else {
            __sync_fetch_and_add(&failed_requests, 1);
        }

        close(sock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc >= 2) target_port = atoi(argv[1]);
    if (argc >= 3) total_requests = atoi(argv[2]);
    if (argc >= 4) concurrency = atoi(argv[3]);

    printf("Starting concurrent load test on 127.0.0.1:%d\n", target_port);
    printf("  Total Requests: %d | Concurrency Level: %d simultaneous clients\n",
           total_requests, concurrency);

    pthread_t *threads = malloc(sizeof(pthread_t) * concurrency);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < concurrency; i++) {
        pthread_create(&threads[i], NULL, client_worker, NULL);
    }

    for (int i = 0; i < concurrency; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    free(threads);

    uint64_t elapsed_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (uint64_t)(end.tv_nsec - start.tv_nsec);
    double elapsed_sec = (double)elapsed_ns / 1e9;
    double rps = (double)successful_requests / elapsed_sec;

    printf("\n--- Load Test Results ---\n");
    printf("Total Requests   : %d\n", total_requests);
    printf("Successful       : %d\n", successful_requests);
    printf("Failed           : %d\n", failed_requests);
    printf("Elapsed Time     : %.4f seconds\n", elapsed_sec);
    printf("Throughput (RPS) : %.2f requests/sec\n", rps);
    printf("-------------------------\n");

    return failed_requests > 0 ? 1 : 0;
}
