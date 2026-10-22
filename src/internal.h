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
    ucontext_t ctx;
    char *stack;
    size_t stack_size;
    void *(*fn)(void *);
    void *arg;
    void *retval;
    struct gt_thread *joiner;  /* Waiting thread blocked on join */
    struct gt_thread *next;    /* Queue linkage */
};

/* Global Scheduler State */
extern struct gt_thread *gt_current;
extern struct gt_thread gt_main_thread;

/* Ready Queue Functions */
void gt_ready_push(struct gt_thread *t);
struct gt_thread *gt_ready_pop(void);
bool gt_ready_empty(void);

/* Core Scheduling Engine */
void gt_schedule(void);

/* Preemption hooks (stubbed for Stage 2, implemented in Stage 5) */
static inline void gt_preempt_disable(void) {}
static inline void gt_preempt_enable(void) {}

#endif /* LIBGT_INTERNAL_H */
