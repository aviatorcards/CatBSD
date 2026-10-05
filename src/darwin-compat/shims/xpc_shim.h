/*
 * CatBSD XPC Compatibility Shim
 * Maps Darwin XPC to Unix domain sockets
 *
 * Darwin's XPC is a message-oriented IPC layer built on Mach ports and
 * launchd-published service names. Nothing under it exists on FreeBSD, so
 * this shim rebuilds the parts Darwin components actually use -- typed
 * message dictionaries, a named service rendezvous, request/reply
 * correlation and an asynchronous event handler -- directly on top of
 * AF_UNIX SOCK_STREAM sockets, which every target platform has.
 *
 * Naming: every symbol here is prefixed catbsd_xpc_ rather than xpc_.
 * Real Darwin already ships xpc_object_t, xpc_dictionary_create() and
 * friends in libSystem, and a header that redeclared them with different
 * underlying types would break the moment both ended up in one
 * translation unit -- the same collision mach_port.h documents for
 * mach_port_t. Non-Apple targets that want source-level Darwin spelling
 * can include xpc_compat.h, which maps the Darwin names onto these.
 */

#ifndef _CATBSD_XPC_SHIM_H_
#define _CATBSD_XPC_SHIM_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Error codes. All connection/dictionary calls return 0 on success and a
 * negative CATBSD_XPC_E* on failure; catbsd_xpc_strerror() renders them.
 */
#define CATBSD_XPC_SUCCESS 0
#define CATBSD_XPC_EINVAL -1  /* bad argument */
#define CATBSD_XPC_ENOMEM -2  /* allocation failed */
#define CATBSD_XPC_EIO -3     /* underlying socket error */
#define CATBSD_XPC_ETIMEOUT -4 /* deadline elapsed */
#define CATBSD_XPC_ECLOSED -5 /* peer hung up / connection cancelled */
#define CATBSD_XPC_EPROTO -6  /* malformed or oversized frame */
#define CATBSD_XPC_EEXIST -7  /* service name already bound */
#define CATBSD_XPC_ENOENT -8  /* no listener for that service name */

/* Value types storable in a message dictionary. */
#define CATBSD_XPC_TYPE_INVALID 0
#define CATBSD_XPC_TYPE_BOOL 1
#define CATBSD_XPC_TYPE_INT64 2
#define CATBSD_XPC_TYPE_UINT64 3
#define CATBSD_XPC_TYPE_DOUBLE 4
#define CATBSD_XPC_TYPE_STRING 5
#define CATBSD_XPC_TYPE_DATA 6

/*
 * Largest serialized message accepted on the wire. A peer is untrusted
 * input: without a cap, a bogus length prefix turns into an unbounded
 * malloc. 1 MiB is far above anything launchd-style control traffic
 * needs.
 */
#define CATBSD_XPC_MAX_MESSAGE (1024 * 1024)

typedef struct catbsd_xpc_object *catbsd_xpc_object_t;
typedef struct catbsd_xpc_connection *catbsd_xpc_connection_t;

/*
 * Delivered on the connection's reader thread once
 * catbsd_xpc_connection_resume() has been called. `msg` is owned by the
 * handler and must be released with catbsd_xpc_release(); a NULL `msg`
 * signals that the connection died, mirroring Darwin's
 * XPC_ERROR_CONNECTION_INVALID -- call catbsd_xpc_connection_last_error()
 * for the reason.
 */
typedef void (*catbsd_xpc_event_handler_t)(catbsd_xpc_connection_t conn,
                                           catbsd_xpc_object_t msg,
                                           void *context);

/*
 * ---------------------------------------------------------------------
 * Message dictionaries
 * ---------------------------------------------------------------------
 */

catbsd_xpc_object_t catbsd_xpc_dictionary_create(void);

/* Reference counting. retain() returns its argument for convenience. */
catbsd_xpc_object_t catbsd_xpc_retain(catbsd_xpc_object_t obj);
void catbsd_xpc_release(catbsd_xpc_object_t obj);

/*
 * Setters. Each replaces any existing value for `key`. Strings and data
 * are copied into the dictionary, so callers keep ownership of what they
 * pass in.
 */
int catbsd_xpc_dictionary_set_bool(catbsd_xpc_object_t dict, const char *key,
                                   int value);
int catbsd_xpc_dictionary_set_int64(catbsd_xpc_object_t dict, const char *key,
                                    int64_t value);
int catbsd_xpc_dictionary_set_uint64(catbsd_xpc_object_t dict, const char *key,
                                     uint64_t value);
int catbsd_xpc_dictionary_set_double(catbsd_xpc_object_t dict, const char *key,
                                     double value);
int catbsd_xpc_dictionary_set_string(catbsd_xpc_object_t dict, const char *key,
                                     const char *value);
int catbsd_xpc_dictionary_set_data(catbsd_xpc_object_t dict, const char *key,
                                   const void *bytes, size_t len);

/*
 * Getters. The scalar forms return CATBSD_XPC_SUCCESS and fill *out only
 * when the key exists AND holds that exact type; otherwise they return
 * CATBSD_XPC_EINVAL and leave *out untouched. This is deliberately
 * stricter than Darwin's xpc_dictionary_get_int64(), which folds "absent"
 * and "genuinely zero" into the same 0 return -- a distinction a
 * supervisor reading job state actually needs.
 */
int catbsd_xpc_dictionary_get_bool(catbsd_xpc_object_t dict, const char *key,
                                   int *out);
int catbsd_xpc_dictionary_get_int64(catbsd_xpc_object_t dict, const char *key,
                                    int64_t *out);
