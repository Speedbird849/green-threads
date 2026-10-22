#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef USE_PTHREAD
  #include <pthread.h>
  #include <sched.h>
  #define ENGINE_NAME "POSIX Kernel Threads (pthreads)"
  typedef pthread_t server_thread_t;
  static inline int server_thread_create(server_thread_t *t, void *(*fn)(void *), void *arg) {
      pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      int res = pthread_create(t, &attr, fn, arg);
      pthread_attr_destroy(&attr);
      return res;
  }
  static inline void server_yield(void) {
      sched_yield();
  }
#else
  #include "libgt.h"
  #define ENGINE_NAME "libgt User-Level Green Threads"
  typedef thread_t server_thread_t;
  static inline int server_thread_create(server_thread_t *t, void *(*fn)(void *), void *arg) {
      return thread_create(t, fn, arg);
  }
  static inline void server_yield(void) {
      thread_yield();
  }
#endif

static volatile int total_requests_handled = 0;
static volatile int server_running = 1;
static int max_requests = 0; // 0 = run until killed

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

static void *handle_client(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    char buf[1024];
    ssize_t n = 0;

    /* Read HTTP request non-blockingly, yielding while waiting for data */
    while (server_running) {
        n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            break;
        } else if (n == 0) {
            close(client_fd);
            return NULL;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            server_yield();
        } else {
            close(client_fd);
            return NULL;
        }
    }

    /* Send HTTP response */
    const char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 17\r\n"
        "Connection: close\r\n\r\n"
        "libgt HTTP server";

    send(client_fd, response, sizeof(response) - 1, 0);
    close(client_fd);

    __sync_fetch_and_add(&total_requests_handled, 1);

    if (max_requests > 0 && total_requests_handled >= max_requests) {
        server_running = 0;
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int port = 8080;
    if (argc >= 2) {
        port = atoi(argv[1]);
    }
    if (argc >= 3) {
        max_requests = atoi(argv[2]);
    }

    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1024) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    set_nonblocking(server_fd);

    printf("=============================================================\n");
    printf("  Dual-Engine HTTP Server Demo\n");
    printf("  Engine: %s\n", ENGINE_NAME);
    printf("  Listening on http://127.0.0.1:%d\n", port);
    if (max_requests > 0) {
        printf("  Target: handling %d requests before clean exit\n", max_requests);
    }
    printf("=============================================================\n");
    fflush(stdout);

    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd >= 0) {
            set_nonblocking(client_fd);
            server_thread_t th;
            server_thread_create(&th, handle_client, (void *)(intptr_t)client_fd);
            server_yield();
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* No new connections pending; yield to let active connection threads execute */
            server_yield();
        } else {
            perror("accept");
            break;
        }
    }

    close(server_fd);
    printf("\nServer shutdown complete. Total requests handled: %d\n", total_requests_handled);
    return 0;
}
