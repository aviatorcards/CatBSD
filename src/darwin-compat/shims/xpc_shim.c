/*
 * CatBSD XPC Compatibility Shim Implementation
 * Darwin XPC semantics over AF_UNIX SOCK_STREAM sockets
 */

#include "xpc_shim.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

/*
 * Frame layout (all integers little-endian, written byte-at-a-time so the
 * wire format doesn't depend on host endianness or struct padding):
 *
 *   offset  size  field
 *   0       4     magic "CXP1"
 *   4       4     payload length
 *   8       8     correlation id
 *   16      4     flags
 *   20      4     entry count
 *   24      ...   payload: repeated entries
 *
 * Each payload entry is: u16 key length, key bytes (no NUL), u8 type,
 * then the value -- 1 byte for BOOL, 8 for INT64/UINT64/DOUBLE, and a u32
 * length plus raw bytes for STRING/DATA.
 */
#define XPC_HEADER_SIZE 24
#define XPC_MAGIC0 'C'
#define XPC_MAGIC1 'X'
#define XPC_MAGIC2 'P'
#define XPC_MAGIC3 '1'
#define XPC_FLAG_REPLY 0x1u

#ifdef MSG_NOSIGNAL
#define XPC_SEND_FLAGS MSG_NOSIGNAL
#else
#define XPC_SEND_FLAGS 0
#endif

/* ------------------------------------------------------------------ */
/* Little-endian encode/decode helpers                                 */
/* ------------------------------------------------------------------ */