int catbsd_xpc_dictionary_get_uint64(catbsd_xpc_object_t dict, const char *key,
                                     uint64_t *out);
int catbsd_xpc_dictionary_get_double(catbsd_xpc_object_t dict, const char *key,
                                     double *out);

/* Borrowed pointers, valid until the key is overwritten or the
 * dictionary is released. NULL if absent or of another type. */
const char *catbsd_xpc_dictionary_get_string(catbsd_xpc_object_t dict,
                                             const char *key);
const void *catbsd_xpc_dictionary_get_data(catbsd_xpc_object_t dict,
                                           const char *key, size_t *len_out);

/* CATBSD_XPC_TYPE_INVALID if the key is absent. */
int catbsd_xpc_dictionary_get_type(catbsd_xpc_object_t dict, const char *key);
size_t catbsd_xpc_dictionary_count(catbsd_xpc_object_t dict);
int catbsd_xpc_dictionary_remove(catbsd_xpc_object_t dict, const char *key);

/* Iterate keys in insertion order; a non-zero return from `fn` stops the
 * walk and is returned from apply(). */
int catbsd_xpc_dictionary_apply(catbsd_xpc_object_t dict,
                                int (*fn)(const char *key, int type,
                                          void *context),
                                void *context);

/* Human-readable dump for logging/debugging; caller free()s the result. */
char *catbsd_xpc_copy_description(catbsd_xpc_object_t dict);

/*
 * Wire format, exposed because it is independently useful (checkpointing
 * a message to disk) and because it makes the framing directly testable
 * without a socket.
 */
int catbsd_xpc_serialize(catbsd_xpc_object_t dict, void **buf_out,
                         size_t *len_out);
catbsd_xpc_object_t catbsd_xpc_deserialize(const void *buf, size_t len);

/* Correlation id stamped on a message when it is sent; 0 before that. */
uint64_t catbsd_xpc_message_get_id(catbsd_xpc_object_t msg);
/* Non-zero if this message arrived as a reply to an earlier request. */
int catbsd_xpc_message_is_reply(catbsd_xpc_object_t msg);

/*
 * ---------------------------------------------------------------------
 * Connections
 * ---------------------------------------------------------------------
 *
 * `name` is a service name, not a path: it is resolved to
 * $CATBSD_XPC_RUNTIME_DIR/<name> (default /tmp/catbsd-xpc-<uid>/<name>),
 * which is what makes these look like launchd-published service names
 * rather than filesystem details the caller has to agree on. A name
 * starting with '/' is used verbatim as a socket path instead.
 */

/* Bind and listen. Fails with CATBSD_XPC_EEXIST if another live listener
 * already holds the name. */
catbsd_xpc_connection_t catbsd_xpc_connection_create_listener(const char *name,
                                                              int *err_out);

/* Connect to a listener. CATBSD_XPC_ENOENT if nothing is listening. */
catbsd_xpc_connection_t catbsd_xpc_connection_create(const char *name,
                                                     int *err_out);

/*
 * Accept one peer on a listener. Blocks up to timeout_ms (negative =
 * forever), returning NULL with *err_out == CATBSD_XPC_ETIMEOUT if no one
 * arrives.
 */
catbsd_xpc_connection_t catbsd_xpc_connection_accept(
    catbsd_xpc_connection_t listener, int timeout_ms, int *err_out);

int catbsd_xpc_connection_send_message(catbsd_xpc_connection_t conn,
                                       catbsd_xpc_object_t msg);

/*
 * Receive the next message not claimed by a pending reply wait. The
 * caller owns *out and must release it.
 */
int catbsd_xpc_connection_receive_message(catbsd_xpc_connection_t conn,
                                          catbsd_xpc_object_t *out,
                                          int timeout_ms);

/*
 * Send and block for the matching reply. Messages that arrive in the
 * meantime are queued for catbsd_xpc_connection_receive_message() rather
 * than dropped, so an unrelated notification racing the reply doesn't
 * lose either one.
 */
int catbsd_xpc_connection_send_message_with_reply_sync(
    catbsd_xpc_connection_t conn, catbsd_xpc_object_t msg, int timeout_ms,
    catbsd_xpc_object_t *reply_out);

/* Answer `request`, tagging the reply with the request's correlation id. */
int catbsd_xpc_connection_reply(catbsd_xpc_connection_t conn,
                                catbsd_xpc_object_t request,
                                catbsd_xpc_object_t reply);

/*
 * Asynchronous delivery. Set the handler first, then resume() to start
 * the reader thread. Once resumed, receive_message() on that connection
 * is no longer valid -- the reader thread owns the socket.
 */
int catbsd_xpc_connection_set_event_handler(catbsd_xpc_connection_t conn,
                                            catbsd_xpc_event_handler_t handler,
                                            void *context);
int catbsd_xpc_connection_resume(catbsd_xpc_connection_t conn);

/* Shut the socket down; a resumed reader thread is joined here. Safe to
 * call more than once. */
void catbsd_xpc_connection_cancel(catbsd_xpc_connection_t conn);
void catbsd_xpc_connection_release(catbsd_xpc_connection_t conn);

int catbsd_xpc_connection_get_fd(catbsd_xpc_connection_t conn);
int catbsd_xpc_connection_last_error(catbsd_xpc_connection_t conn);
const char *catbsd_xpc_connection_get_name(catbsd_xpc_connection_t conn);

const char *catbsd_xpc_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* _CATBSD_XPC_SHIM_H_ */
