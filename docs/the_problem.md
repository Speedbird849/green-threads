# The Core Problem: User-Level Threading (libgt / Green Threads)

At its core, this project is about **building our own miniature operating system inside a single standard C program**.

To understand why this is fascinating and challenging, we need to examine how multitasking works at the machine level, why standard operating system threads exist, and the fundamental technical hurdles of implementing user-space threads from scratch.

---

## 1. The Core Illusion: Sequential CPU vs. Multitasking

At the hardware level, a CPU core is fundamentally sequential:
1. Fetch the instruction pointed to by the **Program Counter (Program Counter / PC)**.
2. Execute it.
3. Advance the PC and repeat.

In standard C, there is only **one call stack**. If `foo()` calls `bar()`, `foo()` is frozen until `bar()` completes and returns. You cannot pause `bar()` halfway through its execution, jump over to `baz()`, let `baz()` run for a while, and then resume `bar()` right where it paused with all its local variables intact.

Normal programming languages and compilers assume **one timeline, one stack, and strictly sequential execution**.

---

## 2. How the OS Normally Solves This (Kernel Threads / 1:1 Model)

When you use standard threads (like `pthread_create` in C/POSIX or `std::thread` in C++), you ask the **Operating System Kernel** to manage multitasking on your behalf:

```
[ Your App: Thread 1 ]   [ Your App: Thread 2 ]   [ Your App: Thread 3 ]
         |                        |                        |
=========|========================|========================|=========  (Kernel Boundary)
[ OS Kernel Thread 1 ]   [ OS Kernel Thread 2 ]   [ OS Kernel Thread 3 ]
```

* **How it works:** The OS kernel gives each thread a kernel stack, schedules them using timer interrupts, and switches between them.
* **The Cost (Why Green Threads exist):** Kernel threads are **heavyweight**.
  1. **Memory Footprint**: Each kernel thread typically reserves 1 to 8 MB of virtual memory for its stack. Creating 100,000 kernel threads can quickly exhaust system memory.
  2. **Switching Overhead**: Every time the kernel switches threads, the CPU must switch privilege levels (**Ring 3 user mode $\to$ Ring 0 kernel mode**), flush hardware pipelines, and run the kernel scheduler. This costs thousands of CPU cycles.

---

## 3. The Green Threads Model (M:1 User-Level Threading)

Green threads eliminate the kernel from the scheduling loop. To the Linux/macOS kernel, our program is just **one single, ordinary thread**:

```
[ Green Thread 1 ]   [ Green Thread 2 ]   [ Green Thread 3 ]   [ Green Thread 4 ]
        \                    |                    |                   /
         +-------------------+--------------------+------------------+
                                      |
                           [ libgt Runtime ]
                                      |
======================================|================================  (Kernel Boundary)
                           [ 1 OS Kernel Thread ]
```

`libgt` is that **User-Space Runtime**. We multiplex hundreds or thousands of "virtual" threads on top of a single OS thread.

---

## 4. The 5 Hard Problems We Must Solve

To make this work in C, we must solve the same foundational problems that operating system kernel designers face:

### Problem 1: Private Stacks
If all threads shared the same memory stack, Thread B calling a function would overwrite Thread A’s local variables, saved frame pointers, and return addresses.
* **Our Solution:** For every thread we create, we allocate a private chunk of memory (e.g., 64 KB) to act as its own dedicated execution stack.

### Problem 2: Context Switching (The Freeze & Resume)
How do you pause a function mid-execution and resume it later?
A CPU's execution state consists of:
* The **Program Counter (PC)**: the next instruction address to execute.
* The **Stack Pointer (SP)**: where active local variables and call frames live.
* General-purpose registers: calculations in progress.
* The **Signal Mask**: which OS signals are currently blocked.
* **Our Solution:** We use POSIX `ucontext` (`swapcontext`) to snapshot all registers of Thread A into RAM, load the saved registers of Thread B into the CPU hardware, and jump to Thread B's last instruction.

### Problem 3: The Scheduler & Ready Queues
When Thread A pauses, who runs next?
* **Our Solution:** We implement a **FIFO Ready Queue**. When a thread yields or its time slice expires, it gets appended to the back of the queue; the scheduler dequeues the thread at the front and switches to it (Round-Robin scheduling).

### Problem 4: The "Suicide" Problem (Stack Deallocation)
When a thread finishes, its memory (stack and control block) must be freed. But **a thread can never free the stack it is currently running on**! If it calls `free(stack)` while executing on `stack`, the very machine instructions running the free routine will corrupt the memory beneath its own feet.
* **Our Solution:** A finished thread marks itself `DONE` (a "zombie") and context-switches away. Its memory is only freed later by `thread_join()` or by the scheduler after switching to a different, safe stack.

### Problem 5: Preemption (Handling Greedy Threads)
In cooperative multitasking, if a thread writes `while(1) {}`, it hogs the CPU forever and starves all other threads.
* **Our Solution:** We configure a repeating hardware interval timer (`setitimer`) that delivers an OS signal (`SIGALRM`) every 10 milliseconds. The signal handler catches the greedy thread mid-flight and forcibly switches away from it, protecting runtime internals with a `preempt_disable` nested counter.

---

## 5. Why Does This Matter in the Real World?

This exact architecture powers some of the most performant runtime environments in modern computing:

* **Go (Goroutines)**: Go does not create a kernel thread per goroutine; it uses an M:N user-space runtime scheduler almost identical in concept to what we are building.
* **Java 21 (Project Loom / Virtual Threads)**: Added lightweight green threads to Java so servers can handle 1,000,000 concurrent requests without thread-pool exhaustion.
* **Erlang / Elixir (BEAM Processes)**: Millions of lightweight actor processes scheduled cooperatively and preemptively in user space.
* **Rust (Async/Await & Tokio)**: Cooperative coroutine-style task scheduling over a pool of worker threads.

By building this library, you are implementing the actual machinery that modern programming language runtimes and operating systems use to deliver high-concurrency computing.
