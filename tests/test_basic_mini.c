#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ucontext.h>

#define STACK_SIZE (64 * 1024)

// -------------------------------------------------------------
// Step 2.1: The Thread Control Block (TCB)
// -------------------------------------------------------------
typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_DONE
} thread_state_t;

typedef struct gt_thread {
    int id;
    thread_state_t state;
    ucontext_t ctx;
    char *stack;
    void *(*fn)(void *);
    void *arg;
    void *retval;
    struct gt_thread *next; // for queue linking
} gt_thread_t;

// -------------------------------------------------------------
// Step 2.3: Ready Queue (FIFO)
// -------------------------------------------------------------
static gt_thread_t *ready_head = NULL;
static gt_thread_t *ready_tail = NULL;
static gt_thread_t *current_thread = NULL;
static gt_thread_t main_thread;
static int next_thread_id = 1;

static void queue_push(gt_thread_t *t) {
    t->next = NULL;
    if (ready_tail == NULL) {
        ready_head = t;
        ready_tail = t;
    } else {
        ready_tail->next = t;
        ready_tail = t;
    }
}

static gt_thread_t *queue_pop(void) {
    if (ready_head == NULL) {
        return NULL;
    }
    gt_thread_t *t = ready_head;
    ready_head = ready_head->next;
    if (ready_head == NULL) {
        ready_tail = NULL;
    }
    t->next = NULL;
    return t;
}

// -------------------------------------------------------------
// Forward declarations
// -------------------------------------------------------------
void thread_yield(void);
void thread_exit(void *retval);

// -------------------------------------------------------------
// Step 2.2: The Trampoline
// -------------------------------------------------------------
// makecontext only guarantees integer arguments portably.
// Instead of passing arguments through makecontext, the newly
// scheduled thread reads its entry function and argument directly
// from `current_thread`, which is already pointing to it.
static void thread_trampoline(void) {
    printf("[Thread %d] Started running via trampoline\n", current_thread->id);
    void *res = current_thread->fn(current_thread->arg);
    thread_exit(res);
}

// -------------------------------------------------------------
// Step 2.4: Adopting main() as Thread 0
// -------------------------------------------------------------
static void thread_init_main_if_needed(void) {
    if (current_thread != NULL) {
        return; // already initialized
    }
    main_thread.id = 0;
    main_thread.state = THREAD_RUNNING;
    main_thread.stack = NULL; // uses OS main stack
    main_thread.fn = NULL;
    main_thread.arg = NULL;
    main_thread.next = NULL;
    current_thread = &main_thread;
}

// -------------------------------------------------------------
// Thread Creation
// -------------------------------------------------------------
gt_thread_t *thread_create(void *(*fn)(void *), void *arg) {
    thread_init_main_if_needed();

    gt_thread_t *t = malloc(sizeof(gt_thread_t));
    if (!t) {
        perror("malloc TCB");
        exit(1);
    }

    t->id = next_thread_id++;
    t->state = THREAD_READY;
    t->fn = fn;
    t->arg = arg;
    t->retval = NULL;
    t->next = NULL;

    t->stack = malloc(STACK_SIZE);
    if (!t->stack) {
        perror("malloc stack");
        exit(1);
    }

    if (getcontext(&t->ctx) == -1) {
        perror("getcontext");
        exit(1);
    }
    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = STACK_SIZE;
    t->ctx.uc_link = NULL;

    makecontext(&t->ctx, thread_trampoline, 0);

    // Place new thread in ready queue
    queue_push(t);
    return t;
}

// -------------------------------------------------------------
// Step 2.5: Scheduler - Yielding & Exiting
// -------------------------------------------------------------
void thread_yield(void) {
    thread_init_main_if_needed();

    gt_thread_t *prev = current_thread;
    gt_thread_t *next = queue_pop();

    if (next == NULL) {
        // No other threads waiting; continue executing current thread
        return;
    }

    // Move current thread back to READY and push onto the queue
    prev->state = THREAD_READY;
    queue_push(prev);

    // Switch CPU execution to next thread
    current_thread = next;
    current_thread->state = THREAD_RUNNING;

    swapcontext(&prev->ctx, &next->ctx);
}

void thread_exit(void *retval) {
    current_thread->state = THREAD_DONE;
    current_thread->retval = retval;

    printf("[Thread %d] Exiting\n", current_thread->id);

    // Pick the next thread to run
    gt_thread_t *prev = current_thread;
    gt_thread_t *next = queue_pop();

    if (next == NULL) {
        printf("All threads finished!\n");
        exit(0);
    }

    // Do NOT push `prev` back into the ready queue
    current_thread = next;
    current_thread->state = THREAD_RUNNING;

    swapcontext(&prev->ctx, &next->ctx);
}

// -------------------------------------------------------------
// Test Demo
// -------------------------------------------------------------
static void *worker(void *arg) {
    char *name = (char *)arg;
    for (int iter = 1; iter <= 3; iter++) {
        printf("  -> %s (iteration %d of 3)\n", name, iter);
        thread_yield();
    }
    return NULL;
}

int main(void) {
    printf("=== Stage 2 Prototype: 3 Threads Yielding ===\n\n");

    thread_create(worker, "Thread Alpha");
    thread_create(worker, "Thread Beta");
    thread_create(worker, "Thread Gamma");

    // Main thread yields repeatedly until all workers finish
    while (ready_head != NULL) {
        printf("[Main Thread 0] Yielding CPU to workers...\n");
        thread_yield();
    }

    printf("\n=== Execution Complete! ===\n");
    return 0;
}
