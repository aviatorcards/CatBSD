/*
 * liblaunch - job definition and supervision implementation
 */

#include "launch_job.h"
#include "plist_lite.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define SOCKET_ENV_PREFIX "LAUNCH_ACTIVATE_SOCKET_FD_"

typedef struct {
  char *name;
  char *path;
  int fd;
} launch_socket_t;

struct launch_job {
  char *label;
  char *plist_path;

  char **argv; /* NULL-terminated */
  int argc;

  char *working_dir;
  char *stdin_path;
  char *stdout_path;
  char *stderr_path;

  char **env;    /* "KEY=VALUE", NULL-terminated */
  int env_count;

  int run_at_load;
  int keepalive;
  int64_t throttle_ms;

  launch_socket_t *sockets;
  int socket_count;

  /* runtime state */
  pid_t pid;
  int state;
  int last_exit;
  int signaled;
  uint64_t start_count;
  int64_t restart_at_ms;
};

struct launch_registry {
  launch_job_t **jobs;
  size_t count;
  size_t cap;
  int64_t default_throttle_ms;
};

/* ------------------------------------------------------------------ */

const char *launch_strerror(int err) {
  switch (err) {
  case LAUNCH_OK:
    return "Success";
  case LAUNCH_EINVAL:
    return "Invalid argument";
  case LAUNCH_ENOMEM:
    return "Out of memory";
  case LAUNCH_EPARSE:
    return "Malformed job definition";
  case LAUNCH_EIO:
    return "I/O error";
  case LAUNCH_ENOENT:
    return "No such job";
  case LAUNCH_ESTATE:
    return "Job is in the wrong state for that";
  case LAUNCH_EEXIST:
    return "A job with that label is already loaded";
  case LAUNCH_ETIMEOUT:
    return "Timed out";
  default:
    return "Unknown error";
  }
}

const char *launch_state_name(int state) {
  switch (state) {
  case LAUNCH_STATE_LOADED:
    return "loaded";
  case LAUNCH_STATE_RUNNING:
    return "running";
  case LAUNCH_STATE_EXITED:
    return "exited";
  case LAUNCH_STATE_WAITING:
    return "waiting";
  case LAUNCH_STATE_STOPPED:
    return "stopped";
  case LAUNCH_STATE_FAILED:
    return "failed";
  default:
    return "unknown";
  }
}

static int64_t mono_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void seterr(char *buf, size_t len, const char *fmt, ...) {
  va_list ap;

  if (buf == NULL || len == 0) {
    return;
  }
  va_start(ap, fmt);
  vsnprintf(buf, len, fmt, ap);
  va_end(ap);
}

/* ------------------------------------------------------------------ */
/* Job construction                                                    */
/* ------------------------------------------------------------------ */

static void job_free(launch_job_t *j) {
  int i;

  if (j == NULL) {
    return;
  }

  for (i = 0; i < j->argc; i++) {
    free(j->argv[i]);
  }
  free(j->argv);

  for (i = 0; i < j->env_count; i++) {
    free(j->env[i]);
  }
  free(j->env);

  for (i = 0; i < j->socket_count; i++) {
    if (j->sockets[i].fd >= 0) {
      close(j->sockets[i].fd);
    }
    if (j->sockets[i].path != NULL) {
      unlink(j->sockets[i].path);
    }
    free(j->sockets[i].name);
    free(j->sockets[i].path);
  }
  free(j->sockets);

  free(j->label);
  free(j->plist_path);
  free(j->working_dir);
  free(j->stdin_path);
  free(j->stdout_path);
  free(j->stderr_path);
  free(j);
}

static char *dup_or_null(const char *s) {
  return (s != NULL) ? strdup(s) : NULL;
}

/*
 * KeepAlive is either a bool or a dict. The dict form has many keys in
 * real launchd; SuccessfulExit is the one that changes whether a crash
 * loop restarts, so it is the one implemented. Anything else in the dict
 * is ignored rather than rejected, so a plist copied off a Mac still
 * loads.
 */
