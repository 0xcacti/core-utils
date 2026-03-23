#include <errno.h>
#include <fcntl.h>
#include <fts.h>
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

typedef enum {
  MOVE_OK,
  MOVE_ERRNO,
  MOVE_TTY_OPEN,
  MOVE_EXDEV,
} move_result_e;

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

static move_result_e try_sfs_move_to_path(const char *source, const char *dest, flags_t flags) {
  bool is_dir;
  bool exists;
  if (check(dest, &exists, &is_dir, flags) < 0) return MOVE_ERRNO;

  if (exists) {
    if (flags.interactive) {
      bool dont_overwrite = false;
      FILE *tty = fopen("/dev/tty", "r+");
      if (tty == NULL) return MOVE_TTY_OPEN;
      prompt(tty, dest, &dont_overwrite);
      if (dont_overwrite) {
        confirm_no_overwrite(tty);
        fclose(tty);
        return MOVE_OK;
      }
      fclose(tty);
    } else if (flags.no_overwrite) {
      return MOVE_OK;
    } else if (flags.force) {
      // Intentionally empty - rename by default is via force
    }
  }

  if (rename(source, dest) == 0) {
    if (flags.verbose) fprintf(stdout, "%s -> %s\n", source, dest);
    return MOVE_OK;
  }
  if (errno == EXDEV) return MOVE_EXDEV;
  return MOVE_ERRNO;
}

static move_result_e try_sfs_move_to_dir(const char *source, const char *dest, flags_t flags) {
  size_t len = strlen(source);
  char source_copy[len + 1];
  memcpy(source_copy, source, len + 1);

  char *name = basename(source_copy);
  if (!name) {
    errno = EINVAL;
    return MOVE_ERRNO;
  }

  char buf[PATH_MAX];
  int n = snprintf(buf, PATH_MAX, "%s/%s", dest, name);
  if (n < 0 || n >= PATH_MAX) {
    errno = ENAMETOOLONG;
    return MOVE_ERRNO;
  }
  return try_sfs_move_to_path(source, buf, flags);
}

static int copy_file_cross_dest(const char *source, const char *dest) {
  int src = open(source, O_RDONLY);
  if (src < 0) return -1;

  struct stat st;
  int r = stat(source, &st);
  if (r < 0) {
    int saved = errno;
    close(src);
    errno = saved;
    return -1;
  }

  int dst = open(dest, O_RDWR | O_CREAT | O_TRUNC, st.st_mode);
  if (dst < 0) {
    int saved = errno;
    close(src);
    errno = saved;
    return -1;
  }

  char buf[64 * 1024];
  for (;;) {
    size_t n = read(src, buf, sizeof(buf));
    if (n == 0) break;
    if (n < 0) {
      if (errno == EINTR) continue;
      int saved = errno;
      close(src);
      close(dst);
      errno = saved;
      return -1;
    }

    ssize_t m = write(dst, buf, n);
    if (m < 0) {
      if (m == EINTR) continue;
      int saved = errno;
      close(src);
      close(dst);
      errno = saved;
      return -1;
    }
  }

  struct timespec times[2] = {
      st.st_atimespec,
      st.st_mtimespec,
  };

  r = utimensat(AT_FDCWD, dest, times, 0);
  if (r < 0) {
    int saved = errno;
    unlink(dest);
    close(src);
    close(dst);
    errno = saved;
    return -1;
  }

  r = unlink(source);
  if (r < 0) {
    int saved = errno;
    unlink(dest);
    close(src);
    close(dst);
    errno = saved;
    return -1;
  }

  close(src);
  close(dst);
  return 0;
}

static move_result_e try_dfs_move_to_path(const char *source, const char *dest, flags_t flags) {
  bool is_dir;
  bool exists;
  if (check(dest, &exists, &is_dir, flags) < 0) return MOVE_ERRNO;

  if (exists) {
    if (flags.interactive) {
      bool dont_overwrite = false;
      FILE *tty = fopen("/dev/tty", "r+");
      if (tty == NULL) return MOVE_TTY_OPEN;
      prompt(tty, dest, &dont_overwrite);
      if (dont_overwrite) {
        confirm_no_overwrite(tty);
        fclose(tty);
        return MOVE_OK;
      }
      fclose(tty);
    } else if (flags.no_overwrite) {
      return MOVE_OK;
    } else if (flags.force) {
      // Intentionally empty - rename by default is via force
    }
  }

  if (copy_file_cross_dest(source, dest) == 0) {
    if (flags.verbose) fprintf(stdout, "%s -> %s\n", source, dest);
    return MOVE_OK;
  }
  return MOVE_ERRNO;
}

static move_result_e try_dfs_file_to_dir(const char *source, const char *dest, flags_t flags) {
  size_t len = strlen(source);
  char source_copy[len + 1];
  memcpy(source_copy, source, len + 1);

  char *name = basename(source_copy);
  if (!name) {
    errno = EINVAL;
    return MOVE_ERRNO;
  }

  char buf[PATH_MAX];
  int n = snprintf(buf, PATH_MAX, "%s/%s", dest, name);
  if (n < 0 || n >= PATH_MAX) {
    errno = ENAMETOOLONG;
    return MOVE_ERRNO;
  }

  return try_dfs_move_to_path(source, buf, flags);
}

