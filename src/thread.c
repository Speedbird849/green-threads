#include "internal.h"
#include <errno.h>

/* Ready Queue */
static struct gt_thread *ready_head = NULL;
static struct gt_thread *ready_tail = NULL;

/* Global Scheduler State */
struct gt_thread *gt_current = NULL;
struct gt_thread gt_main_thread;
static int next_thread_id = 1;

/* Active Thread Table (Hash table of active threads) */
#define THREAD_TABLE_SIZE 1024
static struct gt_thread *thread_table[THREAD_TABLE_SIZE] = {0};

static inline unsigned int hash_thread(struct gt_thread *t) {
    return ((uintptr_t)t >> 4) % THREAD_TABLE_SIZE;
}

void gt_table_insert(struct gt_thread *t) {
    if (!t) return;
    unsigned int idx = hash_thread(t);
    t->table_next = thread_table[idx];
    thread_table[idx] = t;
}

void gt_table_remove(struct gt_thread *t) {
    if (!t) return;
    unsigned int idx = hash_thread(t);
    struct gt_thread **curr = &thread_table[idx];
    while (*curr) {
        if (*curr == t) {
            *curr = t->table_next;
            t->table_next = NULL;
            return;
        }
        curr = &(*curr)->table_next;
    }
}

bool gt_table_contains(struct gt_thread *t) {
    if (!t) return false;
    unsigned int idx = hash_thread(t);
    struct gt_thread *curr = thread_table[idx];
    while (curr) {
        if (curr == t) return true;
        curr = curr->table_next;
    }
    return false;
}

void gt_thread_destroy(struct gt_thread *t) {
    if (!t) return;
    gt_table_remove(t);
    if (t != &gt_main_thread) {
        if (t->stack) {
            free(t->stack);
            t->stack = NULL;
        }
        free(t);
    }
}

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
    gt_main_thread.joined = false;
    gt_main_thread.stack = NULL;
    gt_main_thread.stack_size = 0;
    gt_main_thread.fn = NULL;
    gt_main_thread.arg = NULL;
    gt_main_thread.retval = NULL;
    gt_main_thread.joiner = NULL;
    gt_main_thread.next = NULL;
    gt_main_thread.table_next = NULL;
    gt_current = &gt_main_thread;
    gt_table_insert(&gt_main_thread);
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
    th->joined = false;
    th->stack_size = GT_STACK_SIZE;
    th->fn = fn;
    th->arg = arg;
    th->retval = NULL;
    th->joiner = NULL;
    th->next = NULL;
    th->table_next = NULL;

    if (getcontext(&th->ctx) == -1) {
        free(th->stack);
        free(th);
        return -errno;
    }

    th->ctx.uc_stack.ss_sp = th->stack;
    th->ctx.uc_stack.ss_size = th->stack_size;
    th->ctx.uc_link = NULL;

    makecontext(&th->ctx, thread_trampoline, 0);

    gt_table_insert(th);
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
        if (prev->state == THREAD_RUNNING) {
            return;
        }
        if (prev->state == THREAD_BLOCKED) {
            fprintf(stderr, "libgt: Deadlock detected! All threads are BLOCKED with empty ready queue.\n");
            exit(1);
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

    /* If another thread is waiting to join this thread, wake it up */
    if (gt_current->joiner != NULL) {
        struct gt_thread *joiner = gt_current->joiner;
        gt_current->joiner = NULL;
        joiner->state = THREAD_READY;
        gt_ready_push(joiner);
    }

    gt_schedule();
}

int thread_join(thread_t t, void **retval) {
    if (!t) {
        return -EINVAL;
    }
    thread_init_main_if_needed();

    /* Verify thread handle is valid in table */
    if (!gt_table_contains(t)) {
        return -EINVAL;
    }

    /* Cannot join yourself: that would deadlock */
    if (t == gt_current) {
        return -EDEADLK;
    }

    /* Cannot join an already joined thread */
    if (t->joined) {
        return -EINVAL;
    }

    /* Only one thread can join a target thread */
    if (t->joiner != NULL && t->joiner != gt_current) {
        return -EINVAL;
    }

    /* If target is still running or ready, block caller until target exits */
    if (t->state != THREAD_DONE) {
        t->joiner = gt_current;
        gt_current->state = THREAD_BLOCKED;
        gt_schedule();
    }

    /* Target thread is now THREAD_DONE */
    if (retval != NULL) {
        *retval = t->retval;
    }
    t->joined = true;

    /* Safe memory reclamation: caller is executing on its own stack */
    gt_thread_destroy(t);
    return 0;
}

thread_t thread_self(void) {
    thread_init_main_if_needed();
    return gt_current;
}

void gt_set_timeslice_us(unsigned us) {
    (void)us;
}