static int parse_keepalive(const plist_value_t *ka) {
  const plist_value_t *se;

  if (ka == NULL) {
    return LAUNCH_KEEPALIVE_NEVER;
  }
  if (plist_type(ka) == PLIST_BOOL) {
    return plist_bool_value(ka, 0) ? LAUNCH_KEEPALIVE_ALWAYS
                                   : LAUNCH_KEEPALIVE_NEVER;
  }
  if (plist_type(ka) != PLIST_DICT) {
    return LAUNCH_KEEPALIVE_NEVER;
  }

  se = plist_dict_get(ka, "SuccessfulExit");
  if (se == NULL) {
    return LAUNCH_KEEPALIVE_ALWAYS;
  }
  return plist_bool_value(se, 0) ? LAUNCH_KEEPALIVE_ON_SUCCESS
                                 : LAUNCH_KEEPALIVE_ON_FAILURE;
}

static int job_set_argv(launch_job_t *j, const plist_value_t *root,
                        char *errbuf, size_t errlen) {
  const plist_value_t *args = plist_dict_get(root, "ProgramArguments");
  const char *program = plist_dict_get_string(root, "Program", NULL);
  size_t n, i;

  if (args != NULL && plist_type(args) == PLIST_ARRAY) {
    n = plist_array_count(args);
    if (n == 0) {
      seterr(errbuf, errlen, "ProgramArguments is empty");
      return LAUNCH_EPARSE;
    }
    /*
     * Darwin's rule: when both are present, Program is the executable and
     * ProgramArguments supplies argv (including argv[0], which is
     * conventionally but not necessarily the same string).
     */
    j->argv = calloc(n + 2, sizeof(*j->argv));
    if (j->argv == NULL) {
      return LAUNCH_ENOMEM;
    }
    if (program != NULL) {
      j->argv[0] = strdup(program);
      j->argc = 1;
    }
    for (i = 0; i < n; i++) {
      const char *s = plist_string_value(plist_array_get(args, i), NULL);
      if (s == NULL) {
        seterr(errbuf, errlen, "ProgramArguments[%zu] is not a string", i);
        return LAUNCH_EPARSE;
      }
      if (program != NULL && i == 0) {
        continue; /* Program already supplied the executable */
      }
      j->argv[j->argc] = strdup(s);
      if (j->argv[j->argc] == NULL) {
        return LAUNCH_ENOMEM;
      }
      j->argc++;
    }
    return LAUNCH_OK;
  }

  if (program != NULL) {
    j->argv = calloc(2, sizeof(*j->argv));
    if (j->argv == NULL) {
      return LAUNCH_ENOMEM;
    }
    j->argv[0] = strdup(program);
    j->argc = 1;
    return LAUNCH_OK;
  }

  seterr(errbuf, errlen, "neither Program nor ProgramArguments is present");
  return LAUNCH_EPARSE;
}

static int job_set_env(launch_job_t *j, const plist_value_t *root) {
  const plist_value_t *env = plist_dict_get(root, "EnvironmentVariables");
  size_t n, i;

  if (env == NULL || plist_type(env) != PLIST_DICT) {
    return LAUNCH_OK;
  }

  n = plist_dict_count(env);
  j->env = calloc(n + 1, sizeof(*j->env));
  if (j->env == NULL) {
    return LAUNCH_ENOMEM;
  }

  for (i = 0; i < n; i++) {
    const char *k = plist_dict_key_at(env, i);
    const char *v = plist_string_value(plist_dict_value_at(env, i), NULL);
    size_t len;

    if (k == NULL || v == NULL) {
      continue;
    }
    len = strlen(k) + strlen(v) + 2;
    j->env[j->env_count] = malloc(len);
    if (j->env[j->env_count] == NULL) {
      return LAUNCH_ENOMEM;
    }
    snprintf(j->env[j->env_count], len, "%s=%s", k, v);
    j->env_count++;
  }
  return LAUNCH_OK;
}

