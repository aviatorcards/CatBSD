/*
 * pbpaste-demo - Paste from Clipboard for CatBSD
 * Darwin-style clipboard utility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Paste data from clipboard using macOS pbpaste */
static int paste_from_clipboard(void) {
  FILE *pb = popen("pbpaste", "r");
  if (!pb) {
    perror("popen");
    return -1;
  }

  char buffer[4096];
  size_t bytes_read;
  size_t total_written = 0;

  /* Read from pbpaste and write to stdout */
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), pb)) > 0) {
    size_t written = fwrite(buffer, 1, bytes_read, stdout);
    if (written != bytes_read) {
      fprintf(stderr, "Error: Failed to write all data to stdout\n");
      pclose(pb);
      return -1;
    }
    total_written += written;
  }

  int status = pclose(pb);
  if (status != 0) {
    fprintf(stderr, "Error: pbpaste failed with status %d\n", status);
    return -1;
  }

  return 0;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s\n", prog);
  printf("       %s > file\n\n", prog);
  printf("Paste data from the clipboard to stdout.\n\n");
  printf("Examples:\n");
  printf("  %s\n", prog);
  printf("  %s > output.txt\n", prog);
  printf("  %s | grep pattern\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  /* Check for help */
  if (argc > 1) {
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "Error: Unknown option '%s'\n", argv[1]);
      usage(argv[0]);
      return 1;
    }
  }

  return paste_from_clipboard();
}
