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

typedef enum {
  MODE_EXACT_PATH,
  MODE_DIRECTORY,
} mode_e;

typedef enum {
  DEST_NONDIR,
  DEST_DIR,
  DEST_MISSING,
} dest_e;

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

static int check_exists(const char *path, bool *exists) {
  struct stat st;
  int r = stat(path, &st);
  if (r == 0) *exists = true;
  if (r == ENOENT) *exists = false;
  return -1;
}

// should work on files that exist
static int check_is_dir(const char *path, bool *is_dir) {
  struct stat st;
  int r = stat(path, &st);
  if (r == ENOENT) return -1;
  *is_dir = S_ISDIR(st.st_mode);
  return 0;
}

// static int check_same_fs(const char *p1, const char *p2, bool *same) {
//   struct stat st1;
//   if (stat(p1, &st1) < 0) return -1;
//   struct stat st2;
//   if (stat(p2, &st2) < 0) return -1;
//
//   *same = st1.st_dev == st2.st_dev;
//   return 0;
// }

// TODO: strip trailing slash
static int try_sfs_move_to_path(const char *source, const char *dest) {
  if (rename(source, dest) == 0) return 0;
  if (errno == EXDEV) return -2;
  return -1;
}

static int try_sfs_move_to_dir(const char *source, const char *dest) {
  char buf[PATH_MAX];
  if (snprintf(buf, PATH_MAX, "%s/%s", dest, source) < 0) return -1;
  return try_sfs_move_to_path(source, dest);
}

static int classify_dest(const char *path, dest_e *dest) {
  bool exists;
  if (check_exists(path, &exists) < 0) return -1;
  if (!exists) {
    *dest = DEST_MISSING;
    return 0;
  }

  bool is_dir;
  if (check_is_dir(path, &is_dir) < 0) return -1;
  if (!is_dir) {
    *dest = DEST_NONDIR;
    return 0;
  }

  *dest = DEST_DIR;
  return 0;
}

static int determine_mode(int num_args, const char *path, mode_e *mode) {
  dest_e dest;
  if (classify_dest(path, &dest) < 0) return -1;
  if (num_args > 2) {
    if (dest != DEST_DIR) {
      errno = ENOTDIR;
      return -1;
    }
    *mode = MODE_DIRECTORY;
    return 0;
  }

  if (dest == DEST_DIR) {
    *mode = MODE_DIRECTORY;
    return 0;
  }

  *mode = MODE_EXACT_PATH;
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

  int num_args = argc - optind;
  if (num_args < 2) usage(argv[0]);
  if (num_args > 2 && flags.dont_follow_symlink) usage(argv[0]);

  mode_e mode;
  if (determine_mode(num_args, argv[argc - 1], &mode) < 0) {
  }

  bool is_dir = false;
  if (check_is_dir(argv[argc - 1], &is_dir) < 0) {
    error_errno(argv[0], argv[argc - 1]);
    exit(2);
  }

  if (!is_dir && num_args > 2) {
    error_errno(argv[0], argv[argc - 1]); // TODO: make this a custom error message
    exit(2);
  }

  mode_e mode;
  if (determine_mode(num_args, argv[argc - 1], &mode) < 0) {
    error_errno(argv[0], argv[argc - 1]);
    exit(2);
  }

  // try same fs
  int ret = 0;
  if (is_dir) {
    ret = try_sfs_move_to_dir(argv[optind], argv[argc - 1]);
  } else {
    ret = try_sfs_move_to_path(argv[optind], argv[argc - 1]);
  }

  switch (ret) {
  case 0:
    return 0;
  case -1:
    error_errno(argv[0], argv[optind]);
    return -1;
  case -2:
    break;
  }

  return ret;
}
