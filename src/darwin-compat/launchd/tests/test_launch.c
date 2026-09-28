/*
 * Test program for CatBSD liblaunch
 *
 * These tests spawn real child processes with /bin/sh, because the
 * behaviour worth proving -- that a KeepAlive job comes back, that a
 * throttle actually delays it, that a stop wins over a restart policy --
 * only exists once there is a process to supervise.
 */

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "launch_job.h"
#include "plist_lite.h"

#define OK "  \xE2\x9C\x93 "

/* Kept short on purpose: these paths become AF_UNIX socket paths, and
 * sun_path is only 104-108 bytes. */
static char g_tmpdir[64];

static int64_t mono_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void make_tmpdir(void) {
  snprintf(g_tmpdir, sizeof(g_tmpdir), "/tmp/catbsd-launch-test-%ld",
           (long)getpid());
  mkdir(g_tmpdir, 0700);
}

static void write_file(const char *path, const char *content) {
  FILE *f = fopen(path, "w");
  assert(f != NULL);
  fputs(content, f);
  fclose(f);
}

/* Build a job plist in the temp dir and return its path in `out`. */
static void write_plist(char *out, size_t outlen, const char *name,
                        const char *body) {
  char xml[4096];

  snprintf(out, outlen, "%s/%s.plist", g_tmpdir, name);
  snprintf(xml, sizeof(xml),
           "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
           "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
           "<plist version=\"1.0\">\n<dict>\n%s</dict>\n</plist>\n",
           body);
  write_file(out, xml);
}

/* ------------------------------------------------------------------ */

