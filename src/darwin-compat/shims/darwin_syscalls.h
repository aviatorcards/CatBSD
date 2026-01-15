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

/* Get process info */
int darwin_proc_pidinfo(int pid, int flavor, uint64_t arg, void *buffer,
                        int buffersize);

#ifdef __cplusplus
}
#endif

#endif /* _DARWIN_SYSCALLS_H_ */
