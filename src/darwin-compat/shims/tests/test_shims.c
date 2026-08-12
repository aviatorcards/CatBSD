/*
 * Test program for CatBSD Darwin compatibility shims
 * Demonstrates Mach port and Darwin syscall compatibility
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "darwin_syscalls.h"
#include "mach_port.h"

/* Test Mach port allocation, a single send/receive round trip, and
 * deallocation. Content is checked with memcmp, not just the return code,
 * since a pointer-passing shim could "succeed" without actually
 * transporting the right bytes. */
void test_mach_ports(void) {
  printf("Testing Mach port compatibility...\n");

  mach_port_t port;
  kern_return_t ret;

  ret = mach_port_allocate(&port);
  assert(ret == KERN_SUCCESS);
  assert(port != MACH_PORT_NULL);
  printf("  \xE2\x9C\x93 mach_port_allocate: port=%d\n", port);

  char send_msg[] = "Hello from CatBSD!";
  char recv_msg[256] = {0};

  ret = mach_msg_send(port, send_msg, sizeof(send_msg));
  assert(ret == KERN_SUCCESS);
  printf("  \xE2\x9C\x93 mach_msg_send: %s\n", mach_error_string(ret));

  ret = mach_msg_receive(port, recv_msg, sizeof(recv_msg), 100);
  assert(ret == KERN_SUCCESS);
  assert(memcmp(send_msg, recv_msg, sizeof(send_msg)) == 0);
  printf("  \xE2\x9C\x93 mach_msg_receive: got back \"%s\" unchanged\n",
         recv_msg);

  ret = mach_port_deallocate(port);
  assert(ret == KERN_SUCCESS);
  printf("  \xE2\x9C\x93 mach_port_deallocate: success\n");

  printf("  All Mach port tests passed!\n\n");
}

/* A naive single-kevent-udata implementation loses all but the last
 * message when several are sent before anyone receives, because kqueue
 * coalesces repeated EVFILT_USER triggers on the same ident. Confirm the
 * real queue behind the shim preserves every message, in order. */
void test_message_queue_fifo(void) {
  printf("Testing Mach port message queue ordering...\n");

  mach_port_t port;
  assert(mach_port_allocate(&port) == KERN_SUCCESS);

  const int count = 50;
  for (int i = 0; i < count; i++) {
    char msg[32];
    int n = snprintf(msg, sizeof(msg), "queued-%d", i);
    assert(mach_msg_send(port, msg, (size_t)n + 1) == KERN_SUCCESS);
  }

  for (int i = 0; i < count; i++) {
    char expected[32];
    char recv_msg[32] = {0};
    snprintf(expected, sizeof(expected), "queued-%d", i);

    kern_return_t ret = mach_msg_receive(port, recv_msg, sizeof(recv_msg), 100);
    assert(ret == KERN_SUCCESS);
    assert(strcmp(expected, recv_msg) == 0);
  }
  printf("  \xE2\x9C\x93 %d queued messages received in FIFO order, "
         "content intact\n",
         count);

  assert(mach_port_deallocate(port) == KERN_SUCCESS);
  printf("  All message queue tests passed!\n\n");
}

/* mach_msg_receive on an empty queue must time out on roughly the
 * requested deadline rather than hanging or returning immediately. */
void test_receive_timeout(void) {
  printf("Testing Mach port receive timeout...\n");

  mach_port_t port;
  assert(mach_port_allocate(&port) == KERN_SUCCESS);

  char buf[16];
  uint64_t start = darwin_absolute_time();
  kern_return_t ret = mach_msg_receive(port, buf, sizeof(buf), 50);
  uint64_t elapsed_ms = (darwin_absolute_time() - start) / 1000000ULL;

  assert(ret == KERN_TIMED_OUT);
  /* Generous bounds: this only needs to prove we waited roughly the
   * requested time, not that scheduling is precise. */
  assert(elapsed_ms >= 40);
  assert(elapsed_ms < 2000);
  printf("  \xE2\x9C\x93 mach_msg_receive on empty port timed out after "
         "~%llums (requested 50ms): %s\n",
         (unsigned long long)elapsed_ms, mach_error_string(ret));

  assert(mach_port_deallocate(port) == KERN_SUCCESS);
  printf("  All receive timeout tests passed!\n\n");
}

/* Port rights used to be a documented no-op. Confirm they now actually
 * accumulate per name and are queryable. */
