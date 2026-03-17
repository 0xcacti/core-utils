#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
  bool force;               //-f
  bool interact;            // -i
  bool no_overwrite;        // -n
  bool verbose;             // -v
  bool dont_follow_symlink; // -h
} flags_t;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s\n", progname);
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
