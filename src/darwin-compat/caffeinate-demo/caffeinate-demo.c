/*
 * caffeinate-demo - Prevent System Sleep for CatBSD
 * Keeps the system awake while running a command
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../shims/darwin_syscalls.h"

static volatile int running = 1;

/* Signal handler */
static void handle_signal(int sig) {
  (void)sig;
  running = 0;
  printf("\n[caffeinate] Releasing system...\n");
}

/* Simulate keeping system awake */
static void keep_awake(void) {
  printf("☕ Caffeinating system (preventing sleep)...\n");
  printf("   Press Ctrl+C to stop\n\n");

  int count = 0;
  while (running) {
    /* In real implementation, would call IOKit power assertions */
    /* For demo, just show we're alive */
    if (count % 10 == 0) {
      time_t now = time(NULL);
      printf("☕ Still awake... (%s)", ctime(&now));
    }
    sleep(1);
    count++;
  }

  printf("\n☕ System can sleep now\n");
}

/* Run a command while keeping system awake */
static int run_command(int argc, char *argv[]) {
  printf("☕ Caffeinating while running command...\n");
  printf("   Command: ");
  for (int i = 0; i < argc; i++) {
    printf("%s ", argv[i]);
  }
  printf("\n\n");

  /* Fork to run command */
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    /* Child - exec command */
    execvp(argv[0], argv);
    perror("execvp");
    exit(1);
  }

  /* Parent - keep system awake while command runs */
  printf("☕ Keeping system awake (PID: %d)...\n", pid);

  int status;
  uint64_t start = darwin_absolute_time();

  while (waitpid(pid, &status, WNOHANG) == 0) {
    /* Simulate power assertion */
    sleep(1);
  }

  uint64_t end = darwin_absolute_time();
  uint64_t elapsed_ms = (end - start) / 1000000;

  printf("\n☕ Command completed\n");
  printf("   Duration: %llu ms\n", (unsigned long long)elapsed_ms);
  printf("   Exit code: %d\n", WEXITSTATUS(status));

  return WEXITSTATUS(status);
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s [options] [command [args...]]\n\n", prog);
  printf("Prevent system sleep while running a command or indefinitely.\n\n");
  printf("Options:\n");
  printf("  -t seconds     Caffeinate for specified seconds\n");
  printf("  -h, --help     Show this help\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s               # Keep awake until Ctrl+C\n", prog);
  printf("  %s -t 60         # Keep awake for 60 seconds\n", prog);
  printf("  %s sleep 10      # Keep awake while 'sleep 10' runs\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  printf("=== CatBSD caffeinate Demo ===\n");
  printf("Prevent system sleep (Darwin-style)\n\n");

  /* Set up signal handler */
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  if (argc == 1) {
    /* No arguments - keep awake indefinitely */
    keep_awake();
    return 0;
  }

  const char *opt = argv[1];

  if (strcmp(opt, "-h") == 0 || strcmp(opt, "--help") == 0) {
    usage(argv[0]);
    return 0;
  } else if (strcmp(opt, "-t") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: -t requires seconds argument\n");
      return 1;
    }
    int seconds = atoi(argv[2]);
    printf("☕ Caffeinating for %d seconds...\n", seconds);

    for (int i = 0; i < seconds && running; i++) {
      if (i % 5 == 0) {
        printf("☕ %d/%d seconds...\n", i, seconds);
      }
      sleep(1);
    }

    printf("\n☕ Done!\n");
    return 0;
  } else {
    /* Assume it's a command to run */
    return run_command(argc - 1, argv + 1);
  }

  return 0;
}