static int job_set_sockets(launch_job_t *j, const plist_value_t *root,
                           char *errbuf, size_t errlen) {
  const plist_value_t *socks = plist_dict_get(root, "Sockets");
  size_t n, i;

  if (socks == NULL || plist_type(socks) != PLIST_DICT) {
    return LAUNCH_OK;
  }

  n = plist_dict_count(socks);
  if (n == 0) {
    return LAUNCH_OK;
  }

  j->sockets = calloc(n, sizeof(*j->sockets));
  if (j->sockets == NULL) {
    return LAUNCH_ENOMEM;
  }

  for (i = 0; i < n; i++) {
    const char *name = plist_dict_key_at(socks, i);
    const plist_value_t *entry = plist_dict_value_at(socks, i);
    const char *path;

    if (name == NULL || plist_type(entry) != PLIST_DICT) {
      seterr(errbuf, errlen, "Sockets/%s is not a dictionary",
             name ? name : "?");
      return LAUNCH_EPARSE;
    }

    path = plist_dict_get_string(entry, "SockPathName", NULL);
    if (path == NULL) {
      /* Only Unix-domain sockets are implemented; a TCP entry should say
       * so rather than being silently ignored and never bound. */
      seterr(errbuf, errlen,
             "Sockets/%s has no SockPathName (only Unix sockets are "
             "supported)",
             name);
      return LAUNCH_EPARSE;
    }

    j->sockets[j->socket_count].name = strdup(name);
    j->sockets[j->socket_count].path = strdup(path);
    j->sockets[j->socket_count].fd = -1;
    if (j->sockets[j->socket_count].name == NULL ||
        j->sockets[j->socket_count].path == NULL) {
      return LAUNCH_ENOMEM;
    }
    j->socket_count++;
  }

  return LAUNCH_OK;
}

static launch_job_t *job_from_plist(const plist_value_t *root,
                                    const char *path, int64_t default_throttle,
                                    int *err_out, char *errbuf, size_t errlen) {
  launch_job_t *j;
  const char *label;
  const plist_value_t *throttle;
  int rc;

  if (plist_type(root) != PLIST_DICT) {
    seterr(errbuf, errlen, "top-level value is not a dictionary");
    *err_out = LAUNCH_EPARSE;
    return NULL;
  }

  label = plist_dict_get_string(root, "Label", NULL);
  if (label == NULL || label[0] == '\0') {
    seterr(errbuf, errlen, "missing required key Label");
    *err_out = LAUNCH_EPARSE;
    return NULL;
  }

  j = calloc(1, sizeof(*j));
  if (j == NULL) {
    *err_out = LAUNCH_ENOMEM;
    return NULL;
  }
  j->pid = -1;
  j->state = LAUNCH_STATE_LOADED;
  j->label = strdup(label);
  j->plist_path = dup_or_null(path);
  j->working_dir = dup_or_null(plist_dict_get_string(root, "WorkingDirectory",
                                                     NULL));
  j->stdin_path =
      dup_or_null(plist_dict_get_string(root, "StandardInPath", NULL));
  j->stdout_path =
      dup_or_null(plist_dict_get_string(root, "StandardOutPath", NULL));
  j->stderr_path =
      dup_or_null(plist_dict_get_string(root, "StandardErrorPath", NULL));
  j->run_at_load = plist_dict_get_bool(root, "RunAtLoad", 0);
  j->keepalive = parse_keepalive(plist_dict_get(root, "KeepAlive"));

  throttle = plist_dict_get(root, "ThrottleInterval");
  j->throttle_ms = (throttle != NULL)
                       ? plist_integer_value(throttle, 0) * 1000
                       : default_throttle;
  if (j->throttle_ms < 0) {
    j->throttle_ms = 0;
  }

  if (j->label == NULL) {
    job_free(j);
    *err_out = LAUNCH_ENOMEM;
    return NULL;
  }

  rc = job_set_argv(j, root, errbuf, errlen);
  if (rc == LAUNCH_OK) {
    rc = job_set_env(j, root);
  }
  if (rc == LAUNCH_OK) {
    rc = job_set_sockets(j, root, errbuf, errlen);
  }
  if (rc != LAUNCH_OK) {
    job_free(j);
    *err_out = rc;
    return NULL;
  }

  *err_out = LAUNCH_OK;
  return j;
}

/* ------------------------------------------------------------------ */
/* Registry                                                            */
/* ------------------------------------------------------------------ */

