/*
 * catbsd-launchctl — command-line interface to catbsd-launchd
 *
 * Usage:
 *   catbsd-launchctl list
 *   catbsd-launchctl load   <path/to/job.plist>
 *   catbsd-launchctl unload <label>
 *   catbsd-launchctl start  <label>
 *   catbsd-launchctl stop   <label>
 *   catbsd-launchctl status <label>
 *
 * Connects to the running daemon, sends one request, prints the result,
 * and exits.  The service name (and thus socket path) is resolved by the
 * XPC shim; override with CATBSD_XPC_RUNTIME_DIR if needed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shims/xpc_shim.h"
#include "../launchd/launchctl_proto.h"

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s <subcommand> [args]\n"
        "\n"
        "Subcommands:\n"
        "  list                    List all loaded jobs\n"
        "  load   <path>           Load a job plist\n"
        "  unload <label>          Unload (stop + remove) a job\n"
        "  start  <label>          Start a loaded job\n"
        "  stop   <label>          Stop a running job\n"
        "  status <label>          Show detailed job status\n"
        "\n"
        "Environment:\n"
        "  CATBSD_XPC_RUNTIME_DIR  Directory for XPC socket files\n"
        "                          (default: /tmp/catbsd-xpc-<uid>)\n",
        argv0);
}

/* Connect, send msg, receive reply.  Caller owns *reply_out on success. */
static int roundtrip(catbsd_xpc_object_t msg, catbsd_xpc_object_t *reply_out)
{
    int err = 0;
    catbsd_xpc_connection_t conn =
        catbsd_xpc_connection_create(LCTL_SERVICE_NAME, &err);
    if (!conn) {
        fprintf(stderr, "error: cannot connect to catbsd-launchd: %s\n"
                        "       (is the daemon running?)\n",
                catbsd_xpc_strerror(err));
        return 1;
    }

    /* 10-second timeout for the daemon to respond */
    err = catbsd_xpc_connection_send_message_with_reply_sync(
            conn, msg, 10000, reply_out);

    catbsd_xpc_connection_cancel(conn);
    catbsd_xpc_connection_release(conn);

    if (err != CATBSD_XPC_SUCCESS) {
        fprintf(stderr, "error: no reply from daemon: %s\n",
                catbsd_xpc_strerror(err));
        return 1;
    }
    return 0;
}

/* Check LCTL_KEY_CODE in a reply and print the error string if non-zero. */
static int check_reply(catbsd_xpc_object_t rep)
{
    int64_t code = 0;
    catbsd_xpc_dictionary_get_int64(rep, LCTL_KEY_CODE, &code);
    if (code != LCTL_OK) {
        const char *msg = catbsd_xpc_dictionary_get_string(rep, LCTL_KEY_ERROR);
        fprintf(stderr, "error: %s\n", msg && *msg ? msg : "(unknown error)");
        return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Subcommand implementations
 * ------------------------------------------------------------------------- */

static int cmd_list(void)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB, LCTL_VERB_LIST);

    catbsd_xpc_object_t rep = NULL;
    int rc = roundtrip(req, &rep);
    catbsd_xpc_release(req);
    if (rc) return rc;

    rc = check_reply(rep);
    if (rc == 0) {
        const char *jobs = catbsd_xpc_dictionary_get_string(rep, LCTL_KEY_JOBS);
        if (jobs) printf("%s", jobs);
    }
    catbsd_xpc_release(rep);
    return rc;
}

static int cmd_load(const char *path)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB, LCTL_VERB_LOAD);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_PATH, path);

    catbsd_xpc_object_t rep = NULL;
    int rc = roundtrip(req, &rep);
    catbsd_xpc_release(req);
    if (rc) return rc;

    rc = check_reply(rep);
    if (rc == 0) printf("loaded %s\n", path);
    catbsd_xpc_release(rep);
    return rc;
}

static int cmd_with_label(const char *verb, const char *label)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB,  verb);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_LABEL, label);

    catbsd_xpc_object_t rep = NULL;
    int rc = roundtrip(req, &rep);
    catbsd_xpc_release(req);
    if (rc) return rc;

    rc = check_reply(rep);
    catbsd_xpc_release(rep);
    if (rc == 0) printf("%s: %s\n", verb, label);
    return rc;
}

static int cmd_status(const char *label)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB,  LCTL_VERB_STATUS);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_LABEL, label);

    catbsd_xpc_object_t rep = NULL;
    int rc = roundtrip(req, &rep);
    catbsd_xpc_release(req);
    if (rc) return rc;

    rc = check_reply(rep);
    if (rc == 0) {
        const char *state = catbsd_xpc_dictionary_get_string(rep, LCTL_KEY_STATE);
        int64_t pid = 0, exit_code = 0;
        uint64_t starts = 0;
        catbsd_xpc_dictionary_get_int64(rep,  LCTL_KEY_PID,    &pid);
        catbsd_xpc_dictionary_get_uint64(rep, LCTL_KEY_STARTS, &starts);
        catbsd_xpc_dictionary_get_int64(rep,  LCTL_KEY_EXIT,   &exit_code);

        printf("label:  %s\n", label);
        printf("state:  %s\n", state ? state : "unknown");
        if (pid > 0)
            printf("pid:    %lld\n", (long long)pid);
        else
            printf("pid:    -\n");
        printf("starts: %llu\n", (unsigned long long)starts);
        if (starts > 0) {
            if (exit_code < 0)
                printf("exit:   signal %lld\n", (long long)-exit_code);
            else
                printf("exit:   %lld\n", (long long)exit_code);
        }
    }
    catbsd_xpc_release(rep);
    return rc;
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *sub = argv[1];

    if (strcmp(sub, "list") == 0) {
        if (argc != 2) { usage(argv[0]); return 1; }
        return cmd_list();
    }
    if (strcmp(sub, "load") == 0) {
        if (argc != 3) { fprintf(stderr, "load requires a path\n"); return 1; }
        return cmd_load(argv[2]);
    }
    if (strcmp(sub, "unload") == 0 ||
        strcmp(sub, "start")  == 0 ||
        strcmp(sub, "stop")   == 0) {
        if (argc != 3) { fprintf(stderr, "%s requires a label\n", sub); return 1; }
        return cmd_with_label(sub, argv[2]);
    }
    if (strcmp(sub, "status") == 0) {
        if (argc != 3) { fprintf(stderr, "status requires a label\n"); return 1; }
        return cmd_status(argv[2]);
    }

    fprintf(stderr, "unknown subcommand '%s'\n\n", sub);
    usage(argv[0]);
    return 1;
}