static void test_plist_parser(void) {
  printf("Testing the plist parser...\n");

  const char *xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
      "<!-- a comment that must be skipped -->\n"
      "<plist version=\"1.0\">\n"
      "<dict>\n"
      "  <key>Label</key><string>com.catbsd.meow</string>\n"
      "  <key>Escaped</key><string>a &amp; b &lt;c&gt; &quot;d&quot; "
      "&#233;</string>\n"
      "  <key>ProgramArguments</key>\n"
      "  <array>\n"
      "    <string>/usr/local/bin/meow</string>\n"
      "    <string>--loud</string>\n"
      "  </array>\n"
      "  <key>RunAtLoad</key><true/>\n"
      "  <key>Disabled</key><false/>\n"
      "  <key>ThrottleInterval</key><integer>7</integer>\n"
      "  <key>Ratio</key><real>0.25</real>\n"
      "  <key>Nested</key><dict><key>Inner</key><integer>-9</integer></dict>\n"
      "  <key>Empty</key><array/>\n"
      "  <key>Cookie</key><data>Y2F0</data>\n"
      "</dict>\n"
      "</plist>\n";

  char err[256];
  plist_value_t *root = plist_parse_string(xml, strlen(xml), err, sizeof(err));
  assert(root != NULL);
  assert(plist_type(root) == PLIST_DICT);

  assert(strcmp(plist_dict_get_string(root, "Label", ""), "com.catbsd.meow") ==
         0);
  /* Entity decoding, including a numeric reference encoded as UTF-8. */
  assert(strcmp(plist_dict_get_string(root, "Escaped", ""),
                "a & b <c> \"d\" \xC3\xA9") == 0);
  printf(OK "XML declaration, DOCTYPE and comments skipped; entities "
            "decoded (including &#233; as UTF-8)\n");

  const plist_value_t *args = plist_dict_get(root, "ProgramArguments");
  assert(plist_array_count(args) == 2);
  assert(strcmp(plist_string_value(plist_array_get(args, 1), ""), "--loud") ==
         0);

  assert(plist_dict_get_bool(root, "RunAtLoad", 0) == 1);
  assert(plist_dict_get_bool(root, "Disabled", 1) == 0);
  assert(plist_dict_get_integer(root, "ThrottleInterval", 0) == 7);
  assert(plist_real_value(plist_dict_get(root, "Ratio"), 0) == 0.25);
  assert(plist_dict_get_integer(plist_dict_get(root, "Nested"), "Inner", 0) ==
         -9);
  assert(plist_array_count(plist_dict_get(root, "Empty")) == 0);
  printf(OK "arrays, nested dicts, bools, integers, reals and a "
            "self-closing empty array all read back correctly\n");

  size_t dlen = 0;
  const unsigned char *data =
      plist_data_value(plist_dict_get(root, "Cookie"), &dlen);
  assert(data != NULL && dlen == 3 && memcmp(data, "cat", 3) == 0);
  printf(OK "<data> base64 decoded to %zu bytes\n", dlen);

  /* Missing keys and wrong types fall back rather than crashing. */
  assert(strcmp(plist_dict_get_string(root, "Nope", "fallback"), "fallback") ==
         0);
  assert(plist_dict_get_integer(root, "Label", 42) == 42);
  printf(OK "absent keys and type mismatches return the caller's "
            "fallback\n");

  plist_free(root);

  /* A malformed document must report where it gave up, not just fail. */
  const char *bad = "<plist version=\"1.0\">\n<dict>\n"
                    "  <key>Label</key><string>unterminated\n"
                    "</dict>\n</plist>\n";
  err[0] = '\0';
  assert(plist_parse_string(bad, strlen(bad), err, sizeof(err)) == NULL);
  assert(strstr(err, "line ") != NULL);
  printf(OK "a malformed plist fails with a located error: \"%s\"\n", err);

  printf("  All plist parser tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

static void test_job_loading(void) {
  printf("Testing job loading...\n");

  launch_registry_t *reg = launch_registry_create();
  assert(reg != NULL);

  char path[512], err[512];
  write_plist(path, sizeof(path), "loadable",
              "  <key>Label</key><string>com.catbsd.loadable</string>\n"
              "  <key>ProgramArguments</key>\n"
              "  <array><string>/bin/sh</string><string>-c</string>"
              "<string>true</string></array>\n"
              "  <key>RunAtLoad</key><true/>\n"
              "  <key>KeepAlive</key><true/>\n"
              "  <key>ThrottleInterval</key><integer>3</integer>\n"
              "  <key>EnvironmentVariables</key>\n"
              "  <dict><key>CATBSD_TEST</key><string>meow</string></dict>\n");

  launch_job_t *job = NULL;
  assert(launch_registry_load_plist(reg, path, &job, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(job != NULL);
  assert(strcmp(launch_job_get_label(job), "com.catbsd.loadable") == 0);
  assert(launch_job_get_run_at_load(job) == 1);
  assert(launch_job_get_keepalive(job) == LAUNCH_KEEPALIVE_ALWAYS);
  assert(launch_job_get_throttle(job) == 3000);
  assert(launch_job_argc(job) == 3);
  assert(strcmp(launch_job_argv_at(job, 0), "/bin/sh") == 0);
  assert(launch_job_get_state(job) == LAUNCH_STATE_LOADED);
  printf(OK "job loaded: label, argv, RunAtLoad, KeepAlive and a 3s "
            "ThrottleInterval all read from the plist\n");

  /* Loading the same label twice must be refused -- two supervisors for
   * one job is how you get an unkillable process. */
  assert(launch_registry_load_plist(reg, path, NULL, err, sizeof(err)) ==
         LAUNCH_EEXIST);
  printf(OK "a duplicate Label is refused: %s\n", err);

  /* A plist with no Label is not a job. */
  char bad_path[512];
  write_plist(bad_path, sizeof(bad_path), "nolabel",
              "  <key>ProgramArguments</key><array><string>/bin/true</string>"
              "</array>\n");
  assert(launch_registry_load_plist(reg, bad_path, NULL, err, sizeof(err)) ==
         LAUNCH_EPARSE);
  assert(strstr(err, "Label") != NULL);
  printf(OK "a plist without Label is rejected: %s\n", err);

  /* Nor is one with nothing to run. */
  char noprog[512];
  write_plist(noprog, sizeof(noprog), "noprog",
              "  <key>Label</key><string>com.catbsd.noprog</string>\n");
  assert(launch_registry_load_plist(reg, noprog, NULL, err, sizeof(err)) ==
         LAUNCH_EPARSE);
  printf(OK "a plist with neither Program nor ProgramArguments is "
            "rejected\n");

  /* KeepAlive as a dict selects the conditional forms. */
  char cond[512];
  write_plist(cond, sizeof(cond), "conditional",
              "  <key>Label</key><string>com.catbsd.conditional</string>\n"
              "  <key>ProgramArguments</key><array><string>/bin/true</string>"
              "</array>\n"
              "  <key>KeepAlive</key>\n"
              "  <dict><key>SuccessfulExit</key><false/></dict>\n");
  launch_job_t *cjob = NULL;
  assert(launch_registry_load_plist(reg, cond, &cjob, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(launch_job_get_keepalive(cjob) == LAUNCH_KEEPALIVE_ON_FAILURE);
  printf(OK "KeepAlive={SuccessfulExit:false} parsed as restart-on-failure\n");

  /* Loading a whole directory skips the broken ones and keeps going. */
  launch_registry_t *dir_reg = launch_registry_create();
  int failed = -1;
  int loaded = launch_registry_load_dir(dir_reg, g_tmpdir, &failed, err,
                                        sizeof(err));
  assert(loaded >= 2);
  assert(failed >= 2); /* nolabel and noprog */
  printf(OK "directory load found %d usable job(s) and skipped %d broken "
            "one(s)\n",
         loaded, failed);

  launch_registry_free(dir_reg);
  launch_registry_free(reg);
  printf("  All job loading tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

static void test_run_and_exit(void) {
  printf("Testing spawn, redirection and exit status...\n");

  launch_registry_t *reg = launch_registry_create();
  char outpath[512], path[512], body[1024], err[512];

  snprintf(outpath, sizeof(outpath), "%s/hello.out", g_tmpdir);
  snprintf(body, sizeof(body),
           "  <key>Label</key><string>com.catbsd.hello</string>\n"
           "  <key>ProgramArguments</key>\n"
           "  <array><string>/bin/sh</string><string>-c</string>"
           "<string>echo $CATBSD_GREETING; exit 0</string></array>\n"
           "  <key>StandardOutPath</key><string>%s</string>\n"
           "  <key>EnvironmentVariables</key>\n"
           "  <dict><key>CATBSD_GREETING</key><string>purr</string></dict>\n",
           outpath);
  write_plist(path, sizeof(path), "hello", body);

  launch_job_t *job = NULL;
  assert(launch_registry_load_plist(reg, path, &job, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(launch_job_start(reg, job, err, sizeof(err)) == LAUNCH_OK);
  assert(launch_job_get_state(job) == LAUNCH_STATE_RUNNING);
  assert(launch_job_get_pid(job) > 0);

  assert(launch_registry_wait_state(reg, job, LAUNCH_STATE_EXITED, 5000) ==
         LAUNCH_OK);
  assert(launch_job_get_last_exit(job) == 0);
  assert(!launch_job_was_signaled(job));
  assert(launch_job_get_start_count(job) == 1);
  printf(OK "job ran once and exited 0; supervisor reaped it\n");

  /* StandardOutPath redirection and EnvironmentVariables both applied. */
  FILE *f = fopen(outpath, "r");
  assert(f != NULL);
  char line[64] = {0};
  assert(fgets(line, sizeof(line), f) != NULL);
  fclose(f);
  assert(strncmp(line, "purr", 4) == 0);
  printf(OK "StandardOutPath captured \"purr\" -- redirection and the "
            "job's environment both reached the child\n");

  /* A non-zero exit is recorded as such. */
  char failpath[512];
  write_plist(failpath, sizeof(failpath), "failing",
              "  <key>Label</key><string>com.catbsd.failing</string>\n"
              "  <key>ProgramArguments</key>\n"
              "  <array><string>/bin/sh</string><string>-c</string>"
              "<string>exit 3</string></array>\n");
  launch_job_t *fjob = NULL;
  assert(launch_registry_load_plist(reg, failpath, &fjob, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(launch_job_start(reg, fjob, err, sizeof(err)) == LAUNCH_OK);
  assert(launch_registry_wait_state(reg, fjob, LAUNCH_STATE_EXITED, 5000) ==
         LAUNCH_OK);
  assert(launch_job_get_last_exit(fjob) == 3);
  printf(OK "a job exiting 3 is recorded with status 3, not restarted "
            "(no KeepAlive)\n");

  /* A command that doesn't exist surfaces as the shell's 127. */
  char missing[512];
  write_plist(missing, sizeof(missing), "missing",
              "  <key>Label</key><string>com.catbsd.missing</string>\n"
              "  <key>ProgramArguments</key>\n"
              "  <array><string>/nonexistent/catbsd-binary</string></array>\n");
  launch_job_t *mjob = NULL;
  assert(launch_registry_load_plist(reg, missing, &mjob, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(launch_job_start(reg, mjob, err, sizeof(err)) == LAUNCH_OK);
  assert(launch_registry_wait_state(reg, mjob, LAUNCH_STATE_EXITED, 5000) ==
         LAUNCH_OK);
  assert(launch_job_get_last_exit(mjob) == 127);
  printf(OK "an unexecutable ProgramArguments[0] reports exit 127\n");

  launch_registry_free(reg);
  printf("  All spawn/exit tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

static void test_keepalive_and_throttle(void) {
  printf("Testing KeepAlive and ThrottleInterval...\n");

  launch_registry_t *reg = launch_registry_create();
  char path[512], err[512];

  /* ThrottleInterval is expressed in whole seconds in the plist format,
   * which would make this a multi-second test; the registry default is
   * overridable precisely so it doesn't have to be. */
  launch_registry_set_default_throttle(reg, 100);

  write_plist(path, sizeof(path), "flapper",
              "  <key>Label</key><string>com.catbsd.flapper</string>\n"
              "  <key>ProgramArguments</key>\n"
              "  <array><string>/bin/sh</string><string>-c</string>"
              "<string>exit 0</string></array>\n"
              "  <key>KeepAlive</key><true/>\n");

  launch_job_t *job = NULL;
  assert(launch_registry_load_plist(reg, path, &job, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(launch_job_get_throttle(job) == 100);

  int64_t start = mono_ms();
  assert(launch_job_start(reg, job, err, sizeof(err)) == LAUNCH_OK);
  launch_registry_run(reg, 550);
  int64_t elapsed = mono_ms() - start;

  uint64_t starts = launch_job_get_start_count(job);
  /*
   * The job exits immediately every time, so without throttling it would
   * respawn as fast as fork() allows -- thousands of times in half a
   * second. The 100ms throttle is what bounds this.
   */
  assert(starts >= 3);
  assert(starts <= (uint64_t)(elapsed / 100) + 2);
  printf(OK "an instantly-exiting KeepAlive job restarted %llu times in "
            "%lldms -- throttled to ~1 per 100ms, not spinning\n",
         (unsigned long long)starts, (long long)elapsed);

  /* An explicit stop outranks KeepAlive. */
  launch_job_stop(reg, job, 1000);
  assert(launch_job_get_state(job) == LAUNCH_STATE_STOPPED);
  uint64_t at_stop = launch_job_get_start_count(job);
  launch_registry_run(reg, 300);
  assert(launch_job_get_start_count(job) == at_stop);
  assert(launch_job_get_state(job) == LAUNCH_STATE_STOPPED);
  printf(OK "after an explicit stop it stayed stopped despite "
            "KeepAlive=true\n");

  launch_registry_free(reg);

  /* Conditional KeepAlive: restart only on failure. */
  reg = launch_registry_create();
  launch_registry_set_default_throttle(reg, 50);

  char okpath[512], failpath[512];
  write_plist(okpath, sizeof(okpath), "clean-exit",
              "  <key>Label</key><string>com.catbsd.clean</string>\n"
              "  <key>ProgramArguments</key>\n"
              "  <array><string>/bin/sh</string><string>-c</string>"
              "<string>exit 0</string></array>\n"
              "  <key>KeepAlive</key>\n"
              "  <dict><key>SuccessfulExit</key><false/></dict>\n");
  write_plist(failpath, sizeof(failpath), "dirty-exit",
              "  <key>Label</key><string>com.catbsd.dirty</string>\n"
              "  <key>ProgramArguments</key>\n"
              "  <array><string>/bin/sh</string><string>-c</string>"
              "<string>exit 1</string></array>\n"
              "  <key>KeepAlive</key>\n"
              "  <dict><key>SuccessfulExit</key><false/></dict>\n");

  launch_job_t *clean = NULL, *dirty = NULL;
  assert(launch_registry_load_plist(reg, okpath, &clean, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(launch_registry_load_plist(reg, failpath, &dirty, err, sizeof(err)) ==
         LAUNCH_OK);

  launch_job_start(reg, clean, err, sizeof(err));
  launch_job_start(reg, dirty, err, sizeof(err));
  launch_registry_run(reg, 350);

  assert(launch_job_get_start_count(clean) == 1);
  assert(launch_job_get_state(clean) == LAUNCH_STATE_EXITED);
  assert(launch_job_get_start_count(dirty) >= 2);
  printf(OK "with SuccessfulExit=false the clean job stayed down (1 start) "
            "while the failing one restarted (%llu starts)\n",
         (unsigned long long)launch_job_get_start_count(dirty));

  launch_registry_free(reg);
  printf("  All KeepAlive tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

static void test_stop_escalation(void) {
  printf("Testing stop and SIGKILL escalation...\n");

  launch_registry_t *reg = launch_registry_create();
  char path[512], err[512];

  write_plist(path, sizeof(path), "sleeper",
              "  <key>Label</key><string>com.catbsd.sleeper</string>\n"
              "  <key>ProgramArguments</key>\n"
              "  <array><string>/bin/sh</string><string>-c</string>"
              "<string>sleep 300</string></array>\n");

  launch_job_t *job = NULL;
  assert(launch_registry_load_plist(reg, path, &job, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(launch_job_start(reg, job, err, sizeof(err)) == LAUNCH_OK);

  pid_t pid = launch_job_get_pid(job);
  assert(pid > 0);
  assert(kill(pid, 0) == 0); /* really alive */

  int64_t start = mono_ms();
  assert(launch_job_stop(reg, job, 1000) == LAUNCH_OK);
  int64_t elapsed = mono_ms() - start;

  assert(launch_job_get_state(job) == LAUNCH_STATE_STOPPED);
  assert(launch_job_get_pid(job) == -1);
  assert(elapsed < 1500);
  printf(OK "a sleeping job stopped in %lldms and was reaped\n",
         (long long)elapsed);

  /* A job that ignores SIGTERM must still die, via the SIGKILL fallback,
   * within roughly the timeout. */
  char stubborn[512];
  write_plist(stubborn, sizeof(stubborn), "stubborn",
              "  <key>Label</key><string>com.catbsd.stubborn</string>\n"
              "  <key>ProgramArguments</key>\n"
              "  <array><string>/bin/sh</string><string>-c</string>"
              "<string>trap '' TERM; while :; do sleep 1; done</string>"
              "</array>\n");
  launch_job_t *sjob = NULL;
  assert(launch_registry_load_plist(reg, stubborn, &sjob, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(launch_job_start(reg, sjob, err, sizeof(err)) == LAUNCH_OK);
  usleep(150000); /* let the shell install its trap */

  start = mono_ms();
  assert(launch_job_stop(reg, sjob, 300) == LAUNCH_OK);
  elapsed = mono_ms() - start;

  assert(launch_job_get_state(sjob) == LAUNCH_STATE_STOPPED);
  assert(launch_job_was_signaled(sjob));
  assert(launch_job_get_last_exit(sjob) == -SIGKILL);
  assert(elapsed >= 250 && elapsed < 2000);
  printf(OK "a job ignoring SIGTERM was SIGKILLed after the ~300ms grace "
            "period (took %lldms)\n",
         (long long)elapsed);

  launch_registry_free(reg);
  printf("  All stop tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

static void test_socket_activation(void) {
  printf("Testing socket activation...\n");

  launch_registry_t *reg = launch_registry_create();
  char path[512], body[1024], sockpath[96], envout[96], err[512];

  snprintf(sockpath, sizeof(sockpath), "%s/activate.sock", g_tmpdir);
  snprintf(envout, sizeof(envout), "%s/activate.env", g_tmpdir);
  snprintf(body, sizeof(body),
           "  <key>Label</key><string>com.catbsd.activated</string>\n"
           "  <key>ProgramArguments</key>\n"
           "  <array><string>/bin/sh</string><string>-c</string>"
           "<string>echo $LAUNCH_ACTIVATE_SOCKET_FD_Listener; sleep 5"
           "</string></array>\n"
           "  <key>StandardOutPath</key><string>%s</string>\n"
           "  <key>Sockets</key>\n"
           "  <dict><key>Listener</key>\n"
           "    <dict><key>SockPathName</key><string>%s</string></dict>\n"
           "  </dict>\n",
           envout, sockpath);
  write_plist(path, sizeof(path), "activated", body);

  launch_job_t *job = NULL;
  assert(launch_registry_load_plist(reg, path, &job, err, sizeof(err)) ==
         LAUNCH_OK);
  assert(launch_job_socket_count(job) == 1);
  assert(strcmp(launch_job_socket_path(job, "Listener"), sockpath) == 0);

  assert(launch_job_start(reg, job, err, sizeof(err)) == LAUNCH_OK);
  assert(launch_job_socket_fd(job, "Listener") >= 0);

  /*
   * The point of socket activation: the socket is already bound and
   * listening, so a client can connect without racing the child's
   * startup.
   */
  struct sockaddr_un addr;
  int client = socket(AF_UNIX, SOCK_STREAM, 0);
  assert(client >= 0);
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sockpath);
  assert(connect(client, (struct sockaddr *)&addr, sizeof(addr)) == 0);
  close(client);
  printf(OK "a client connected to the job's socket while the job was "
            "still starting\n");

  /* And the child was told which descriptor it is. */
  usleep(300000);
  FILE *f = fopen(envout, "r");
  assert(f != NULL);
  char line[64] = {0};
  assert(fgets(line, sizeof(line), f) != NULL);
  fclose(f);
  int child_fd = atoi(line);
  assert(child_fd > 2);
  assert(child_fd == launch_job_socket_fd(job, "Listener"));
  printf(OK "the child inherited fd %d and was told about it through "
            "LAUNCH_ACTIVATE_SOCKET_FD_Listener\n",
         child_fd);

  /* The child-side lookup helper reads that same convention. */
  char var[128];
  snprintf(var, sizeof(var), "%d", child_fd);
  setenv("LAUNCH_ACTIVATE_SOCKET_FD_Listener", var, 1);
  assert(launch_activate_socket("Listener") == child_fd);
  unsetenv("LAUNCH_ACTIVATE_SOCKET_FD_Listener");
  assert(launch_activate_socket("Listener") == -1 && errno == ENOENT);
  printf(OK "launch_activate_socket() recovers the fd by name, and "
            "reports ENOENT when unset\n");

  launch_registry_free(reg);
  printf("  All socket activation tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

static void test_registry_listing(void) {
  printf("Testing registry listing and unload...\n");

  launch_registry_t *reg = launch_registry_create();
  char path[512], err[512];

  for (int i = 0; i < 3; i++) {
    char name[64], body[512];
    snprintf(name, sizeof(name), "listed%d", i);
    snprintf(body, sizeof(body),
             "  <key>Label</key><string>com.catbsd.listed%d</string>\n"
             "  <key>ProgramArguments</key>\n"
             "  <array><string>/bin/sh</string><string>-c</string>"
             "<string>sleep 5</string></array>\n"
             "  <key>RunAtLoad</key><true/>\n",
             i);
    write_plist(path, sizeof(path), name, body);
    assert(launch_registry_load_plist(reg, path, NULL, err, sizeof(err)) ==
           LAUNCH_OK);
  }

  assert(launch_registry_count(reg) == 3);
  assert(launch_registry_start_all(reg) == 3);

  for (size_t i = 0; i < launch_registry_count(reg); i++) {
    launch_job_t *j = launch_registry_at(reg, i);
    assert(launch_job_get_state(j) == LAUNCH_STATE_RUNNING);
    assert(launch_job_get_pid(j) > 0);
  }
  printf(OK "start_all launched all 3 RunAtLoad jobs\n");

  assert(launch_registry_find(reg, "com.catbsd.listed1") != NULL);
  assert(launch_registry_find(reg, "com.catbsd.nope") == NULL);

  /* Unloading a running job stops it first rather than orphaning it. */
  launch_job_t *doomed = launch_registry_find(reg, "com.catbsd.listed1");
  pid_t doomed_pid = launch_job_get_pid(doomed);
  assert(launch_registry_unload(reg, "com.catbsd.listed1") == LAUNCH_OK);
  assert(launch_registry_count(reg) == 2);
  assert(launch_registry_find(reg, "com.catbsd.listed1") == NULL);
  assert(kill(doomed_pid, 0) != 0); /* really gone, not orphaned */
  printf(OK "unloading a running job stopped its process (pid %ld) "
            "before removing it\n",
         (long)doomed_pid);

  assert(launch_registry_unload(reg, "com.catbsd.listed1") == LAUNCH_ENOENT);

  int stopped = launch_registry_stop_all(reg, 1000);
  assert(stopped == 2);
  printf(OK "stop_all stopped the remaining %d job(s)\n", stopped);

  launch_registry_free(reg);
  printf("  All registry tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

int main(void) {
  printf("=== CatBSD liblaunch Test ===\n\n");

  make_tmpdir();
  printf("Scratch directory: %s\n\n", g_tmpdir);

  test_plist_parser();
  test_job_loading();
  test_run_and_exit();
  test_keepalive_and_throttle();
  test_stop_escalation();
  test_socket_activation();
  test_registry_listing();

  printf("=== All tests passed! ===\n");
  printf("\nJob plists parsed without libxml2, children supervised with\n");
  printf("KeepAlive and throttling, sockets activated before exec.\n");
  return 0;
}
