/*
 * liblaunch - job definition and supervision for CatBSD
 *
 * launchd-demo showed the shape of a launchd: parse a job, start it, stop
 * it, list what's running. This library is that logic factored out of the
 * demo and made real -- job plists actually parsed, children actually
 * supervised, KeepAlive and ThrottleInterval actually honoured, sockets
 * actually pre-created and handed to the child.
 *
 * What it deliberately is not: an init system. There is no PID 1
 * behaviour, no session or domain model, no XPC bootstrap namespace, no
 * launchctl protocol. Those belong on top of this, and the pieces they
 * need -- IPC (xpc_shim), timers (libdispatch) -- now exist beside it.
 *
 * Supervision is explicit rather than signal-driven: the caller decides
 * when to call launch_registry_tick(). A library that installed its own
 * SIGCHLD handler would fight whatever the embedding program already does
 * with signals, and the tick model drops straight into an existing event
 * loop or a dispatch timer.
 */

#ifndef _CATBSD_LAUNCH_JOB_H_
#define _CATBSD_LAUNCH_JOB_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LAUNCH_OK 0
#define LAUNCH_EINVAL -1
#define LAUNCH_ENOMEM -2
#define LAUNCH_EPARSE -3 /* the plist did not parse or lacked a Label */
#define LAUNCH_EIO -4
#define LAUNCH_ENOENT -5
#define LAUNCH_ESTATE -6   /* job is not in a state where this makes sense */
#define LAUNCH_EEXIST -7   /* a job with that Label is already loaded */
#define LAUNCH_ETIMEOUT -8

/* Job lifecycle states. */
#define LAUNCH_STATE_LOADED 0  /* known, never started */
#define LAUNCH_STATE_RUNNING 1 /* child alive */
#define LAUNCH_STATE_EXITED 2  /* child exited, no restart pending */
#define LAUNCH_STATE_WAITING 3 /* restart pending, held by ThrottleInterval */
#define LAUNCH_STATE_STOPPED 4 /* stopped on request; will not restart */
#define LAUNCH_STATE_FAILED 5  /* could not be spawned at all */

/* KeepAlive interpretation, from the plist's bool or its
 * SuccessfulExit sub-key. */
#define LAUNCH_KEEPALIVE_NEVER 0
#define LAUNCH_KEEPALIVE_ALWAYS 1
#define LAUNCH_KEEPALIVE_ON_FAILURE 2 /* SuccessfulExit=false */
#define LAUNCH_KEEPALIVE_ON_SUCCESS 3 /* SuccessfulExit=true */

typedef struct launch_job launch_job_t;
typedef struct launch_registry launch_registry_t;

const char *launch_strerror(int err);
const char *launch_state_name(int state);

/*
 * ---------------------------------------------------------------------
 * Registry
 * ---------------------------------------------------------------------
 */

launch_registry_t *launch_registry_create(void);

/* Stops everything still running, then frees. */
void launch_registry_free(launch_registry_t *reg);

/*
 * ThrottleInterval default for jobs that don't set one. Darwin's is 10
 * seconds; the same value here would make any restart test a ten-second
 * test, so it is settable.
 */
void launch_registry_set_default_throttle(launch_registry_t *reg,
                                          int64_t millis);

/* Load one job plist. On failure returns a LAUNCH_E* code and fills
 * `errbuf` with a message naming the file and, for parse errors, the
 * line. */
int launch_registry_load_plist(launch_registry_t *reg, const char *path,
                               launch_job_t **job_out, char *errbuf,
                               size_t errlen);

/* Load every *.plist in `dir`. Returns the number loaded, or a negative
 * LAUNCH_E* if the directory itself could not be read; individual bad
 * plists are skipped and counted in `*failed_out`. */
int launch_registry_load_dir(launch_registry_t *reg, const char *dir,
                             int *failed_out, char *errbuf, size_t errlen);

launch_job_t *launch_registry_find(launch_registry_t *reg, const char *label);
size_t launch_registry_count(launch_registry_t *reg);
launch_job_t *launch_registry_at(launch_registry_t *reg, size_t index);

/* Remove a job from the registry, stopping it first if it is running. */
int launch_registry_unload(launch_registry_t *reg, const char *label);

/*
 * ---------------------------------------------------------------------
 * Supervision
 * ---------------------------------------------------------------------
 */

int launch_job_start(launch_registry_t *reg, launch_job_t *job, char *errbuf,
                     size_t errlen);

/*
 * Send SIGTERM, wait up to `timeout_ms` for the child to leave, then
 * SIGKILL. The job moves to LAUNCH_STATE_STOPPED and is not restarted,
 * whatever its KeepAlive says -- an operator's stop outranks the plist.
 */
int launch_job_stop(launch_registry_t *reg, launch_job_t *job, int timeout_ms);

/* Start every job whose plist has RunAtLoad. Returns how many started. */
int launch_registry_start_all(launch_registry_t *reg);

/*
 * Reap exited children and act on their KeepAlive: this is where restarts
 * and throttling happen. Returns the number of jobs whose state changed,
 * so a caller can log only on transitions.
 */
int launch_registry_tick(launch_registry_t *reg);

/* Tick repeatedly for `duration_ms`. Returns total state changes. */
int launch_registry_run(launch_registry_t *reg, int duration_ms);

/* Tick until `job` reaches `state`, or `timeout_ms` elapses. */
int launch_registry_wait_state(launch_registry_t *reg, launch_job_t *job,
                               int state, int timeout_ms);

/* Stop everything currently running. Returns how many were stopped. */
int launch_registry_stop_all(launch_registry_t *reg, int timeout_ms);

/*
 * ---------------------------------------------------------------------
 * Job accessors
 * ---------------------------------------------------------------------
 */

const char *launch_job_get_label(const launch_job_t *job);
const char *launch_job_get_plist_path(const launch_job_t *job);
pid_t launch_job_get_pid(const launch_job_t *job);
int launch_job_get_state(const launch_job_t *job);
int launch_job_get_keepalive(const launch_job_t *job);
int launch_job_get_run_at_load(const launch_job_t *job);
int64_t launch_job_get_throttle(const launch_job_t *job);
uint64_t launch_job_get_start_count(const launch_job_t *job);

/* Exit status of the most recent run: the exit code, or -signo if the
 * child was killed. Meaningless before the first exit. */
int launch_job_get_last_exit(const launch_job_t *job);
int launch_job_was_signaled(const launch_job_t *job);

int launch_job_argc(const launch_job_t *job);
const char *launch_job_argv_at(const launch_job_t *job, int index);

/*
 * ---------------------------------------------------------------------
 * Socket activation
 * ---------------------------------------------------------------------
 *
 * Sockets named in the job's <key>Sockets</key> dict are created and
 * listening before the child is forked, so a client can connect the
 * instant the job is loaded -- the child inherits a socket that is
 * already accepting. The fd number is passed in the environment as
 * LAUNCH_ACTIVATE_SOCKET_FD_<NAME>, and the child retrieves it with
 * launch_activate_socket(), which is the shape Darwin's function of the
 * same name has.
 */

/* Parent side: the listening fd for `name`, or -1. */
int launch_job_socket_fd(const launch_job_t *job, const char *name);
const char *launch_job_socket_path(const launch_job_t *job, const char *name);
int launch_job_socket_count(const launch_job_t *job);

/* Child side: recover an inherited listening socket by name. Returns the
 * fd, or -1 with errno set to ENOENT if this process wasn't handed one. */
int launch_activate_socket(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _CATBSD_LAUNCH_JOB_H_ */
