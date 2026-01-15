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

/* Mach port type */
typedef int mach_port_t;
typedef int kern_return_t;

/* Mach port names */
#define MACH_PORT_NULL          ((mach_port_t) 0)
#define MACH_PORT_DEAD          ((mach_port_t) ~0)

/* Return codes */
#define KERN_SUCCESS            0
#define KERN_FAILURE            -1
#define KERN_INVALID_ARGUMENT   -2
#define KERN_NO_SPACE           -3
#define KERN_INVALID_NAME       -4

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
kern_return_t mach_port_insert_right(mach_port_t port, mach_port_t name, int right);

/* Message operations */
kern_return_t mach_msg_send(mach_port_t port, void *msg, size_t len);
kern_return_t mach_msg_receive(mach_port_t port, void *msg, size_t len, int timeout);

/* Utility functions */
const char* mach_error_string(kern_return_t error);

#ifdef __cplusplus
}
#endif

#endif /* _MACH_PORT_SHIM_H_ */