static void put_u16(unsigned char *p, uint16_t v) {
  p[0] = (unsigned char)(v & 0xFF);
  p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void put_u32(unsigned char *p, uint32_t v) {
  p[0] = (unsigned char)(v & 0xFF);
  p[1] = (unsigned char)((v >> 8) & 0xFF);
  p[2] = (unsigned char)((v >> 16) & 0xFF);
  p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static void put_u64(unsigned char *p, uint64_t v) {
  for (int i = 0; i < 8; i++) {
    p[i] = (unsigned char)((v >> (8 * i)) & 0xFF);
  }
}

static uint16_t get_u16(const unsigned char *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const unsigned char *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static uint64_t get_u64(const unsigned char *p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) {
    v |= (uint64_t)p[i] << (8 * i);
  }
  return v;
}

/* ------------------------------------------------------------------ */
/* Deadlines                                                           */
/* ------------------------------------------------------------------ */

/*
 * A deadline is computed once and then converted to a fresh poll timeout
 * on every iteration, so a stream of partial reads or EINTRs can't extend
 * the caller's timeout indefinitely the way a per-call timeout would.
 */
typedef struct {
  struct timespec at;
  int active;
} xpc_deadline_t;

static void deadline_set(xpc_deadline_t *dl, int timeout_ms) {
  if (timeout_ms < 0) {
    dl->active = 0;
    return;
  }
  clock_gettime(CLOCK_MONOTONIC, &dl->at);
  dl->at.tv_sec += timeout_ms / 1000;
  dl->at.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
  if (dl->at.tv_nsec >= 1000000000L) {
    dl->at.tv_sec += 1;
    dl->at.tv_nsec -= 1000000000L;
  }
  dl->active = 1;
}

/* -1 = wait forever, -2 = already expired, otherwise milliseconds left. */
static int deadline_remaining_ms(const xpc_deadline_t *dl) {
  struct timespec now;
  long long ms;

  if (!dl->active) {
    return -1;
  }

  clock_gettime(CLOCK_MONOTONIC, &now);
  ms = (long long)(dl->at.tv_sec - now.tv_sec) * 1000LL +
       (long long)(dl->at.tv_nsec - now.tv_nsec) / 1000000LL;
  if (ms <= 0) {
    return -2;
  }
  if (ms > 2147483647LL) {
    ms = 2147483647LL;
  }
  return (int)ms;
}

/* ------------------------------------------------------------------ */
/* Message dictionaries                                                */
/* ------------------------------------------------------------------ */

typedef struct xpc_entry {
  struct xpc_entry *next;
  char *key;
  int type;
  union {
    int b;
    int64_t i;
    uint64_t u;
    double d;
  } scalar;
  unsigned char *bytes; /* STRING (NUL-terminated) or DATA */
  size_t len;           /* payload length, excluding a string's NUL */
} xpc_entry_t;

struct catbsd_xpc_object {
  pthread_mutex_t lock;
  int refcount;
  xpc_entry_t *head;
  xpc_entry_t *tail;
  size_t count;
  uint64_t msg_id;
  int is_reply;
};

static void entry_free(xpc_entry_t *e) {
  free(e->key);
  free(e->bytes);
  free(e);
}

catbsd_xpc_object_t catbsd_xpc_dictionary_create(void) {
  catbsd_xpc_object_t d = calloc(1, sizeof(*d));
  if (d == NULL) {
    return NULL;
  }
  pthread_mutex_init(&d->lock, NULL);
  d->refcount = 1;
  return d;
}

catbsd_xpc_object_t catbsd_xpc_retain(catbsd_xpc_object_t obj) {
  if (obj == NULL) {
    return NULL;
  }
  pthread_mutex_lock(&obj->lock);
  obj->refcount++;
  pthread_mutex_unlock(&obj->lock);
  return obj;
}

void catbsd_xpc_release(catbsd_xpc_object_t obj) {
  xpc_entry_t *e, *next;
  int dead;

  if (obj == NULL) {
    return;
  }

  pthread_mutex_lock(&obj->lock);
  dead = (--obj->refcount <= 0);
  pthread_mutex_unlock(&obj->lock);

  if (!dead) {
    return;
  }

  for (e = obj->head; e != NULL; e = next) {
    next = e->next;
    entry_free(e);
  }
  pthread_mutex_destroy(&obj->lock);
  free(obj);
}

/* Caller must hold obj->lock. */
static xpc_entry_t *entry_find(catbsd_xpc_object_t d, const char *key) {
  xpc_entry_t *e;
  for (e = d->head; e != NULL; e = e->next) {
    if (strcmp(e->key, key) == 0) {
      return e;
    }
  }
  return NULL;
}

/*
 * Get the entry for `key`, creating it if needed, and reset it to an
 * empty slot of `type`. Existing bytes are freed here so every setter
 * gets replace-in-place semantics without repeating the cleanup.
 * Caller must hold obj->lock.
 */
static xpc_entry_t *entry_reset(catbsd_xpc_object_t d, const char *key,
                                int type) {
  xpc_entry_t *e = entry_find(d, key);

  if (e == NULL) {
    e = calloc(1, sizeof(*e));
    if (e == NULL) {
      return NULL;
    }
    e->key = strdup(key);
    if (e->key == NULL) {
      free(e);
      return NULL;
    }
    if (d->tail != NULL) {
      d->tail->next = e;
    } else {
      d->head = e;
    }
    d->tail = e;
    d->count++;
  } else {
    free(e->bytes);
    e->bytes = NULL;
    e->len = 0;
  }

  e->type = type;
  memset(&e->scalar, 0, sizeof(e->scalar));
  return e;
}

#define XPC_SET_PROLOGUE(d, key)                                               \
  if ((d) == NULL || (key) == NULL || (key)[0] == '\0' ||                      \
      strlen(key) > 65535) {                                                   \
    return CATBSD_XPC_EINVAL;                                                  \
  }

int catbsd_xpc_dictionary_set_bool(catbsd_xpc_object_t d, const char *key,
                                   int value) {
  xpc_entry_t *e;
  XPC_SET_PROLOGUE(d, key)

  pthread_mutex_lock(&d->lock);
  e = entry_reset(d, key, CATBSD_XPC_TYPE_BOOL);
  if (e != NULL) {
    e->scalar.b = value ? 1 : 0;
  }
  pthread_mutex_unlock(&d->lock);
  return e ? CATBSD_XPC_SUCCESS : CATBSD_XPC_ENOMEM;
}

int catbsd_xpc_dictionary_set_int64(catbsd_xpc_object_t d, const char *key,
                                    int64_t value) {
  xpc_entry_t *e;
  XPC_SET_PROLOGUE(d, key)

  pthread_mutex_lock(&d->lock);
  e = entry_reset(d, key, CATBSD_XPC_TYPE_INT64);
  if (e != NULL) {
    e->scalar.i = value;
  }
  pthread_mutex_unlock(&d->lock);
  return e ? CATBSD_XPC_SUCCESS : CATBSD_XPC_ENOMEM;
}

int catbsd_xpc_dictionary_set_uint64(catbsd_xpc_object_t d, const char *key,
                                     uint64_t value) {
  xpc_entry_t *e;
  XPC_SET_PROLOGUE(d, key)

  pthread_mutex_lock(&d->lock);
  e = entry_reset(d, key, CATBSD_XPC_TYPE_UINT64);
  if (e != NULL) {
    e->scalar.u = value;
  }
  pthread_mutex_unlock(&d->lock);
  return e ? CATBSD_XPC_SUCCESS : CATBSD_XPC_ENOMEM;
}

int catbsd_xpc_dictionary_set_double(catbsd_xpc_object_t d, const char *key,
                                     double value) {
  xpc_entry_t *e;
  XPC_SET_PROLOGUE(d, key)

  pthread_mutex_lock(&d->lock);
  e = entry_reset(d, key, CATBSD_XPC_TYPE_DOUBLE);
  if (e != NULL) {
    e->scalar.d = value;
  }
  pthread_mutex_unlock(&d->lock);
  return e ? CATBSD_XPC_SUCCESS : CATBSD_XPC_ENOMEM;
}

/* Shared body for the two byte-carrying types. */
static int dictionary_set_bytes(catbsd_xpc_object_t d, const char *key,
                                int type, const void *bytes, size_t len) {
  xpc_entry_t *e;
  unsigned char *copy;

  if (len > CATBSD_XPC_MAX_MESSAGE) {
    return CATBSD_XPC_EINVAL;
  }

  copy = malloc(len + 1);
  if (copy == NULL) {
    return CATBSD_XPC_ENOMEM;
  }
  if (len > 0) {
    memcpy(copy, bytes, len);
  }
  copy[len] = '\0'; /* keeps STRING values directly usable as C strings */

  pthread_mutex_lock(&d->lock);
  e = entry_reset(d, key, type);
  if (e == NULL) {
    pthread_mutex_unlock(&d->lock);
    free(copy);
    return CATBSD_XPC_ENOMEM;
  }
  e->bytes = copy;
  e->len = len;
  pthread_mutex_unlock(&d->lock);
  return CATBSD_XPC_SUCCESS;
}

int catbsd_xpc_dictionary_set_string(catbsd_xpc_object_t d, const char *key,
                                     const char *value) {
  XPC_SET_PROLOGUE(d, key)
  if (value == NULL) {
    return CATBSD_XPC_EINVAL;
  }
  return dictionary_set_bytes(d, key, CATBSD_XPC_TYPE_STRING, value,
                              strlen(value));
}

int catbsd_xpc_dictionary_set_data(catbsd_xpc_object_t d, const char *key,
                                   const void *bytes, size_t len) {
  XPC_SET_PROLOGUE(d, key)
  if (bytes == NULL && len > 0) {
    return CATBSD_XPC_EINVAL;
  }
  return dictionary_set_bytes(d, key, CATBSD_XPC_TYPE_DATA, bytes, len);
}

/* Scalar getters share a shape: exact type match or CATBSD_XPC_EINVAL. */
#define XPC_GET_SCALAR(d, key, out, want_type, field)                          \
  do {                                                                         \
    xpc_entry_t *e;                                                            \
    int rc = CATBSD_XPC_EINVAL;                                                \
    if ((d) == NULL || (key) == NULL || (out) == NULL) {                       \
      return CATBSD_XPC_EINVAL;                                                \
    }                                                                          \
    pthread_mutex_lock(&(d)->lock);                                            \
    e = entry_find((d), (key));                                                \
    if (e != NULL && e->type == (want_type)) {                                 \
      *(out) = e->scalar.field;                                                \
      rc = CATBSD_XPC_SUCCESS;                                                 \
    }                                                                          \
    pthread_mutex_unlock(&(d)->lock);                                          \
    return rc;                                                                 \
  } while (0)

int catbsd_xpc_dictionary_get_bool(catbsd_xpc_object_t d, const char *key,
                                   int *out) {
  XPC_GET_SCALAR(d, key, out, CATBSD_XPC_TYPE_BOOL, b);
}

int catbsd_xpc_dictionary_get_int64(catbsd_xpc_object_t d, const char *key,
                                    int64_t *out) {
  XPC_GET_SCALAR(d, key, out, CATBSD_XPC_TYPE_INT64, i);
}

int catbsd_xpc_dictionary_get_uint64(catbsd_xpc_object_t d, const char *key,
                                     uint64_t *out) {
  XPC_GET_SCALAR(d, key, out, CATBSD_XPC_TYPE_UINT64, u);
}

int catbsd_xpc_dictionary_get_double(catbsd_xpc_object_t d, const char *key,
                                     double *out) {
  XPC_GET_SCALAR(d, key, out, CATBSD_XPC_TYPE_DOUBLE, d);
}

const char *catbsd_xpc_dictionary_get_string(catbsd_xpc_object_t d,
                                             const char *key) {
  xpc_entry_t *e;
  const char *out = NULL;

  if (d == NULL || key == NULL) {
    return NULL;
  }

  pthread_mutex_lock(&d->lock);
  e = entry_find(d, key);
  if (e != NULL && e->type == CATBSD_XPC_TYPE_STRING) {
    out = (const char *)e->bytes;
  }
  pthread_mutex_unlock(&d->lock);
  return out;
}

const void *catbsd_xpc_dictionary_get_data(catbsd_xpc_object_t d,
                                           const char *key, size_t *len_out) {
  xpc_entry_t *e;
  const void *out = NULL;

  if (d == NULL || key == NULL) {
    return NULL;
  }

  pthread_mutex_lock(&d->lock);
  e = entry_find(d, key);
  if (e != NULL && e->type == CATBSD_XPC_TYPE_DATA) {
    out = e->bytes;
    if (len_out != NULL) {
      *len_out = e->len;
    }
  }
  pthread_mutex_unlock(&d->lock);
  return out;
}

int catbsd_xpc_dictionary_get_type(catbsd_xpc_object_t d, const char *key) {
  xpc_entry_t *e;
  int type = CATBSD_XPC_TYPE_INVALID;

  if (d == NULL || key == NULL) {
    return CATBSD_XPC_TYPE_INVALID;
  }

  pthread_mutex_lock(&d->lock);
  e = entry_find(d, key);
  if (e != NULL) {
    type = e->type;
  }
  pthread_mutex_unlock(&d->lock);
  return type;
}

size_t catbsd_xpc_dictionary_count(catbsd_xpc_object_t d) {
  size_t n;

  if (d == NULL) {
    return 0;
  }
  pthread_mutex_lock(&d->lock);
  n = d->count;
  pthread_mutex_unlock(&d->lock);
  return n;
}

int catbsd_xpc_dictionary_remove(catbsd_xpc_object_t d, const char *key) {
  xpc_entry_t *e, **prev;
  int rc = CATBSD_XPC_EINVAL;

  if (d == NULL || key == NULL) {
    return CATBSD_XPC_EINVAL;
  }

  pthread_mutex_lock(&d->lock);
  prev = &d->head;
  e = d->head;
  while (e != NULL && strcmp(e->key, key) != 0) {
    prev = &e->next;
    e = e->next;
  }
  if (e != NULL) {
    *prev = e->next;
    if (d->tail == e) {
      /* Removed the tail: walk forward to find the new last entry. */
      xpc_entry_t *scan;
      d->tail = NULL;
      for (scan = d->head; scan != NULL; scan = scan->next) {
        d->tail = scan;
      }
    }
    d->count--;
    entry_free(e);
    rc = CATBSD_XPC_SUCCESS;
  }
  pthread_mutex_unlock(&d->lock);
  return rc;
}

/*
 * Keys and types are snapshotted before `fn` runs so the callback may
 * safely touch the same dictionary; holding the lock across a caller
 * callback would deadlock the first time someone read a value from
 * inside their own iterator.
 */
int catbsd_xpc_dictionary_apply(catbsd_xpc_object_t d,
                                int (*fn)(const char *key, int type,
                                          void *context),
                                void *context) {
  xpc_entry_t *e;
  char **keys;
  int *types;
  size_t n, i;
  int rc = 0;

  if (d == NULL || fn == NULL) {
    return CATBSD_XPC_EINVAL;
  }

  pthread_mutex_lock(&d->lock);
  n = d->count;
  keys = (n > 0) ? calloc(n, sizeof(*keys)) : NULL;
  types = (n > 0) ? calloc(n, sizeof(*types)) : NULL;
  if (n > 0 && (keys == NULL || types == NULL)) {
    pthread_mutex_unlock(&d->lock);
    free(keys);
    free(types);
    return CATBSD_XPC_ENOMEM;
  }
  i = 0;
  for (e = d->head; e != NULL && i < n; e = e->next, i++) {
    keys[i] = strdup(e->key);
    types[i] = e->type;
  }
  pthread_mutex_unlock(&d->lock);

  for (i = 0; i < n; i++) {
    if (rc == 0 && keys[i] != NULL) {
      rc = fn(keys[i], types[i], context);
    }
    free(keys[i]);
  }
  free(keys);
  free(types);
  return rc;
}

uint64_t catbsd_xpc_message_get_id(catbsd_xpc_object_t msg) {
  uint64_t id;
  if (msg == NULL) {
    return 0;
  }
  pthread_mutex_lock(&msg->lock);
  id = msg->msg_id;
  pthread_mutex_unlock(&msg->lock);
  return id;
}

int catbsd_xpc_message_is_reply(catbsd_xpc_object_t msg) {
  int r;
  if (msg == NULL) {
    return 0;
  }
  pthread_mutex_lock(&msg->lock);
  r = msg->is_reply;
  pthread_mutex_unlock(&msg->lock);
  return r;
}

char *catbsd_xpc_copy_description(catbsd_xpc_object_t d) {
  xpc_entry_t *e;
  size_t cap = 256, used = 0;
  char *out;

  if (d == NULL) {
    return strdup("<null>");
  }

  out = malloc(cap);
  if (out == NULL) {
    return NULL;
  }
  used = (size_t)snprintf(out, cap, "<xpc dictionary: %zu entries>\n", d->count);

  pthread_mutex_lock(&d->lock);
  for (e = d->head; e != NULL; e = e->next) {
    char line[512];
    int n = 0;

    switch (e->type) {
    case CATBSD_XPC_TYPE_BOOL:
      n = snprintf(line, sizeof(line), "  %s => bool %s\n", e->key,
                   e->scalar.b ? "true" : "false");
      break;
    case CATBSD_XPC_TYPE_INT64:
      n = snprintf(line, sizeof(line), "  %s => int64 %lld\n", e->key,
                   (long long)e->scalar.i);
      break;
    case CATBSD_XPC_TYPE_UINT64:
      n = snprintf(line, sizeof(line), "  %s => uint64 %llu\n", e->key,
                   (unsigned long long)e->scalar.u);
      break;
    case CATBSD_XPC_TYPE_DOUBLE:
      n = snprintf(line, sizeof(line), "  %s => double %g\n", e->key,
                   e->scalar.d);
      break;
    case CATBSD_XPC_TYPE_STRING:
      n = snprintf(line, sizeof(line), "  %s => string \"%.200s\"\n", e->key,
                   (const char *)e->bytes);
      break;
    case CATBSD_XPC_TYPE_DATA:
      n = snprintf(line, sizeof(line), "  %s => data (%zu bytes)\n", e->key,
                   e->len);
      break;
    default:
      n = snprintf(line, sizeof(line), "  %s => <invalid>\n", e->key);
      break;
    }

    if (n < 0) {
      continue;
    }
    if (used + (size_t)n + 1 > cap) {
      char *bigger;
      while (used + (size_t)n + 1 > cap) {
        cap *= 2;
      }
      bigger = realloc(out, cap);
      if (bigger == NULL) {
        pthread_mutex_unlock(&d->lock);
        return out;
      }
      out = bigger;
    }
    memcpy(out + used, line, (size_t)n + 1);
    used += (size_t)n;
  }
  pthread_mutex_unlock(&d->lock);

  return out;
}

/* ------------------------------------------------------------------ */
/* Serialization                                                       */
/* ------------------------------------------------------------------ */

/* Caller must hold d->lock. */
static size_t payload_size_locked(catbsd_xpc_object_t d) {
  xpc_entry_t *e;
  size_t total = 0;

  for (e = d->head; e != NULL; e = e->next) {
    total += 2 + strlen(e->key) + 1; /* key len + key + type tag */
    switch (e->type) {
    case CATBSD_XPC_TYPE_BOOL:
      total += 1;
      break;
    case CATBSD_XPC_TYPE_INT64:
    case CATBSD_XPC_TYPE_UINT64:
    case CATBSD_XPC_TYPE_DOUBLE:
      total += 8;
      break;
    case CATBSD_XPC_TYPE_STRING:
    case CATBSD_XPC_TYPE_DATA:
      total += 4 + e->len;
      break;
    default:
      break;
    }
  }
  return total;
}

int catbsd_xpc_serialize(catbsd_xpc_object_t d, void **buf_out,
                         size_t *len_out) {
  xpc_entry_t *e;
  unsigned char *buf, *p;
  size_t payload, total;

  if (d == NULL || buf_out == NULL || len_out == NULL) {
    return CATBSD_XPC_EINVAL;
  }

  pthread_mutex_lock(&d->lock);

  payload = payload_size_locked(d);
  total = XPC_HEADER_SIZE + payload;
  if (total > CATBSD_XPC_MAX_MESSAGE) {
    pthread_mutex_unlock(&d->lock);
    return CATBSD_XPC_EPROTO;
  }

  buf = malloc(total);
  if (buf == NULL) {
    pthread_mutex_unlock(&d->lock);
    return CATBSD_XPC_ENOMEM;
  }

  buf[0] = XPC_MAGIC0;
  buf[1] = XPC_MAGIC1;
  buf[2] = XPC_MAGIC2;
  buf[3] = XPC_MAGIC3;
  put_u32(buf + 4, (uint32_t)payload);
  put_u64(buf + 8, d->msg_id);
  put_u32(buf + 16, d->is_reply ? XPC_FLAG_REPLY : 0u);
  put_u32(buf + 20, (uint32_t)d->count);

  p = buf + XPC_HEADER_SIZE;
  for (e = d->head; e != NULL; e = e->next) {
    size_t klen = strlen(e->key);

    put_u16(p, (uint16_t)klen);
    p += 2;
    memcpy(p, e->key, klen);
    p += klen;
    *p++ = (unsigned char)e->type;

    switch (e->type) {
    case CATBSD_XPC_TYPE_BOOL:
      *p++ = (unsigned char)(e->scalar.b ? 1 : 0);
      break;
    case CATBSD_XPC_TYPE_INT64:
      put_u64(p, (uint64_t)e->scalar.i);
      p += 8;
      break;
    case CATBSD_XPC_TYPE_UINT64:
      put_u64(p, e->scalar.u);
      p += 8;
      break;
    case CATBSD_XPC_TYPE_DOUBLE: {
      /* Bit-copy rather than a cast: the value must survive the round
       * trip exactly, including NaN payloads and negative zero. */
      uint64_t bits;
      memcpy(&bits, &e->scalar.d, sizeof(bits));
      put_u64(p, bits);
      p += 8;
      break;
    }
    case CATBSD_XPC_TYPE_STRING:
    case CATBSD_XPC_TYPE_DATA:
      put_u32(p, (uint32_t)e->len);
      p += 4;
      if (e->len > 0) {
        memcpy(p, e->bytes, e->len);
        p += e->len;
      }
      break;
    default:
      break;
    }
  }

  pthread_mutex_unlock(&d->lock);

  *buf_out = buf;
  *len_out = total;
  return CATBSD_XPC_SUCCESS;
}

/*
 * Every field is bounds-checked against the declared payload length
 * before it is read: this parses data straight off a socket, so a
 * truncated or hostile frame must fail rather than walk off the buffer.
 */
catbsd_xpc_object_t catbsd_xpc_deserialize(const void *buf, size_t len) {
  const unsigned char *p = buf;
  catbsd_xpc_object_t d;
  uint32_t payload, count, i;
  size_t off;

  if (buf == NULL || len < XPC_HEADER_SIZE) {
    return NULL;
  }
  if (p[0] != XPC_MAGIC0 || p[1] != XPC_MAGIC1 || p[2] != XPC_MAGIC2 ||
      p[3] != XPC_MAGIC3) {
    return NULL;
  }

  payload = get_u32(p + 4);
  if ((size_t)payload + XPC_HEADER_SIZE > len) {
    return NULL;
  }
  count = get_u32(p + 20);

  d = catbsd_xpc_dictionary_create();
  if (d == NULL) {
    return NULL;
  }
  d->msg_id = get_u64(p + 8);
  d->is_reply = (get_u32(p + 16) & XPC_FLAG_REPLY) ? 1 : 0;

  p += XPC_HEADER_SIZE;
  off = 0;

  for (i = 0; i < count; i++) {
    uint16_t klen;
    unsigned char type;
    char key[65536];
    int rc = CATBSD_XPC_SUCCESS;

    if (off + 2 > payload) {
      goto bad;
    }
    klen = get_u16(p + off);
    off += 2;
    if (off + klen + 1 > payload || klen == 0) {
      goto bad;
    }
    memcpy(key, p + off, klen);
    key[klen] = '\0';
    off += klen;
    type = p[off++];

    switch (type) {
    case CATBSD_XPC_TYPE_BOOL:
      if (off + 1 > payload) {
        goto bad;
      }
      rc = catbsd_xpc_dictionary_set_bool(d, key, p[off]);
      off += 1;
      break;
    case CATBSD_XPC_TYPE_INT64:
      if (off + 8 > payload) {
        goto bad;
      }
      rc = catbsd_xpc_dictionary_set_int64(d, key, (int64_t)get_u64(p + off));
      off += 8;
      break;
    case CATBSD_XPC_TYPE_UINT64:
      if (off + 8 > payload) {
        goto bad;
      }
      rc = catbsd_xpc_dictionary_set_uint64(d, key, get_u64(p + off));
      off += 8;
      break;
    case CATBSD_XPC_TYPE_DOUBLE: {
      uint64_t bits;
      double value;
      if (off + 8 > payload) {
        goto bad;
      }
      bits = get_u64(p + off);
      memcpy(&value, &bits, sizeof(value));
      rc = catbsd_xpc_dictionary_set_double(d, key, value);
      off += 8;
      break;
    }
    case CATBSD_XPC_TYPE_STRING:
    case CATBSD_XPC_TYPE_DATA: {
      uint32_t vlen;
      if (off + 4 > payload) {
        goto bad;
      }
      vlen = get_u32(p + off);
      off += 4;
      if ((size_t)vlen > payload - off) {
        goto bad;
      }
      rc = dictionary_set_bytes(d, key,
                                type == CATBSD_XPC_TYPE_STRING
                                    ? CATBSD_XPC_TYPE_STRING
                                    : CATBSD_XPC_TYPE_DATA,
                                p + off, vlen);
      off += vlen;
      break;
    }
    default:
      goto bad;
    }

    if (rc != CATBSD_XPC_SUCCESS) {
      goto bad;
    }
  }

  return d;

bad:
  catbsd_xpc_release(d);
  return NULL;
}

/* ------------------------------------------------------------------ */
/* Connections                                                         */
/* ------------------------------------------------------------------ */

typedef struct pending_msg {
  struct pending_msg *next;
  catbsd_xpc_object_t obj;
} pending_msg_t;

struct catbsd_xpc_connection {
  int fd;
  int is_listener;
  char *name;
  char *path;      /* non-NULL only for a listener, which unlinks it */
  char *lock_path; /* listener's ownership lock, unlinked with the socket */
  int lock_fd;     /* -1 unless this is a listener */
  uint64_t next_id;
  int last_error;
  int cancelled;
  int resumed;

  pthread_mutex_t lock;      /* connection state + pending queue */
  pthread_mutex_t read_lock; /* serializes socket reads */
  pending_msg_t *q_head;
  pending_msg_t *q_tail;

  pthread_t reader;
  catbsd_xpc_event_handler_t handler;
  void *handler_ctx;
};

const char *catbsd_xpc_strerror(int err) {
  switch (err) {
  case CATBSD_XPC_SUCCESS:
    return "Success";
  case CATBSD_XPC_EINVAL:
    return "Invalid argument";
  case CATBSD_XPC_ENOMEM:
    return "Out of memory";
  case CATBSD_XPC_EIO:
    return "I/O error";
  case CATBSD_XPC_ETIMEOUT:
    return "Operation timed out";
  case CATBSD_XPC_ECLOSED:
    return "Connection closed";
  case CATBSD_XPC_EPROTO:
    return "Protocol error";
  case CATBSD_XPC_EEXIST:
    return "Service name already in use";
  case CATBSD_XPC_ENOENT:
    return "No such service";
  default:
    return "Unknown error";
  }
}

/*
 * Resolve a service name to a socket path. Bare names live under a
 * per-uid runtime directory so two users on one machine can each publish
 * "com.catbsd.jobd" without colliding; an absolute name is taken as a
 * literal path for callers that want to place the socket themselves.
 */
static int resolve_path(const char *name, char *out, size_t outlen) {
  const char *dir;
  char dirbuf[256];
  int n;

  if (name == NULL || name[0] == '\0') {
    return CATBSD_XPC_EINVAL;
  }

  if (name[0] == '/') {
    if (strlen(name) >= outlen) {
      return CATBSD_XPC_EINVAL;
    }
    snprintf(out, outlen, "%s", name);
    return CATBSD_XPC_SUCCESS;
  }

  /* A bare name is a single path component -- reject anything that would
   * escape the runtime directory. */
  if (strchr(name, '/') != NULL || strcmp(name, "..") == 0) {
    return CATBSD_XPC_EINVAL;
  }

  dir = getenv("CATBSD_XPC_RUNTIME_DIR");
  if (dir == NULL || dir[0] == '\0') {
    snprintf(dirbuf, sizeof(dirbuf), "/tmp/catbsd-xpc-%lu",
             (unsigned long)getuid());
    dir = dirbuf;
  }

  if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
    return CATBSD_XPC_EIO;
  }

  n = snprintf(out, outlen, "%s/%s", dir, name);
  if (n < 0 || (size_t)n >= outlen) {
    return CATBSD_XPC_EINVAL;
  }
  return CATBSD_XPC_SUCCESS;
}

static catbsd_xpc_connection_t conn_alloc(int fd, const char *name) {
  catbsd_xpc_connection_t c = calloc(1, sizeof(*c));
  if (c == NULL) {
    return NULL;
  }
  c->fd = fd;
  c->lock_fd = -1;
  c->next_id = 1;
  c->name = (name != NULL) ? strdup(name) : NULL;
  pthread_mutex_init(&c->lock, NULL);
  pthread_mutex_init(&c->read_lock, NULL);

#ifdef SO_NOSIGPIPE
  /* macOS/FreeBSD: suppress SIGPIPE per-socket, since MSG_NOSIGNAL only
   * exists on some of the targets. */
  {
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
  }
#endif
  return c;
}

static void set_err(catbsd_xpc_connection_t c, int err) {
  pthread_mutex_lock(&c->lock);
  c->last_error = err;
  pthread_mutex_unlock(&c->lock);
}

catbsd_xpc_connection_t catbsd_xpc_connection_create_listener(const char *name,
                                                              int *err_out) {
  struct sockaddr_un addr;
  catbsd_xpc_connection_t c;
  char path[sizeof(addr.sun_path)];
  char lockpath[sizeof(addr.sun_path) + 8];
  int fd = -1, lock_fd = -1, rc;

  rc = resolve_path(name, path, sizeof(path));
  if (rc != CATBSD_XPC_SUCCESS) {
    goto fail_noclose;
  }

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    rc = CATBSD_XPC_EIO;
    goto fail_noclose;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

  /*
   * Claim the name with an advisory lock on a sidecar file before
   * touching the socket path.
   *
   * A bound AF_UNIX path outlives the process that created it, so the
   * path existing proves nothing about whether anyone is still serving
   * it -- a crashed listener leaves one behind forever. The obvious test
   * (connect and see if it succeeds) is worse than useless here: on a
   * SOCK_STREAM listener the probe connection is queued in the accept
   * backlog by the kernel, so the live owner's next accept() hands back a
   * dead peer instead of its real client. An flock the kernel drops when
   * the holder dies answers the same question with no side effects.
   */
  snprintf(lockpath, sizeof(lockpath), "%s.lock", path);
  lock_fd = open(lockpath, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
  if (lock_fd < 0) {
    rc = CATBSD_XPC_EIO;
    goto fail;
  }
  if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    rc = (errno == EWOULDBLOCK) ? CATBSD_XPC_EEXIST : CATBSD_XPC_EIO;
    goto fail;
  }

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    if (errno != EADDRINUSE) {
      rc = CATBSD_XPC_EIO;
      goto fail;
    }
    /* We hold the lock, so this socket file is a corpse. */
    unlink(path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
      rc = CATBSD_XPC_EIO;
      goto fail;
    }
  }

  if (listen(fd, 16) != 0) {
    rc = CATBSD_XPC_EIO;
    unlink(path);
    goto fail;
  }

  c = conn_alloc(fd, name);
  if (c == NULL) {
    rc = CATBSD_XPC_ENOMEM;
    unlink(path);
    goto fail;
  }
  c->is_listener = 1;
  c->path = strdup(path);
  c->lock_path = strdup(lockpath);
  c->lock_fd = lock_fd;

  if (err_out != NULL) {
    *err_out = CATBSD_XPC_SUCCESS;
  }
  return c;

fail:
  if (lock_fd >= 0) {
    close(lock_fd); /* also drops the flock */
  }
  close(fd);
fail_noclose:
  if (err_out != NULL) {
    *err_out = rc;
  }
  return NULL;
}

