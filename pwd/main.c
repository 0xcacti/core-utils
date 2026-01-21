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

int main(int argc, char *argv[]) {
  int ch;

  bool logical = true;
  bool physical = false;
  // I don't know how to get logical

  while ((ch = getopt(argc, argv, "L:P:")) != -1) {
    switch (ch) {
    case 'L':
      break;
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (optind == argc) {
  }
  return 0;
}
