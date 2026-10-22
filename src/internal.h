#ifndef LIBGT_INTERNAL_H
#define LIBGT_INTERNAL_H

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <ucontext.h>
#include "libgt.h"

#define GT_STACK_SIZE (64 * 1024)

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DONE
} thread_state_t;

struct gt_thread {
    int id;
    thread_state_t state;
    bool joined;               /* Marked true when join completes */
    ucontext_t ctx;
    char *stack;
    size_t stack_size;
    void *(*fn)(void *);
    void *arg;
    void *retval;
    struct gt_thread *joiner;  /* Waiting thread blocked on join */
    struct gt_thread *next;    /* Queue linkage (ready / wait queues) */
    struct gt_thread *table_next; /* Active threads table bucket link */
};

/* Global Scheduler State */
extern struct gt_thread *gt_current;
extern struct gt_thread gt_main_thread;

/* Ready Queue Functions */
void gt_ready_push(struct gt_thread *t);
struct gt_thread *gt_ready_pop(void);
bool gt_ready_empty(void);

/* Active Thread Table (for safe join validation) */
void gt_table_insert(struct gt_thread *t);
void gt_table_remove(struct gt_thread *t);
bool gt_table_contains(struct gt_thread *t);

/* Memory Reclamation */
void gt_thread_destroy(struct gt_thread *t);

/* Core Scheduling Engine */
void gt_schedule(void);

/* Preemption hooks (stubbed for Stages 2-4, implemented in Stage 5) */
static inline void gt_preempt_disable(void) {}
static inline void gt_preempt_enable(void) {}

#endif /* LIBGT_INTERNAL_H */