catbsd_xpc_connection_t catbsd_xpc_connection_create(const char *name,
                                                     int *err_out) {
  struct sockaddr_un addr;
  catbsd_xpc_connection_t c;
  char path[sizeof(addr.sun_path)];
  int fd, rc;

  rc = resolve_path(name, path, sizeof(path));
  if (rc != CATBSD_XPC_SUCCESS) {
    goto fail_noclose;
  }

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    rc = CATBSD_XPC_EIO;
    goto fail_noclose;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    rc = (errno == ENOENT || errno == ECONNREFUSED) ? CATBSD_XPC_ENOENT
                                                    : CATBSD_XPC_EIO;
    goto fail;
  }

  c = conn_alloc(fd, name);
  if (c == NULL) {
    rc = CATBSD_XPC_ENOMEM;
    goto fail;
  }

  if (err_out != NULL) {
    *err_out = CATBSD_XPC_SUCCESS;
  }
  return c;

fail:
  close(fd);
fail_noclose:
  if (err_out != NULL) {
    *err_out = rc;
  }
  return NULL;
}

catbsd_xpc_connection_t catbsd_xpc_connection_accept(
    catbsd_xpc_connection_t listener, int timeout_ms, int *err_out) {
  catbsd_xpc_connection_t c;
  xpc_deadline_t dl;
  int rc, peer;

  if (listener == NULL || !listener->is_listener) {
    if (err_out != NULL) {
      *err_out = CATBSD_XPC_EINVAL;
    }
    return NULL;
  }

  deadline_set(&dl, timeout_ms);

  for (;;) {
    struct pollfd pfd;
    int wait_ms = deadline_remaining_ms(&dl);
    int n;

    if (wait_ms == -2) {
      rc = CATBSD_XPC_ETIMEOUT;
      goto fail;
    }

    pfd.fd = listener->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    n = poll(&pfd, 1, wait_ms);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      rc = CATBSD_XPC_EIO;
      goto fail;
    }
    if (n == 0) {
      rc = CATBSD_XPC_ETIMEOUT;
      goto fail;
    }

    peer = accept(listener->fd, NULL, NULL);
    if (peer < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      rc = CATBSD_XPC_EIO;
      goto fail;
    }
    break;
  }

  c = conn_alloc(peer, listener->name);
  if (c == NULL) {
    close(peer);
    rc = CATBSD_XPC_ENOMEM;
    goto fail;
  }

  if (err_out != NULL) {
    *err_out = CATBSD_XPC_SUCCESS;
  }
  return c;

