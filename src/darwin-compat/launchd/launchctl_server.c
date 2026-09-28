/*
 * launchctl_server.c — server-side launchd control endpoint
 *
 * Dispatches incoming LCTL_VERB_* requests to the launch_registry_t and
 * sends back a reply dict containing LCTL_KEY_CODE and LCTL_KEY_ERROR.
 * One request is handled per lctl_server_poll() call so the caller can
 * interleave with launch_registry_tick() in a single-threaded loop.
 */

#include "launchctl_server.h"
#include "launchctl_proto.h"
#include "launch_job.h"

#include "../shims/xpc_shim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------------- */

struct lctl_server {
    launch_registry_t       *reg;
    catbsd_xpc_connection_t  listener;
    char                     socket_path[256];
};

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/* Send a reply dict with a code and an optional error string. */
static int reply_err(catbsd_xpc_connection_t peer,
                     catbsd_xpc_object_t request,
                     int64_t code, const char *errmsg)
{
    catbsd_xpc_object_t rep = catbsd_xpc_dictionary_create();
    if (!rep) return -1;
    catbsd_xpc_dictionary_set_int64(rep,  LCTL_KEY_CODE, code);
    catbsd_xpc_dictionary_set_string(rep, LCTL_KEY_ERROR,
                                     errmsg ? errmsg : "");
    int rc = catbsd_xpc_connection_reply(peer, request, rep);
    catbsd_xpc_release(rep);
    return rc;
}

static int reply_ok(catbsd_xpc_connection_t peer,
                    catbsd_xpc_object_t request)
{
    return reply_err(peer, request, LCTL_OK, "");
}

/* -------------------------------------------------------------------------
 * Verb handlers
 * Each receives the peer connection, the request dict, and the registry.
 * It must send exactly one reply before returning.
 * ------------------------------------------------------------------------- */

static void handle_list(catbsd_xpc_connection_t peer,
                        catbsd_xpc_object_t req,
                        launch_registry_t *reg)
{
    (void)req;

    /* Build a tabular text blob:
     *   LABEL                PID    STATE
     *   com.catbsd.example   1234   running
     * Simple text is easier to consume in the CLI without a dict-of-dicts.
     */
    char buf[16384];
    int  off = 0;
    size_t n = launch_registry_count(reg);

    off += snprintf(buf + off, sizeof(buf) - (size_t)off,
                    "%-40s  %-8s  %s\n", "LABEL", "PID", "STATE");

    for (size_t i = 0; i < n && off < (int)sizeof(buf) - 1; i++) {
        launch_job_t *job = launch_registry_at(reg, i);
        pid_t pid = launch_job_get_pid(job);
        int   state = launch_job_get_state(job);

        if (pid > 0)
            off += snprintf(buf + off, sizeof(buf) - (size_t)off,
                            "%-40s  %-8d  %s\n",
                            launch_job_get_label(job),
                            (int)pid,
                            lctl_state_string(state));
        else
            off += snprintf(buf + off, sizeof(buf) - (size_t)off,
                            "%-40s  %-8s  %s\n",
                            launch_job_get_label(job),
                            "-",
                            lctl_state_string(state));
    }
    if (n == 0)
        off += snprintf(buf + off, sizeof(buf) - (size_t)off,
                        "(no jobs loaded)\n");

    catbsd_xpc_object_t rep = catbsd_xpc_dictionary_create();
    if (!rep) { reply_err(peer, req, LCTL_E_INTERNAL, "out of memory"); return; }
    catbsd_xpc_dictionary_set_int64(rep,  LCTL_KEY_CODE,  LCTL_OK);
    catbsd_xpc_dictionary_set_string(rep, LCTL_KEY_ERROR, "");
    catbsd_xpc_dictionary_set_string(rep, LCTL_KEY_JOBS,  buf);
    catbsd_xpc_connection_reply(peer, req, rep);
    catbsd_xpc_release(rep);
}

