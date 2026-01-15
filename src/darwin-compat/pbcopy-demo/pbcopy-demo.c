/*
 * pbcopy-demo - Copy to Clipboard for CatBSD
 * Darwin-style clipboard utility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CLIPBOARD_SIZE (10 * 1024 * 1024) // 10MB limit

/* Copy data to clipboard using macOS pbcopy */
static int copy_to_clipboard(const char *data, size_t len) {
  FILE *pb = popen("pbcopy", "w");
  if (!pb) {
    perror("popen");
    return -1;
  }

  size_t written = fwrite(data, 1, len, pb);
  if (written != len) {
    fprintf(stderr, "Error: Failed to write all data to clipboard\n");
    pclose(pb);
    return -1;
  }

  int status = pclose(pb);
  if (status != 0) {
    fprintf(stderr, "Error: pbcopy failed with status %d\n", status);
    return -1;
  }

  return 0;
}

/* Read from stdin and copy to clipboard */
static int copy_stdin(void) {
  char *buffer = NULL;
  size_t buffer_size = 0;
  size_t total_read = 0;
  size_t chunk_size = 4096;

  /* Read all input */
  while (1) {
    /* Resize buffer if needed */
    if (total_read + chunk_size > buffer_size) {
      buffer_size = total_read + chunk_size;

      if (buffer_size > MAX_CLIPBOARD_SIZE) {
        fprintf(stderr, "Error: Input exceeds maximum clipboard size\n");
        free(buffer);
        return -1;
      }

      char *new_buffer = realloc(buffer, buffer_size);
      if (!new_buffer) {
        perror("realloc");
        free(buffer);
        return -1;
      }
      buffer = new_buffer;
    }

    /* Read chunk */
    size_t bytes_read = fread(buffer + total_read, 1, chunk_size, stdin);
    total_read += bytes_read;

    if (bytes_read < chunk_size) {
      if (feof(stdin)) {
        break;
      }
      if (ferror(stdin)) {
        perror("fread");
        free(buffer);
        return -1;
      }
    }
  }

  /* Copy to clipboard */
  int result = copy_to_clipboard(buffer, total_read);

  free(buffer);

  if (result == 0) {
    fprintf(stderr, "📋 Copied %zu bytes to clipboard\n", total_read);
  }

  return result;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s < file\n", prog);
  printf("       command | %s\n\n", prog);
  printf("Copy data from stdin to the clipboard.\n\n");
  printf("Examples:\n");
  printf("  cat file.txt | %s\n", prog);
  printf("  echo \"Hello\" | %s\n", prog);
  printf("  %s < document.txt\n", prog);
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

  /* Check if stdin is a terminal */
  if (isatty(STDIN_FILENO)) {
    fprintf(stderr, "Error: No input provided\n");
    fprintf(stderr, "Hint: Use 'echo text | %s' or '%s < file'\n", argv[0],
            argv[0]);
    return 1;
  }

  return copy_stdin();
}
