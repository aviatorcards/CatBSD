/*
 * catbsd-init — CatBSD PID 1 / service supervisor
 *
 * Responsibilities:
 *   1. Read boot configuration (catbsd-init.conf or env overrides)
 *   2. Optionally mount essential filesystems (proc, devfs) — skipped
 *      when not running as PID 1 so local testing always works
 *   3. Load daemon plists from DaemonDirectory
 *   4. Start all RunAtLoad jobs
 *   5. Open the launchctl control socket
 *   6. Run the main event loop:
 *        - poll for one launchctl request (non-blocking if any arrived)
 *        - tick the job supervisor (reap + restart children)
 *        - repeat
 *   7. On SIGTERM / SIGINT: stop all jobs then exit cleanly
 *   8. On SIGHUP: reload the daemon directory (load new plists)
 *
 * Single-user mode (CATBSD_INIT_SINGLE_USER=1 or SingleUser=true in conf):
 *   Skips steps 3–5 and exec's SingleUserShell directly.  The shell runs
 *   as a child of this process; when it exits, catbsd-init exits too.
 *
 * Non-PID-1 usage (development / testing):
 *   All behaviour is identical except filesystem mounts are skipped.
 *   Use CATBSD_INIT_DAEMON_DIR to point at a test directory.
 *
 *   CATBSD_INIT_DAEMON_DIR=./etc/catbsd/daemons ./catbsd-init
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * We sit above src/darwin-compat/, so include paths go through there.
 * When installed the headers would be in standard locations; here we
 * use relative paths matching the source tree layout.
 */
#include "../darwin-compat/launchd/launch_job.h"
#include "../darwin-compat/launchd/launchctl_server.h"
#include "../darwin-compat/launchd/launchctl_proto.h"
#include "../darwin-compat/launchd/plist_lite.h"

/* -------------------------------------------------------------------------
 * Logging
 * ------------------------------------------------------------------------- */

static int g_log_level = 1; /* 0=quiet 1=normal 2=verbose 3=debug */