static move_result_e try_dfs_dir_to_dir(const char *source, const char *dest, flags_t flags) {
  int fts_flags = FTS_PHYSICAL | FTS_NOCHDIR;
  char *paths[2];
  paths[0] = (char *)source;
  paths[1] = NULL;
  FTS *fts = fts_open(paths, fts_flags, NULL);
  if (fts == NULL) return MOVE_ERRNO;
  FTSENT *ent = NULL;

  bool had_error = false;
  while ((ent = fts_read(fts)) != NULL) {
    switch (ent->fts_info) {
    case FTS_D: {
      const char *rel = ent->fts_path + strlen(source);
      char buf[PATH_MAX];
      snprintf(buf, PATH_MAX, "%s%s", dest, rel);
      if (mkdir(buf, ent->fts_statp->st_mode) < 0 && errno != EEXIST) {
        had_error = true;
      }
      break;
    }
    case FTS_DP: {
      const char *rel = ent->fts_path + strlen(source);
      char buf[PATH_MAX];
      snprintf(buf, PATH_MAX, "%s%s", dest, rel);

      struct timespec times[2] = {
          ent->fts_statp->st_atimespec,
          ent->fts_statp->st_mtimespec,
      };

      if (utimensat(AT_FDCWD, buf, times, 0) < 0) {
        had_error = true;
        break;
      }

      if (rmdir(ent->fts_accpath) < 0) {
        had_error = true;
        break;
      }
      break;
    }
    case FTS_F: {
      const char *rel = ent->fts_path + strlen(source);
      char buf[PATH_MAX];
      snprintf(buf, PATH_MAX, "%s%s", dest, rel);
      if (try_dfs_move_to_path(ent->fts_path, buf, flags) != MOVE_OK) {
        had_error = true;
      }
      break;
    }

    case FTS_SL: {
      char target[PATH_MAX];
      ssize_t len = readlink(ent->fts_path, target, sizeof(target) - 1);
      if (len < 0) {
        had_error = true;
        break;
      }
      target[len] = '\0';

      const char *rel = ent->fts_path + strlen(source);
      char buf[PATH_MAX];
      snprintf(buf, PATH_MAX, "%s%s", dest, rel);

      bool exists = false;
      struct stat st;
      if (lstat(buf, &st) == 0) {
        exists = true;
      } else if (errno != ENOENT) {
        had_error = true;
        break;
      }
      if (exists) {
        if (flags.no_overwrite) {
          break;
        }

        if (flags.interactive) {
          bool dont_overwrite = false;
          FILE *tty = fopen("/dev/tty", "r+");
          if (tty == NULL) return MOVE_TTY_OPEN;
          prompt(tty, dest, &dont_overwrite);
          if (dont_overwrite) {
            confirm_no_overwrite(tty);
            fclose(tty);
            break;
          }
          fclose(tty);
        }

        if (unlink(buf) < 0) {
          had_error = true;
          break;
        }
      }

      if (symlink(target, buf) < 0) {
        had_error = true;
        break;
      }

      if (unlink(ent->fts_path) < 0) {
        had_error = true;
        break;
      }

      if (flags.verbose) fprintf(stdout, "%s -> %s\n", ent->fts_path, buf);
      break;
    }
    case FTS_DNR:
    case FTS_ERR:
    case FTS_NS: {
      int saved = ent->fts_errno;
      fts_close(fts);
      errno = saved;
      return MOVE_ERRNO;
    }
    }
  }

  if (errno != 0) {
    fts_close(fts);
    return MOVE_ERRNO;
  }
  fts_close(fts);
  return had_error ? MOVE_ERRNO : MOVE_OK;
}

static move_result_e try_dfs_move_to_dir(const char *source, const char *dest, flags_t flags) {
  bool is_dir, exists;
  if (check(source, &exists, &is_dir, flags) < 0) return MOVE_ERRNO;
  if (is_dir) {
    return try_dfs_dir_to_dir(source, dest, flags);
  } else {
    return try_dfs_file_to_dir(source, dest, flags);
  }
}

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

static move_result_e move_one(const char *source, const char *dest, mode_e mode, flags_t flags) {
  move_result_e res;
  switch (mode) {
  case MODE_DIRECTORY:
    res = try_sfs_move_to_dir(source, dest, flags);
    break;
  case MODE_EXACT_PATH:
    res = try_sfs_move_to_path(source, dest, flags);
    break;
  }

  if (res != MOVE_EXDEV) return res;

  switch (mode) {
  case MODE_DIRECTORY:
    res = try_dfs_move_to_dir(source, dest, flags);
    break;
  case MODE_EXACT_PATH:
    res = try_dfs_move_to_path(source, dest, flags);
    break;
  }

  return res;
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

    move_result_e res = move_one(argv[i], argv[argc - 1], mode, flags);
    if (res == MOVE_OK) continue;

    ret = 1;
    if (res == MOVE_ERRNO) {
      error_errno(argv[0], argv[i]);
    } else if (res == MOVE_TTY_OPEN) {
      error_msg(argv[0], "/dev/tty", "failed to open");
    }
  }

  return ret;
}