fail:
  if (err_out != NULL) {
    *err_out = rc;
  }
  return NULL;
}

static int io_write_full(int fd, const void *buf, size_t len) {
  const unsigned char *p = buf;
  size_t sent = 0;

  while (sent < len) {
    ssize_t n = send(fd, p + sent, len - sent, XPC_SEND_FLAGS);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EPIPE || errno == ECONNRESET) {
        return CATBSD_XPC_ECLOSED;
      }
      return CATBSD_XPC_EIO;
    }
    if (n == 0) {
      return CATBSD_XPC_ECLOSED;
    }
    sent += (size_t)n;
  }
  return CATBSD_XPC_SUCCESS;
}

static int io_read_full(int fd, void *buf, size_t len, xpc_deadline_t *dl) {
  unsigned char *p = buf;
  size_t got = 0;

  while (got < len) {
    struct pollfd pfd;
    int wait_ms = deadline_remaining_ms(dl);
    ssize_t n;
    int pr;

    if (wait_ms == -2) {
      return CATBSD_XPC_ETIMEOUT;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pr = poll(&pfd, 1, wait_ms);
    if (pr < 0) {
      if (errno == EINTR) {
        continue;
      }
      return CATBSD_XPC_EIO;
    }
    if (pr == 0) {
      return CATBSD_XPC_ETIMEOUT;
    }

    n = recv(fd, p + got, len - got, 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == ECONNRESET) {
        return CATBSD_XPC_ECLOSED;
      }
      return CATBSD_XPC_EIO;
    }
    if (n == 0) {
      /* Orderly shutdown. Mid-frame this is a truncated message, but
       * either way the connection is finished. */
      return CATBSD_XPC_ECLOSED;
    }
    got += (size_t)n;
  }
  return CATBSD_XPC_SUCCESS;
}