#define LOG_INFO(fmt, ...)  do { if (g_log_level >= 1) \
    fprintf(stderr, "[catbsd-init] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_VERB(fmt, ...)  do { if (g_log_level >= 2) \
    fprintf(stderr, "[catbsd-init] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_DBG(fmt, ...)   do { if (g_log_level >= 3) \
    fprintf(stderr, "[catbsd-init] DEBUG " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_ERR(fmt, ...)   fprintf(stderr, "[catbsd-init] ERROR " fmt "\n", ##__VA_ARGS__)

/* -------------------------------------------------------------------------
 * Boot configuration
 * ------------------------------------------------------------------------- */

typedef struct {
    char daemon_dir[512];
    char control_socket[512];  /* empty = use XPC shim default */
    char single_user_shell[256];
    int  single_user;
    int  shutdown_timeout_ms;
    int  log_level;
} init_config_t;

static void config_defaults(init_config_t *cfg)
{
    strncpy(cfg->daemon_dir,        "/etc/catbsd/daemons", sizeof(cfg->daemon_dir) - 1);
    cfg->control_socket[0]        = '\0';
    strncpy(cfg->single_user_shell, "/bin/sh",             sizeof(cfg->single_user_shell) - 1);
    cfg->single_user              = 0;
    cfg->shutdown_timeout_ms      = 5000;
    cfg->log_level                = 1;
}

/*
 * Read catbsd-init.conf from `path` and merge into `cfg`.
 * Missing keys leave the defaults untouched.  A missing file is not an
 * error — defaults are fine.
 */
static void config_load_file(init_config_t *cfg, const char *path)
{
    char errbuf[256];
    plist_value_t *root = plist_parse_file(path, errbuf, sizeof(errbuf));
    if (!root) return; /* file absent or unreadable — use defaults */

    const char *s;
    int64_t     i;

    s = plist_dict_get_string(root, "DaemonDirectory", NULL);
    if (s) strncpy(cfg->daemon_dir, s, sizeof(cfg->daemon_dir) - 1);

    s = plist_dict_get_string(root, "ControlSocket", NULL);
    if (s) strncpy(cfg->control_socket, s, sizeof(cfg->control_socket) - 1);

    s = plist_dict_get_string(root, "SingleUserShell", NULL);
    if (s) strncpy(cfg->single_user_shell, s, sizeof(cfg->single_user_shell) - 1);

    /* SingleUser: bool true or integer non-zero */
    if (plist_dict_get_bool(root, "SingleUser", 0))
        cfg->single_user = 1;

    i = plist_dict_get_integer(root, "ShutdownTimeoutMs", cfg->shutdown_timeout_ms);
    cfg->shutdown_timeout_ms = (int)i;

    i = plist_dict_get_integer(root, "LogLevel", cfg->log_level);
    cfg->log_level = (int)i;

    plist_free(root);
}

/* Environment variable overrides — CATBSD_INIT_<KEY> */
static void config_apply_env(init_config_t *cfg)
{
    const char *v;

    v = getenv("CATBSD_INIT_DAEMON_DIR");
    if (v && *v) strncpy(cfg->daemon_dir, v, sizeof(cfg->daemon_dir) - 1);

    v = getenv("CATBSD_INIT_CONTROL_SOCKET");
    if (v && *v) strncpy(cfg->control_socket, v, sizeof(cfg->control_socket) - 1);

    v = getenv("CATBSD_INIT_SINGLE_USER");
    if (v && *v && atoi(v)) cfg->single_user = 1;

    v = getenv("CATBSD_INIT_SINGLE_USER_SHELL");
    if (v && *v) strncpy(cfg->single_user_shell, v, sizeof(cfg->single_user_shell) - 1);

    v = getenv("CATBSD_INIT_LOG_LEVEL");
    if (v && *v) cfg->log_level = atoi(v);
}

/* -------------------------------------------------------------------------
 * Signal handling
 *
 * PID 1 is special: unhandled signals are silently ignored rather than
 * acting on their default disposition.  We must explicitly install
 * handlers for everything we care about.
 *
 * We use a volatile flag set in the handler and checked in the main loop
 * rather than doing real work inside the handler.  Signal handlers that
 * call non-async-signal-safe functions are a common source of hard bugs.
 * ------------------------------------------------------------------------- */

static volatile sig_atomic_t g_do_shutdown = 0;  /* SIGTERM / SIGINT */
static volatile sig_atomic_t g_do_reload   = 0;  /* SIGHUP           */

static void handle_shutdown(int sig) { (void)sig; g_do_shutdown = 1; }
static void handle_reload(int sig)   { (void)sig; g_do_reload   = 1; }

static void setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = handle_shutdown;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    sa.sa_handler = handle_reload;
    sigaction(SIGHUP, &sa, NULL);

    /* SIGCHLD: leave as SIG_DFL so the kernel queues the notification;
     * launch_registry_tick() calls waitpid() per job and handles reaping.
     * We never call waitpid(-1) so we don't compete with the registry. */
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);

    /* Ignore SIGPIPE: a closed launchctl connection must not kill init. */
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}

/* -------------------------------------------------------------------------
 * Filesystem mounts (only when running as PID 1)
 * These are best-effort: failures are logged but don't abort boot.
 * ------------------------------------------------------------------------- */

static void mount_essential(void)
{
    /* Only attempt mounts when we are the init process. */
    if (getpid() != 1) {
        LOG_VERB("not PID 1, skipping filesystem mounts");
        return;
    }

    LOG_INFO("mounting essential filesystems");

    /* proc(5) — process information */
    if (system("mount -t procfs proc /proc 2>/dev/null") != 0)
        LOG_VERB("  /proc mount skipped (may already be mounted)");
    else
        LOG_VERB("  /proc mounted");

    /* devfs(5) — device nodes */
    if (system("mount -t devfs devfs /dev 2>/dev/null") != 0)
        LOG_VERB("  /dev mount skipped (may already be mounted)");
    else
        LOG_VERB("  /dev mounted");
}

/* -------------------------------------------------------------------------
 * Single-user mode
 *
 * exec() the shell directly so it runs as a child of catbsd-init.
 * When the shell exits we exit too — single-user mode is a maintenance
 * shell, not a full boot.
 * ------------------------------------------------------------------------- */

static int run_single_user(const char *shell)
{
    LOG_INFO("entering single-user mode: %s", shell);
    fprintf(stderr,
        "\n"
        "  ╔══════════════════════════════════════════╗\n"
        "  ║   CatBSD — Single-User Mode              ║\n"
        "  ║   Type 'exit' to shut down.              ║\n"
        "  ╚══════════════════════════════════════════╝\n"
        "\n");

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERR("fork: %s", strerror(errno));
        return 1;
    }
    if (pid == 0) {
        /* child — exec the shell */
        execl(shell, shell, "--login", (char *)NULL);
        /* if exec fails try a bare /bin/sh */
        execl("/bin/sh", "/bin/sh", (char *)NULL);
        _exit(127);
    }

    /* parent — wait for the shell to exit */
    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;

    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    LOG_INFO("single-user shell exited (status %d), shutting down", code);
    return code;
}

/* -------------------------------------------------------------------------
 * Shutdown
 * ------------------------------------------------------------------------- */

static void do_shutdown(launch_registry_t *reg, lctl_server_t *srv,
                        int timeout_ms)
{
    LOG_INFO("shutting down — stopping all jobs");
    if (srv) lctl_server_destroy(srv);

    int stopped = launch_registry_stop_all(reg, timeout_ms);
    LOG_INFO("%d job(s) stopped", stopped);

    launch_registry_free(reg);
    LOG_INFO("goodbye 🐾");
}

/* -------------------------------------------------------------------------
 * Reload — load any new plists added to DaemonDirectory since boot.
 * Jobs already in the registry are unchanged (duplicate labels are
 * rejected by launch_registry_load_plist, which is fine).
 * ------------------------------------------------------------------------- */

static void do_reload(launch_registry_t *reg, const char *daemon_dir)
{
    LOG_INFO("reloading daemon directory: %s", daemon_dir);
    int failed = 0;
    char errbuf[512];
    int loaded = launch_registry_load_dir(reg, daemon_dir,
                                          &failed, errbuf, sizeof(errbuf));
    if (loaded < 0)
        LOG_ERR("reload failed: %s", errbuf);
    else
        LOG_INFO("reload: %d new job(s) loaded, %d skipped/failed",
                 loaded, failed);
}

/* -------------------------------------------------------------------------
 * Startup banner
 * ------------------------------------------------------------------------- */

static void print_banner(void)
{
    fprintf(stderr,
        "\n"
        "  ╔══════════════════════════════════════════╗\n"
        "  ║   CatBSD — Purring to Life  🐾            ║\n"
        "  ║   catbsd-init v0.2.0-alpha               ║\n"
        "  ╚══════════════════════════════════════════╝\n"
        "\n");
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    /* ---- 1. Load configuration ---------------------------------------- */
    init_config_t cfg;
    config_defaults(&cfg);

    /* Config file: first argument or well-known path */
    const char *conf_path = "/etc/catbsd/catbsd-init.conf";
    if (argc >= 2 && argv[1][0] != '-') conf_path = argv[1];

    config_load_file(&cfg, conf_path);
    config_apply_env(&cfg);   /* env overrides always win */

    /* -s flag → single-user */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--single-user") == 0)
            cfg.single_user = 1;
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            cfg.log_level = 2;
    }

    g_log_level = cfg.log_level;

    print_banner();
    LOG_INFO("PID %d, daemon_dir=%s, single_user=%d",
             (int)getpid(), cfg.daemon_dir, cfg.single_user);

    /* ---- 2. Signal handlers ------------------------------------------- */
    setup_signals();

    /* ---- 3. Mount essential filesystems (PID 1 only) ------------------ */
    mount_essential();

    /* ---- 4. Single-user mode ------------------------------------------ */
    if (cfg.single_user)
        return run_single_user(cfg.single_user_shell);

    /* ---- 5. Create job registry --------------------------------------- */
    launch_registry_t *reg = launch_registry_create();
    if (!reg) { LOG_ERR("launch_registry_create failed"); return 1; }

    /* ---- 6. Load daemon directory ------------------------------------- */
    LOG_INFO("loading daemons from %s", cfg.daemon_dir);
    int failed = 0;
    char errbuf[512];
    int loaded = launch_registry_load_dir(reg, cfg.daemon_dir,
                                          &failed, errbuf, sizeof(errbuf));
    if (loaded < 0) {
        LOG_ERR("cannot read daemon directory: %s", errbuf);
        /* Non-fatal: proceed with an empty registry.  On a fresh install
         * the directory may not exist yet. */
    } else {
        LOG_INFO("loaded %d job(s), %d skipped/failed", loaded, failed);
    }

    /* ---- 7. Start RunAtLoad jobs -------------------------------------- */
    int started = launch_registry_start_all(reg);
    LOG_INFO("started %d RunAtLoad job(s)", started);

    /* ---- 8. Open launchctl control socket ----------------------------- */
    const char *svc_name = (cfg.control_socket[0] != '\0')
                           ? cfg.control_socket
                           : LCTL_SERVICE_NAME;

    lctl_server_t *srv = lctl_server_create(reg, svc_name,
                                             errbuf, sizeof(errbuf));
    if (!srv) {
        LOG_ERR("cannot open control socket '%s': %s", svc_name, errbuf);
        LOG_ERR("continuing without launchctl support");
        srv = NULL;
    } else {
        LOG_INFO("control socket: %s", lctl_server_socket_path(srv));
        LOG_INFO("ready — use 'catbsd-launchctl list' to inspect jobs");
    }

    /* ---- 9. Main event loop ------------------------------------------- */
    LOG_INFO("entering supervision loop");

    while (!g_do_shutdown) {
        /* Handle SIGHUP (reload) */
        if (g_do_reload) {
            g_do_reload = 0;
            do_reload(reg, cfg.daemon_dir);
        }

        /* Handle one launchctl request (returns immediately if none waiting) */
        if (srv) lctl_server_poll(srv, 0);

        /* Reap exited children and apply KeepAlive restart policy */
        int changed = launch_registry_tick(reg);
        if (changed > 0)
            LOG_VERB("%d job state change(s)", changed);

        /* Sleep 10 ms between ticks — low enough for snappy launchctl
         * responses, low CPU when idle */
        usleep(10000);
    }

    /* ---- 10. Shutdown ------------------------------------------------- */
    LOG_INFO("SIGTERM/SIGINT received — beginning shutdown");
    do_shutdown(reg, srv, cfg.shutdown_timeout_ms);

    return 0;
}
