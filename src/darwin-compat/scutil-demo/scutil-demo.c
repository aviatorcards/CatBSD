/*
 * scutil-demo - System Configuration Utility for CatBSD
 * Darwin-style system configuration access
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

/* Get computer name */
static int get_computer_name(void) {
  char name[256];

  /* Try to get from hostname */
  if (gethostname(name, sizeof(name)) == 0) {
    /* Remove domain if present */
    char *dot = strchr(name, '.');
    if (dot)
      *dot = '\0';

    printf("%s\n", name);
    return 0;
  }

  fprintf(stderr, "Error: Could not get computer name\n");
  return -1;
}

/* Get local hostname */
static int get_local_hostname(void) {
  char name[256];

  if (gethostname(name, sizeof(name)) == 0) {
    printf("%s\n", name);
    return 0;
  }

  fprintf(stderr, "Error: Could not get hostname\n");
  return -1;
}

/* Get hostname */
static int get_hostname(void) {
  struct utsname uts;

  if (uname(&uts) == 0) {
    printf("%s\n", uts.nodename);
    return 0;
  }

  fprintf(stderr, "Error: Could not get hostname\n");
  return -1;
}

/* Show all info */
static int show_all(void) {
  struct utsname uts;
  char hostname[256];

  printf("=== System Configuration ===\n\n");

  if (gethostname(hostname, sizeof(hostname)) == 0) {
    printf("ComputerName: %s\n", hostname);
    printf("LocalHostName: %s\n", hostname);
    printf("HostName: %s\n", hostname);
  }

  if (uname(&uts) == 0) {
    printf("System: %s\n", uts.sysname);
    printf("Release: %s\n", uts.release);
    printf("Version: %s\n", uts.version);
    printf("Machine: %s\n", uts.machine);
  }

  return 0;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s --get <key>\n\n", prog);
  printf("Get system configuration values.\n\n");
  printf("Keys:\n");
  printf("  ComputerName      Computer name\n");
  printf("  LocalHostName     Local hostname\n");
  printf("  HostName          Hostname\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s --get ComputerName\n", prog);
  printf("  %s --get HostName\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  if (argc < 2) {
    show_all();
    return 0;
  }

  if (strcmp(argv[1], "--get") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing key argument\n");
      usage(argv[0]);
      return 1;
    }

    const char *key = argv[2];

    if (strcmp(key, "ComputerName") == 0) {
      return get_computer_name();
    } else if (strcmp(key, "LocalHostName") == 0) {
      return get_local_hostname();
    } else if (strcmp(key, "HostName") == 0) {
      return get_hostname();
    } else {
      fprintf(stderr, "Error: Unknown key '%s'\n", key);
      return 1;
    }
  } else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    usage(argv[0]);
    return 0;
  } else {
    fprintf(stderr, "Error: Unknown option '%s'\n", argv[1]);
    usage(argv[0]);
    return 1;
  }

  return 0;
}