launch_registry_t *launch_registry_create(void) {
  launch_registry_t *reg = calloc(1, sizeof(*reg));

  if (reg == NULL) {
    return NULL;
  }
  /* Darwin's default is 10 seconds. */
  reg->default_throttle_ms = 10000;
  return reg;
}

void launch_registry_set_default_throttle(launch_registry_t *reg,
                                          int64_t millis) {
  if (reg != NULL && millis >= 0) {
    reg->default_throttle_ms = millis;
  }
}

void launch_registry_free(launch_registry_t *reg) {
  size_t i;

  if (reg == NULL) {
    return;
  }

  launch_registry_stop_all(reg, 2000);

  for (i = 0; i < reg->count; i++) {
    job_free(reg->jobs[i]);
  }
  free(reg->jobs);
  free(reg);
}

static int registry_append(launch_registry_t *reg, launch_job_t *j) {
  if (reg->count == reg->cap) {
    size_t cap = reg->cap ? reg->cap * 2 : 8;
    launch_job_t **jobs = realloc(reg->jobs, cap * sizeof(*jobs));
    if (jobs == NULL) {
      return LAUNCH_ENOMEM;
    }
    reg->jobs = jobs;
    reg->cap = cap;
  }
  reg->jobs[reg->count++] = j;
  return LAUNCH_OK;
}

int launch_registry_load_plist(launch_registry_t *reg, const char *path,
                               launch_job_t **job_out, char *errbuf,
                               size_t errlen) {
  plist_value_t *root;
  launch_job_t *j;
  char parse_err[256];
  int rc = LAUNCH_OK;

  if (errbuf != NULL && errlen > 0) {
    errbuf[0] = '\0';
  }
  if (reg == NULL || path == NULL) {
    return LAUNCH_EINVAL;
  }

  root = plist_parse_file(path, parse_err, sizeof(parse_err));
  if (root == NULL) {
    seterr(errbuf, errlen, "%s: %s", path, parse_err);
    return LAUNCH_EPARSE;
  }

  j = job_from_plist(root, path, reg->default_throttle_ms, &rc, parse_err,
                     sizeof(parse_err));
  plist_free(root);

  if (j == NULL) {
    seterr(errbuf, errlen, "%s: %s", path, parse_err);
    return rc;
  }

  if (launch_registry_find(reg, j->label) != NULL) {
    seterr(errbuf, errlen, "%s: label %s is already loaded", path, j->label);
    job_free(j);
    return LAUNCH_EEXIST;
  }

  rc = registry_append(reg, j);
  if (rc != LAUNCH_OK) {
    job_free(j);
    return rc;
  }

  if (job_out != NULL) {
    *job_out = j;
  }
  return LAUNCH_OK;
}

int launch_registry_load_dir(launch_registry_t *reg, const char *dir,
                             int *failed_out, char *errbuf, size_t errlen) {
  DIR *d;
  struct dirent *ent;
  int loaded = 0, failed = 0;

  if (failed_out != NULL) {
    *failed_out = 0;
  }
  if (reg == NULL || dir == NULL) {
    return LAUNCH_EINVAL;
  }

  d = opendir(dir);
  if (d == NULL) {
    seterr(errbuf, errlen, "cannot open %s: %s", dir, strerror(errno));
    return LAUNCH_ENOENT;
  }

  while ((ent = readdir(d)) != NULL) {
    char path[1024];
    size_t len = strlen(ent->d_name);
    char one_err[256];

    if (len < 7 || strcmp(ent->d_name + len - 6, ".plist") != 0) {
      continue;
    }
    snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

    if (launch_registry_load_plist(reg, path, NULL, one_err,
                                   sizeof(one_err)) == LAUNCH_OK) {
      loaded++;
    } else {
      failed++;
      /* Keep going: one broken plist in a directory shouldn't stop the
       * other jobs from loading. The last message is reported back. */
      seterr(errbuf, errlen, "%s", one_err);
    }
  }

  closedir(d);
  if (failed_out != NULL) {
    *failed_out = failed;
  }
  return loaded;
}

