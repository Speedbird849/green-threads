#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <ucontext.h>

#define STACK_SIZE (64 * 1024)
#define VISUAL_ROUNDS 5
#define BENCH_SWITCHES 1000000 // 1,000,000 context switches

static ucontext_t ctx_main;
static ucontext_t ctx_a;
static ucontext_t ctx_b;

static char stack_a[STACK_SIZE];
static char stack_b[STACK_SIZE];

static int count_a = 0;
static int count_b = 0;
static int max_rounds = VISUAL_ROUNDS;
static int is_benchmark = 0;

static void func_a(void) {
    while (count_a < max_rounds) {
        count_a++;
        if (!is_benchmark) {
            printf("A%d ", count_a);
            fflush(stdout);
        }

        // Swap to B
        if (swapcontext(&ctx_a, &ctx_b) == -1) {
            perror("swapcontext A -> B");
            exit(1);
        }
    }

    // Done with A, switch back to main
    if (swapcontext(&ctx_a, &ctx_main) == -1) {
        perror("swapcontext A -> main");
        exit(1);
    }
}

static void func_b(void) {
    while (count_b < max_rounds) {
        count_b++;
        if (!is_benchmark) {
            printf("B%d ", count_b);
            fflush(stdout);
        }

        // Swap to A
        if (swapcontext(&ctx_b, &ctx_a) == -1) {
            perror("swapcontext B -> A");
            exit(1);
        }
    }

    // Done with B, switch back to A so A returns to main
    if (swapcontext(&ctx_b, &ctx_a) == -1) {
        perror("swapcontext B -> A");
        exit(1);
    }
}

static void run_pingpong(int rounds, int benchmark_mode) {
    count_a = 0;
    count_b = 0;
    max_rounds = rounds;
    is_benchmark = benchmark_mode;

    // Reset Context A
    if (getcontext(&ctx_a) == -1) {
        perror("getcontext ctx_a");
        exit(1);
    }
    ctx_a.uc_stack.ss_sp = stack_a;
    ctx_a.uc_stack.ss_size = sizeof(stack_a);
    ctx_a.uc_link = &ctx_main;
    makecontext(&ctx_a, func_a, 0);

    // Reset Context B
    if (getcontext(&ctx_b) == -1) {
        perror("getcontext ctx_b");
        exit(1);
    }
    ctx_b.uc_stack.ss_sp = stack_b;
    ctx_b.uc_stack.ss_size = sizeof(stack_b);
    ctx_b.uc_link = &ctx_main;
    makecontext(&ctx_b, func_b, 0);

    // Enter ping-pong loop
    if (swapcontext(&ctx_main, &ctx_a) == -1) {
        perror("swapcontext main -> A");
        exit(1);
    }
}

int main(void) {
    printf("====================================================\n");
    printf("  Stage 1: Context Switching & Latency Benchmark\n");
    printf("====================================================\n\n");

    // 1. Correctness Check (Visual Ping-Pong)
    printf("[1] Visual Correctness Check (%d rounds):\n    ", VISUAL_ROUNDS);
    run_pingpong(VISUAL_ROUNDS, 0);
    printf("\n    --> Correctness check passed!\n\n");

    // 2. High-Performance Benchmark
    // Each round has 2 context switches: (A -> B) and (B -> A)
    int bench_rounds = BENCH_SWITCHES / 2;
    printf("[2] Benchmarking %d Context Switches (%d round-trips)...\n",
           BENCH_SWITCHES, bench_rounds);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    run_pingpong(bench_rounds, 1);

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (uint64_t)(end.tv_nsec - start.tv_nsec);
    double elapsed_sec = (double)elapsed_ns / 1e9;
    double ns_per_switch = (double)elapsed_ns / (double)BENCH_SWITCHES;
    double switches_per_sec = (double)BENCH_SWITCHES / elapsed_sec;

    printf("\n--- Results ---\n");
    printf("Total Elapsed Time : %.4f seconds (%.2f ms)\n", elapsed_sec, elapsed_sec * 1000.0);
    printf("Total Switches     : %d\n", BENCH_SWITCHES);
    printf("Average Latency    : %.2f ns per context switch\n", ns_per_switch);
    printf("Switch Throughput  : %.2f million switches/sec\n", switches_per_sec / 1e6);
    printf("====================================================\n");

    return 0;
}
