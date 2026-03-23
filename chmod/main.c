#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
  SYMLINK_NONE, // -P
  SYMLINK_CMD,  // -H
  SYMLINK_ALL,  // -L
} symlink_behavior_e;

typedef struct {
  bool force;     // -f
  bool no_follow; // -h
  bool verbose;   // -v
  bool recurse;   // -R
  symlink_behavior_e sym_mode;
} flags_t;

typedef enum {
  MODE_OCTAL,
  MODE_SYMBOLIC,
  MODE_BAD,
} mode_form_e;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s [-fhv] [-R [-H | -L | -P]] mode file ...", progname);
  exit(2);
}

static mode_form_e parse_mode(const char *mode) {
  return MODE_OCTAL;
}

static void parse_octal(const char *mode) {
  return;
}

int main(int argc, char *argv[]) {
  int ch;
  flags_t flags = {0};
  flags.sym_mode = SYMLINK_NONE;
  while ((ch = getopt(argc, argv, "fhvRPHL")) != -1) {
    switch (ch) {
    case 'f':
      flags.force = true;
      break;
    case 'h':
      flags.no_follow = true;
      break;
    case 'v':
      flags.verbose = true;
      break;
    case 'R':
      flags.recurse = true;
      break;
    case 'P':
      flags.sym_mode = SYMLINK_NONE;
      break;
    case 'H':
      flags.sym_mode = SYMLINK_CMD;
      break;
    case 'L':
      flags.sym_mode = SYMLINK_ALL;
      break;
    default:
      usage(argv[0]);
    }
  }

  mode_form_e mode = parse_mode(argv[optind]);

  return 0;
}
