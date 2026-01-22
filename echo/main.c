#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
}

int main(int argc, char *argv[]) {
  const char *progname = argv[0];

  if (argc < 2) {
    error_msg(progname, "missing operand");
    return EXIT_FAILURE;
  }

  int exit_status = EXIT_SUCCESS;

  for (int i = 1; i < argc; i++) {
    const char *filename = argv[i];
    if (unlink(filename) != 0) {
      error_errno(progname, filename);
      exit_status = EXIT_FAILURE;
    }
  }

  return exit_status;
}
