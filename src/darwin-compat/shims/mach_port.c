/*
 * CatBSD Mach Port Compatibility Shim Implementation
 * Maps Mach port operations to FreeBSD kqueue
 */

#include "mach_port.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/*
 * A Mach port is represented by a kqueue fd, which only gives us
 * notification ("something arrived") -- kqueue itself has nowhere to
 * stash the message bytes. We keep the actual message data and right
 * grants in a small in-process registry keyed by that fd, and use
 * EVFILT_USER purely as the wakeup signal for mach_msg_receive.
 */

typedef struct msg_node {
  struct msg_node *next;
  size_t len;
  unsigned char data[];
} msg_node_t;

typedef struct right_node {
  struct right_node *next;
  mach_port_t name;
  int rights; /* bitmask of CATBSD_PORT_RIGHT_* */
} right_node_t;

typedef struct port_entry {
  struct port_entry *next;
  mach_port_t port;
  msg_node_t *msg_head;
  msg_node_t *msg_tail;
  right_node_t *rights;
  pthread_mutex_t lock;
} port_entry_t;

static port_entry_t *g_ports = NULL;
static pthread_mutex_t g_registry_lock = PTHREAD_MUTEX_INITIALIZER;

/* Find an existing entry for `port`, or create one. Registry-locked. */
static port_entry_t *port_entry_get(mach_port_t port, int create) {
  port_entry_t *entry;

  pthread_mutex_lock(&g_registry_lock);

  for (entry = g_ports; entry != NULL; entry = entry->next) {
    if (entry->port == port) {
      pthread_mutex_unlock(&g_registry_lock);
      return entry;
    }
  }

  if (!create) {
    pthread_mutex_unlock(&g_registry_lock);
    return NULL;
  }

  entry = malloc(sizeof(*entry));
  if (entry == NULL) {
    pthread_mutex_unlock(&g_registry_lock);
    return NULL;
  }

  entry->port = port;
  entry->msg_head = NULL;
  entry->msg_tail = NULL;
  entry->rights = NULL;
  pthread_mutex_init(&entry->lock, NULL);
  entry->next = g_ports;
  g_ports = entry;

  pthread_mutex_unlock(&g_registry_lock);
  return entry;
}

static void port_entry_remove(mach_port_t port) {
  port_entry_t *entry, **prev;
  msg_node_t *msg, *next_msg;
  right_node_t *right, *next_right;

  pthread_mutex_lock(&g_registry_lock);

  prev = &g_ports;
  entry = g_ports;
  while (entry != NULL && entry->port != port) {
    prev = &entry->next;
    entry = entry->next;
  }

  if (entry == NULL) {
    pthread_mutex_unlock(&g_registry_lock);
    return;
  }

  *prev = entry->next;
  pthread_mutex_unlock(&g_registry_lock);

  for (msg = entry->msg_head; msg != NULL; msg = next_msg) {
    next_msg = msg->next;
    free(msg);
  }
  for (right = entry->rights; right != NULL; right = next_right) {
    next_right = right->next;
    free(right);
  }
  pthread_mutex_destroy(&entry->lock);
  free(entry);
}

/* Pop the oldest queued message for `entry`, or NULL if empty. */
static msg_node_t *port_entry_dequeue(port_entry_t *entry) {
  msg_node_t *node;

  pthread_mutex_lock(&entry->lock);
  node = entry->msg_head;
  if (node != NULL) {
    entry->msg_head = node->next;
    if (entry->msg_head == NULL) {
      entry->msg_tail = NULL;
    }
  }
  pthread_mutex_unlock(&entry->lock);

  return node;
}

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

  if (port_entry_get(kq, 1) == NULL) {
    close(kq);
    return KERN_NO_SPACE;
  }

  *port = kq;
  return KERN_SUCCESS;
}

/*
 * Deallocate a Mach port (close kqueue, drop its queued messages and rights)
 */
kern_return_t mach_port_deallocate(mach_port_t port) {
  if (port == MACH_PORT_NULL || port == MACH_PORT_DEAD) {
    return KERN_INVALID_NAME;
  }

  port_entry_remove(port);

  if (close(port) < 0) {
    return KERN_FAILURE;
  }

  return KERN_SUCCESS;
}

/*
 * Grant `name` a right against `port`. Rights accumulate in a bitmask
 * rather than the previous no-op, so callers can round-trip them with
 * mach_port_has_right().
 */
kern_return_t mach_port_insert_right(mach_port_t port, mach_port_t name,
                                     int right) {
  port_entry_t *entry;
  right_node_t *node;

  if (port == MACH_PORT_NULL || port == MACH_PORT_DEAD) {
    return KERN_INVALID_NAME;
  }

  if (right != CATBSD_PORT_RIGHT_SEND && right != CATBSD_PORT_RIGHT_RECEIVE &&
      right != CATBSD_PORT_RIGHT_SEND_ONCE && right != CATBSD_PORT_RIGHT_PORT_SET) {
    return KERN_INVALID_ARGUMENT;
  }

  entry = port_entry_get(port, 1);
  if (entry == NULL) {
    return KERN_NO_SPACE;
  }

  pthread_mutex_lock(&entry->lock);

  for (node = entry->rights; node != NULL; node = node->next) {
    if (node->name == name) {
      break;
    }
  }

  if (node == NULL) {
    node = malloc(sizeof(*node));
    if (node == NULL) {
      pthread_mutex_unlock(&entry->lock);
      return KERN_NO_SPACE;
    }
    node->name = name;
    node->rights = 0;
    node->next = entry->rights;
    entry->rights = node;
  }

  node->rights |= right;

  pthread_mutex_unlock(&entry->lock);

  return KERN_SUCCESS;
}

