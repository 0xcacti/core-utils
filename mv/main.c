#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static void error_msg(const char *progname, const char *thing, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, thing, msg);
}

static bool has_trailing_slash(const char *path) {
  size_t len = strlen(path);
  if (len == 0) return false;
  if (strcmp(path, "/") == 0) return false;
  return path[len - 1] == '/';
}

static int check(const char *path, bool *exists, bool *is_dir, flags_t flags) {
  struct stat st;
  int r;
  if (!flags.dont_follow_symlink) {
    r = stat(path, &st);
  } else {
    r = lstat(path, &st);
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

static int prompt(FILE *tty, const char *dest, bool *dont_overwrite) {
  fprintf(tty, "overwrite %s? (y/n) [n] ", dest);
  fflush(tty);
  int ch, first;
  first = ch = fgetc(tty);
  while (ch != '\n' && ch != EOF) ch = fgetc(tty);
  *dont_overwrite = (first == 'n' || first == 'N');
  return 0;
}

static void confirm_no_overwrite(FILE *tty) {
  fwrite("not overwritten\n", 1, 16, tty);
}

static int try_sfs_move_to_path(const char *source, const char *dest, flags_t flags) {
  bool is_dir;
  bool exists;
  if (check(dest, &exists, &is_dir, flags) < 0) return -1;

  if (exists) {
    if (flags.interactive) {
      bool dont_overwrite = false;
      FILE *tty = fopen("/dev/tty", "r+");
      if (tty == NULL) return -2;
      prompt(tty, dest, &dont_overwrite);
      if (dont_overwrite) {
        confirm_no_overwrite(tty);
        fclose(tty);
        return 0;
      }
      fclose(tty);
    } else if (flags.no_overwrite) {
      return 0;
    } else if (flags.force) {
      // Intentionally empty - rename by default is via force
    }
  }

  if (rename(source, dest) == 0) {
    if (flags.verbose) fprintf(stdout, "%s -> %s\n", source, dest);
    return 0;
  }
  if (errno == EXDEV) return -3;
  return -1;
}

static int try_sfs_move_to_dir(const char *source, const char *dest, flags_t flags) {
  size_t len = strlen(source);
  char source_copy[len + 1];
  memcpy(source_copy, source, len + 1);

  char *name = basename(source_copy);
  if (!name) {
    errno = EINVAL;
    return -1;
  }

  char buf[PATH_MAX];
  int n = snprintf(buf, PATH_MAX, "%s/%s", dest, name);
  if (n < 0 || n >= PATH_MAX) {
    errno = ENAMETOOLONG;
    return -1;
  }
  return try_sfs_move_to_path(source, buf, flags);
}

static int copy_file_cross_dest(const char *source, const char *dest) {
  FILE *s_file = fopen(source, "r");
}

static int try_dfs_move_to_path(const char *source, const char *dest, flags_t flags) {
  bool is_dir;
  bool exists;
  if (check(dest, &exists, &is_dir, flags) < 0) return -1;

  if (exists) {
    if (flags.interactive) {
      bool dont_overwrite = false;
      FILE *tty = fopen("/dev/tty", "r+");
      if (tty == NULL) return -2;
      prompt(tty, dest, &dont_overwrite);
      if (dont_overwrite) {
        confirm_no_overwrite(tty);
        fclose(tty);
        return 0;
      }
      fclose(tty);
    } else if (flags.no_overwrite) {
      return 0;
    } else if (flags.force) {
      // Intentionally empty - rename by default is via force
    }
  }

  if (rename(source, dest) == 0) {
    if (flags.verbose) fprintf(stdout, "%s -> %s\n", source, dest);
    return 0;
  }
  if (errno == EXDEV) return -3;
  return -1;
}

// static int try_sfs_move_to_dir(const char *source, const char *dest, flags_t flags) {
//   size_t len = strlen(source);
//   char source_copy[len + 1];
//   memcpy(source_copy, source, len + 1);
//
//   char *name = basename(source_copy);
//   if (!name) {
//     errno = EINVAL;
//     return -1;
//   }
//
//   char buf[PATH_MAX];
//   int n = snprintf(buf, PATH_MAX, "%s/%s", dest, name);
//   if (n < 0 || n >= PATH_MAX) {
//     errno = ENAMETOOLONG;
//     return -1;
//   }
//   return try_sfs_move_to_path(source, buf, flags);
// }

static int classify_dest(const char *path, dest_e *dest, flags_t flags) {
  bool exists;
  bool is_dir;
  if (check(path, &exists, &is_dir, flags) < 0) return -1;
  if (!exists) {
    *dest = DEST_MISSING;
    return 0;
  }
  if (!is_dir) {
    *dest = DEST_NONDIR;
    return 0;
  }

  *dest = DEST_DIR;
  return 0;
}

static int determine_mode(int num_args, const char *path, mode_e *mode, flags_t flags) {
  dest_e dest;
  if (classify_dest(path, &dest, flags) < 0) return -1;
  if (num_args > 2) {
    if (dest != DEST_DIR) {
      errno = ENOTDIR;
      return -1;
    }
    *mode = MODE_DIRECTORY;
    return 0;
  }

  bool trailing_slash = has_trailing_slash(path);
  if (trailing_slash) {
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
  if (determine_mode(num_args, argv[argc - 1], &mode, flags) < 0) {
    error_errno(argv[0], argv[argc - 1]);
    exit(2);
  }

  int ret = 0;
  for (int i = optind; i < argc - 1; i++) {
    int r = 0;

    if (mode == MODE_DIRECTORY) {
      r = try_sfs_move_to_dir(argv[i], argv[argc - 1], flags);
    } else if (mode == MODE_EXACT_PATH) {
      r = try_sfs_move_to_path(argv[i], argv[argc - 1], flags);
    }
    if (r == -1) {
      ret = 1;
      error_errno(argv[0], argv[i]);
    }
    if (r == -2) {
      ret = 1;
      error_msg(argv[0], "/dev/tty", "failed to open");
    }
    if (r == -3) {
      error_msg(argv[0], "cross-device not yet implemented", argv[i]);
      ret = 1;
    }
  }

  return ret;
}
