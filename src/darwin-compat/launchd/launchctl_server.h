/*
 * launchctl_server.h — server-side launchd control endpoint
 *
 * Wraps a catbsd_xpc_connection_t listener and dispatches incoming
 * launchctl verbs to an existing launch_registry_t.  The caller keeps
 * ownership of the registry and drives the supervision tick loop; this
 * file just handles the control channel.
 *
 * Typical embedding in a daemon:
 *
 *   launch_registry_t *reg = launch_registry_create();
 *   ...
 *   lctl_server_t *srv = lctl_server_create(reg, LCTL_SERVICE_NAME, &err);
 *
 *   for (;;) {
 *       lctl_server_poll(srv, 10);      // handle ≤1 pending request
 *       launch_registry_tick(reg);       // reap + restart children
 *       usleep(10000);
 *   }
 */

#ifndef _CATBSD_LAUNCHCTL_SERVER_H_
#define _CATBSD_LAUNCHCTL_SERVER_H_

#include "launch_job.h"
#include "launchctl_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lctl_server lctl_server_t;

/*
 * Create a listener on `service_name` backed by `reg`.
 * Returns NULL and fills `errbuf` on failure.
 */
lctl_server_t *lctl_server_create(launch_registry_t *reg,
                                  const char *service_name,
                                  char *errbuf, size_t errlen);

/*
 * Accept and handle at most one pending request, waiting up to
 * `timeout_ms` for a new connection.  Returns 1 if a request was
 * handled, 0 if the timeout elapsed with nothing arriving, -1 on a
 * fatal listener error.
 *
 * This is deliberately one-request-at-a-time so the caller can
 * interleave it with launch_registry_tick() in a simple loop without
 * needing threads.
 */
int lctl_server_poll(lctl_server_t *srv, int timeout_ms);

/*
 * Shut down the listener and free all resources.
 */
void lctl_server_destroy(lctl_server_t *srv);

/* Socket path the server is listening on (for diagnostics / tests). */
const char *lctl_server_socket_path(const lctl_server_t *srv);

#ifdef __cplusplus
}
#endif

#endif /* _CATBSD_LAUNCHCTL_SERVER_H_ */
