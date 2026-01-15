/*
 * defaults-demo - User Defaults System for CatBSD
 * Darwin-style preference management
 */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PREFS_DIR "Library/Preferences"
#define MAX_PATH 1024

/* Get preferences directory path */
static int get_prefs_dir(char *path, size_t len) {
  const char *home = getenv("HOME");
  if (!home) {
    fprintf(stderr, "Error: HOME not set\n");
    return -1;
  }

  snprintf(path, len, "%s/%s", home, PREFS_DIR);

  /* Create directory if it doesn't exist */
  struct stat st;
  if (stat(path, &st) != 0) {
    if (mkdir(path, 0755) != 0) {
      perror("mkdir");
      return -1;
    }
  }

  return 0;
}

/* Get plist file path for domain */
static int get_plist_path(const char *domain, char *path, size_t len) {
  char prefs_dir[MAX_PATH];
  if (get_prefs_dir(prefs_dir, sizeof(prefs_dir)) != 0) {
    return -1;
  }

  snprintf(path, len, "%s/%s.plist", prefs_dir, domain);
  return 0;
}

/* Create new plist document */
static xmlDoc *create_plist_doc(void) {
  xmlDoc *doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNode *root = xmlNewNode(NULL, BAD_CAST "plist");
  xmlNewProp(root, BAD_CAST "version", BAD_CAST "1.0");
  xmlDocSetRootElement(doc, root);

  xmlNode *dict = xmlNewChild(root, NULL, BAD_CAST "dict", NULL);

  return doc;
}

/* Find or create dict in plist */
static xmlNode *get_dict_node(xmlDoc *doc) {
  xmlNode *root = xmlDocGetRootElement(doc);
  if (!root)
    return NULL;

  for (xmlNode *node = root->children; node; node = node->next) {
    if (node->type == XML_ELEMENT_NODE &&
        xmlStrcmp(node->name, BAD_CAST "dict") == 0) {
      return node;
    }
  }

  return NULL;
}

/* Write a preference */
static int write_pref(const char *domain, const char *key, const char *value) {
  char path[MAX_PATH];
  if (get_plist_path(domain, path, sizeof(path)) != 0) {
    return -1;
  }

  xmlDoc *doc = NULL;

  /* Try to load existing plist */
  if (access(path, F_OK) == 0) {
    doc = xmlReadFile(path, NULL, 0);
  }

  /* Create new if doesn't exist */
  if (!doc) {
    doc = create_plist_doc();
  }

  xmlNode *dict = get_dict_node(doc);
  if (!dict) {
    fprintf(stderr, "Error: Invalid plist structure\n");
    xmlFreeDoc(doc);
    return -1;
  }

  /* Find and remove existing key */
  xmlNode *prev = NULL;
  for (xmlNode *node = dict->children; node; node = node->next) {
    if (node->type == XML_ELEMENT_NODE &&
        xmlStrcmp(node->name, BAD_CAST "key") == 0) {
      xmlChar *content = xmlNodeGetContent(node);
      if (xmlStrcmp(content, BAD_CAST key) == 0) {
        /* Remove key and following value */
        xmlNode *next = node->next;
        while (next && next->type != XML_ELEMENT_NODE) {
          next = next->next;
        }
        if (next) {
          xmlUnlinkNode(next);
          xmlFreeNode(next);
        }
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        xmlFree(content);
        break;
      }
      xmlFree(content);
    }
    prev = node;
  }

  /* Add new key-value pair */
  xmlNewChild(dict, NULL, BAD_CAST "key", BAD_CAST key);
  xmlNewChild(dict, NULL, BAD_CAST "string", BAD_CAST value);

  /* Save plist */
  if (xmlSaveFormatFileEnc(path, doc, "UTF-8", 1) < 0) {
    fprintf(stderr, "Error: Failed to save plist\n");
    xmlFreeDoc(doc);
    return -1;
  }

  xmlFreeDoc(doc);
  printf("✓ Set '%s' = '%s' in domain '%s'\n", key, value, domain);
  return 0;
}

/* Read a preference */
static int read_pref(const char *domain, const char *key) {
  char path[MAX_PATH];
  if (get_plist_path(domain, path, sizeof(path)) != 0) {
    return -1;
  }

  if (access(path, F_OK) != 0) {
    fprintf(stderr, "Domain '%s' does not exist\n", domain);
    return -1;
  }

  xmlDoc *doc = xmlReadFile(path, NULL, 0);
  if (!doc) {
    fprintf(stderr, "Error: Could not parse plist\n");
    return -1;
  }

  xmlNode *dict = get_dict_node(doc);
  if (!dict) {
    fprintf(stderr, "Error: Invalid plist structure\n");
    xmlFreeDoc(doc);
    return -1;
  }

  /* Find key */
  int found = 0;
  for (xmlNode *node = dict->children; node; node = node->next) {
    if (node->type == XML_ELEMENT_NODE &&
        xmlStrcmp(node->name, BAD_CAST "key") == 0) {
      xmlChar *content = xmlNodeGetContent(node);
      if (xmlStrcmp(content, BAD_CAST key) == 0) {
        /* Get next element (the value) */
        xmlNode *next = node->next;
        while (next && next->type != XML_ELEMENT_NODE) {
          next = next->next;
        }
        if (next) {
          xmlChar *value = xmlNodeGetContent(next);
          printf("%s\n", value);
          xmlFree(value);
          found = 1;
        }
        xmlFree(content);
        break;
      }
      xmlFree(content);
    }
  }

  xmlFreeDoc(doc);

  if (!found) {
    fprintf(stderr, "Key '%s' not found in domain '%s'\n", key, domain);
    return -1;
  }

  return 0;
}

