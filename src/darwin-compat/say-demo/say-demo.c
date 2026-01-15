/*
 * say-demo - Simplified Text-to-Speech for CatBSD
 * Demonstrates audio output without full Speech framework
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Available voices (simplified) */
static const char *voices[] = {"Alex", "Samantha", "Victoria", "Daniel", NULL};

/* List available voices */
static void list_voices(void) {
  printf("Available voices:\n");
  for (int i = 0; voices[i] != NULL; i++) {
    printf("  %s\n", voices[i]);
  }
}

/* Speak text using macOS 'say' command (for demo) */
static int speak_text(const char *text, const char *voice) {
  char command[1024];

  printf("🔊 Speaking: \"%s\"\n", text);

  if (voice) {
    printf("   Voice: %s\n", voice);
    snprintf(command, sizeof(command), "say -v '%s' '%s'", voice, text);
  } else {
    snprintf(command, sizeof(command), "say '%s'", text);
  }

  /* Execute system say command */
  int result = system(command);

  if (result == 0) {
    printf("✓ Speech completed\n");
  } else {
    fprintf(stderr, "✗ Speech failed\n");
  }

  return result;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s [options] \"text to speak\"\n\n", prog);
  printf("Options:\n");
  printf("  -v voice       Use specified voice\n");
  printf("  --voice-list   List available voices\n");
  printf("  -h, --help     Show this help\n");
  printf("\n");
  printf("Example:\n");
  printf("  %s \"Hello from CatBSD!\"\n", prog);
  printf("  %s -v Alex \"CatBSD is purring to life\"\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  const char *voice = NULL;
  const char *text = NULL;

  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  /* Parse arguments */
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0) {
      if (i + 1 < argc) {
        voice = argv[++i];
      } else {
        fprintf(stderr, "Error: -v requires a voice name\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--voice-list") == 0) {
      list_voices();
      return 0;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      /* Assume it's the text to speak */
      text = argv[i];
      break;
    }
  }

  if (text == NULL) {
    fprintf(stderr, "Error: No text provided\n");
    usage(argv[0]);
    return 1;
  }

  /* Validate voice if specified */
  if (voice) {
    int valid = 0;
    for (int i = 0; voices[i] != NULL; i++) {
      if (strcasecmp(voice, voices[i]) == 0) {
        valid = 1;
        break;
      }
    }
    if (!valid) {
      fprintf(stderr, "Warning: Voice '%s' not in list, trying anyway...\n",
              voice);
    }
  }

  printf("=== CatBSD say Demo ===\n");
  printf("Text-to-Speech using Darwin compatibility\n\n");

  return speak_text(text, voice);
}
