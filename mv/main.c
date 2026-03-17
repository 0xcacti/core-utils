#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
  bool force;               // -f
  bool interactive;         // -i
  bool no_overwrite;        // -n
  bool verbose;             // -v
  bool dont_follow_symlink; // -h
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
  while ((ch = getopt(argc, argv, "finvh")) != -1) {
    switch (ch) {
    case 'f':
      flags.force = true;
      break;
    case 'i':
      flags.interactive = true;
      break;
    case 'n':
      flags.no_overwrite = true;
      break;
    case 'v':
      flags.verbose = true;
      break;
    case 'h':
      flags.dont_follow_symlink = true;
      break;
    default:
      usage(argv[0]);
    }
  }

  int ret = 0;
  if (optind == argc) usage(argv[0]);
  if (optind + 1 < argc && flags.dont_follow_symlink) usage(argv[0]);

  return ret;
}
