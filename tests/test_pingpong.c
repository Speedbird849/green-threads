#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#define STACK_SIZE 64 * 1024
#define PING_PONG_ROUNDS 5

static ucontext_t ctx_main;
static ucontext_t ctx_a;
static ucontext_t ctx_b;

static char stack_a[STACK_SIZE];
static char stack_b[STACK_SIZE];

static int count_a = 0;
static int count_b = 0;

static void func_a(void) {
    while (count_a < PING_PONG_ROUNDS) {
        count_a++;
        printf("A%d ", count_a);
        fflush(stdout);

        // Yield to thread B
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
    while (count_b < PING_PONG_ROUNDS) {
        count_b++;
        printf("B%d ", count_b);
        fflush(stdout);

        // Yield to thread A
        if (swapcontext(&ctx_b, &ctx_a) == -1) {
            perror("swapcontext B -> A");
            exit(1);
        }
    }

    // Done with B, switch back to A so A can terminate/switch back to main
    if (swapcontext(&ctx_b, &ctx_a) == -1) {
        perror("swapcontext B -> A");
        exit(1);
    }
}

int main(void) {
    printf("--- Stage 1: Context Switching Ping-Pong ---\n");

    // Initialize Context A
    if (getcontext(&ctx_a) == -1) {
        perror("getcontext ctx_a");
        return 1;
    }
    ctx_a.uc_stack.ss_sp = stack_a;
    ctx_a.uc_stack.ss_size = sizeof(stack_a);
    ctx_a.uc_link = &ctx_main;
    makecontext(&ctx_a, func_a, 0);

    // Initialize Context B
    if (getcontext(&ctx_b) == -1) {
        perror("getcontext ctx_b");
        return 1;
    }
    ctx_b.uc_stack.ss_sp = stack_b;
    ctx_b.uc_stack.ss_size = sizeof(stack_b);
    ctx_b.uc_link = &ctx_main;
    makecontext(&ctx_b, func_b, 0);

    printf("Starting ping-pong:\n");
    // Switch from main context into Context A
    if (swapcontext(&ctx_main, &ctx_a) == -1) {
        perror("swapcontext main -> A");
        return 1;
    }

    printf("\nSuccessfully returned to main context!\n");
    return 0;
}
