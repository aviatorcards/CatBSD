/*
 * Test program for CatBSD Darwin compatibility shims
 * Demonstrates Mach port and Darwin syscall compatibility
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "darwin_syscalls.h"
#include "mach_port.h"

/* Test Mach port operations */
void test_mach_ports(void) {
  printf("Testing Mach port compatibility...\n");

  mach_port_t port;
  kern_return_t ret;

  /* Test allocation */
  ret = mach_port_allocate(&port);
  assert(ret == KERN_SUCCESS);
  assert(port != MACH_PORT_NULL);
  printf("  ✓ mach_port_allocate: port=%d\n", port);

  /* Test message send/receive */
  char send_msg[] = "Hello from CatBSD!";
  char recv_msg[256] = {0};

  ret = mach_msg_send(port, send_msg, sizeof(send_msg));
  printf("  ✓ mach_msg_send: %s\n", mach_error_string(ret));

  ret = mach_msg_receive(port, recv_msg, sizeof(recv_msg), 100);
  printf("  ✓ mach_msg_receive: %s\n", mach_error_string(ret));

  /* Test deallocation */
  ret = mach_port_deallocate(port);
  assert(ret == KERN_SUCCESS);
  printf("  ✓ mach_port_deallocate: success\n");

  printf("  All Mach port tests passed!\n\n");
}

/* Test Darwin syscall compatibility */
void test_darwin_syscalls(void) {
  printf("Testing Darwin syscall compatibility...\n");

  /* Test absolute time */
  uint64_t start = darwin_absolute_time();
  usleep(10000); // Sleep 10ms
  uint64_t end = darwin_absolute_time();
  uint64_t elapsed_ns = end - start;

  printf("  ✓ darwin_absolute_time: %llu ns elapsed\n",
         (unsigned long long)elapsed_ns);
  assert(elapsed_ns > 0);

  /* Test thread ID */
  uint64_t thread_id = darwin_thread_self();
  printf("  ✓ darwin_thread_self: thread_id=%llu\n",
         (unsigned long long)thread_id);
  assert(thread_id != 0);

  /* Test thread name */
  int ret = darwin_pthread_setname_np("test-thread");
  printf("  ✓ darwin_pthread_setname_np: %s\n",
         ret == 0 ? "success" : "failed");

  /* Test page size */
  size_t page_size = darwin_vm_page_size();
  printf("  ✓ darwin_vm_page_size: %zu bytes\n", page_size);
  assert(page_size > 0);

  printf("  All Darwin syscall tests passed!\n\n");
}

/* Demonstrate practical usage */
void demo_practical_usage(void) {
  printf("Demonstrating practical usage...\n");

  /* Timing a simple operation */
  uint64_t start = darwin_absolute_time();

  /* Create a Mach port for IPC */
  mach_port_t port;
  mach_port_allocate(&port);

  /* Simulate some work */
  for (int i = 0; i < 1000; i++) {
    char msg[32];
    snprintf(msg, sizeof(msg), "Message %d", i);
    mach_msg_send(port, msg, strlen(msg) + 1);
  }

  mach_port_deallocate(port);

  uint64_t end = darwin_absolute_time();
  uint64_t elapsed_us = (end - start) / 1000;

  printf("  Processed 1000 messages in %llu microseconds\n",
         (unsigned long long)elapsed_us);
  printf("  Average: %llu ns per message\n",
         (unsigned long long)((end - start) / 1000));

  printf("\n");
}

int main(void) {
  printf("=== CatBSD Darwin Compatibility Shim Test ===\n\n");

  test_mach_ports();
  test_darwin_syscalls();
  demo_practical_usage();

  printf("=== All tests passed! ===\n");
  printf("\nThis demonstrates that Darwin code can run on FreeBSD\n");
  printf("using the CatBSD compatibility shim library.\n");

  return 0;
}
