#include "internal.h"
#include <errno.h>

/* Ready Queue */
static struct gt_thread *ready_head = NULL;
static struct gt_thread *ready_tail = NULL;

/* Global Scheduler State */
struct gt_thread *gt_current = NULL;
struct gt_thread gt_main_thread;
static int next_thread_id = 1;

void gt_ready_push(struct gt_thread *t) {
    t->next = NULL;
    if (ready_tail == NULL) {
        ready_head = t;
        ready_tail = t;
    } else {
        ready_tail->next = t;
        ready_tail = t;
    }
}

struct gt_thread *gt_ready_pop(void) {
    if (ready_head == NULL) {
        return NULL;
    }
    struct gt_thread *t = ready_head;
    ready_head = ready_head->next;
    if (ready_head == NULL) {
        ready_tail = NULL;
    }
    t->next = NULL;
    return t;
}

bool gt_ready_empty(void) {
    return ready_head == NULL;
}

static void thread_trampoline(void) {
    void *retval = gt_current->fn(gt_current->arg);
    thread_exit(retval);
}

static void thread_init_main_if_needed(void) {
    if (gt_current != NULL) {
        return;
    }
    gt_main_thread.id = 0;
    gt_main_thread.state = THREAD_RUNNING;
    gt_main_thread.stack = NULL;
    gt_main_thread.stack_size = 0;
    gt_main_thread.fn = NULL;
    gt_main_thread.arg = NULL;
    gt_main_thread.retval = NULL;
    gt_main_thread.joiner = NULL;
    gt_main_thread.next = NULL;
    gt_current = &gt_main_thread;
}

int thread_create(thread_t *t, void *(*fn)(void *), void *arg) {
    if (!fn) {
        return -EINVAL;
    }
    thread_init_main_if_needed();

    struct gt_thread *th = malloc(sizeof(struct gt_thread));
    if (!th) {
        return -ENOMEM;
    }

    th->stack = malloc(GT_STACK_SIZE);
    if (!th->stack) {
        free(th);
        return -ENOMEM;
    }

    th->id = next_thread_id++;
    th->state = THREAD_READY;
    th->stack_size = GT_STACK_SIZE;
    th->fn = fn;
    th->arg = arg;
    th->retval = NULL;
    th->joiner = NULL;
    th->next = NULL;

    if (getcontext(&th->ctx) == -1) {
        free(th->stack);
        free(th);
        return -errno;
    }

    th->ctx.uc_stack.ss_sp = th->stack;
    th->ctx.uc_stack.ss_size = th->stack_size;
    th->ctx.uc_link = NULL;

    makecontext(&th->ctx, thread_trampoline, 0);

    gt_ready_push(th);
    if (t) {
        *t = th;
    }
    return 0;
}

void gt_schedule(void) {
    struct gt_thread *prev = gt_current;
    struct gt_thread *next = gt_ready_pop();

    if (next == NULL) {
        /* If no other threads are ready:
         * - If current is still running, it simply continues.
         * - If current has finished, all runnable threads are done. */
        if (prev->state == THREAD_RUNNING) {
            return;
        }
        /* All threads have exited */
        exit(0);
    }

    if (prev->state == THREAD_RUNNING) {
        prev->state = THREAD_READY;
        gt_ready_push(prev);
    }

    gt_current = next;
    gt_current->state = THREAD_RUNNING;

    if (swapcontext(&prev->ctx, &next->ctx) == -1) {
        perror("swapcontext");
        exit(1);
    }
}

void thread_yield(void) {
    thread_init_main_if_needed();
    gt_schedule();
}

void thread_exit(void *retval) {
    thread_init_main_if_needed();
    gt_current->state = THREAD_DONE;
    gt_current->retval = retval;
    gt_schedule();
}

thread_t thread_self(void) {
    thread_init_main_if_needed();
    return gt_current;
}

/* Stubs for future stages */
int thread_join(thread_t t, void **retval) {
    (void)t;
    (void)retval;
    return 0;
}

void gt_set_timeslice_us(unsigned us) {
    (void)us;
}