/* Read exactly one frame off the wire and turn it into a dictionary. */
static int frame_read(catbsd_xpc_connection_t c, catbsd_xpc_object_t *out,
                      xpc_deadline_t *dl) {
  unsigned char header[XPC_HEADER_SIZE];
  unsigned char *buf;
  uint32_t payload;
  catbsd_xpc_object_t obj;
  int rc;

  rc = io_read_full(c->fd, header, sizeof(header), dl);
  if (rc != CATBSD_XPC_SUCCESS) {
    return rc;
  }

  if (header[0] != XPC_MAGIC0 || header[1] != XPC_MAGIC1 ||
      header[2] != XPC_MAGIC2 || header[3] != XPC_MAGIC3) {
    return CATBSD_XPC_EPROTO;
  }

  payload = get_u32(header + 4);
  if ((size_t)payload + XPC_HEADER_SIZE > CATBSD_XPC_MAX_MESSAGE) {
    return CATBSD_XPC_EPROTO;
  }

  buf = malloc(XPC_HEADER_SIZE + payload);
  if (buf == NULL) {
    return CATBSD_XPC_ENOMEM;
  }
  memcpy(buf, header, XPC_HEADER_SIZE);

  if (payload > 0) {
    rc = io_read_full(c->fd, buf + XPC_HEADER_SIZE, payload, dl);
    if (rc != CATBSD_XPC_SUCCESS) {
      free(buf);
      return rc;
    }
  }

  obj = catbsd_xpc_deserialize(buf, XPC_HEADER_SIZE + payload);
  free(buf);
  if (obj == NULL) {
    return CATBSD_XPC_EPROTO;
  }

  *out = obj;
  return CATBSD_XPC_SUCCESS;
}

