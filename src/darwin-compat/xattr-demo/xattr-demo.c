/*
 * xattr-demo - Extended Attributes Utility for CatBSD
 * Darwin-style extended attribute management
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __FreeBSD__
#include "freebsd_xattr.h"
#include <sys/extattr.h>
#include <sys/types.h>
/* FreeBSD uses different xattr API - using compatibility wrapper */
#define ENOATTR ENOENT
#else
#include <sys/xattr.h>
#endif

#define MAX_ATTR_SIZE 65536

/* List all extended attributes */
static int list_attrs(const char *path) {
  ssize_t bufsize = listxattr(path, NULL, 0, 0);
  if (bufsize < 0) {
    perror("listxattr");
    return -1;
  }

  if (bufsize == 0) {
    printf("No extended attributes for: %s\n", path);
    return 0;
  }

  char *buf = malloc(bufsize);
  if (!buf) {
    perror("malloc");
    return -1;
  }

  bufsize = listxattr(path, buf, bufsize, 0);
  if (bufsize < 0) {
    perror("listxattr");
    free(buf);
    return -1;
  }

  printf("Extended attributes for: %s\n", path);

  char *attr = buf;
  while (attr < buf + bufsize) {
    printf("  %s\n", attr);
    attr += strlen(attr) + 1;
  }

  free(buf);
  return 0;
}

/* Print attribute value */
static int print_attr(const char *path, const char *attr) {
  ssize_t valsize = getxattr(path, attr, NULL, 0, 0, 0);
  if (valsize < 0) {
    if (errno == ENOATTR) {
      fprintf(stderr, "Attribute '%s' not found\n", attr);
    } else {
      perror("getxattr");
    }
    return -1;
  }

  if (valsize > MAX_ATTR_SIZE) {
    fprintf(stderr, "Attribute too large: %zd bytes\n", valsize);
    return -1;
  }

  char *value = malloc(valsize + 1);
  if (!value) {
    perror("malloc");
    return -1;
  }

  valsize = getxattr(path, attr, value, valsize, 0, 0);
  if (valsize < 0) {
    perror("getxattr");
    free(value);
    return -1;
  }

  value[valsize] = '\0';
  printf("%s\n", value);

  free(value);
  return 0;
}

/* Write attribute */
static int write_attr(const char *path, const char *attr, const char *value) {
  if (setxattr(path, attr, value, strlen(value), 0, 0) < 0) {
    perror("setxattr");
    return -1;
  }

  printf("✓ Set attribute '%s' on %s\n", attr, path);
  return 0;
}

/* Delete attribute */
static int delete_attr(const char *path, const char *attr) {
  if (removexattr(path, attr, 0) < 0) {
    if (errno == ENOATTR) {
      fprintf(stderr, "Attribute '%s' not found\n", attr);
    } else {
      perror("removexattr");
    }
    return -1;
  }

  printf("✓ Deleted attribute '%s' from %s\n", attr, path);
  return 0;
}

/* Clear all attributes */
static int clear_attrs(const char *path) {
  ssize_t bufsize = listxattr(path, NULL, 0, 0);
  if (bufsize < 0) {
    perror("listxattr");
    return -1;
  }

  if (bufsize == 0) {
    printf("No attributes to clear\n");
    return 0;
  }

  char *buf = malloc(bufsize);
  if (!buf) {
    perror("malloc");
    return -1;
  }

  bufsize = listxattr(path, buf, bufsize, 0);
  if (bufsize < 0) {
    perror("listxattr");
    free(buf);
    return -1;
  }

  int count = 0;
  char *attr = buf;
  while (attr < buf + bufsize) {
    if (removexattr(path, attr, 0) == 0) {
      count++;
    }
    attr += strlen(attr) + 1;
  }

  printf("✓ Cleared %d attributes from %s\n", count, path);
  free(buf);
  return 0;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s [options] file\n\n", prog);
  printf("Options:\n");
  printf("  -l             List all attributes\n");
  printf("  -p attr        Print attribute value\n");
  printf("  -w attr value  Write attribute\n");
  printf("  -d attr        Delete attribute\n");
  printf("  -c             Clear all attributes\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s -l file.txt\n", prog);
  printf("  %s -w com.catbsd.tag \"important\" file.txt\n", prog);
  printf("  %s -p com.catbsd.tag file.txt\n", prog);
  printf("  %s -d com.catbsd.tag file.txt\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  const char *opt = argv[1];

  if (strcmp(opt, "-l") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing file argument\n");
      return 1;
    }
    return list_attrs(argv[2]);

  } else if (strcmp(opt, "-p") == 0) {
    if (argc < 4) {
      fprintf(stderr, "Error: Missing attribute or file argument\n");
      return 1;
    }
    return print_attr(argv[3], argv[2]);

  } else if (strcmp(opt, "-w") == 0) {
    if (argc < 5) {
      fprintf(stderr, "Error: Missing attribute, value, or file argument\n");
      return 1;
    }
    return write_attr(argv[4], argv[2], argv[3]);

  } else if (strcmp(opt, "-d") == 0) {
    if (argc < 4) {
      fprintf(stderr, "Error: Missing attribute or file argument\n");
      return 1;
    }
    return delete_attr(argv[3], argv[2]);

  } else if (strcmp(opt, "-c") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing file argument\n");
      return 1;
    }
    return clear_attrs(argv[2]);

  } else if (strcmp(opt, "-h") == 0 || strcmp(opt, "--help") == 0) {
    usage(argv[0]);
    return 0;

  } else {
    fprintf(stderr, "Error: Unknown option '%s'\n", opt);
    usage(argv[0]);
    return 1;
  }

  return 0;
}
