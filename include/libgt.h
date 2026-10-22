#ifndef LIBGT_H
#define LIBGT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque thread handle */
typedef struct gt_thread *thread_t;

/* Core Thread Lifecycle API */
int      thread_create(thread_t *t, void *(*fn)(void *), void *arg);
void     thread_yield(void);
int      thread_join(thread_t t, void **retval);
void     thread_exit(void *retval);
thread_t thread_self(void);

/* Synchronization Primitives (Forward declarations & typedefs) */
typedef struct gt_mutex gt_mutex_t;
typedef struct gt_sem gt_sem_t;

void gt_mutex_init(gt_mutex_t *m);
void gt_mutex_lock(gt_mutex_t *m);
int  gt_mutex_trylock(gt_mutex_t *m);
void gt_mutex_unlock(gt_mutex_t *m);

void gt_sem_init(gt_sem_t *s, int value);
void gt_sem_wait(gt_sem_t *s);
void gt_sem_post(gt_sem_t *s);

/* Preemption Control (0 disables preemption) */
void gt_set_timeslice_us(unsigned us);

#ifdef __cplusplus
}
#endif

#endif /* LIBGT_H */