/* Caller must hold c->lock. */
static void queue_push_locked(catbsd_xpc_connection_t c,
                              catbsd_xpc_object_t obj) {
  pending_msg_t *n = malloc(sizeof(*n));
  if (n == NULL) {
    catbsd_xpc_release(obj);
    return;
  }
  n->next = NULL;
  n->obj = obj;
  if (c->q_tail != NULL) {
    c->q_tail->next = n;
  } else {
    c->q_head = n;
  }
  c->q_tail = n;
}

/* Caller must hold c->lock. Returns NULL when the queue is empty. */
static catbsd_xpc_object_t queue_pop_locked(catbsd_xpc_connection_t c) {
  pending_msg_t *n = c->q_head;
  catbsd_xpc_object_t obj;

  if (n == NULL) {
    return NULL;
  }
  c->q_head = n->next;
  if (c->q_head == NULL) {
    c->q_tail = NULL;
  }
  obj = n->obj;
  free(n);
  return obj;
}

/* Caller must hold c->lock. Pull out a queued reply with this id, if any. */
static catbsd_xpc_object_t queue_take_reply_locked(catbsd_xpc_connection_t c,
                                                   uint64_t id) {
  pending_msg_t *n = c->q_head, **prev = &c->q_head;
  pending_msg_t *last = NULL;

  while (n != NULL) {
    if (n->obj->is_reply && n->obj->msg_id == id) {
      catbsd_xpc_object_t obj = n->obj;
      *prev = n->next;
      if (c->q_tail == n) {
        c->q_tail = last;
      }
      free(n);
      return obj;
    }
    last = n;
    prev = &n->next;
    n = n->next;
  }
  return NULL;
}

