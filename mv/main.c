#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
  int standin;
} flags_t;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO,
          "usage: %s [-f | -i | -n] [-hv] source target\n"
          "       %s [-f | -i | -n] [-v] source ... directory\n",
          progname, progname);
  exit(2);
}

// static void error_errno(const char *progname, const char *filename) {
//   dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
// }

int main(int argc, char **argv) {
  int ch;
  flags_t flags = {0};
  (void)flags;
  while ((ch = getopt(argc, argv, "xhkasd:")) != -1) {
    switch (ch) {
    default:
      usage(argv[0]);
    }
  }

  int ret = 0;
  if (optind == argc) usage(argv[0]);
  for (int i = optind; i < argc; i++) {
  }
  return ret;
}
