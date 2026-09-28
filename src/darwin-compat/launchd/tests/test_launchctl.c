/*
 * test_launchctl.c — integration test for the launchctl control protocol
 *
 * Spins up an lctl_server_t on a private service name, runs a small
 * daemon-side event loop in a background thread, then exercises all six
 * verbs (list, load, start, stop, status, unload) from the client side.
 *
 * Uses /bin/sh children so the supervision side is real — a job loaded
 * via the control channel should actually fork and execute.
 */

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../shims/xpc_shim.h"
#include "launch_job.h"
#include "launchctl_proto.h"
#include "launchctl_server.h"
#include "plist_lite.h"

#define OK "  \xE2\x9C\x93 "

/* -------------------------------------------------------------------------
 * Daemon-side thread
 * ------------------------------------------------------------------------- */

typedef struct {
    launch_registry_t *reg;
    lctl_server_t     *srv;
    volatile int       stop;
} daemon_ctx_t;

static void *daemon_thread(void *arg)
{
    daemon_ctx_t *ctx = arg;
    while (!ctx->stop) {
        lctl_server_poll(ctx->srv, 20);   /* handle one request per tick */
        launch_registry_tick(ctx->reg);
        usleep(10000);                     /* 10 ms tick */
    }
    return NULL;
}

/* -------------------------------------------------------------------------
 * Client helpers
 * ------------------------------------------------------------------------- */

static catbsd_xpc_object_t client_roundtrip(const char *svc,
                                             catbsd_xpc_object_t req)
{
    int err = 0;
    catbsd_xpc_connection_t conn =
        catbsd_xpc_connection_create(svc, &err);
    assert(conn != NULL);

    catbsd_xpc_object_t rep = NULL;
    int rc = catbsd_xpc_connection_send_message_with_reply_sync(
                 conn, req, 5000, &rep);
    assert(rc == CATBSD_XPC_SUCCESS);
    assert(rep != NULL);

    catbsd_xpc_connection_cancel(conn);
    catbsd_xpc_connection_release(conn);
    return rep;
}

/* Returns the LCTL_KEY_CODE from a reply. */
static int64_t reply_code(catbsd_xpc_object_t rep)
{
    int64_t code = 0;
    catbsd_xpc_dictionary_get_int64(rep, LCTL_KEY_CODE, &code);
    return code;
}

/* -------------------------------------------------------------------------
 * Plist helpers
 * ------------------------------------------------------------------------- */

static char g_tmpdir[64];

static void make_tmpdir(void)
{
    snprintf(g_tmpdir, sizeof(g_tmpdir),
             "/tmp/catbsd-launchctl-test-%ld", (long)getpid());
    mkdir(g_tmpdir, 0700);
}

static void write_plist(char *out, size_t olen,
                        const char *name, const char *body)
{
    snprintf(out, olen, "%s/%s.plist", g_tmpdir, name);
    FILE *f = fopen(out, "w");
    assert(f);
    fprintf(f,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict>\n%s</dict></plist>\n", body);
    fclose(f);
}

/* -------------------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------------------- */

static void test_list_empty(const char *svc)
{
    printf("Testing list on empty registry...\n");

    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB, LCTL_VERB_LIST);
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);

    assert(reply_code(rep) == LCTL_OK);
    const char *jobs = catbsd_xpc_dictionary_get_string(rep, LCTL_KEY_JOBS);
    assert(jobs && strstr(jobs, "no jobs") != NULL);
    printf(OK "empty registry reported '(no jobs loaded)'\n");
    catbsd_xpc_release(rep);
}

static void test_load(const char *svc, const char *path,
                      const char *label, int expect_ok)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB, LCTL_VERB_LOAD);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_PATH, path);
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);

    int64_t code = reply_code(rep);
    if (expect_ok) {
        assert(code == LCTL_OK);
        printf(OK "loaded %s\n", label);
    } else {
        assert(code != LCTL_OK);
        printf(OK "bad plist rejected (code %lld)\n", (long long)code);
    }
    catbsd_xpc_release(rep);
}