static int send_locked(catbsd_xpc_connection_t c, catbsd_xpc_object_t msg,
                       uint64_t id, int is_reply, uint64_t *id_out) {
  void *buf;
  size_t len;
  int rc;

  pthread_mutex_lock(&msg->lock);
  msg->msg_id = id;
  msg->is_reply = is_reply;
  pthread_mutex_unlock(&msg->lock);

  rc = catbsd_xpc_serialize(msg, &buf, &len);
  if (rc != CATBSD_XPC_SUCCESS) {
    return rc;
  }

  rc = io_write_full(c->fd, buf, len);
  free(buf);

  if (rc == CATBSD_XPC_SUCCESS && id_out != NULL) {
    *id_out = id;
  }
  return rc;
}

int catbsd_xpc_connection_send_message(catbsd_xpc_connection_t c,
                                       catbsd_xpc_object_t msg) {
  uint64_t id;
  int rc;

  if (c == NULL || msg == NULL || c->is_listener) {
    return CATBSD_XPC_EINVAL;
  }

  pthread_mutex_lock(&c->lock);
  if (c->cancelled) {
    pthread_mutex_unlock(&c->lock);
    return CATBSD_XPC_ECLOSED;
  }
  id = c->next_id++;
  rc = send_locked(c, msg, id, 0, NULL);
  if (rc != CATBSD_XPC_SUCCESS) {
    c->last_error = rc;
  }
  pthread_mutex_unlock(&c->lock);
  return rc;
}

int catbsd_xpc_connection_reply(catbsd_xpc_connection_t c,
                                catbsd_xpc_object_t request,
                                catbsd_xpc_object_t reply) {
  uint64_t id;
  int rc;

  if (c == NULL || request == NULL || reply == NULL || c->is_listener) {
    return CATBSD_XPC_EINVAL;
  }

  id = catbsd_xpc_message_get_id(request);

  pthread_mutex_lock(&c->lock);
  if (c->cancelled) {
    pthread_mutex_unlock(&c->lock);
    return CATBSD_XPC_ECLOSED;
  }
  rc = send_locked(c, reply, id, 1, NULL);
  if (rc != CATBSD_XPC_SUCCESS) {
    c->last_error = rc;
  }
  pthread_mutex_unlock(&c->lock);
  return rc;
}

int catbsd_xpc_connection_receive_message(catbsd_xpc_connection_t c,
                                          catbsd_xpc_object_t *out,
                                          int timeout_ms) {
  xpc_deadline_t dl;
  catbsd_xpc_object_t obj;
  int rc;

  if (c == NULL || out == NULL || c->is_listener) {
    return CATBSD_XPC_EINVAL;
  }

  pthread_mutex_lock(&c->lock);
  if (c->resumed) {
    pthread_mutex_unlock(&c->lock);
    return CATBSD_XPC_EINVAL; /* the reader thread owns the socket */
  }
  obj = queue_pop_locked(c);
  pthread_mutex_unlock(&c->lock);

  if (obj != NULL) {
    *out = obj;
    return CATBSD_XPC_SUCCESS;
  }

  deadline_set(&dl, timeout_ms);

  pthread_mutex_lock(&c->read_lock);
  /* Another thread may have queued something while we waited for the
   * read lock; prefer that over touching the socket. */
  pthread_mutex_lock(&c->lock);
  obj = queue_pop_locked(c);
  pthread_mutex_unlock(&c->lock);
  if (obj != NULL) {
    pthread_mutex_unlock(&c->read_lock);
    *out = obj;
    return CATBSD_XPC_SUCCESS;
  }

  rc = frame_read(c, &obj, &dl);
  pthread_mutex_unlock(&c->read_lock);

  if (rc != CATBSD_XPC_SUCCESS) {
    set_err(c, rc);
    return rc;
  }

  *out = obj;
  return CATBSD_XPC_SUCCESS;
}