static void handle_status(catbsd_xpc_connection_t peer,
                          catbsd_xpc_object_t req,
                          launch_registry_t *reg)
{
    const char *label = catbsd_xpc_dictionary_get_string(req, LCTL_KEY_LABEL);
    if (!label || !*label) {
        reply_err(peer, req, LCTL_E_BADARG, "missing 'label'");
        return;
    }
    launch_job_t *job = launch_registry_find(reg, label);
    if (!job) {
        reply_err(peer, req, LCTL_E_NOTFOUND, "no such job");
        return;
    }

    catbsd_xpc_object_t rep = catbsd_xpc_dictionary_create();
    if (!rep) { reply_err(peer, req, LCTL_E_INTERNAL, "out of memory"); return; }

    pid_t pid = launch_job_get_pid(job);
    catbsd_xpc_dictionary_set_int64(rep,  LCTL_KEY_CODE,   LCTL_OK);
    catbsd_xpc_dictionary_set_string(rep, LCTL_KEY_ERROR,  "");
    catbsd_xpc_dictionary_set_string(rep, LCTL_KEY_LABEL,  label);
    catbsd_xpc_dictionary_set_string(rep, LCTL_KEY_STATE,
                                     lctl_state_string(launch_job_get_state(job)));
    catbsd_xpc_dictionary_set_int64(rep,  LCTL_KEY_PID,
                                    pid > 0 ? (int64_t)pid : -1);
    catbsd_xpc_dictionary_set_uint64(rep, LCTL_KEY_STARTS,
                                     launch_job_get_start_count(job));
    catbsd_xpc_dictionary_set_int64(rep,  LCTL_KEY_EXIT,
                                    (int64_t)launch_job_get_last_exit(job));
    catbsd_xpc_connection_reply(peer, req, rep);
    catbsd_xpc_release(rep);
}

static void handle_load(catbsd_xpc_connection_t peer,
                        catbsd_xpc_object_t req,
                        launch_registry_t *reg)
{
    const char *path = catbsd_xpc_dictionary_get_string(req, LCTL_KEY_PATH);
    if (!path || !*path) {
        reply_err(peer, req, LCTL_E_BADARG, "missing 'path'");
        return;
    }

    char errbuf[512];
    launch_job_t *job = NULL;
    int rc = launch_registry_load_plist(reg, path, &job, errbuf, sizeof(errbuf));
    if (rc != 0) {
        reply_err(peer, req, LCTL_E_PLIST, errbuf);
        return;
    }
    reply_ok(peer, req);
}

static void handle_unload(catbsd_xpc_connection_t peer,
                          catbsd_xpc_object_t req,
                          launch_registry_t *reg)
{
    const char *label = catbsd_xpc_dictionary_get_string(req, LCTL_KEY_LABEL);
    if (!label || !*label) {
        reply_err(peer, req, LCTL_E_BADARG, "missing 'label'");
        return;
    }
    if (!launch_registry_find(reg, label)) {
        reply_err(peer, req, LCTL_E_NOTFOUND, "no such job");
        return;
    }
    int rc = launch_registry_unload(reg, label);
    if (rc != 0) {
        reply_err(peer, req, LCTL_E_INTERNAL, "unload failed");
        return;
    }
    reply_ok(peer, req);
}

static void handle_start(catbsd_xpc_connection_t peer,
                         catbsd_xpc_object_t req,
                         launch_registry_t *reg)
{
    const char *label = catbsd_xpc_dictionary_get_string(req, LCTL_KEY_LABEL);
    if (!label || !*label) {
        reply_err(peer, req, LCTL_E_BADARG, "missing 'label'");
        return;
    }
    launch_job_t *job = launch_registry_find(reg, label);
    if (!job) {
        reply_err(peer, req, LCTL_E_NOTFOUND, "no such job");
        return;
    }
    if (launch_job_get_state(job) == 1 /* LAUNCH_STATE_RUNNING */) {
        reply_err(peer, req, LCTL_E_ALREADY, "job is already running");
        return;
    }

    char errbuf[512];
    int rc = launch_job_start(reg, job, errbuf, sizeof(errbuf));
    if (rc != 0) {
        reply_err(peer, req, LCTL_E_INTERNAL, errbuf);
        return;
    }
    reply_ok(peer, req);
}

static void handle_stop(catbsd_xpc_connection_t peer,
                        catbsd_xpc_object_t req,
                        launch_registry_t *reg)
{
    const char *label = catbsd_xpc_dictionary_get_string(req, LCTL_KEY_LABEL);
    if (!label || !*label) {
        reply_err(peer, req, LCTL_E_BADARG, "missing 'label'");
        return;
    }
    launch_job_t *job = launch_registry_find(reg, label);
    if (!job) {
        reply_err(peer, req, LCTL_E_NOTFOUND, "no such job");
        return;
    }
    if (launch_job_get_state(job) == 4 /* LAUNCH_STATE_STOPPED */) {
        reply_err(peer, req, LCTL_E_ALREADY, "job is already stopped");
        return;
    }

    /* 5 s grace period before SIGKILL */
    int rc = launch_job_stop(reg, job, 5000);
    if (rc != 0) {
        reply_err(peer, req, LCTL_E_INTERNAL, "stop failed");
        return;
    }
    reply_ok(peer, req);
}