launch_job_t *launch_registry_find(launch_registry_t *reg, const char *label) {
  size_t i;

  if (reg == NULL || label == NULL) {
    return NULL;
  }
  for (i = 0; i < reg->count; i++) {
    if (strcmp(reg->jobs[i]->label, label) == 0) {
      return reg->jobs[i];
    }
  }
  return NULL;
}

size_t launch_registry_count(launch_registry_t *reg) {
  return (reg != NULL) ? reg->count : 0;
}

launch_job_t *launch_registry_at(launch_registry_t *reg, size_t index) {
  if (reg == NULL || index >= reg->count) {
    return NULL;
  }
  return reg->jobs[index];
}

int launch_registry_unload(launch_registry_t *reg, const char *label) {
  size_t i;

  if (reg == NULL || label == NULL) {
    return LAUNCH_EINVAL;
  }

  for (i = 0; i < reg->count; i++) {
    if (strcmp(reg->jobs[i]->label, label) != 0) {
      continue;
    }
    if (reg->jobs[i]->state == LAUNCH_STATE_RUNNING) {
      launch_job_stop(reg, reg->jobs[i], 2000);
    }
    job_free(reg->jobs[i]);
    memmove(&reg->jobs[i], &reg->jobs[i + 1],
            (reg->count - i - 1) * sizeof(*reg->jobs));
    reg->count--;
    return LAUNCH_OK;
  }
  return LAUNCH_ENOENT;
}

/* ------------------------------------------------------------------ */
/* Spawning                                                            */
/* ------------------------------------------------------------------ */

static int socket_create(launch_socket_t *s, char *errbuf, size_t errlen) {
  struct sockaddr_un addr;
  int fd;

  if (s->fd >= 0) {
    return LAUNCH_OK; /* already listening from a previous run */
  }

  if (strlen(s->path) >= sizeof(addr.sun_path)) {
    seterr(errbuf, errlen, "socket path too long: %s", s->path);
    return LAUNCH_EINVAL;
  }

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    seterr(errbuf, errlen, "socket(): %s", strerror(errno));
    return LAUNCH_EIO;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", s->path);

  unlink(s->path); /* a previous run's socket file is not a live listener */
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
      listen(fd, 16) != 0) {
    seterr(errbuf, errlen, "bind/listen %s: %s", s->path, strerror(errno));
    close(fd);
    return LAUNCH_EIO;
  }

  s->fd = fd;
  return LAUNCH_OK;
}

/*
 * Build the child's environment in the parent.
 *
 * The child of a fork() in a process that may have other threads can only
 * safely call async-signal-safe functions, and setenv() is not one of
 * them -- it allocates. So the full environment block is assembled here,
 * before the fork, and the child does nothing but point `environ` at it.
 */
static char **build_child_env(const launch_job_t *j) {
  size_t base = 0, i;
  size_t extra = (size_t)j->env_count + (size_t)j->socket_count;
  char **env;
  size_t n = 0;

  while (environ != NULL && environ[base] != NULL) {
    base++;
  }

  env = calloc(base + extra + 1, sizeof(*env));
  if (env == NULL) {
    return NULL;
  }

  for (i = 0; i < base; i++) {
    env[n++] = environ[i]; /* borrowed; the child never frees these */
  }
  for (i = 0; i < (size_t)j->env_count; i++) {
    env[n++] = j->env[i];
  }

  for (i = 0; i < (size_t)j->socket_count; i++) {
    char *entry;
    size_t len = strlen(SOCKET_ENV_PREFIX) + strlen(j->sockets[i].name) + 16;

    entry = malloc(len);
    if (entry == NULL) {
      /* Only the socket entries were allocated here, so only they leak
       * if we bail; free them and give up on the spawn. */
      while (n > base + (size_t)j->env_count) {
        free(env[--n]);
      }
      free(env);
      return NULL;
    }
    snprintf(entry, len, SOCKET_ENV_PREFIX "%s=%d", j->sockets[i].name,
             j->sockets[i].fd);
    env[n++] = entry;
  }

  env[n] = NULL;
  return env;
}

static void free_child_env(char **env, const launch_job_t *j) {
  size_t base = 0, i, n = 0;

  if (env == NULL) {
    return;
  }
  while (env[n] != NULL) {
    n++;
  }
  while (environ != NULL && environ[base] != NULL) {
    base++;
  }
  /* Only the socket entries were malloc'd by build_child_env. */
  for (i = base + (size_t)j->env_count; i < n; i++) {
    free(env[i]);
  }
  free(env);
}