static void test_list_has_label(const char *svc, const char *label)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB, LCTL_VERB_LIST);
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);

    assert(reply_code(rep) == LCTL_OK);
    const char *jobs = catbsd_xpc_dictionary_get_string(rep, LCTL_KEY_JOBS);
    assert(jobs && strstr(jobs, label) != NULL);
    printf(OK "list contains '%s'\n", label);
    catbsd_xpc_release(rep);
}

static void test_start(const char *svc, const char *label)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB,  LCTL_VERB_START);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_LABEL, label);
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);
    assert(reply_code(rep) == LCTL_OK);
    printf(OK "start '%s' ok\n", label);
    catbsd_xpc_release(rep);
}

static void test_status_state(const char *svc, const char *label,
                              const char *expected_state)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB,  LCTL_VERB_STATUS);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_LABEL, label);
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);

    assert(reply_code(rep) == LCTL_OK);
    const char *state = catbsd_xpc_dictionary_get_string(rep, LCTL_KEY_STATE);
    assert(state && strcmp(state, expected_state) == 0);
    printf(OK "status '%s' = %s\n", label, state);
    catbsd_xpc_release(rep);
}

static void test_stop(const char *svc, const char *label)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB,  LCTL_VERB_STOP);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_LABEL, label);
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);
    assert(reply_code(rep) == LCTL_OK);
    printf(OK "stop '%s' ok\n", label);
    catbsd_xpc_release(rep);
}

static void test_unload(const char *svc, const char *label)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB,  LCTL_VERB_UNLOAD);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_LABEL, label);
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);
    assert(reply_code(rep) == LCTL_OK);
    printf(OK "unload '%s' ok\n", label);
    catbsd_xpc_release(rep);
}

static void test_notfound(const char *svc, const char *verb,
                          const char *label)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB,  verb);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_LABEL, label);
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);
    assert(reply_code(rep) == LCTL_E_NOTFOUND);
    printf(OK "'%s %s' on unknown label → NOTFOUND\n", verb, label);
    catbsd_xpc_release(rep);
}

static void test_badverb(const char *svc)
{
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB, "meow");
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);
    assert(reply_code(rep) == LCTL_E_BADVERB);
    printf(OK "unknown verb 'meow' → BADVERB\n");
    catbsd_xpc_release(rep);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

