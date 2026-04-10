#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/unistd.h>
#include <unistd.h>

typedef enum {
  LINK_OK,
  LINK_WARN,
  LINK_EXISTS,
  LINK_ERRNO,
  LINK_SKIPPED,
} link_result_e;

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

typedef struct {
  bool exists;
  bool is_symlink;
  bool is_dir_nofollow;
  bool is_dir_follow;
} path_class_t;

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

static void error_msg(const char *progname, const char *m1, const char *m2) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, m1, m2);
}

static bool target_acts_as_dir(const path_class_t *class, const flags_t flags) {
  if (class->exists != true) return false;
  if (flags.no_target_symlink_follow == true) return class->is_dir_nofollow;
  return class->is_dir_follow;
}

static int classify_path(const char *path, path_class_t *class) {
  struct stat st_nofollow;
  if (lstat(path, &st_nofollow) < 0) {
    if (errno == ENOENT) {
      return 0;
    }
    return -1;
  }
  class->exists = true;
  class->is_symlink = S_ISLNK(st_nofollow.st_mode);
  class->is_dir_nofollow = S_ISDIR(st_nofollow.st_mode);

  struct stat st_follow;
  if (stat(path, &st_follow) < 0) {
    if (errno == ENOENT && class->is_symlink == true) {
      return 0;
    }
    return -1;
  }

  class->is_dir_follow = S_ISDIR(st_follow.st_mode);
  return 0;
}

static bool can_force_replace_dir(const path_class_t *class, const flags_t flags) {
  if (!class->exists) return false;
  if (flags.link_mode != LINK_SYMBOLIC) return false;
  if (!flags.force_target_directory) return false;
  return target_acts_as_dir(class, flags);
}

static int prompt(const char *dest, bool *overwrite) {
  fprintf(stderr, "replace %s? ", dest);
  fflush(stderr);
  int ch, first;
  first = ch = fgetc(stdin);
  while (ch != '\n' && ch != EOF) ch = fgetc(stdin);
  *overwrite = (first == 'y' || first == 'Y');
  return 0;
}

static link_result_e ln_at_path(const char *source, const char *resolved_dest, flags_t flags) {
  path_class_t class = {0};
  if (flags.replace_mode != REPLACE_DEFAULT) {
    if (classify_path(resolved_dest, &class) < 0) return LINK_ERRNO;
    if (class.exists) {
      switch (flags.replace_mode) {
      case REPLACE_DEFAULT:
        break;
      case REPLACE_INTERACTIVE: {
        bool dont_overwrite = false;
        prompt(resolved_dest, &dont_overwrite);
        if (!dont_overwrite) {
          fwrite("not replaced\n", 1, 13, stderr);
          return LINK_SKIPPED;
        }
      }
        // Intentionally fallthrough
      case REPLACE_FORCE: {
        bool dest_is_dir = target_acts_as_dir(&class, flags);
        bool can_replace_dir = can_force_replace_dir(&class, flags);
        if (can_replace_dir) {
          if (rmdir(resolved_dest) < 0) return LINK_ERRNO;
        } else if (!dest_is_dir) {
          if (unlink(resolved_dest) < 0) return LINK_ERRNO;
        }
        break;
      }
      }
    }
  }

  int r;
  link_result_e ret = LINK_OK;

  if (flags.link_mode == LINK_HARD) {
    switch (flags.source_mode) {
    case SOURCE_SYMLINK_FOLLOW:
      r = linkat(AT_FDCWD, source, AT_FDCWD, resolved_dest, AT_SYMLINK_FOLLOW);
      break;
    case SOURCE_SYMLINK_NO_FOLLOW:
      r = link(source, resolved_dest);
      break;
    }
  } else if (flags.link_mode == LINK_SYMBOLIC) {
    if (flags.warn_dangling_source) {
      struct stat st;
      errno = 0;
      if (stat(source, &st) < 0) {
        if (errno == ENOENT) {
          ret = LINK_WARN;
        }
        ret = LINK_ERRNO;
      }
    }
    r = symlink(source, resolved_dest);
  } else {
    fprintf(stderr, "invalid link mode\n");
    exit(1);
  }

  if (r < 0 && ret != LINK_OK) {
    if (errno == EEXIST) return LINK_EXISTS;
    return ret;
  }

  return LINK_OK;
}

static link_result_e ln_exact_path(const char *source, const char *dest,
                                   char attempted_dest[PATH_MAX], flags_t flags) {
  int n = snprintf(attempted_dest, PATH_MAX, "%s", dest);
  if (n < 0 || n >= PATH_MAX) {
    errno = ENAMETOOLONG;
    return LINK_ERRNO;
  }
  return ln_at_path(source, attempted_dest, flags);
}

static link_result_e ln_target_dir(const char *source, const char *dest,
                                   char attempted_dest[PATH_MAX], flags_t flags) {
  size_t len = strlen(source);
  char source_copy[len + 1];
  memcpy(source_copy, source, len + 1);

  char *name = basename(source_copy);
  if (!name) return LINK_ERRNO;

  int n = snprintf(attempted_dest, PATH_MAX, "%s/%s", dest, name);
  if (n < 0 || n >= PATH_MAX) {
    errno = ENAMETOOLONG;
    return LINK_ERRNO;
  }

  return ln_at_path(source, attempted_dest, flags);
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
  path_class_t class = {0};
  if (classify_path(argv[argc - 1], &class) < 0) {
    error_errno(argv[0], argv[argc - 1]);
    exit(2);
  }

  bool is_dir = target_acts_as_dir(&class, flags);
  if (num_args > 2 && !is_dir) usage(argv[0]);

  int ret = 0;
  for (int i = optind; i < argc - 1; i++) {
    link_result_e r;
    char attempted_dest[PATH_MAX];
    if (is_dir) {
      r = ln_target_dir(argv[i], argv[argc - 1], attempted_dest, flags);
    } else {
      r = ln_exact_path(argv[i], argv[argc - 1], attempted_dest, flags);
    }

    switch (r) {
    case LINK_OK:
      if (flags.verbose) {
        fprintf(stdout, "%s => %s\n", attempted_dest, argv[i]);
      }
      break;
    case LINK_ERRNO:
      ret = 1;
      error_errno(argv[0], attempted_dest);
      break;
    case LINK_EXISTS:
      ret = 1;
      error_msg(argv[0], attempted_dest, "File exists");
      break;
    case LINK_SKIPPED:
      break;
    case LINK_WARN:
      error_msg(argv[0], source,
    }
  }

  return ret;
}
