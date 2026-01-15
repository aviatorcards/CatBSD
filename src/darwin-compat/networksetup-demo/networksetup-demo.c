/*
 * networksetup-demo - Network Configuration Utility for CatBSD
 * Darwin-style network management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* List network services */
static int list_services(void) {
  printf("Network Services:\n");

  /* Use networksetup on macOS */
  int result =
      system("networksetup -listallnetworkservices 2>/dev/null | tail -n +2");

  if (result != 0) {
    /* Fallback: list network interfaces */
    printf("  Wi-Fi\n");
    printf("  Ethernet\n");
  }

  return 0;
}

/* Get network info */
static int get_info(const char *service) {
  char cmd[512];

  printf("Network Service: %s\n\n", service);

  /* Try to get info from networksetup */
  snprintf(cmd, sizeof(cmd), "networksetup -getinfo '%s' 2>/dev/null", service);
  int result = system(cmd);

  if (result != 0) {
    printf("DHCP Configuration\n");
    printf("IP address: (dynamic)\n");
    printf("Subnet mask: (dynamic)\n");
    printf("Router: (dynamic)\n");
  }

  return 0;
}

/* Get DNS servers */
static int get_dns(const char *service) {
  char cmd[512];

  printf("DNS Servers for %s:\n", service);

  snprintf(cmd, sizeof(cmd), "networksetup -getdnsservers '%s' 2>/dev/null",
           service);
  int result = system(cmd);

  if (result != 0) {
    printf("  (using DHCP)\n");
  }

  return 0;
}

/* Usage */
static void usage(const char *prog) {
  printf("Usage: %s <command> [args]\n\n", prog);
  printf("Network configuration utility.\n\n");
  printf("Commands:\n");
  printf("  -listallnetworkservices          List all network services\n");
  printf("  -getinfo <service>                Get network service info\n");
  printf("  -getdnsservers <service>          Get DNS servers\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s -listallnetworkservices\n", prog);
  printf("  %s -getinfo Wi-Fi\n", prog);
  printf("  %s -getdnsservers Wi-Fi\n", prog);
  printf("\n");
}

/* Main */
int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  const char *cmd = argv[1];

  if (strcmp(cmd, "-listallnetworkservices") == 0) {
    return list_services();

  } else if (strcmp(cmd, "-getinfo") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing service argument\n");
      return 1;
    }
    return get_info(argv[2]);

  } else if (strcmp(cmd, "-getdnsservers") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing service argument\n");
      return 1;
    }
    return get_dns(argv[2]);

  } else if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
    usage(argv[0]);
    return 0;

  } else {
    fprintf(stderr, "Error: Unknown command '%s'\n", cmd);
    usage(argv[0]);
    return 1;
  }

  return 0;
}
