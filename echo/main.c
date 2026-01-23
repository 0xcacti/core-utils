#include <errno.h>
#include <getopt.h>
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

  int suppress_newline = 0;

  int ch;
  while ((ch = getopt(argc, argv, "n")) != -1) {
    switch (ch) {
    case 'n':
      suppress_newline = 1;
      break;
    }
  }

  char *output = argv[optind];
  if (output == NULL) return 0;

  if (suppress_newline) {
    printf("%s", output);
  } else {
    printf("%s\n", output);
  }
  return 0;
}
