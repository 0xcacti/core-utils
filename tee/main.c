#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "Usage: %s [-achm] [-t [[CC]YY]MMDDhhmm[.SS]] [-r file] file ...\n",
          progname);
  exit(2);
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
  exit(2);
}

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
  exit(2);
}

int main(int argc, char **argv) {
  int opt;
  bool opt_a = false;
  bool opt_i = false;
  while ((opt = getopt(argc, argv, "ai")) != -1) {
    switch (opt) {
    case 'a':
      opt_a = true;
      break;
    case 'i':
      opt_i = true;
      break;
    default:
      usage(argv[0]);
    }
  }
  return 0;
}
