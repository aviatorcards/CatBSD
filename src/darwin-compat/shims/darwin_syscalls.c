/*
 * CatBSD Darwin Syscall Compatibility Shim Implementation
 */

#include "darwin_syscalls.h"
#include <errno.h>
#include <pthread.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/user.h>
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
 * FreeBSD/macOS: KERN_PROC_PID sysctl, whose kinfo_proc layout differs
 * per platform, normalized here into darwin_proc_bsdinfo_t.
 */
int darwin_proc_pidinfo(int pid, int flavor, uint64_t arg, void *buffer,
                        int buffersize) {
  (void)arg;

  if (buffer == NULL || pid < 0) {
    errno = EINVAL;
    return -1;
  }

  if (flavor != DARWIN_PROC_PIDTBSDINFO) {
    errno = EINVAL;
    return -1;
  }

  if ((size_t)buffersize < sizeof(darwin_proc_bsdinfo_t)) {
    errno = ENOSPC;
    return -1;
  }

#if defined(__APPLE__) || defined(__FreeBSD__)
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
  struct kinfo_proc kp;
  size_t len = sizeof(kp);

  memset(&kp, 0, sizeof(kp));
  if (sysctl(mib, 4, &kp, &len, NULL, 0) != 0) {
    return -1;
  }
  if (len == 0) {
    /* No such process: sysctl succeeds but returns nothing. */
    errno = ESRCH;
    return -1;
  }

  darwin_proc_bsdinfo_t info;
  memset(&info, 0, sizeof(info));

#if defined(__APPLE__)
  info.pbi_pid = kp.kp_proc.p_pid;
  info.pbi_ppid = kp.kp_eproc.e_ppid;
  info.pbi_uid = kp.kp_eproc.e_ucred.cr_uid;
  info.pbi_status = (uint32_t)kp.kp_proc.p_stat;
  strlcpy(info.pbi_comm, kp.kp_proc.p_comm, sizeof(info.pbi_comm));
#else /* __FreeBSD__ */
  info.pbi_pid = kp.ki_pid;
  info.pbi_ppid = kp.ki_ppid;
  info.pbi_uid = kp.ki_uid;
  info.pbi_status = (uint32_t)kp.ki_stat;
  strlcpy(info.pbi_comm, kp.ki_comm, sizeof(info.pbi_comm));
#endif

  memcpy(buffer, &info, sizeof(info));
  return (int)sizeof(info);
#else
  errno = ENOTSUP;
  return -1;
#endif
}