/* Async-signal-safe redirect helper; runs only in the child. */
static int redirect(const char *path, int target_fd, int flags) {
  int fd;

  if (path == NULL) {
    return 0;
  }
  fd = open(path, flags, 0644);
  if (fd < 0) {
    return -1;
  }
  if (dup2(fd, target_fd) < 0) {
    close(fd);
    return -1;
  }
  if (fd != target_fd) {
    close(fd);
  }
  return 0;
}

int launch_job_start(launch_registry_t *reg, launch_job_t *j, char *errbuf,
                     size_t errlen) {
  char **child_env;
  pid_t pid;
  int i;

  if (errbuf != NULL && errlen > 0) {
    errbuf[0] = '\0';
  }
  if (reg == NULL || j == NULL) {
    return LAUNCH_EINVAL;
  }
  if (j->state == LAUNCH_STATE_RUNNING) {
    seterr(errbuf, errlen, "%s is already running (pid %ld)", j->label,
           (long)j->pid);
    return LAUNCH_ESTATE;
  }
  if (j->argc == 0) {
    return LAUNCH_EINVAL;
  }

  for (i = 0; i < j->socket_count; i++) {
    int rc = socket_create(&j->sockets[i], errbuf, errlen);
    if (rc != LAUNCH_OK) {
      j->state = LAUNCH_STATE_FAILED;
      return rc;
    }
  }

  child_env = build_child_env(j);
  if (child_env == NULL) {
    return LAUNCH_ENOMEM;
  }

  pid = fork();
  if (pid < 0) {
    seterr(errbuf, errlen, "fork(): %s", strerror(errno));
    free_child_env(child_env, j);
    j->state = LAUNCH_STATE_FAILED;
    return LAUNCH_EIO;
  }

  if (pid == 0) {
    /* --- child: async-signal-safe calls only until execvp --- */
    if (j->working_dir != NULL && chdir(j->working_dir) != 0) {
      _exit(126);
    }
    if (redirect(j->stdin_path, STDIN_FILENO, O_RDONLY) != 0 ||
        redirect(j->stdout_path, STDOUT_FILENO,
                 O_WRONLY | O_CREAT | O_APPEND) != 0 ||
        redirect(j->stderr_path, STDERR_FILENO,
                 O_WRONLY | O_CREAT | O_APPEND) != 0) {
      _exit(126);
    }

    /* Activation sockets must survive exec, so clear FD_CLOEXEC on them
     * (they inherit whatever the parent had). */
    for (i = 0; i < j->socket_count; i++) {
      int flags = fcntl(j->sockets[i].fd, F_GETFD);
      if (flags >= 0) {
        fcntl(j->sockets[i].fd, F_SETFD, flags & ~FD_CLOEXEC);
      }
    }

    environ = child_env;
    execvp(j->argv[0], j->argv);
    /* 127 is the shell's convention for "command not found", which is
     * what the parent reports back as the failure reason. */
    _exit(127);
  }

  /* --- parent --- */
  free_child_env(child_env, j);
  j->pid = pid;
  j->state = LAUNCH_STATE_RUNNING;
  j->start_count++;
  j->restart_at_ms = 0;
  return LAUNCH_OK;
}

