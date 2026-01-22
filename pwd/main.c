#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "usage: %s [-L | -P]\n", progname);
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
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
    if (cwd == NULL) {
      perror("getenv");
      return 1;
    }
    printf("%s\n", cwd);
  } else {
    char actual_cwd[PATH_MAX];
    if (getcwd(actual_cwd, sizeof(actual_cwd)) == NULL) {
      perror("getcwd");
      return 1;
    }
    printf("%s\n", actual_cwd);
  }

  return 0;
}
