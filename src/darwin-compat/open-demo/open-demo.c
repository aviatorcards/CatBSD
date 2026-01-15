/*
 * open-demo - File/URL Opener for CatBSD
 * Darwin-style file and URL opening utility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Open file or URL */
static int open_target(const char *target, const char *app, int reveal) {
  char cmd[2048];

  printf("📂 Opening: %s\n", target);

  if (app) {
    printf("   With: %s\n", app);
  }
  if (reveal) {
    printf("   Mode: Reveal in Finder\n");
  }

  /* Build open command */
  snprintf(cmd, sizeof(cmd), "open");

  if (app) {
    strncat(cmd, " -a '", sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, app, sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, "'", sizeof(cmd) - strlen(cmd) - 1);
  }

  if (reveal) {
    strncat(cmd, " -R", sizeof(cmd) - strlen(cmd) - 1);
  }

  /* Add target */
  strncat(cmd, " '", sizeof(cmd) - strlen(cmd) - 1);
  strncat(cmd, target, sizeof(cmd) - strlen(cmd) - 1);
  strncat(cmd, "'", sizeof(cmd) - strlen(cmd) - 1);

  /* Execute */
  int result = system(cmd);

  if (result == 0) {
    printf("✓ Opened successfully\n");
  } else {
    fprintf(stderr, "✗ Failed to open\n");
  }

  return result;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s [options] file|URL\n\n", prog);
  printf("Open files and URLs with default or specified applications.\n\n");
  printf("Options:\n");
  printf("  -a application    Open with specified application\n");
  printf("  -R                Reveal in Finder (don't open)\n");
  printf("  -h, --help        Show this help\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s file.txt\n", prog);
  printf("  %s -a TextEdit document.txt\n", prog);
  printf("  %s -R file.pdf\n", prog);
  printf("  %s https://catbsd.org\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  const char *app = NULL;
  const char *target = NULL;
  int reveal = 0;

  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  /* Parse arguments */
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-a") == 0) {
      if (i + 1 < argc) {
        app = argv[++i];
      } else {
        fprintf(stderr, "Error: -a requires application name\n");
        return 1;
      }
    } else if (strcmp(argv[i], "-R") == 0) {
      reveal = 1;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      target = argv[i];
    }
  }

  if (!target) {
    fprintf(stderr, "Error: No file or URL specified\n");
    usage(argv[0]);
    return 1;
  }

  printf("=== CatBSD open Demo ===\n");
  return open_target(target, app, reveal);
}
