#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s [-fhv] [-R [-H | -L | -P]] mode file ...", progname);
  exit(2);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  int ch;
  while ((ch = getopt(argc, argv, "fhvRHLP")) != -1) {
    switch (ch) {
    case 'f':
      break;
    default:
      usage(argv[0]);
    }
  }

  return 0;
}
