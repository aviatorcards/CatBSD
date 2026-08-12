/*
 * CatBSD Darwin Syscall Compatibility Shim
 * Provides FreeBSD implementations of Darwin-specific syscalls
 */

#ifndef _DARWIN_SYSCALLS_H_
#define _DARWIN_SYSCALLS_H_

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Time functions
 */

/* Get absolute time in nanoseconds (like mach_absolute_time) */
uint64_t darwin_absolute_time(void);

/* Convert absolute time to nanoseconds */
uint64_t darwin_absolute_to_nanoseconds(uint64_t absolute_time);

/*
 * Thread functions
 */

/* Get current thread ID (Darwin-style) */
uint64_t darwin_thread_self(void);

/* Set thread name */
int darwin_pthread_setname_np(const char *name);

/*
 * Memory functions
 */

/* Get page size */
size_t darwin_vm_page_size(void);

/*
 * Process functions
 */

/*
 * The only implemented flavor: basic BSD-visible process info, modeled
 * after Darwin's PROC_PIDTBSDINFO. Other flavor values are rejected.
 */
#define DARWIN_PROC_PIDTBSDINFO 3

typedef struct darwin_proc_bsdinfo {
  int32_t pbi_pid;
  int32_t pbi_ppid;
  uint32_t pbi_uid;
  uint32_t pbi_status; /* platform process state, e.g. SRUN/SSLEEP */
  char pbi_comm[64];
} darwin_proc_bsdinfo_t;

/*
 * Get process info, mirroring Darwin's proc_pidinfo(). Only
 * DARWIN_PROC_PIDTBSDINFO is implemented; `buffer` must point at a
 * darwin_proc_bsdinfo_t of at least `buffersize` bytes. `arg` is unused
 * (real proc_pidinfo overloads it per-flavor; no other flavor exists here).
 * Returns the number of bytes written on success, -1 on error (errno set).
 */
int darwin_proc_pidinfo(int pid, int flavor, uint64_t arg, void *buffer,
                        int buffersize);

#ifdef __cplusplus
}
#endif

#endif /* _DARWIN_SYSCALLS_H_ */
