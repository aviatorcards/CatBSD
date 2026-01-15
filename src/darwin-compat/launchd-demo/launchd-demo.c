/*
 * launchd-demo - Simplified launchd for CatBSD
 * Demonstrates porting Darwin components using compatibility shims
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../shims/darwin_syscalls.h"
#include "../shims/mach_port.h"

#define MAX_DAEMONS 32
#define MAX_NAME_LEN 256

/* Daemon structure */
typedef struct {
  char name[MAX_NAME_LEN];
  pid_t pid;
  mach_port_t port;
  int active;
} daemon_t;

/* Global daemon registry */
static daemon_t daemons[MAX_DAEMONS];
static int daemon_count = 0;

/* Find daemon by name */
static daemon_t *find_daemon(const char *name) {
  for (int i = 0; i < daemon_count; i++) {
    if (strcmp(daemons[i].name, name) == 0) {
      return &daemons[i];
    }
  }
  return NULL;
}

/* Start a daemon */
static int start_daemon(const char *name, const char *path) {
  if (daemon_count >= MAX_DAEMONS) {
    fprintf(stderr, "Error: Maximum daemons reached\n");
    return -1;
  }

  /* Check if already running */
  if (find_daemon(name)) {
    fprintf(stderr, "Error: Daemon '%s' already running\n", name);
    return -1;
  }

  /* Create Mach port for IPC */
  mach_port_t port;
  if (mach_port_allocate(&port) != KERN_SUCCESS) {
    fprintf(stderr, "Error: Failed to allocate Mach port\n");
    return -1;
  }

  /* Fork daemon process */
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    mach_port_deallocate(port);
    return -1;
  }

  if (pid == 0) {
    /* Child process - exec the daemon */
    printf("[launchd] Starting daemon: %s\n", name);

    /* Set thread name for debugging */
    darwin_pthread_setname_np(name);

    /* In a real implementation, we'd exec the actual daemon */
    /* For demo, just sleep */
    while (1) {
      sleep(10);
    }
    exit(0);
  }

  /* Parent - register daemon */
  daemon_t *d = &daemons[daemon_count++];
  strncpy(d->name, name, MAX_NAME_LEN - 1);
  d->pid = pid;
  d->port = port;
  d->active = 1;

  printf("✓ Started daemon '%s' (PID: %d, Port: %d)\n", name, pid, port);
  return 0;
}

/* Stop a daemon */
static int stop_daemon(const char *name) {
  daemon_t *d = find_daemon(name);
  if (!d) {
    fprintf(stderr, "Error: Daemon '%s' not found\n", name);
    return -1;
  }

  if (!d->active) {
    fprintf(stderr, "Error: Daemon '%s' not running\n", name);
    return -1;
  }

  /* Kill the daemon process */
  printf("[launchd] Stopping daemon: %s (PID: %d)\n", name, d->pid);
  kill(d->pid, SIGTERM);

  /* Wait for process to exit */
  int status;
  waitpid(d->pid, &status, 0);

  /* Deallocate Mach port */
  mach_port_deallocate(d->port);

  d->active = 0;
  printf("✓ Stopped daemon '%s'\n", name);
  return 0;
}

/* List all daemons */
static void list_daemons(void) {
  printf("\n=== CatBSD launchd - Daemon List ===\n\n");

  if (daemon_count == 0) {
    printf("No daemons registered.\n");
    return;
  }

  printf("%-30s %-10s %-10s %s\n", "NAME", "PID", "PORT", "STATUS");
  printf("%-30s %-10s %-10s %s\n", "----", "---", "----", "------");

  for (int i = 0; i < daemon_count; i++) {
    daemon_t *d = &daemons[i];
    printf("%-30s %-10d %-10d %s\n", d->name, d->pid, d->port,
           d->active ? "Running" : "Stopped");
  }

  printf("\n");
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s <command> [args]\n\n", prog);
  printf("Commands:\n");
  printf("  start <name>   - Start a daemon\n");
  printf("  stop <name>    - Stop a daemon\n");
  printf("  list           - List all daemons\n");
  printf("  help           - Show this help\n");
  printf("\n");
  printf("Example:\n");
  printf("  %s start com.example.daemon\n", prog);
  printf("  %s list\n", prog);
  printf("  %s stop com.example.daemon\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  printf("=== CatBSD launchd Demo ===\n");
  printf("Using Mach port compatibility shims\n\n");

  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  const char *cmd = argv[1];

  if (strcmp(cmd, "start") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing daemon name\n");
      usage(argv[0]);
      return 1;
    }
    return start_daemon(argv[2], NULL);

  } else if (strcmp(cmd, "stop") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing daemon name\n");
      usage(argv[0]);
      return 1;
    }
    return stop_daemon(argv[2]);

  } else if (strcmp(cmd, "list") == 0) {
    list_daemons();
    return 0;

  } else if (strcmp(cmd, "help") == 0) {
    usage(argv[0]);
    return 0;

  } else {
    fprintf(stderr, "Error: Unknown command '%s'\n", cmd);
    usage(argv[0]);
    return 1;
  }

  return 0;
}