/* -------------------------------------------------------------------------
 * Dispatch table
 * ------------------------------------------------------------------------- */

typedef void (*verb_handler_t)(catbsd_xpc_connection_t,
                               catbsd_xpc_object_t,
                               launch_registry_t *);

static const struct { const char *verb; verb_handler_t fn; } dispatch[] = {
    { LCTL_VERB_LIST,   handle_list   },
    { LCTL_VERB_STATUS, handle_status },
    { LCTL_VERB_LOAD,   handle_load   },
    { LCTL_VERB_UNLOAD, handle_unload },
    { LCTL_VERB_START,  handle_start  },
    { LCTL_VERB_STOP,   handle_stop   },
    { NULL, NULL }
};

/* -------------------------------------------------------------------------
 * Handle one request on `peer`
 * ------------------------------------------------------------------------- */
static void handle_request(lctl_server_t *srv,
                           catbsd_xpc_connection_t peer)
{
    int err = 0;
    catbsd_xpc_object_t req = NULL;

    /* 5-second timeout for the client to send its request */
    int rc = catbsd_xpc_connection_receive_message(peer, &req, 5000);
    if (rc != CATBSD_XPC_SUCCESS || !req) {
        catbsd_xpc_connection_cancel(peer);
        catbsd_xpc_connection_release(peer);
        return;
    }
    (void)err;

    const char *verb = catbsd_xpc_dictionary_get_string(req, LCTL_KEY_VERB);
    if (!verb || !*verb) {
        reply_err(peer, req, LCTL_E_BADVERB, "missing 'verb'");
        goto done;
    }

    for (int i = 0; dispatch[i].verb; i++) {
        if (strcmp(verb, dispatch[i].verb) == 0) {
            dispatch[i].fn(peer, req, srv->reg);
            goto done;
        }
    }

    /* Unknown verb */
    char msg[128];
    snprintf(msg, sizeof(msg), "unknown verb '%s'", verb);
    reply_err(peer, req, LCTL_E_BADVERB, msg);

done:
    catbsd_xpc_release(req);
    catbsd_xpc_connection_cancel(peer);
    catbsd_xpc_connection_release(peer);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

lctl_server_t *lctl_server_create(launch_registry_t *reg,
                                  const char *service_name,
                                  char *errbuf, size_t errlen)
{
    lctl_server_t *srv = calloc(1, sizeof(*srv));
    if (!srv) {
        snprintf(errbuf, errlen, "out of memory");
        return NULL;
    }

    int xpc_err = 0;
    srv->listener = catbsd_xpc_connection_create_listener(service_name,
                                                          &xpc_err);
    if (!srv->listener) {
        snprintf(errbuf, errlen, "cannot bind '%s': %s",
                 service_name, catbsd_xpc_strerror(xpc_err));
        free(srv);
        return NULL;
    }

    srv->reg = reg;
    snprintf(srv->socket_path, sizeof(srv->socket_path), "%s",
             catbsd_xpc_connection_get_name(srv->listener));
    return srv;
}

int lctl_server_poll(lctl_server_t *srv, int timeout_ms)
{
    int err = 0;
    catbsd_xpc_connection_t peer =
        catbsd_xpc_connection_accept(srv->listener, timeout_ms, &err);

    if (!peer) {
        if (err == CATBSD_XPC_ETIMEOUT)
            return 0;   /* nothing arrived — caller can keep looping */
        return -1;      /* fatal listener error */
    }

    handle_request(srv, peer);
    /* peer is released inside handle_request */
    return 1;
}

void lctl_server_destroy(lctl_server_t *srv)
{
    if (!srv) return;
    catbsd_xpc_connection_cancel(srv->listener);
    catbsd_xpc_connection_release(srv->listener);
    free(srv);
}

const char *lctl_server_socket_path(const lctl_server_t *srv)
{
    return srv ? srv->socket_path : NULL;
}
