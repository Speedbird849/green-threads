#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "libgt.h"

#define NUM_THREADS 5
#define NUM_ITERS 3

static int execution_log[NUM_THREADS * NUM_ITERS];
static int log_index = 0;

static void *worker(void *arg) {
    long id = (long)arg;
    for (int iter = 1; iter <= NUM_ITERS; iter++) {
        printf("  [Thread %ld] iteration %d of %d\n", id, iter, NUM_ITERS);
        execution_log[log_index++] = (int)id;
        thread_yield();
    }
    printf("  [Thread %ld] completed all iterations.\n", id);
    return NULL;
}

int main(void) {
    printf("====================================================\n");
    printf("  Stage 2 Test: 5-Thread Round-Robin Scheduling\n");
    printf("====================================================\n\n");

    thread_t threads[NUM_THREADS];

    printf("Creating %d green threads...\n", NUM_THREADS);
    for (long i = 0; i < NUM_THREADS; i++) {
        int ret = thread_create(&threads[i], worker, (void *)(i + 1));
        assert(ret == 0);
    }

    printf("Main thread yielding CPU to ready queue...\n\n");
    /* Yield until all 5 threads finish all iterations */
    while (log_index < NUM_THREADS * NUM_ITERS) {
        thread_yield();
    }

    printf("\n--- Verifying Round-Robin Order ---\n");
    for (int iter = 0; iter < NUM_ITERS; iter++) {
        printf("Round %d order: ", iter + 1);
        for (int t = 0; t < NUM_THREADS; t++) {
            int idx = iter * NUM_THREADS + t;
            int expected_id = t + 1;
            int actual_id = execution_log[idx];
            printf("T%d ", actual_id);
            assert(actual_id == expected_id && "Execution did not match FIFO round-robin order!");
        }
        printf("--> PASS\n");
    }

    printf("\nAll assertions passed! 5 threads yielded in round-robin order.\n");
    printf("====================================================\n");
    return 0;
}
