#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct flags {
  bool d_flag;
  bool f_flag;
  bool i_flag;
  bool r_flag;
  bool v_flag;
};

typedef enum {
  ROOT_DIR,
  DOTS,
} invalid_e;

typedef enum {
  SUCCESS,
  UNLINK_FAIL,
  DIR_FAIL,
} rm_result_e;

struct flags flags = {
    .d_flag = false,
    .f_flag = false,
    .i_flag = false,
    .r_flag = false,
    .v_flag = false,
};

static bool is_term = false;

static void usage(const char *progname) {
  fprintf(stderr, "Usage: %s [-f | -i] [-drv] file ...\n", progname);
  exit(EXIT_FAILURE);
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
  exit(2);
}

static bool is_illegal(char *path, invalid_e *type) {
  char *p;
  struct stat sb;
  struct stat root;
  if (type == NULL)
    exit(1);
  if (stat("/", &root) != 0)
    return false;
  if (lstat(path, &sb) == 0 && root.st_ino == sb.st_ino &&
      root.st_dev == sb.st_dev) {
    *type = ROOT_DIR;
    return true;
  }
  p = strrchr(path, '\0');
  while (--p > path && *p == '/')
    *p = '\0';

  // go to after last '/'
  if ((p = strrchr(path, '/')) != NULL) {
    p = p + 1;
  } else {
    p = (char *)path;
  }

  if (p[0] == '.' && p[1] == '\0') {
    *type = DOTS;
    return true;
  }
  if (p[0] == '.' && p[1] == '.' && p[2] == '\0') {
    *type = DOTS;
    return true;
  }

  return false;
}

static void delete_argv_at(char **argv, int *argc, int i) {
  for (int j = i + 1; j < *argc; j++) {
    argv[j - 1] = argv[j];
  }
  (*argc) -= 1;
  argv[*argc] = NULL;
}

int check(char *path, char *name, struct stat *st) {
  int ch, first;
  char modep[15];

  if (flags.i_flag)
    fprintf(stderr, "remove %s? ", path);
  else {
    if (!is_term || S_ISLNK(st->st_mode) || !access(name, W_OK) ||
        errno != EACCES)
      return 1;
    strmode(st->st_mode, modep);
    fprintf(stderr, "override %s%s%s/%s for %s? ", modep + 1,
            modep[9] == ' ' ? "" : " ", user_from_uid(st->st_uid, 0),
            group_from_gid(st->st_gid, 0), path);
  }
  (void)fflush(stderr);

  first = ch = getchar();
  while (ch != '\n' && ch != EOF)
    ch = getchar();
  return (first == 'y' || first == 'Y');
}

void rm_file(const char *path, rm_result_e *result) {
  if (result == NULL)
    exit(1);
  struct stat st;
  if (lstat(path, &st) != 0) {
    if (!flags.f_flag || errno != ENOENT) {
      *result = UNLINK_FAIL;
    }
  }

  if (S_ISDIR(st.st_mode) && !flags.d_flag) {
    *result = DIR_FAIL;
    return;
  }
}

int main(int argc, char *argv[]) {
  int ch;
  while ((ch = getopt(argc, argv, "dfirv")) != -1) {
    switch (ch) {
    case 'd':
      flags.d_flag = true;
      break;
    case 'f':
      flags.f_flag = true;
      break;
    case 'i':
      flags.i_flag = true;
      break;
    case 'r':
      flags.r_flag = true;
      break;
    case 'v':
      flags.v_flag = true;
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  if (optind >= argc && !flags.f_flag)
    usage(argv[0]);
  int ret = 0;

  for (int i = optind; i < argc;) {
    invalid_e type;
    if (is_illegal(argv[i], &type)) {
      switch (type) {
      case ROOT_DIR:
        fprintf(stderr, "%s: \"/\" may not be removed\n", argv[0]);
        break;
      case DOTS:
        fprintf(stderr, "%s: \".\" or \"..\" may not be removed\n", argv[0]);
        break;
      default:
        break;
      }
      ret = 1;
      delete_argv_at(argv, &argc, i);
      continue;
    }
    i++;
  }

  if (!argv[optind])
    return ret;
  is_term = isatty(STDIN_FILENO);

  for (int i = optind; i < argc; i++) {
    rm_result_e result = SUCCESS;
    if (flags.r_flag) {
    } else {
      rm_file(argv[i], &result);
      if (result == UNLINK_FAIL) {
        ret = 1;
        fprintf(stderr, "%s\n", argv[i]);
        continue;
      } else if (result == DIR_FAIL) {
        ret = 1;
        fprintf(stderr, "%s: is a directory\n", argv[i]);
        continue;
      }
    }
  }

  return ret;
}
