#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include "libgt.h"

#define STRESS_CYCLES 100000

/* --- Test 1: Return Values --- */
static void *worker_ret_val(void *arg) {
    long val = (long)arg;
    return (void *)(val * 10);
}

static void test_return_value(void) {
    printf("[Test 1] Return value passing...\n");
    thread_t t;
    int err = thread_create(&t, worker_ret_val, (void *)42);
    assert(err == 0);

    void *ret = NULL;
    err = thread_join(t, &ret);
    assert(err == 0);
    assert((long)ret == 420);
    printf("    --> PASS (returned 420)\n\n");
}

/* --- Test 2: Join Before Exit (Parent blocks waiting for child) --- */
static int child_ran_steps = 0;
static void *worker_join_before(void *arg) {
    (void)arg;
    for (int i = 0; i < 5; i++) {
        child_ran_steps++;
        thread_yield();
    }
    return (void *)999;
}

static void test_join_before_exit(void) {
    printf("[Test 2] Join before exit (blocking join)...\n");
    child_ran_steps = 0;
    thread_t t;
    int err = thread_create(&t, worker_join_before, NULL);
    assert(err == 0);

    /* Child has not finished yet; join should block main until child completes all 5 steps */
    void *ret = NULL;
    err = thread_join(t, &ret);
    assert(err == 0);
    assert((long)ret == 999);
    assert(child_ran_steps == 5);
    printf("    --> PASS (child completed all %d steps before join returned)\n\n", child_ran_steps);
}

/* --- Test 3: Join After Exit (Child finishes before parent joins) --- */
static void *worker_quick_exit(void *arg) {
    (void)arg;
    return (void *)777;
}

static void test_join_after_exit(void) {
    printf("[Test 3] Join after exit (non-blocking join)...\n");
    thread_t t;
    int err = thread_create(&t, worker_quick_exit, NULL);
    assert(err == 0);

    /* Yield multiple times so child definitely completes and enters THREAD_DONE */
    for (int i = 0; i < 3; i++) {
        thread_yield();
    }

    /* Join should return immediately without blocking */
    void *ret = NULL;
    err = thread_join(t, &ret);
    assert(err == 0);
    assert((long)ret == 777);
    printf("    --> PASS (joined already-finished thread)\n\n");
}

/* --- Test 4: Error Handling --- */
static void *worker_dummy(void *arg) {
    (void)arg;
    return (void *)1;
}

static void test_error_handling(void) {
    printf("[Test 4] Error conditions...\n");

    /* 4a. NULL thread */
    int err = thread_join(NULL, NULL);
    assert(err == -EINVAL);
    printf("    4a. Joining NULL returns -EINVAL --> PASS\n");

    /* 4b. Joining self (deadlock) */
    err = thread_join(thread_self(), NULL);
    assert(err == -EDEADLK);
    printf("    4b. Joining self returns -EDEADLK --> PASS\n");

    /* 4c. Double join */
    thread_t t;
    err = thread_create(&t, worker_dummy, NULL);
    assert(err == 0);

    err = thread_join(t, NULL);
    assert(err == 0);

    err = thread_join(t, NULL);
    assert(err == -EINVAL);
    printf("    4c. Double join returns -EINVAL --> PASS\n\n");
}

/* --- Test 5: 100,000 Create / Join Stress Test (No Leaks) --- */
static void *worker_stress(void *arg) {
    long id = (long)arg;
    return (void *)(id * 2);
}

static void test_stress_create_join(void) {
    printf("[Test 5] Stress Test: %d create/join cycles...\n", STRESS_CYCLES);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (long i = 1; i <= STRESS_CYCLES; i++) {
        thread_t t;
        int err = thread_create(&t, worker_stress, (void *)i);
        assert(err == 0);

        void *ret = NULL;
        err = thread_join(t, &ret);
        assert(err == 0);
        assert((long)ret == i * 2);

        if (i % 25000 == 0) {
            printf("    [Progress] %ld / %d cycles complete...\n", i, STRESS_CYCLES);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t elapsed_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (uint64_t)(end.tv_nsec - start.tv_nsec);
    double elapsed_sec = (double)elapsed_ns / 1e9;
    double us_per_cycle = ((double)elapsed_ns / 1000.0) / STRESS_CYCLES;

    printf("    --> PASS (%d create/join cycles in %.3f seconds, ~%.2f µs per cycle)\n\n",
           STRESS_CYCLES, elapsed_sec, us_per_cycle);
}

int main(void) {
    printf("====================================================\n");
    printf("  Stage 3 Test: Thread Join, Lifecycle & Cleanup\n");
    printf("====================================================\n\n");

    test_return_value();
    test_join_before_exit();
    test_join_after_exit();
    test_error_handling();
    test_stress_create_join();

    printf("====================================================\n");
    printf("  All Stage 3 tests PASSED!\n");
    printf("====================================================\n");
    return 0;
}