int main(void)
{
    /* Ignore SIGPIPE so a closed client socket doesn't kill the test */
    signal(SIGPIPE, SIG_IGN);

    make_tmpdir();

    /* Use a private service name so parallel test runs don't collide */
    char svc[64];
    snprintf(svc, sizeof(svc), "com.catbsd.launchd.test.%ld", (long)getpid());

    printf("=== CatBSD launchctl Control Protocol Test ===\n\n");
    printf("Service: %s\n\n", svc);

    /* --- Daemon side --------------------------------------------------- */
    launch_registry_t *reg = launch_registry_create();
    assert(reg != NULL);
    /* Fast throttle so KeepAlive jobs restart quickly in tests */
    launch_registry_set_default_throttle(reg, 50);

    char errbuf[512];
    lctl_server_t *srv = lctl_server_create(reg, svc, errbuf, sizeof(errbuf));
    if (!srv) { fprintf(stderr, "lctl_server_create: %s\n", errbuf); return 1; }
    printf("Daemon listening on %s\n\n", lctl_server_socket_path(srv));

    daemon_ctx_t ctx = { .reg = reg, .srv = srv, .stop = 0 };
    pthread_t tid;
    pthread_create(&tid, NULL, daemon_thread, &ctx);

    /* --- Tests --------------------------------------------------------- */

    printf("Testing empty registry...\n");
    test_list_empty(svc);
    printf("\n");

    printf("Testing error handling...\n");
    test_badverb(svc);
    test_notfound(svc, LCTL_VERB_START,  "com.catbsd.nosuchjob");
    test_notfound(svc, LCTL_VERB_STOP,   "com.catbsd.nosuchjob");
    test_notfound(svc, LCTL_VERB_STATUS, "com.catbsd.nosuchjob");

    /* load a bad plist */
    char bad_path[128];
    write_plist(bad_path, sizeof(bad_path), "bad",
        "<!-- missing Label -->\n"
        "<key>ProgramArguments</key><array>"
        "<string>/bin/sh</string></array>\n");
    test_load(svc, bad_path, "bad", 0);
    printf("  All error-handling tests passed!\n\n");

    /* --- Load a sleeping job ------------------------------------------ */
    printf("Testing load / list / status...\n");
    char sleep_path[128];
    write_plist(sleep_path, sizeof(sleep_path), "sleeper",
        "<key>Label</key><string>com.catbsd.sleeper</string>\n"
        "<key>ProgramArguments</key><array>"
        "<string>/bin/sh</string>"
        "<string>-c</string>"
        "<string>sleep 60</string></array>\n");
    test_load(svc, sleep_path, "com.catbsd.sleeper", 1);
    test_list_has_label(svc, "com.catbsd.sleeper");
    test_status_state(svc, "com.catbsd.sleeper", "loaded");
    printf("  All load/list/status tests passed!\n\n");

    /* --- Start / stop -------------------------------------------------- */
    printf("Testing start / stop...\n");
    test_start(svc, "com.catbsd.sleeper");
    usleep(150000);  /* let the child get going */
    test_status_state(svc, "com.catbsd.sleeper", "running");
    test_stop(svc, "com.catbsd.sleeper");
    usleep(150000);  /* let the supervisor reap */
    test_status_state(svc, "com.catbsd.sleeper", "stopped");
    printf("  All start/stop tests passed!\n\n");

    /* --- Unload -------------------------------------------------------- */
    printf("Testing unload...\n");
    test_unload(svc, "com.catbsd.sleeper");
    /* After unload, status should return NOTFOUND */
    test_notfound(svc, LCTL_VERB_STATUS, "com.catbsd.sleeper");
    printf("  All unload tests passed!\n\n");

    /* --- KeepAlive job: load, start, observe restart, stop ------------- */
    printf("Testing KeepAlive job lifecycle...\n");
    char ka_path[128];
    write_plist(ka_path, sizeof(ka_path), "keepalive",
        "<key>Label</key><string>com.catbsd.keepalive</string>\n"
        "<key>ProgramArguments</key><array>"
        "<string>/bin/sh</string>"
        "<string>-c</string>"
        "<string>exit 1</string></array>\n"
        "<key>KeepAlive</key><true/>\n");
    test_load(svc, ka_path, "com.catbsd.keepalive", 1);
    test_start(svc, "com.catbsd.keepalive");
    usleep(400000);  /* let it restart a few times */

    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_VERB,  LCTL_VERB_STATUS);
    catbsd_xpc_dictionary_set_string(req, LCTL_KEY_LABEL, "com.catbsd.keepalive");
    catbsd_xpc_object_t rep = client_roundtrip(svc, req);
    catbsd_xpc_release(req);
    assert(reply_code(rep) == LCTL_OK);
    uint64_t starts = 0;
    catbsd_xpc_dictionary_get_uint64(rep, LCTL_KEY_STARTS, &starts);
    assert(starts >= 2);
    printf(OK "KeepAlive job restarted %llu times (expected ≥ 2)\n",
           (unsigned long long)starts);
    catbsd_xpc_release(rep);

    test_stop(svc, "com.catbsd.keepalive");
    usleep(150000);
    test_status_state(svc, "com.catbsd.keepalive", "stopped");
    printf("  All KeepAlive tests passed!\n\n");

    /* --- Shutdown ------------------------------------------------------ */
    ctx.stop = 1;
    pthread_join(tid, NULL);
    lctl_server_destroy(srv);
    launch_registry_stop_all(reg, 2000);
    launch_registry_free(reg);

    printf("=== All tests passed! ===\n\n");
    printf("launchctl verbs list/load/start/stop/status/unload\n"
           "over XPC — six verbs, real supervised children.\n");
    return 0;
}
