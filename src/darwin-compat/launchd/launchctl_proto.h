/*
 * launchctl_proto.h — CatBSD launchd control protocol
 *
 * Every request is a catbsd_xpc_object_t (dictionary) containing at least
 * LCTL_KEY_VERB.  Replies always contain LCTL_KEY_CODE (int64, 0 = ok) and
 * LCTL_KEY_ERROR (string, empty on success).
 *
 * The transport is the XPC shim (../shims/xpc_shim.h): a named service over
 * an AF_UNIX socket, with built-in request/reply correlation.  The service
 * name is LCTL_SERVICE_NAME, resolved by the shim to a runtime directory so
 * callers never have to agree on a raw socket path.
 */

#ifndef _CATBSD_LAUNCHCTL_PROTO_H_
#define _CATBSD_LAUNCHCTL_PROTO_H_

/* -------------------------------------------------------------------------
 * Service name (resolved by XPC shim to $CATBSD_XPC_RUNTIME_DIR/<name>)
 * ------------------------------------------------------------------------- */
#define LCTL_SERVICE_NAME "com.catbsd.launchd"

/* -------------------------------------------------------------------------
 * Keys present in every request
 * ------------------------------------------------------------------------- */
#define LCTL_KEY_VERB    "verb"    /* string — one of LCTL_VERB_* below     */

/* -------------------------------------------------------------------------
 * Keys used in specific verbs
 * ------------------------------------------------------------------------- */
#define LCTL_KEY_LABEL   "label"   /* string — job label                    */
#define LCTL_KEY_PATH    "path"    /* string — plist path (load verb)       */

/* -------------------------------------------------------------------------
 * Keys present in every reply
 * ------------------------------------------------------------------------- */
#define LCTL_KEY_CODE    "code"    /* int64 — 0 = ok, LCTL_E_* on error    */
#define LCTL_KEY_ERROR   "error"   /* string — human message (empty on ok)  */

/* -------------------------------------------------------------------------
 * Keys in list / status replies
 * ------------------------------------------------------------------------- */
/* list reply: each job is a sub-dict embedded under its label */
#define LCTL_KEY_JOBS    "jobs"    /* string — newline-joined tabular text  */

/* status reply fields (one job) */
#define LCTL_KEY_STATE   "state"   /* string — LOADED/RUNNING/EXITED/…      */
#define LCTL_KEY_PID     "pid"     /* int64 — child pid, or -1 when stopped */
#define LCTL_KEY_STARTS  "starts"  /* uint64 — total start count            */
#define LCTL_KEY_EXIT    "exit"    /* int64 — last exit code / -signo       */

/* -------------------------------------------------------------------------
 * Verbs
 * ------------------------------------------------------------------------- */
/* Load a plist file and add it to the registry. */
#define LCTL_VERB_LOAD   "load"
/* Unload (stop + remove) a job by label. */
#define LCTL_VERB_UNLOAD "unload"
/* Start a loaded-but-not-running job by label. */
#define LCTL_VERB_START  "start"
/* Stop a running job by label (SIGTERM → SIGKILL). */
#define LCTL_VERB_STOP   "stop"
/* List all jobs in the registry (tabular text in LCTL_KEY_JOBS). */
#define LCTL_VERB_LIST   "list"
/* Get detailed status for a single job by label. */
#define LCTL_VERB_STATUS "status"

/* -------------------------------------------------------------------------
 * Error codes (returned in LCTL_KEY_CODE)
 * -------------------------------------------------------------------------
 * 0 is success.  Negative values mirror LAUNCH_E_* from launch_job.h.
 * Additional protocol-level errors start at -100.
 * ------------------------------------------------------------------------- */
#define LCTL_OK           0
#define LCTL_E_NOTFOUND  -1   /* no job with that label                     */
#define LCTL_E_ALREADY   -2   /* job already in requested state             */
#define LCTL_E_BADVERB   -3   /* unknown verb                               */
#define LCTL_E_BADARG    -4   /* required key missing or wrong type         */
#define LCTL_E_INTERNAL  -5   /* launch_registry error (see error string)   */
#define LCTL_E_PLIST     -6   /* plist parse / load error                   */

/* Human-readable label for a LAUNCH_STATE_* value */
static inline const char *lctl_state_string(int state) {
    switch (state) {
        case 0: return "loaded";
        case 1: return "running";
        case 2: return "exited";
        case 3: return "waiting";
        case 4: return "stopped";
        case 5: return "failed";
        default: return "unknown";
    }
}

#endif /* _CATBSD_LAUNCHCTL_PROTO_H_ */