void test_port_rights(void) {
  printf("Testing Mach port rights tracking...\n");

  mach_port_t port;
  assert(mach_port_allocate(&port) == KERN_SUCCESS);

  mach_port_t name = 42;

  assert(!mach_port_has_right(port, name, CATBSD_PORT_RIGHT_SEND));

  assert(mach_port_insert_right(port, name, CATBSD_PORT_RIGHT_SEND) ==
         KERN_SUCCESS);
  assert(mach_port_has_right(port, name, CATBSD_PORT_RIGHT_SEND));
  assert(!mach_port_has_right(port, name, CATBSD_PORT_RIGHT_RECEIVE));
  printf("  \xE2\x9C\x93 insert_right(SEND) is visible via has_right, "
         "and only for SEND\n");

  assert(mach_port_insert_right(port, name, CATBSD_PORT_RIGHT_RECEIVE) ==
         KERN_SUCCESS);
  assert(mach_port_has_right(port, name, CATBSD_PORT_RIGHT_SEND));
  assert(mach_port_has_right(port, name, CATBSD_PORT_RIGHT_RECEIVE));
  assert(mach_port_has_right(port, name,
                             CATBSD_PORT_RIGHT_SEND | CATBSD_PORT_RIGHT_RECEIVE));
  printf("  \xE2\x9C\x93 rights accumulate: name now holds SEND and "
         "RECEIVE simultaneously\n");

  /* A different, never-granted name must not see rights it wasn't given. */
  assert(!mach_port_has_right(port, name + 1, CATBSD_PORT_RIGHT_SEND));
  printf("  \xE2\x9C\x93 rights are scoped per name, not per port\n");

  assert(mach_port_insert_right(port, name, 0xFF) == KERN_INVALID_ARGUMENT);
  printf("  \xE2\x9C\x93 insert_right rejects an unrecognized right value\n");

  assert(mach_port_deallocate(port) == KERN_SUCCESS);
  printf("  All port rights tests passed!\n\n");
}

typedef struct {
  mach_port_t port;
  int delay_ms;
  const char *msg;
} delayed_sender_args_t;

static void *delayed_sender(void *arg) {
  delayed_sender_args_t *args = (delayed_sender_args_t *)arg;
  usleep((useconds_t)args->delay_ms * 1000);
  mach_msg_send(args->port, (void *)args->msg, strlen(args->msg) + 1);
  return NULL;
}

/* mach_port_wait_any is what CATBSD_PORT_RIGHT_PORT_SET actually enables:
 * blocking on several ports at once instead of one receive per port. */
void test_port_wait_any(void) {
  printf("Testing mach_port_wait_any (port sets)...\n");

  const int count = 4;
  mach_port_t ports[4];
  for (int i = 0; i < count; i++) {
    assert(mach_port_allocate(&ports[i]) == KERN_SUCCESS);
  }

  /* Case 1: a message already queued before waiting -- the fast path. */
  const char *early_msg = "already-here";
  assert(mach_msg_send(ports[2], (void *)early_msg,
                       strlen(early_msg) + 1) == KERN_SUCCESS);

  int ready = -1;
  kern_return_t ret = mach_port_wait_any(ports, count, &ready, 200);
  assert(ret == KERN_SUCCESS);
  assert(ready == 2);
  printf("  \xE2\x9C\x93 wait_any returns immediately for an "
         "already-queued message on port index %d\n", ready);

  char buf[32] = {0};
  assert(mach_msg_receive(ports[ready], buf, sizeof(buf), 100) ==
         KERN_SUCCESS);
  assert(strcmp(buf, early_msg) == 0);
  printf("  \xE2\x9C\x93 message content intact after wait_any + "
         "receive\n");

  /* Case 2: real blocking wait -- a background thread sends after a
   * delay, on a different port index than case 1, to prove wait_any
   * is actually waking on new activity rather than re-detecting stale
   * state left over from case 1. */
  delayed_sender_args_t sender_args = {ports[1], 80, "delayed-hello"};
  pthread_t thread;
  assert(pthread_create(&thread, NULL, delayed_sender, &sender_args) == 0);

  uint64_t start = darwin_absolute_time();
  ready = -1;
  ret = mach_port_wait_any(ports, count, &ready, 2000);
  uint64_t elapsed_ms = (darwin_absolute_time() - start) / 1000000ULL;
  pthread_join(thread, NULL);

  assert(ret == KERN_SUCCESS);
  assert(ready == 1);
  /* Should wake close to the 80ms send delay, nowhere near the 2000ms
   * timeout -- proves this is a real wakeup, not a delayed timeout. */
  assert(elapsed_ms < 1000);
  printf("  \xE2\x9C\x93 wait_any woke for a message sent from another "
         "thread ~%llums later, correctly on port index %d\n",
         (unsigned long long)elapsed_ms, ready);

  memset(buf, 0, sizeof(buf));
  assert(mach_msg_receive(ports[ready], buf, sizeof(buf), 100) ==
         KERN_SUCCESS);
  assert(strcmp(buf, "delayed-hello") == 0);

  /* Case 3: nothing sent anywhere -- must time out, not hang or
   * false-positive on a port. */
  ready = -1;
  start = darwin_absolute_time();
  ret = mach_port_wait_any(ports, count, &ready, 50);
  elapsed_ms = (darwin_absolute_time() - start) / 1000000ULL;
  assert(ret == KERN_TIMED_OUT);
  assert(elapsed_ms >= 40 && elapsed_ms < 2000);
  printf("  \xE2\x9C\x93 wait_any times out after ~%llums when no port "
         "has a message\n", (unsigned long long)elapsed_ms);

  for (int i = 0; i < count; i++) {
    assert(mach_port_deallocate(ports[i]) == KERN_SUCCESS);
  }
  printf("  All port set tests passed!\n\n");
}

