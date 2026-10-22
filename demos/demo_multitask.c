#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "libgt.h"

#define NUM_WORKERS 4
#define BAR_WIDTH 20

typedef struct {
    int id;
    const char *name;
    const char *color;
    int total_chunks;
    int processed;
    uint32_t checksum;
} task_t;

/* ANSI colors */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_MAGENTA "\033[35m"

static void render_progress_bar(const task_t *task) {
    int percent = (task->processed * 100) / task->total_chunks;
    int filled = (task->processed * BAR_WIDTH) / task->total_chunks;

    printf("  %s[%-18s]%s [", task->color, task->name, COLOR_RESET);
    for (int i = 0; i < BAR_WIDTH; i++) {
        if (i < filled) {
            printf("=");
        } else if (i == filled) {
            printf(">");
        } else {
            printf(" ");
        }
    }
    printf("] %3d%% (chunk %d/%d)\n", percent, task->processed, task->total_chunks);
    fflush(stdout);
}

static void *worker_func(void *arg) {
    task_t *task = (task_t *)arg;

    while (task->processed < task->total_chunks) {
        task->processed++;

        /* Simulate lightweight computational work */
        task->checksum = (task->checksum * 1664525u + 1013904223u) ^ (uint32_t)task->processed;

        /* Display live progress */
        render_progress_bar(task);

        /* Cooperatively yield CPU to the next green thread */
        thread_yield();
    }

    printf("  %s[%-18s] COMPLETE! Checksum: 0x%08X%s\n",
           task->color, task->name, task->checksum, COLOR_RESET);

    return (void *)(uintptr_t)task->checksum;
}

int main(void) {
    printf("%s==============================================================%s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s   libgt Live Demo: Concurrent Multi-Task Pipeline (%d Threads)%s\n", COLOR_BOLD, NUM_WORKERS, COLOR_RESET);
    printf("%s   Simulating interleaved user-level cooperative concurrency%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s==============================================================%s\n\n", COLOR_BOLD, COLOR_RESET);

    task_t tasks[NUM_WORKERS] = {
        {1, "Image Resizer",      COLOR_CYAN,    6, 0, 0x12345678},
        {2, "Database Indexer",   COLOR_GREEN,   8, 0, 0xABCDEF01},
        {3, "Network Downloader", COLOR_YELLOW,  5, 0, 0xFEEDFACE},
        {4, "Crypto Hasher",      COLOR_MAGENTA, 7, 0, 0xCAFEBABE}
    };

    thread_t threads[NUM_WORKERS];
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    printf("1. Spawning %d green threads onto the run queue...\n\n", NUM_WORKERS);
    for (int i = 0; i < NUM_WORKERS; i++) {
        int err = thread_create(&threads[i], worker_func, &tasks[i]);
        if (err != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }
    }

    printf("2. Joining all threads and waiting for task completion...\n\n");
    uint32_t results[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        void *ret = NULL;
        int err = thread_join(threads[i], &ret);
        if (err != 0) {
            fprintf(stderr, "Failed to join thread %d\n", i);
            return 1;
        }
        results[i] = (uint32_t)(uintptr_t)ret;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_nsec - start.tv_nsec) / 1e6;

    printf("\n%s==============================================================%s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s                     Execution Summary                        %s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s==============================================================%s\n", COLOR_BOLD, COLOR_RESET);
    int total_chunks = 0;
    for (int i = 0; i < NUM_WORKERS; i++) {
        printf("  * %-20s : %2d chunks processed | Checksum: 0x%08X\n",
               tasks[i].name, tasks[i].processed, results[i]);
        total_chunks += tasks[i].processed;
    }
    printf("--------------------------------------------------------------\n");
    printf("  Total Chunks Processed : %d\n", total_chunks);
    printf("  Total Execution Time   : %.3f ms\n", elapsed_ms);
    printf("  Kernel OS Threads Used : 1 (Single Process, Single Kernel Thread)\n");
    printf("%s==============================================================%s\n", COLOR_BOLD, COLOR_RESET);

    return 0;
}
