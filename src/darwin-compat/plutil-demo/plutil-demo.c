/*
 * plutil-demo - Simplified Property List Utility for CatBSD
 * Demonstrates plist parsing without full CoreFoundation
 */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple plist value types */
typedef enum {
  PLIST_STRING,
  PLIST_INTEGER,
  PLIST_BOOLEAN,
  PLIST_DICT,
  PLIST_ARRAY
} plist_type_t;

/* Print XML plist in human-readable format */
static void print_plist_node(xmlNode *node, int indent) {
  xmlNode *cur = NULL;

  for (int i = 0; i < indent; i++)
    printf("  ");

  if (xmlStrcmp(node->name, (const xmlChar *)"key") == 0) {
    printf("Key: %s\n", xmlNodeGetContent(node));
  } else if (xmlStrcmp(node->name, (const xmlChar *)"string") == 0) {
    printf("String: %s\n", xmlNodeGetContent(node));
  } else if (xmlStrcmp(node->name, (const xmlChar *)"integer") == 0) {
    printf("Integer: %s\n", xmlNodeGetContent(node));
  } else if (xmlStrcmp(node->name, (const xmlChar *)"true") == 0) {
    printf("Boolean: true\n");
  } else if (xmlStrcmp(node->name, (const xmlChar *)"false") == 0) {
    printf("Boolean: false\n");
  } else if (xmlStrcmp(node->name, (const xmlChar *)"dict") == 0) {
    printf("Dictionary:\n");
    for (cur = node->children; cur; cur = cur->next) {
      if (cur->type == XML_ELEMENT_NODE) {
        print_plist_node(cur, indent + 1);
      }
    }
  } else if (xmlStrcmp(node->name, (const xmlChar *)"array") == 0) {
    printf("Array:\n");
    for (cur = node->children; cur; cur = cur->next) {
      if (cur->type == XML_ELEMENT_NODE) {
        print_plist_node(cur, indent + 1);
      }
    }
  }
}

/* Validate plist format */
static int validate_plist(const char *filename) {
  xmlDoc *doc = NULL;
  xmlNode *root = NULL;
  int valid = 0;

  /* Parse XML file */
  doc = xmlReadFile(filename, NULL, 0);
  if (doc == NULL) {
    fprintf(stderr, "Error: Could not parse file %s\n", filename);
    return 0;
  }

  /* Get root element */
  root = xmlDocGetRootElement(doc);

  /* Check if it's a plist */
  if (root && xmlStrcmp(root->name, (const xmlChar *)"plist") == 0) {
    printf("✓ %s: OK\n", filename);
    valid = 1;
  } else {
    fprintf(stderr, "✗ %s: Not a valid plist\n", filename);
  }

  xmlFreeDoc(doc);
  return valid;
}

/* Print plist contents */
static int print_plist(const char *filename) {
  xmlDoc *doc = NULL;
  xmlNode *root = NULL;

  doc = xmlReadFile(filename, NULL, 0);
  if (doc == NULL) {
    fprintf(stderr, "Error: Could not parse file %s\n", filename);
    return -1;
  }

  root = xmlDocGetRootElement(doc);
  if (root == NULL) {
    fprintf(stderr, "Error: Empty document\n");
    xmlFreeDoc(doc);
    return -1;
  }

  printf("\n=== Property List: %s ===\n\n", filename);

  /* Print all child nodes */
  xmlNode *cur = NULL;
  for (cur = root->children; cur; cur = cur->next) {
    if (cur->type == XML_ELEMENT_NODE) {
      print_plist_node(cur, 0);
    }
  }

  printf("\n");

  xmlFreeDoc(doc);
  return 0;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s [options] file.plist\n\n", prog);
  printf("Options:\n");
  printf("  -lint          Validate plist format\n");
  printf("  -p             Print plist in human-readable format\n");
  printf("  -help          Show this help\n");
  printf("\n");
  printf("Example:\n");
  printf("  %s -lint test.plist\n", prog);
  printf("  %s -p test.plist\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  /* Initialize libxml */
  LIBXML_TEST_VERSION

  const char *cmd = argv[1];

  if (strcmp(cmd, "-lint") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing filename\n");
      usage(argv[0]);
      return 1;
    }
    int result = validate_plist(argv[2]);
    xmlCleanupParser();
    return result ? 0 : 1;

  } else if (strcmp(cmd, "-p") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing filename\n");
      usage(argv[0]);
      return 1;
    }
    int result = print_plist(argv[2]);
    xmlCleanupParser();
    return result;

  } else if (strcmp(cmd, "-help") == 0) {
    usage(argv[0]);
    xmlCleanupParser();
    return 0;

  } else {
    fprintf(stderr, "Error: Unknown option '%s'\n", cmd);
    usage(argv[0]);
    xmlCleanupParser();
    return 1;
  }

  return 0;
}