int launch_job_stop(launch_registry_t *reg, launch_job_t *j, int timeout_ms) {
  int64_t deadline;

  if (reg == NULL || j == NULL) {
    return LAUNCH_EINVAL;
  }
  if (j->state != LAUNCH_STATE_RUNNING || j->pid <= 0) {
    /* Not running: still honour the intent, so a throttled job doesn't
     * come back a moment after being told to stop. */
    j->state = LAUNCH_STATE_STOPPED;
    j->restart_at_ms = 0;
    return LAUNCH_OK;
  }

  kill(j->pid, SIGTERM);

  deadline = mono_ms() + (timeout_ms > 0 ? timeout_ms : 0);
  for (;;) {
    int status;
    pid_t r = waitpid(j->pid, &status, WNOHANG);

    if (r == j->pid) {
      j->signaled = WIFSIGNALED(status);
      j->last_exit = j->signaled ? -WTERMSIG(status) : WEXITSTATUS(status);
      break;
    }
    if (r < 0 && errno == ECHILD) {
      break; /* already reaped elsewhere */
    }
    if (mono_ms() >= deadline) {
      /* Escalate. A job that ignores SIGTERM must not be able to keep the
       * supervisor waiting forever. */
      kill(j->pid, SIGKILL);
      waitpid(j->pid, &status, 0);
      j->signaled = 1;
      j->last_exit = -SIGKILL;
      break;
    }
    usleep(5000);
  }

  j->pid = -1;
  j->state = LAUNCH_STATE_STOPPED;
  j->restart_at_ms = 0;
  return LAUNCH_OK;
}

int launch_registry_start_all(launch_registry_t *reg) {
  size_t i;
  int started = 0;

  if (reg == NULL) {
    return 0;
  }
  for (i = 0; i < reg->count; i++) {
    launch_job_t *j = reg->jobs[i];
    if (j->run_at_load && j->state == LAUNCH_STATE_LOADED) {
      if (launch_job_start(reg, j, NULL, 0) == LAUNCH_OK) {
        started++;
      }
    }
  }
  return started;
}

/*
 * Decide what an exit means. Note that a job stopped by an operator is
 * never restarted here: launch_job_stop() has already moved it to
 * STOPPED, and this only ever looks at jobs it finds RUNNING.
 */
static int should_restart(const launch_job_t *j, int exited_cleanly) {
  switch (j->keepalive) {
  case LAUNCH_KEEPALIVE_ALWAYS:
    return 1;
  case LAUNCH_KEEPALIVE_ON_FAILURE:
    return !exited_cleanly;
  case LAUNCH_KEEPALIVE_ON_SUCCESS:
    return exited_cleanly;
  default:
    return 0;
  }
}

int launch_registry_tick(launch_registry_t *reg) {
  size_t i;
  int changes = 0;
  int64_t now;

  if (reg == NULL) {
    return 0;
  }

  /* Reap first, so a job that exited this instant can be restarted in the
   * same tick if its throttle allows. */
  for (i = 0; i < reg->count; i++) {
    launch_job_t *j = reg->jobs[i];
    int status;
    pid_t r;

    if (j->state != LAUNCH_STATE_RUNNING || j->pid <= 0) {
      continue;
    }

    /*
     * waitpid on the specific pid rather than -1: this library may be
     * embedded in a program with children of its own, and reaping those
     * would break it in a way that is very hard to debug.
     */
    r = waitpid(j->pid, &status, WNOHANG);
    if (r == 0) {
      continue; /* still alive */
    }
    if (r < 0 && errno != ECHILD) {
      continue;
    }

    if (r > 0) {
      j->signaled = WIFSIGNALED(status);
      j->last_exit = j->signaled ? -WTERMSIG(status) : WEXITSTATUS(status);
    }
    j->pid = -1;
    changes++;

    if (should_restart(j, !j->signaled && j->last_exit == 0)) {
      j->state = LAUNCH_STATE_WAITING;
      j->restart_at_ms = mono_ms() + j->throttle_ms;
    } else {
      j->state = LAUNCH_STATE_EXITED;
      j->restart_at_ms = 0;
    }
  }

  now = mono_ms();
  for (i = 0; i < reg->count; i++) {
    launch_job_t *j = reg->jobs[i];

    if (j->state == LAUNCH_STATE_WAITING && now >= j->restart_at_ms) {
      if (launch_job_start(reg, j, NULL, 0) == LAUNCH_OK) {
        changes++;
      } else {
        j->state = LAUNCH_STATE_FAILED;
        changes++;
      }
    }
  }

  return changes;
}

int launch_registry_run(launch_registry_t *reg, int duration_ms) {
  int64_t deadline = mono_ms() + (duration_ms > 0 ? duration_ms : 0);
  int changes = 0;

  do {
    changes += launch_registry_tick(reg);
    if (mono_ms() >= deadline) {
      break;
    }
    usleep(5000);
  } while (1);

  return changes;
}

