/*
 * ditto-demo - File Copy Utility for CatBSD
 * Darwin-style file/directory copying with metadata preservation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Copy file or directory */
static int copy_path(const char *src, const char *dst, int preserve) {
  char cmd[2048];

  printf("📋 Copying: %s → %s\n", src, dst);

  if (preserve) {
    printf("   Mode: Preserve metadata\n");
  }

  /* Check if source exists */
  struct stat st;
  if (stat(src, &st) != 0) {
    fprintf(stderr, "Error: Source not found: %s\n", src);
    return -1;
  }

  /* Build ditto command */
  if (S_ISDIR(st.st_mode)) {
    /* Directory copy - use cp -R */
    if (preserve) {
      snprintf(cmd, sizeof(cmd), "cp -Rp '%s' '%s'", src, dst);
    } else {
      snprintf(cmd, sizeof(cmd), "cp -R '%s' '%s'", src, dst);
    }
  } else {
    /* File copy */
    if (preserve) {
      snprintf(cmd, sizeof(cmd), "cp -p '%s' '%s'", src, dst);
    } else {
      snprintf(cmd, sizeof(cmd), "cp '%s' '%s'", src, dst);
    }
  }

  /* Execute */
  int result = system(cmd);

  if (result == 0) {
    printf("✓ Copy complete\n");
  } else {
    fprintf(stderr, "✗ Copy failed\n");
  }

  return result;
}

/* Create archive */
static int create_archive(const char *src, const char *dst) {
  char cmd[2048];

  printf("📦 Creating archive: %s → %s\n", src, dst);

  /* Use zip for archive creation */
  // Original: snprintf(cmd, sizeof(cmd), "cd '%s' && zip -r '%s' . > /dev/null
  // 2>&1", src, dst); Fix: Use zip directly with src and dst paths, so dst is
  // relative to current working directory
  snprintf(cmd, sizeof(cmd), "zip -r '%s' '%s' > /dev/null 2>&1", dst, src);

  int result = system(cmd);

  if (result == 0) {
    printf("✓ Archive created\n");
  } else {
    fprintf(stderr, "✗ Archive creation failed\n");
  }

  return result;
}

/* Extract archive */
static int extract_archive(const char *src, const char *dst) {
  char cmd[2048];

  printf("📦 Extracting archive: %s → %s\n", src, dst);

  /* Create destination directory */
  mkdir(dst, 0755);

  /* Use unzip for extraction */
  snprintf(cmd, sizeof(cmd), "unzip -q '%s' -d '%s'", src, dst);

  int result = system(cmd);

  if (result == 0) {
    printf("✓ Archive extracted\n");
  } else {
    fprintf(stderr, "✗ Extraction failed\n");
  }

  return result;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s [options] src dst\n\n", prog);
  printf("Copy files and directories, preserving metadata.\n\n");
  printf("Options:\n");
  printf("  -c -k src dst    Create zip archive\n");
  printf("  -x -k src dst    Extract zip archive\n");
  printf("  (no options)     Copy preserving metadata\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s file.txt copy.txt\n", prog);
  printf("  %s dir1 dir2\n", prog);
  printf("  %s -c -k mydir archive.zip\n", prog);
  printf("  %s -x -k archive.zip extracted/\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  int create = 0;
  int extract = 0;
  const char *src = NULL;
  const char *dst = NULL;

  if (argc < 3) {
    usage(argv[0]);
    return 1;
  }

  /* Parse arguments */
  int i = 1;
  while (i < argc) {
    if (strcmp(argv[i], "-c") == 0) {
      create = 1;
      i++;
    } else if (strcmp(argv[i], "-x") == 0) {
      extract = 1;
      i++;
    } else if (strcmp(argv[i], "-k") == 0) {
      /* Archive flag - just skip */
      i++;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else if (!src) {
      src = argv[i];
      i++;
    } else if (!dst) {
      dst = argv[i];
      i++;
    } else {
      i++;
    }
  }

  if (!src || !dst) {
    fprintf(stderr, "Error: Missing source or destination\n");
    usage(argv[0]);
    return 1;
  }

  printf("=== CatBSD ditto Demo ===\n");

  if (create) {
    return create_archive(src, dst);
  } else if (extract) {
    return extract_archive(src, dst);
  } else {
    return copy_path(src, dst, 1);
  }
}