int catbsd_xpc_connection_send_message_with_reply_sync(
    catbsd_xpc_connection_t c, catbsd_xpc_object_t msg, int timeout_ms,
    catbsd_xpc_object_t *reply_out) {
  xpc_deadline_t dl;
  uint64_t id;
  int rc;

  if (c == NULL || msg == NULL || reply_out == NULL || c->is_listener) {
    return CATBSD_XPC_EINVAL;
  }

  pthread_mutex_lock(&c->lock);
  if (c->cancelled) {
    pthread_mutex_unlock(&c->lock);
    return CATBSD_XPC_ECLOSED;
  }
  if (c->resumed) {
    pthread_mutex_unlock(&c->lock);
    return CATBSD_XPC_EINVAL;
  }
  id = c->next_id++;
  rc = send_locked(c, msg, id, 0, NULL);
  pthread_mutex_unlock(&c->lock);

  if (rc != CATBSD_XPC_SUCCESS) {
    set_err(c, rc);
    return rc;
  }

  deadline_set(&dl, timeout_ms);

  for (;;) {
    catbsd_xpc_object_t obj;

    pthread_mutex_lock(&c->lock);
    obj = queue_take_reply_locked(c, id);
    pthread_mutex_unlock(&c->lock);
    if (obj != NULL) {
      *reply_out = obj;
      return CATBSD_XPC_SUCCESS;
    }

    pthread_mutex_lock(&c->read_lock);
    rc = frame_read(c, &obj, &dl);
    pthread_mutex_unlock(&c->read_lock);

    if (rc != CATBSD_XPC_SUCCESS) {
      set_err(c, rc);
      return rc;
    }

    if (obj->is_reply && obj->msg_id == id) {
      *reply_out = obj;
      return CATBSD_XPC_SUCCESS;
    }

    /* Something else arrived first -- keep it for receive_message()
     * rather than dropping traffic on the floor. */
    pthread_mutex_lock(&c->lock);
    queue_push_locked(c, obj);
    pthread_mutex_unlock(&c->lock);
  }
}

static void *reader_thread(void *arg) {
  catbsd_xpc_connection_t c = arg;

  for (;;) {
    xpc_deadline_t dl;
    catbsd_xpc_object_t obj = NULL;
    int rc, cancelled;

    deadline_set(&dl, -1); /* block; cancel() shuts the socket down */
    rc = frame_read(c, &obj, &dl);

    pthread_mutex_lock(&c->lock);
    cancelled = c->cancelled;
    if (rc != CATBSD_XPC_SUCCESS) {
      c->last_error = rc;
    }
    pthread_mutex_unlock(&c->lock);

    if (rc != CATBSD_XPC_SUCCESS) {
      if (!cancelled && c->handler != NULL) {
        c->handler(c, NULL, c->handler_ctx);
      }
      return NULL;
    }

    if (c->handler != NULL) {
      c->handler(c, obj, c->handler_ctx);
    } else {
      catbsd_xpc_release(obj);
    }
  }
}

int catbsd_xpc_connection_set_event_handler(catbsd_xpc_connection_t c,
                                            catbsd_xpc_event_handler_t handler,
                                            void *context) {
  if (c == NULL || c->is_listener) {
    return CATBSD_XPC_EINVAL;
  }
  pthread_mutex_lock(&c->lock);
  if (c->resumed) {
    pthread_mutex_unlock(&c->lock);
    return CATBSD_XPC_EINVAL; /* handler swaps mid-flight would race */
  }
  c->handler = handler;
  c->handler_ctx = context;
  pthread_mutex_unlock(&c->lock);
  return CATBSD_XPC_SUCCESS;
}

int catbsd_xpc_connection_resume(catbsd_xpc_connection_t c) {
  int rc = CATBSD_XPC_SUCCESS;

  if (c == NULL || c->is_listener) {
    return CATBSD_XPC_EINVAL;
  }

  pthread_mutex_lock(&c->lock);
  if (c->resumed || c->cancelled) {
    pthread_mutex_unlock(&c->lock);
    return CATBSD_XPC_EINVAL;
  }
  if (pthread_create(&c->reader, NULL, reader_thread, c) != 0) {
    rc = CATBSD_XPC_EIO;
  } else {
    c->resumed = 1;
  }
  pthread_mutex_unlock(&c->lock);
  return rc;
}

void catbsd_xpc_connection_cancel(catbsd_xpc_connection_t c) {
  int join_reader = 0;
  pthread_t reader;

  if (c == NULL) {
    return;
  }

  pthread_mutex_lock(&c->lock);
  if (c->cancelled) {
    pthread_mutex_unlock(&c->lock);
    return;
  }
  c->cancelled = 1;
  if (c->resumed) {
    join_reader = 1;
    reader = c->reader;
    c->resumed = 0;
  }
  pthread_mutex_unlock(&c->lock);

  /*
   * shutdown() rather than close(): it wakes the reader thread's poll()
   * immediately while leaving the descriptor valid, so the thread can
   * unwind without ever touching a recycled fd number.
   */
  if (c->fd >= 0) {
    shutdown(c->fd, SHUT_RDWR);
  }

  if (join_reader) {
    pthread_join(reader, NULL);
  }

  if (c->fd >= 0) {
    close(c->fd);
    c->fd = -1;
  }

  if (c->path != NULL) {
    unlink(c->path);
  }
  if (c->lock_fd >= 0) {
    /* Unlink before closing: dropping the flock first would let a racing
     * listener acquire the lock on a file we are about to delete. */
    if (c->lock_path != NULL) {
      unlink(c->lock_path);
    }
    close(c->lock_fd);
    c->lock_fd = -1;
  }
}

void catbsd_xpc_connection_release(catbsd_xpc_connection_t c) {
  catbsd_xpc_object_t obj;

  if (c == NULL) {
    return;
  }

  catbsd_xpc_connection_cancel(c);

  pthread_mutex_lock(&c->lock);
  while ((obj = queue_pop_locked(c)) != NULL) {
    catbsd_xpc_release(obj);
  }
  pthread_mutex_unlock(&c->lock);

  pthread_mutex_destroy(&c->lock);
  pthread_mutex_destroy(&c->read_lock);
  free(c->name);
  free(c->path);
  free(c->lock_path);
  free(c);
}

int catbsd_xpc_connection_get_fd(catbsd_xpc_connection_t c) {
  return (c != NULL) ? c->fd : -1;
}

int catbsd_xpc_connection_last_error(catbsd_xpc_connection_t c) {
  int e;
  if (c == NULL) {
    return CATBSD_XPC_EINVAL;
  }
  pthread_mutex_lock(&c->lock);
  e = c->last_error;
  pthread_mutex_unlock(&c->lock);
  return e;
}

const char *catbsd_xpc_connection_get_name(catbsd_xpc_connection_t c) {
  return (c != NULL) ? c->name : NULL;
}
