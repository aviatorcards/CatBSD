/*
 * CatBSD Mach Port Compatibility Shim Implementation
 * Maps Mach port operations to FreeBSD kqueue
 */

#include "mach_port.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * Allocate a Mach port (implemented as kqueue)
 *
 * In Darwin, mach_port_allocate creates a new Mach port for IPC.
 * In FreeBSD, we use kqueue as the equivalent event notification mechanism.
 */
kern_return_t mach_port_allocate(mach_port_t *port) {
  int kq;

  if (port == NULL) {
    return KERN_INVALID_ARGUMENT;
  }

  /* Create a kqueue to represent the Mach port */
  kq = kqueue();
  if (kq < 0) {
    return KERN_NO_SPACE;
  }

  *port = kq;
  return KERN_SUCCESS;
}

/*
 * Deallocate a Mach port (close kqueue)
 */
kern_return_t mach_port_deallocate(mach_port_t port) {
  if (port == MACH_PORT_NULL || port == MACH_PORT_DEAD) {
    return KERN_INVALID_NAME;
  }

  if (close(port) < 0) {
    return KERN_FAILURE;
  }

  return KERN_SUCCESS;
}

/*
 * Insert a right into a port (placeholder)
 *
 * Mach ports have complex rights management. For now, this is a no-op.
 * Real implementation would need to track port rights.
 */
kern_return_t mach_port_insert_right(mach_port_t port, mach_port_t name,
                                     int right) {
  /* Placeholder - rights management not fully implemented */
  (void)port;
  (void)name;
  (void)right;

  return KERN_SUCCESS;
}

/*
 * Send a message to a Mach port
 *
 * In Darwin, mach_msg_send sends a message to a Mach port.
 * In FreeBSD, we trigger a kevent to notify waiters.
 */
kern_return_t mach_msg_send(mach_port_t port, void *msg, size_t len) {
  struct kevent kev;

  if (port == MACH_PORT_NULL || port == MACH_PORT_DEAD) {
    return KERN_INVALID_NAME;
  }

  if (msg == NULL || len == 0) {
    return KERN_INVALID_ARGUMENT;
  }

  /*
   * Set up a kevent to signal message arrival
   * In a real implementation, we'd need to queue the message data
   */
  EV_SET(&kev, 1, EVFILT_USER, EV_ADD | EV_ENABLE, NOTE_TRIGGER, 0, msg);

  if (kevent(port, &kev, 1, NULL, 0, NULL) < 0) {
    return KERN_FAILURE;
  }

  return KERN_SUCCESS;
}

/*
 * Receive a message from a Mach port
 *
 * In Darwin, mach_msg_receive waits for and receives a message.
 * In FreeBSD, we wait on kevent.
 */
kern_return_t mach_msg_receive(mach_port_t port, void *msg, size_t len,
                               int timeout_ms) {
  struct kevent kev;
  struct timespec timeout;
  struct timespec *timeout_ptr = NULL;
  int ret;

  if (port == MACH_PORT_NULL || port == MACH_PORT_DEAD) {
    return KERN_INVALID_NAME;
  }

  if (msg == NULL || len == 0) {
    return KERN_INVALID_ARGUMENT;
  }

  /* Set up timeout if specified */
  if (timeout_ms >= 0) {
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_nsec = (timeout_ms % 1000) * 1000000;
    timeout_ptr = &timeout;
  }

  /* Wait for event */
  ret = kevent(port, NULL, 0, &kev, 1, timeout_ptr);
  if (ret < 0) {
    return KERN_FAILURE;
  }

  if (ret == 0) {
    /* Timeout */
    return KERN_FAILURE;
  }

  /*
   * In a real implementation, we'd retrieve the queued message data
   * For now, just copy from udata if available
   */
  if (kev.udata != NULL) {
    memcpy(msg, kev.udata, len);
  }

  return KERN_SUCCESS;
}

/*
 * Get error string for Mach error code
 */
const char *mach_error_string(kern_return_t error) {
  switch (error) {
  case KERN_SUCCESS:
    return "Success";
  case KERN_FAILURE:
    return "Failure";
  case KERN_INVALID_ARGUMENT:
    return "Invalid argument";
  case KERN_NO_SPACE:
    return "No space available";
  case KERN_INVALID_NAME:
    return "Invalid name";
  default:
    return "Unknown error";
  }
}
