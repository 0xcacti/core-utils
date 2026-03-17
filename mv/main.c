#include <errno.h>
#include <getopt.h>
#include <limits.h>
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

typedef enum { SINGLE_TARGET; }
MODE_T;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO,
          "usage: %s [-f | -i | -n] [-hv] source target\n"
          "       %s [-f | -i | -n] [-v] source ... directory\n",
          progname, progname);
  exit(2);
}

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
}

static int check_is_dir(const char *path, bool *is_dir) {
  struct stat st;
  if (stat(path, &st) < 0) return -1;
  *is_dir = S_ISDIR(st.st_mode);
  return 0;
}

static int check_same_fs(const char *p1, const char *p2, bool *same) {
  struct stat st1;
  if (stat(p1, &st1) < 0) return -1;
  struct stat st2;
  if (stat(p2, &st2) < 0) return -1;

  *same = st1.st_dev == st2.st_dev;
  return 0;
}

// TODO: strip trailing slash
static int move_to_dest(const char *source, const char *dest) {
  bool is_dir = false;
  if (check_is_dir(dest, &is_dir) < 0) return -1;
  if (is_dir) {
    char buf[PATH_MAX];
    if (snprintf(buf, PATH_MAX, "%s/%s", dest, source) < 0) return -1;
    if (rename(source, buf) < 0) return -1;
  } else {
    if (rename(source, dest) < 0) return -1;
  }
  return 0;
}

int main(int argc, char **argv) {
  int ch;
  flags_t flags = {0};
  (void)flags;
  while ((ch = getopt(argc, argv, "finvh")) != -1) {
    switch (ch) {
    case 'f':
      flags.interactive = false;
      flags.no_overwrite = false;
      flags.force = true;
      break;
    case 'i':
      flags.force = false;
      flags.no_overwrite = false;
      flags.interactive = true;
      break;
    case 'n':
      flags.interactive = false;
      flags.force = false;
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
  int num_args = argc - optind;
  if (num_args < 2) usage(argv[0]);
  if (num_args > 2 && flags.dont_follow_symlink) usage(argv[0]);

  bool same_fs = false;
  if (check_same_fs(argv[optind], argv[optind + 1], &same_fs) < 0) {
    error_errno(argv[0], argv[optind]);
  }
  if (move_to_dest(argv[optind], argv[optind + 1]) < 0) {
    error_errno(argv[0], argv[optind]);
  }

  return ret;
}
