#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int ch;

  while ((ch = getopt(argc, argv, "")) != -1) {
    switch (ch) {
    default:
      error_errno(argv[0], "bad option");
      return 1;
    }
  }

  if (optind == argc) {
  }
  return 0;
}