int launch_registry_wait_state(launch_registry_t *reg, launch_job_t *j,
                               int state, int timeout_ms) {
  int64_t deadline = mono_ms() + (timeout_ms > 0 ? timeout_ms : 0);

  if (reg == NULL || j == NULL) {
    return LAUNCH_EINVAL;
  }

  for (;;) {
    launch_registry_tick(reg);
    if (j->state == state) {
      return LAUNCH_OK;
    }
    if (mono_ms() >= deadline) {
      return LAUNCH_ETIMEOUT;
    }
    usleep(5000);
  }
}

int launch_registry_stop_all(launch_registry_t *reg, int timeout_ms) {
  size_t i;
  int stopped = 0;

  if (reg == NULL) {
    return 0;
  }
  for (i = 0; i < reg->count; i++) {
    if (reg->jobs[i]->state == LAUNCH_STATE_RUNNING) {
      launch_job_stop(reg, reg->jobs[i], timeout_ms);
      stopped++;
    }
  }
  return stopped;
}

/* ------------------------------------------------------------------ */
/* Accessors                                                           */
/* ------------------------------------------------------------------ */

const char *launch_job_get_label(const launch_job_t *j) {
  return (j != NULL) ? j->label : NULL;
}

const char *launch_job_get_plist_path(const launch_job_t *j) {
  return (j != NULL) ? j->plist_path : NULL;
}

pid_t launch_job_get_pid(const launch_job_t *j) {
  return (j != NULL) ? j->pid : -1;
}

int launch_job_get_state(const launch_job_t *j) {
  return (j != NULL) ? j->state : LAUNCH_STATE_LOADED;
}

int launch_job_get_keepalive(const launch_job_t *j) {
  return (j != NULL) ? j->keepalive : LAUNCH_KEEPALIVE_NEVER;
}

int launch_job_get_run_at_load(const launch_job_t *j) {
  return (j != NULL) ? j->run_at_load : 0;
}

int64_t launch_job_get_throttle(const launch_job_t *j) {
  return (j != NULL) ? j->throttle_ms : 0;
}

uint64_t launch_job_get_start_count(const launch_job_t *j) {
  return (j != NULL) ? j->start_count : 0;
}

int launch_job_get_last_exit(const launch_job_t *j) {
  return (j != NULL) ? j->last_exit : 0;
}

int launch_job_was_signaled(const launch_job_t *j) {
  return (j != NULL) ? j->signaled : 0;
}

int launch_job_argc(const launch_job_t *j) {
  return (j != NULL) ? j->argc : 0;
}

const char *launch_job_argv_at(const launch_job_t *j, int index) {
  if (j == NULL || index < 0 || index >= j->argc) {
    return NULL;
  }
  return j->argv[index];
}

int launch_job_socket_count(const launch_job_t *j) {
  return (j != NULL) ? j->socket_count : 0;
}

int launch_job_socket_fd(const launch_job_t *j, const char *name) {
  int i;

  if (j == NULL || name == NULL) {
    return -1;
  }
  for (i = 0; i < j->socket_count; i++) {
    if (strcmp(j->sockets[i].name, name) == 0) {
      return j->sockets[i].fd;
    }
  }
  return -1;
}

const char *launch_job_socket_path(const launch_job_t *j, const char *name) {
  int i;

  if (j == NULL || name == NULL) {
    return NULL;
  }
  for (i = 0; i < j->socket_count; i++) {
    if (strcmp(j->sockets[i].name, name) == 0) {
      return j->sockets[i].path;
    }
  }
  return NULL;
}

int launch_activate_socket(const char *name) {
  char var[256];
  const char *value;
  long fd;

  if (name == NULL) {
    errno = EINVAL;
    return -1;
  }

  snprintf(var, sizeof(var), SOCKET_ENV_PREFIX "%s", name);
  value = getenv(var);
  if (value == NULL || value[0] == '\0') {
    errno = ENOENT;
    return -1;
  }

  fd = strtol(value, NULL, 10);
  if (fd < 0 || fd > INT_MAX) {
    errno = EINVAL;
    return -1;
  }
  return (int)fd;
}
