/*
 * CatBSD Darwin Syscall Compatibility Shim Implementation
 */

#include "darwin_syscalls.h"
#include <pthread.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif
#include <string.h>
#include <unistd.h>

/*
 * Get absolute time in nanoseconds
 * Darwin equivalent: mach_absolute_time()
 * FreeBSD: Use clock_gettime with CLOCK_MONOTONIC
 */
uint64_t darwin_absolute_time(void) {
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }

  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/*
 * Convert absolute time to nanoseconds
 * On FreeBSD, absolute time is already in nanoseconds
 */
uint64_t darwin_absolute_to_nanoseconds(uint64_t absolute_time) {
  return absolute_time;
}

/*
 * Get current thread ID (Darwin-style)
 * Darwin: Returns a uint64_t thread ID
 * FreeBSD: Use pthread_self() and cast
 */
uint64_t darwin_thread_self(void) {
  pthread_t thread = pthread_self();
  return (uint64_t)(uintptr_t)thread;
}

/*
 * Set thread name
 * Darwin: pthread_setname_np(const char *name)
 * FreeBSD: pthread_set_name_np(pthread_t thread, const char *name)
 * macOS: pthread_setname_np(const char *name)
 */
int darwin_pthread_setname_np(const char *name) {
  if (name == NULL) {
    return -1;
  }

#ifdef __FreeBSD__
  /* FreeBSD requires thread parameter */
  pthread_set_name_np(pthread_self(), name);
#elif defined(__APPLE__)
  /* macOS has the same signature as Darwin */
  pthread_setname_np(name);
#else
  /* Other systems - best effort */
  (void)name;
#endif

  return 0;
}

/*
 * Get VM page size
 * Darwin: vm_page_size global variable
 * FreeBSD: getpagesize() function
 */
size_t darwin_vm_page_size(void) { return (size_t)getpagesize(); }

/*
 * Get process info
 * Darwin: proc_pidinfo() syscall
 * FreeBSD: Use sysctl or procstat
 *
 * This is a simplified stub - full implementation would need
 * to handle different info flavors
 */
int darwin_proc_pidinfo(int pid, int flavor, uint64_t arg, void *buffer,
                        int buffersize) {
  /* Placeholder implementation */
  (void)pid;
  (void)flavor;
  (void)arg;
  (void)buffer;
  (void)buffersize;

  /* TODO: Implement using sysctl */
  return -1;
}