/* Test Darwin syscall compatibility */
void test_darwin_syscalls(void) {
  printf("Testing Darwin syscall compatibility...\n");

  /* Test absolute time */
  uint64_t start = darwin_absolute_time();
  usleep(10000); // Sleep 10ms
  uint64_t end = darwin_absolute_time();
  uint64_t elapsed_ns = end - start;

  printf("  \xE2\x9C\x93 darwin_absolute_time: %llu ns elapsed\n",
         (unsigned long long)elapsed_ns);
  assert(elapsed_ns > 0);

  /* Test thread ID */
  uint64_t thread_id = darwin_thread_self();
  printf("  \xE2\x9C\x93 darwin_thread_self: thread_id=%llu\n",
         (unsigned long long)thread_id);
  assert(thread_id != 0);

  /* Test thread name */
  int ret = darwin_pthread_setname_np("test-thread");
  printf("  \xE2\x9C\x93 darwin_pthread_setname_np: %s\n",
         ret == 0 ? "success" : "failed");

  /* Test page size */
  size_t page_size = darwin_vm_page_size();
  printf("  \xE2\x9C\x93 darwin_vm_page_size: %zu bytes\n", page_size);
  assert(page_size > 0);

  printf("  All Darwin syscall tests passed!\n\n");
}

/* Test process info lookup against our own running process, whose pid
 * and command name we already know from getpid()/argv[0]. */
void test_proc_pidinfo(void) {
  printf("Testing Darwin proc_pidinfo compatibility...\n");

  darwin_proc_bsdinfo_t info;
  memset(&info, 0, sizeof(info));

  int n = darwin_proc_pidinfo(getpid(), DARWIN_PROC_PIDTBSDINFO, 0, &info,
                              sizeof(info));
  assert(n == (int)sizeof(info));
  assert(info.pbi_pid == getpid());
  assert(info.pbi_comm[0] != '\0');
  printf("  \xE2\x9C\x93 darwin_proc_pidinfo(self): pid=%d ppid=%d "
         "comm=\"%s\"\n",
         info.pbi_pid, info.pbi_ppid, info.pbi_comm);

  /* An unsupported flavor must fail cleanly rather than returning
   * whatever happened to be in the caller's buffer. */
  int bad = darwin_proc_pidinfo(getpid(), DARWIN_PROC_PIDTBSDINFO + 99, 0,
                                &info, sizeof(info));
  assert(bad == -1);
  printf("  \xE2\x9C\x93 darwin_proc_pidinfo rejects an unimplemented "
         "flavor\n");

  printf("  All proc_pidinfo tests passed!\n\n");
}

/* Demonstrate practical usage: a full send-then-receive round trip over
 * many messages, timed with darwin_absolute_time. */
void demo_practical_usage(void) {
  printf("Demonstrating practical usage...\n");

  uint64_t start = darwin_absolute_time();

  mach_port_t port;
  mach_port_allocate(&port);

  const int total = 1000;
  for (int i = 0; i < total; i++) {
    char msg[32];
    snprintf(msg, sizeof(msg), "Message %d", i);
    mach_msg_send(port, msg, strlen(msg) + 1);
  }

  int received = 0;
  for (int i = 0; i < total; i++) {
    char msg[32] = {0};
    if (mach_msg_receive(port, msg, sizeof(msg), 100) == KERN_SUCCESS) {
      received++;
    }
  }
  assert(received == total);

  mach_port_deallocate(port);

  uint64_t end = darwin_absolute_time();
  uint64_t elapsed_us = (end - start) / 1000;

  printf("  Sent and received %d/%d messages in %llu microseconds\n",
         received, total, (unsigned long long)elapsed_us);
  printf("  Average: %llu ns per message\n",
         (unsigned long long)((end - start) / (uint64_t)total));

  printf("\n");
}

int main(void) {
  printf("=== CatBSD Darwin Compatibility Shim Test ===\n\n");

  test_mach_ports();
  test_message_queue_fifo();
  test_receive_timeout();
  test_port_rights();
  test_port_wait_any();
  test_darwin_syscalls();
  test_proc_pidinfo();
  demo_practical_usage();

  printf("=== All tests passed! ===\n");
  printf("\nThis demonstrates that Darwin code can run on FreeBSD\n");
  printf("using the CatBSD compatibility shim library.\n");

  return 0;
}
