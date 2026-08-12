/*
 * CatBSD Mach Port Compatibility Shim
 * Maps Mach port operations to FreeBSD kqueue
 */

#ifndef _MACH_PORT_SHIM_H_
#define _MACH_PORT_SHIM_H_

#include <sys/types.h>
#include <sys/event.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mach port type
 *
 * On real Darwin, mach_port_t already exists system-wide (as unsigned
 * int) and gets pulled in transitively by many headers, including
 * pthread.h. Redeclaring it here with a different underlying type is a
 * hard compile error the moment both end up in the same translation
 * unit, so on Apple platforms we defer to the system's own definition
 * instead of shadowing it; non-Darwin targets (the actual FreeBSD
 * compat use case) get our own.
 */
#if defined(__APPLE__)
#include <mach/port.h>
#else
typedef int mach_port_t;
#endif
typedef int kern_return_t;

/* Mach port names */
#ifndef MACH_PORT_NULL
#define MACH_PORT_NULL          ((mach_port_t) 0)
#endif
#ifndef MACH_PORT_DEAD
#define MACH_PORT_DEAD          ((mach_port_t) ~0)
#endif

/* Return codes */
#define KERN_SUCCESS            0
#define KERN_FAILURE            -1
#define KERN_INVALID_ARGUMENT   -2
#define KERN_NO_SPACE           -3
#define KERN_INVALID_NAME       -4
#define KERN_TIMED_OUT          -5

/*
 * Port rights (OR-able bitmask; a given name may hold more than one at
 * once). Named CATBSD_PORT_RIGHT_* rather than MACH_PORT_RIGHT_* because
 * real Darwin's <mach/port.h> already defines that name as an ordinal
 * mach_port_right_t tag (0,1,2,3), which is not bit-OR-able the way this
 * shim's simplified rights model requires.
 */
#define CATBSD_PORT_RIGHT_SEND        0x01
#define CATBSD_PORT_RIGHT_RECEIVE     0x02
#define CATBSD_PORT_RIGHT_SEND_ONCE   0x04
#define CATBSD_PORT_RIGHT_PORT_SET    0x08

/* Mach message types */
typedef struct {
    uint32_t msgh_bits;
    uint32_t msgh_size;
    mach_port_t msgh_remote_port;
    mach_port_t msgh_local_port;
    uint32_t msgh_reserved;
    uint32_t msgh_id;
} mach_msg_header_t;

/* Port operations */
kern_return_t mach_port_allocate(mach_port_t *port);
kern_return_t mach_port_deallocate(mach_port_t port);

/*
 * Record that `name` holds `right` (one of MACH_PORT_RIGHT_*) against `port`.
 * Rights accumulate: calling this twice with different rights for the same
 * name means the name now holds both.
 */
kern_return_t mach_port_insert_right(mach_port_t port, mach_port_t name, int right);

/* Non-zero if `name` currently holds every bit set in `right` for `port`. */
int mach_port_has_right(mach_port_t port, mach_port_t name, int right);

/* Message operations */
kern_return_t mach_msg_send(mach_port_t port, void *msg, size_t len);
kern_return_t mach_msg_receive(mach_port_t port, void *msg, size_t len, int timeout);

/*
 * Port sets: block until at least one of `ports[0..count)` has a message
 * queued, or `timeout_ms` elapses (negative = wait forever). On
 * KERN_SUCCESS, *ready_index_out is the index into `ports` of a port
 * that's ready -- follow up with mach_msg_receive() on ports[*ready_index_out]
 * to actually drain it. This is what CATBSD_PORT_RIGHT_PORT_SET names:
 * the ability to wait on many ports at once, the way a real init system
 * waits on every supervised daemon's port in one loop instead of one
 * blocking receive per port.
 */
kern_return_t mach_port_wait_any(const mach_port_t *ports, int count,
                                 int *ready_index_out, int timeout_ms);

/* Utility functions */
const char* mach_error_string(kern_return_t error);

#ifdef __cplusplus
}
#endif

#endif /* _MACH_PORT_SHIM_H_ */
