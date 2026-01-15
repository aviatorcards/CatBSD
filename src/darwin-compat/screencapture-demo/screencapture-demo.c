/*
 * screencapture-demo - Screenshot Utility for CatBSD
 * Darwin-style screenshot capture
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Capture screenshot */
static int capture_screen(const char *filename, int interactive, int window,
                          int clipboard) {
  char cmd[1024];
  char default_name[256];

  /* Generate default filename if not provided */
  if (!filename) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(default_name, sizeof(default_name),
             "Screenshot_%04d-%02d-%02d_at_%02d.%02d.%02d.png",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
             t->tm_min, t->tm_sec);
    filename = default_name;
  }

  printf("📸 Capturing screenshot");
  if (interactive) {
    printf(" (interactive selection)");
  } else if (window) {
    printf(" (window)");
  } else if (clipboard) {
    printf(" (to clipboard)");
  }
  printf("\n");

  if (!clipboard) {
    printf("   Output: %s\n", filename);
  }

  /* Build screencapture command */
  snprintf(cmd, sizeof(cmd), "screencapture");

  if (interactive) {
    strncat(cmd, " -i", sizeof(cmd) - strlen(cmd) - 1);
  }

  if (window) {
    strncat(cmd, " -w", sizeof(cmd) - strlen(cmd) - 1);
  }

  if (clipboard) {
    strncat(cmd, " -c", sizeof(cmd) - strlen(cmd) - 1);
  } else {
    strncat(cmd, " '", sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, filename, sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, "'", sizeof(cmd) - strlen(cmd) - 1);
  }

  /* Execute */
  int result = system(cmd);

  if (result == 0) {
    printf("✓ Screenshot captured\n");
  } else {
    fprintf(stderr, "✗ Screenshot failed\n");
  }

  return result;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s [options] [file]\n\n", prog);
  printf("Capture screenshots from the command line.\n\n");
  printf("Options:\n");
  printf("  -i             Interactive selection\n");
  printf("  -w             Capture window\n");
  printf("  -c             Copy to clipboard\n");
  printf("  -h, --help     Show this help\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s screenshot.png\n", prog);
  printf("  %s -i selection.png\n", prog);
  printf("  %s -w window.png\n", prog);
  printf("  %s -c\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  int interactive = 0;
  int window = 0;
  int clipboard = 0;
  const char *filename = NULL;

  /* Parse arguments */
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-i") == 0) {
      interactive = 1;
    } else if (strcmp(argv[i], "-w") == 0) {
      window = 1;
    } else if (strcmp(argv[i], "-c") == 0) {
      clipboard = 1;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      filename = argv[i];
    }
  }

  printf("=== CatBSD screencapture Demo ===\n");
  return capture_screen(filename, interactive, window, clipboard);
}
