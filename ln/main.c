#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
  bool hard;
} flags_t;

typedef enum {
  MODE_OCTAL,
  MODE_SYMBOLIC,
  MODE_BAD,
} mode_form_e;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO,
          "usage: %s [-L | -P | -s [-F]] [-f | -iw] [-hnv] source_file [target_file]\n"
          "       %s [-L | -P | -s [-F]] [-f | -iw] [-hnv] source_file ... target_dir\n",
          progname, progname);
  exit(2);
}

int main(int argc, char *argv[]) {
  int ch;
  flags_t flags = {0};

  while ((ch = getopt(argc, argv, "h")) != -1) {
    switch (ch) {
    case 'h':
      flags.hard = true;
      break;
    default:
      usage(argv[0]);
    }
  }
  if (optind == argc) usage(argv[0]);

  return 0;
}
