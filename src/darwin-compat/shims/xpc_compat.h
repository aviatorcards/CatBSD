/*
 * CatBSD XPC Darwin-name compatibility header
 *
 * xpc_shim.h deliberately prefixes everything catbsd_xpc_ so it can never
 * collide with libSystem's real XPC. Ported Darwin sources, though, are
 * written against the Darwin spelling. Including this header after
 * xpc_shim.h maps the Darwin names onto the shim, which usually reduces
 * a port's diff to one extra #include:
 *
 *   #ifdef FREEBSD_COMPAT
 *   #include "xpc_shim.h"
 *   #include "xpc_compat.h"
 *   #else
 *   #include <xpc/xpc.h>
 *   #endif
 *
 * This is macros only -- no wrapper functions, so it costs nothing at
 * runtime and adds no symbols to link against.
 *
 * Note the signatures are the shim's, not Darwin's: the shim's getters
 * report absent-vs-zero and its constructors take an error out-parameter.
 * The names line up; the call sites still need review. This header saves
 * mechanical renaming, not semantic porting.
 */

#ifndef _CATBSD_XPC_COMPAT_H_
#define _CATBSD_XPC_COMPAT_H_

#if defined(__APPLE__)
#error "xpc_compat.h would shadow the real XPC on Darwin; include <xpc/xpc.h>"
#endif

#ifndef _CATBSD_XPC_SHIM_H_
#include "xpc_shim.h"
#endif

typedef catbsd_xpc_object_t xpc_object_t;
typedef catbsd_xpc_connection_t xpc_connection_t;

#define XPC_TYPE_BOOL CATBSD_XPC_TYPE_BOOL
#define XPC_TYPE_INT64 CATBSD_XPC_TYPE_INT64
#define XPC_TYPE_UINT64 CATBSD_XPC_TYPE_UINT64
#define XPC_TYPE_DOUBLE CATBSD_XPC_TYPE_DOUBLE
#define XPC_TYPE_STRING CATBSD_XPC_TYPE_STRING
#define XPC_TYPE_DATA CATBSD_XPC_TYPE_DATA

#define xpc_dictionary_create catbsd_xpc_dictionary_create
#define xpc_retain catbsd_xpc_retain
#define xpc_release catbsd_xpc_release

#define xpc_dictionary_set_bool catbsd_xpc_dictionary_set_bool
#define xpc_dictionary_set_int64 catbsd_xpc_dictionary_set_int64
#define xpc_dictionary_set_uint64 catbsd_xpc_dictionary_set_uint64
#define xpc_dictionary_set_double catbsd_xpc_dictionary_set_double
#define xpc_dictionary_set_string catbsd_xpc_dictionary_set_string
#define xpc_dictionary_set_data catbsd_xpc_dictionary_set_data

#define xpc_dictionary_get_bool catbsd_xpc_dictionary_get_bool
#define xpc_dictionary_get_int64 catbsd_xpc_dictionary_get_int64
#define xpc_dictionary_get_uint64 catbsd_xpc_dictionary_get_uint64
#define xpc_dictionary_get_double catbsd_xpc_dictionary_get_double
#define xpc_dictionary_get_string catbsd_xpc_dictionary_get_string
#define xpc_dictionary_get_data catbsd_xpc_dictionary_get_data
#define xpc_dictionary_apply catbsd_xpc_dictionary_apply
#define xpc_copy_description catbsd_xpc_copy_description

#define xpc_connection_create catbsd_xpc_connection_create
#define xpc_connection_create_listener catbsd_xpc_connection_create_listener
#define xpc_connection_send_message catbsd_xpc_connection_send_message
#define xpc_connection_send_message_with_reply_sync                            \
  catbsd_xpc_connection_send_message_with_reply_sync
#define xpc_connection_set_event_handler                                       \
  catbsd_xpc_connection_set_event_handler
#define xpc_connection_resume catbsd_xpc_connection_resume
#define xpc_connection_cancel catbsd_xpc_connection_cancel

#endif /* _CATBSD_XPC_COMPAT_H_ */
