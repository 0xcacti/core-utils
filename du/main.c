#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s\n", progname);
  exit(2);
}

int main(int argc, char **argv) {
  usage(argv[0]);
}