int mach_port_has_right(mach_port_t port, mach_port_t name, int right) {
  port_entry_t *entry;
  right_node_t *node;
  int has = 0;

  entry = port_entry_get(port, 0);
  if (entry == NULL) {
    return 0;
  }

  pthread_mutex_lock(&entry->lock);
  for (node = entry->rights; node != NULL; node = node->next) {
    if (node->name == name) {
      has = (node->rights & right) == right;
      break;
    }
  }
  pthread_mutex_unlock(&entry->lock);

  return has;
}

/*
 * Send a message to a Mach port
 *
 * The message bytes are copied into a FIFO queue owned by the port's
 * registry entry; EVFILT_USER is only used to wake up a blocked
 * mach_msg_receive. Multiple sends queue rather than overwrite each
 * other, unlike a naive single-kevent-udata approach.
 */
kern_return_t mach_msg_send(mach_port_t port, void *msg, size_t len) {
  port_entry_t *entry;
  msg_node_t *node;
  struct kevent kev;

  if (port == MACH_PORT_NULL || port == MACH_PORT_DEAD) {
    return KERN_INVALID_NAME;
  }

  if (msg == NULL || len == 0) {
    return KERN_INVALID_ARGUMENT;
  }

  entry = port_entry_get(port, 1);
  if (entry == NULL) {
    return KERN_NO_SPACE;
  }

  node = malloc(sizeof(*node) + len);
  if (node == NULL) {
    return KERN_NO_SPACE;
  }
  node->next = NULL;
  node->len = len;
  memcpy(node->data, msg, len);

  pthread_mutex_lock(&entry->lock);
  if (entry->msg_tail != NULL) {
    entry->msg_tail->next = node;
  } else {
    entry->msg_head = node;
  }
  entry->msg_tail = node;
  pthread_mutex_unlock(&entry->lock);

  /* Wake anyone blocked in mach_msg_receive on this port. */
  EV_SET(&kev, 1, EVFILT_USER, EV_ADD | EV_ENABLE, NOTE_TRIGGER, 0, NULL);
  if (kevent(port, &kev, 1, NULL, 0, NULL) < 0) {
    return KERN_FAILURE;
  }

  return KERN_SUCCESS;
}

/*
 * Receive a message from a Mach port
 *
 * Drains the port's message queue; if it's empty, blocks on the port's
 * kqueue (honoring timeout_ms as a real deadline recomputed across
 * spurious/racing wakeups) until a sender signals arrival, then retries.
 */
kern_return_t mach_msg_receive(mach_port_t port, void *msg, size_t len,
                               int timeout_ms) {
  port_entry_t *entry;
  msg_node_t *node;
  struct timespec deadline;
  int has_deadline = 0;

  if (port == MACH_PORT_NULL || port == MACH_PORT_DEAD) {
    return KERN_INVALID_NAME;
  }

  if (msg == NULL || len == 0) {
    return KERN_INVALID_ARGUMENT;
  }

  entry = port_entry_get(port, 0);
  if (entry == NULL) {
    return KERN_INVALID_NAME;
  }

  if (timeout_ms >= 0) {
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
      deadline.tv_sec += 1;
      deadline.tv_nsec -= 1000000000L;
    }
    has_deadline = 1;
  }

  for (;;) {
    node = port_entry_dequeue(entry);
    if (node != NULL) {
      size_t copy_len = node->len < len ? node->len : len;
      memcpy(msg, node->data, copy_len);
      free(node);
      return KERN_SUCCESS;
    }

    struct timespec remaining;
    struct timespec *remaining_ptr = NULL;

    if (has_deadline) {
      struct timespec now;

      clock_gettime(CLOCK_MONOTONIC, &now);
      remaining.tv_sec = deadline.tv_sec - now.tv_sec;
      remaining.tv_nsec = deadline.tv_nsec - now.tv_nsec;
      if (remaining.tv_nsec < 0) {
        remaining.tv_sec -= 1;
        remaining.tv_nsec += 1000000000L;
      }
      if (remaining.tv_sec < 0 ||
          (remaining.tv_sec == 0 && remaining.tv_nsec <= 0)) {
        return KERN_TIMED_OUT;
      }
      remaining_ptr = &remaining;
    }

    struct kevent kev;
    int ret = kevent(port, NULL, 0, &kev, 1, remaining_ptr);

    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      return KERN_FAILURE;
    }
    if (ret == 0) {
      return KERN_TIMED_OUT;
    }
    /* Woke up -- loop back and drain the queue. */
  }
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
  case KERN_TIMED_OUT:
    return "Operation timed out";
  default:
    return "Unknown error";
  }
}
