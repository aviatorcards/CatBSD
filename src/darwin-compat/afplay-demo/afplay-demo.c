/*
 * afplay-demo - Audio File Player for CatBSD
 * Darwin-style audio playback utility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Play audio file */
static int play_audio(const char *file, float volume, int duration) {
  char cmd[1024];

  printf("🔊 Playing: %s\n", file);

  if (volume >= 0) {
    printf("   Volume: %.1f\n", volume);
  }
  if (duration > 0) {
    printf("   Duration: %d seconds\n", duration);
  }

  /* Build afplay command */
  snprintf(cmd, sizeof(cmd), "afplay");

  if (volume >= 0) {
    char vol_arg[64];
    snprintf(vol_arg, sizeof(vol_arg), " -v %.2f", volume);
    strncat(cmd, vol_arg, sizeof(cmd) - strlen(cmd) - 1);
  }

  if (duration > 0) {
    char time_arg[64];
    snprintf(time_arg, sizeof(time_arg), " -t %d", duration);
    strncat(cmd, time_arg, sizeof(cmd) - strlen(cmd) - 1);
  }

  /* Add filename */
  strncat(cmd, " '", sizeof(cmd) - strlen(cmd) - 1);
  strncat(cmd, file, sizeof(cmd) - strlen(cmd) - 1);
  strncat(cmd, "'", sizeof(cmd) - strlen(cmd) - 1);

  /* Execute */
  int result = system(cmd);

  if (result == 0) {
    printf("✓ Playback complete\n");
  } else {
    fprintf(stderr, "✗ Playback failed\n");
  }

  return result;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s [options] file\n\n", prog);
  printf("Play audio files from the command line.\n\n");
  printf("Options:\n");
  printf("  -v volume      Set volume (0.0 to 1.0)\n");
  printf("  -t seconds     Play for specified seconds\n");
  printf("  -h, --help     Show this help\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s sound.mp3\n", prog);
  printf("  %s -v 0.5 music.wav\n", prog);
  printf("  %s -t 10 song.m4a\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  float volume = -1.0;
  int duration = -1;
  const char *file = NULL;

  /* Parse arguments */
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0) {
      if (i + 1 < argc) {
        volume = atof(argv[++i]);
        if (volume < 0.0 || volume > 1.0) {
          fprintf(stderr, "Error: Volume must be between 0.0 and 1.0\n");
          return 1;
        }
      } else {
        fprintf(stderr, "Error: -v requires volume argument\n");
        return 1;
      }
    } else if (strcmp(argv[i], "-t") == 0) {
      if (i + 1 < argc) {
        duration = atoi(argv[++i]);
        if (duration <= 0) {
          fprintf(stderr, "Error: Duration must be positive\n");
          return 1;
        }
      } else {
        fprintf(stderr, "Error: -t requires duration argument\n");
        return 1;
      }
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      file = argv[i];
    }
  }

  if (!file) {
    fprintf(stderr, "Error: No audio file specified\n");
    usage(argv[0]);
    return 1;
  }

  /* Check if file exists */
  if (access(file, F_OK) != 0) {
    fprintf(stderr, "Error: File not found: %s\n", file);
    return 1;
  }

  printf("=== CatBSD afplay Demo ===\n");
  return play_audio(file, volume, duration);
}
