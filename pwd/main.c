#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "usage: %s [-L | -P]\n", progname);
}

static void error_errno(const char *progname, const char *op) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, op, strerror(errno));
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
}

static int same_dir(const char *path) {
  struct stat a, b;
  if (stat(path, &a) != 0) return 0;
  if (stat(".", &b) != 0) return 0;
  return (a.st_dev == b.st_dev) && (a.st_ino == b.st_ino);
}

static int print_physical(const char *progname) {
  char buf[PATH_MAX];
  if (getcwd(buf, sizeof(buf)) == NULL) {
    error_errno(progname, "getcwd");
    return 1;
  }
  puts(buf);
  return 0;
}

int main(int argc, char *argv[]) {
  int ch;

  bool is_logical = true;

  while ((ch = getopt(argc, argv, "LP")) != -1) {
    switch (ch) {
    case 'L':
      is_logical = true;
      break;
    case 'P':
      is_logical = false;
      break;
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (optind != argc) {
    error_msg(argv[0], "too many arguments");
    return 1;
  }

  if (is_logical) {
    char *cwd = getenv("PWD");
    if (cwd != NULL && cwd[0] == '/' && same_dir(cwd)) {
      puts(cwd);
      return 0;
    }
    return print_physical(argv[0]);
  }

  return print_physical(argv[0]);
}
