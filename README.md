# User-Level Threading Library (Green Threads)

A user-space C11 threading library that runs many concurrent threads inside a single OS process. The library itself—not the kernel—manages stacks, saves/restores CPU execution contexts, and decides which thread runs next.

---

## 1. Overview

The operating system kernel only sees a single thread. The green threads library:
- Allocates independent stacks for each user thread.
- Saves and restores CPU register state using POSIX execution contexts (`ucontext`).
- Implements a cooperative and preemption-capable scheduler with a FIFO ready queue.
- Provides standard concurrency primitives: `join`, mutexes, and semaphores.
- Handles timer-based preemption (`SIGALRM`) and stack safety guard pages (`mprotect`).

---

## 2. Roadmap & Stages

| Stage | Milestone | Status | Description |
|---|---|---|---|
| **1. Context Switch** | Ping-Pong contexts | **Done** | Two contexts alternating execution using `swapcontext` (`tests/test_pingpong.c`). |
| **2. Cooperative Scheduler** | TCB & Ready Queue | **In Progress** | TCB lifecycle, trampoline bootstrap, FIFO queue, and round-robin `thread_yield()` (`tests/test_basic_mini.c`). |
| **3. Lifecycle & Cleanup** | `thread_join` & Return Values | Planned | Clean termination, zombie list reclamation, returning exit values. |
| **4. Synchronization** | Mutexes & Semaphores | Planned | Sleep/wait queues for `gt_mutex_t` and `gt_sem_t`. |
| **5. Preemption** | Timer-driven `SIGALRM` | Planned | Timeslice interrupts, `preempt_disable` critical sections, starvation prevention. |
| **6. Safety & Diagnostics** | Guard Pages & Deadlock Detection | Planned | `mprotect` guard pages on stacks, alternate signal stack, deadlock reporting. |
| **7. Demos & Benchmarks** | Concurrency Demos & Pthread Comparison | Planned | Dining philosophers, producer-consumer, context-switch latency vs. `pthreads`. |

---

## 3. Architecture & Core Concepts

### Thread Control Block (TCB)
Each thread is tracked via a control block holding:
- **Thread ID (`id`)**: Unique thread identifier (Thread 0 is reserved for `main`).
- **State (`state`)**: `THREAD_READY`, `THREAD_RUNNING`, `THREAD_BLOCKED`, or `THREAD_DONE`.
- **Execution Context (`ctx`)**: Saved registers, program counter, and stack pointer via `ucontext_t`.
- **Stack Memory (`stack`)**: Dedicated heap/mmap-allocated memory.
- **Trampoline Pointer**: Points to the thread start function `void *(*fn)(void *)` and user arguments.

### The Trampoline Pattern
On 64-bit platforms, POSIX `makecontext` takes variadic arguments as `int`s, making pointer passing non-portable and prone to truncation. Furthermore, a thread function must never fall off the end of its stack. 

The library solves both issues with a small trampoline:
```c
static void thread_trampoline(void) {
    void *retval = current_thread->fn(current_thread->arg);
    thread_exit(retval);
}
```

---

## 4. Building and Running

### Prerequisites
- **C Compiler**: `clang` or `gcc` with C11 support
- **Make**: GNU Make
- **Supported OS**: Linux (glibc) or macOS (Darwin arm64/x86_64)
- *(Optional)*: Docker

### Native Build
```bash
# Build all test targets
make all

# Run test suite
make test

# Clean artifacts
make clean
```

### Docker (Ubuntu 24.04 glibc environment)
To run in a clean Linux glibc environment with Valgrind:
```bash
# Build the dev container image
docker build -t green-threads-dev .

# Run test suite inside container
make docker-test

# Run tests under Valgrind leak checker
make docker-valgrind

# Open an interactive shell inside container
make docker-shell
```

---

## 5. Planned API

```c
typedef struct gt_thread *thread_t;

// Thread Management
int      thread_create(thread_t *t, void *(*fn)(void *), void *arg);
void     thread_yield(void);
int      thread_join(thread_t t, void **retval);
void     thread_exit(void *retval);
thread_t thread_self(void);

// Mutexes
void gt_mutex_init(gt_mutex_t *m);
void gt_mutex_lock(gt_mutex_t *m);
int  gt_mutex_trylock(gt_mutex_t *m);
void gt_mutex_unlock(gt_mutex_t *m);

// Semaphores
void gt_sem_init(gt_sem_t *s, int value);
void gt_sem_wait(gt_sem_t *s);
void gt_sem_post(gt_sem_t *s);

// Preemption Configuration
void gt_set_timeslice_us(unsigned us); // 0 disables preemption
```