/* Delete a preference */
static int delete_pref(const char *domain, const char *key) {
  char path[MAX_PATH];
  if (get_plist_path(domain, path, sizeof(path)) != 0) {
    return -1;
  }

  if (access(path, F_OK) != 0) {
    fprintf(stderr, "Domain '%s' does not exist\n", domain);
    return -1;
  }

  xmlDoc *doc = xmlReadFile(path, NULL, 0);
  if (!doc) {
    fprintf(stderr, "Error: Could not parse plist\n");
    return -1;
  }

  xmlNode *dict = get_dict_node(doc);
  if (!dict) {
    fprintf(stderr, "Error: Invalid plist structure\n");
    xmlFreeDoc(doc);
    return -1;
  }

  /* Find and remove key */
  int found = 0;
  for (xmlNode *node = dict->children; node; node = node->next) {
    if (node->type == XML_ELEMENT_NODE &&
        xmlStrcmp(node->name, BAD_CAST "key") == 0) {
      xmlChar *content = xmlNodeGetContent(node);
      if (xmlStrcmp(content, BAD_CAST key) == 0) {
        /* Remove key and following value */
        xmlNode *next = node->next;
        while (next && next->type != XML_ELEMENT_NODE) {
          next = next->next;
        }
        if (next) {
          xmlUnlinkNode(next);
          xmlFreeNode(next);
        }
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        found = 1;
        xmlFree(content);
        break;
      }
      xmlFree(content);
    }
  }

  if (!found) {
    fprintf(stderr, "Key '%s' not found in domain '%s'\n", key, domain);
    xmlFreeDoc(doc);
    return -1;
  }

  /* Save plist */
  if (xmlSaveFormatFileEnc(path, doc, "UTF-8", 1) < 0) {
    fprintf(stderr, "Error: Failed to save plist\n");
    xmlFreeDoc(doc);
    return -1;
  }

  xmlFreeDoc(doc);
  printf("✓ Deleted '%s' from domain '%s'\n", key, domain);
  return 0;
}

/* List all domains */
static int list_domains(void) {
  char prefs_dir[MAX_PATH];
  if (get_prefs_dir(prefs_dir, sizeof(prefs_dir)) != 0) {
    return -1;
  }

  char cmd[MAX_PATH + 50];
  snprintf(cmd, sizeof(cmd),
           "ls -1 %s/*.plist 2>/dev/null | sed 's/.*\\///;s/\\.plist$//'",
           prefs_dir);

  printf("Preference domains:\n");
  system(cmd);

  return 0;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s <command> <domain> [key] [value]\n\n", prog);
  printf("Commands:\n");
  printf("  write <domain> <key> <value>  - Write a preference\n");
  printf("  read <domain> <key>            - Read a preference\n");
  printf("  delete <domain> <key>          - Delete a preference\n");
  printf("  domains                        - List all domains\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s write com.catbsd.app theme dark\n", prog);
  printf("  %s read com.catbsd.app theme\n", prog);
  printf("  %s delete com.catbsd.app theme\n", prog);
  printf("  %s domains\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  LIBXML_TEST_VERSION

  if (argc < 2) {
    usage(argv[0]);
    xmlCleanupParser();
    return 1;
  }

  const char *cmd = argv[1];
  int result = 0;

  if (strcmp(cmd, "write") == 0) {
    if (argc < 5) {
      fprintf(stderr, "Error: write requires domain, key, and value\n");
      usage(argv[0]);
      result = 1;
    } else {
      result = write_pref(argv[2], argv[3], argv[4]);
    }
  } else if (strcmp(cmd, "read") == 0) {
    if (argc < 4) {
      fprintf(stderr, "Error: read requires domain and key\n");
      usage(argv[0]);
      result = 1;
    } else {
      result = read_pref(argv[2], argv[3]);
    }
  } else if (strcmp(cmd, "delete") == 0) {
    if (argc < 4) {
      fprintf(stderr, "Error: delete requires domain and key\n");
      usage(argv[0]);
      result = 1;
    } else {
      result = delete_pref(argv[2], argv[3]);
    }
  } else if (strcmp(cmd, "domains") == 0) {
    result = list_domains();
  } else {
    fprintf(stderr, "Error: Unknown command '%s'\n", cmd);
    usage(argv[0]);
    result = 1;
  }

  xmlCleanupParser();
  return result;
}
