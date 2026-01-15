/*
 * sw_vers-demo - System Version Utility for CatBSD
 * Displays CatBSD system information in Darwin style
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#define CATBSD_VERSION "0.1.0-alpha"
#define CATBSD_BUILD "24A1"
#define CATBSD_NAME "CatBSD Darwin Utilities Toolkit"

/* Print specific version info */
static void print_info(const char *key) {
  struct utsname uts;
  uname(&uts);

  if (strcmp(key, "ProductName") == 0) {
    printf("%s\n", CATBSD_NAME);
  } else if (strcmp(key, "ProductVersion") == 0) {
    printf("%s\n", CATBSD_VERSION);
  } else if (strcmp(key, "BuildVersion") == 0) {
    printf("%s\n", CATBSD_BUILD);
  } else if (strcmp(key, "ProductBuildVersion") == 0) {
    printf("%s\n", CATBSD_BUILD);
  } else if (strcmp(key, "KernelVersion") == 0) {
    printf("%s %s\n", uts.sysname, uts.release);
  } else if (strcmp(key, "Architecture") == 0) {
    printf("%s\n", uts.machine);
  } else {
    fprintf(stderr, "Error: Unknown key '%s'\n", key);
    exit(1);
  }
}

/* Print all version info */
static void print_all(void) {
  struct utsname uts;
  uname(&uts);

  printf("ProductName:            %s\n", CATBSD_NAME);
  printf("ProductVersion:         %s\n", CATBSD_VERSION);
  printf("BuildVersion:           %s\n", CATBSD_BUILD);
  printf("KernelVersion:          %s %s\n", uts.sysname, uts.release);
  printf("Architecture:           %s\n", uts.machine);
  printf("Hostname:               %s\n", uts.nodename);
}

/* Print CatBSD banner */
static void print_banner(void) {
  printf("\n");
  printf("   ╱|、\n");
  printf("  (˚ˎ 。7   CatBSD %s\n", CATBSD_VERSION);
  printf("   |、˜〵   Where FreeBSD meets Darwin\n");
  printf("   じしˍ,)ノ\n");
  printf("\n");
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s [option]\n\n", prog);
  printf("Options:\n");
  printf("  -productName           Print product name\n");
  printf("  -productVersion        Print product version\n");
  printf("  -buildVersion          Print build version\n");
  printf("  -kernelVersion         Print kernel version\n");
  printf("  -architecture          Print architecture\n");
  printf("  -banner                Print CatBSD banner\n");
  printf("  (no option)            Print all information\n");
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  if (argc == 1) {
    /* No arguments - print all */
    print_all();
    return 0;
  }

  const char *opt = argv[1];

  if (strcmp(opt, "-productName") == 0) {
    print_info("ProductName");
  } else if (strcmp(opt, "-productVersion") == 0) {
    print_info("ProductVersion");
  } else if (strcmp(opt, "-buildVersion") == 0) {
    print_info("BuildVersion");
  } else if (strcmp(opt, "-kernelVersion") == 0) {
    print_info("KernelVersion");
  } else if (strcmp(opt, "-architecture") == 0) {
    print_info("Architecture");
  } else if (strcmp(opt, "-banner") == 0) {
    print_banner();
  } else if (strcmp(opt, "-h") == 0 || strcmp(opt, "--help") == 0) {
    usage(argv[0]);
  } else {
    fprintf(stderr, "Error: Unknown option '%s'\n", opt);
    usage(argv[0]);
    return 1;
  }

  return 0;
}
