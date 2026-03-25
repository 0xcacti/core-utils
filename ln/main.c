#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
  LINK_HARD,
  LINK_SYMBOLIC,
} link_mode_e;

typedef enum {
  SOURCE_SYMLINK_FOLLOW,
  SOURCE_SYMLINK_NO_FOLLOW,
} source_symlink_mode_e;

typedef enum {
  REPLACE_DEFAULT,
  REPLACE_FORCE,
  REPLACE_INTERACTIVE,
} replace_mode_e;

typedef struct {
  link_mode_e link_mode;             // default hard, -s => symbolic
  source_symlink_mode_e source_mode; // -L / -P, only meaningful for hard links
  replace_mode_e replace_mode;       // -f / -i
  bool verbose;                      // -v
  bool warn_dangling_source;         // -w, only meaningful with -s
  bool no_target_symlink_follow;     // -h, -n
  bool force_target_directory;       // -F, only meaningful with -s
} flags_t;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO,
          "usage: %s [-L | -P | -s [-F]] [-f | -iw] [-hnv] source_file [target_file]\n"
          "       %s [-L | -P | -s [-F]] [-f | -iw] [-hnv] source_file ... target_dir\n",
          progname, progname);
  exit(2);
}

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
}

static int classify_path(const char *path, bool is_target, bool *exists, bool *is_dir,
                         flags_t flags) {
  struct stat st;
  int r;
  if (is_target) {
    r = (flags.no_target_symlink_follow) ? lstat(path, &st) : stat(path, &st);
  } else {
    if (flags.source_mode == SOURCE_SYMLINK_FOLLOW) {
      r = stat(path, &st);
    } else {
      r = lstat(path, &st);
    }
  }

  if (r == 0) {
    *is_dir = S_ISDIR(st.st_mode);
    *exists = true;
    return 0;
  }

  if (errno == ENOENT) {
    *exists = false;
    *is_dir = false;
    return 0;
  }

  return -1;
}

int main(int argc, char *argv[]) {
  int ch;
  flags_t flags = {
      .link_mode = LINK_HARD,
      .source_mode = SOURCE_SYMLINK_FOLLOW,
      .replace_mode = REPLACE_DEFAULT,
      .verbose = false,
      .no_target_symlink_follow = false,
      .force_target_directory = false,
      .warn_dangling_source = false,
  };

  while ((ch = getopt(argc, argv, "LPsFfiwhnv")) != -1) {
    switch (ch) {
    case 'L':
      flags.source_mode = SOURCE_SYMLINK_FOLLOW;
      break;
    case 'P':
      flags.source_mode = SOURCE_SYMLINK_NO_FOLLOW;
      break;
    case 's':
      flags.link_mode = LINK_SYMBOLIC;
      break;
    case 'F':
      flags.force_target_directory = true;
      break;
    case 'f':
      flags.replace_mode = REPLACE_FORCE;
      flags.warn_dangling_source = false;
      break;
    case 'i':
      flags.replace_mode = REPLACE_INTERACTIVE;
      break;
    case 'w':
      flags.warn_dangling_source = true;
      break;
    case 'h':
    case 'n':
      flags.no_target_symlink_follow = true;
      break;
    case 'v':
      flags.verbose = true;
      break;
    default:
      usage(argv[0]);
    }
  }

  if (flags.force_target_directory == true && flags.replace_mode == REPLACE_DEFAULT) {
    flags.replace_mode = REPLACE_FORCE;
  }

  if (flags.link_mode == LINK_SYMBOLIC && flags.source_mode != SOURCE_SYMLINK_FOLLOW) {
    usage(argv[0]);
  }

  if (flags.link_mode != LINK_SYMBOLIC && flags.force_target_directory) {
    usage(argv[0]);
  }

  if (flags.link_mode != LINK_SYMBOLIC && flags.warn_dangling_source) {
    usage(argv[0]);
  }

  int num_args = argc - optind;
  if (num_args < 2) usage(argv[0]);
  bool exists = false;
  bool is_dir = false;
  if (classify_path(argv[argc - 1], true, &exists, &is_dir, flags) < 0) {
    error_errno(argv[0], argv[argc - 1]);
    exit(2);
  }

  if (num_args > 2 && !is_dir) usage(argv[0]);

  return 0;
}
