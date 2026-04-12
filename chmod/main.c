#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct {
  mode_form_e form;
  int user;
  int group;
  int other;
  int all;
} parsed_mode_t;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s [-fhv] [-R [-H | -L | -P]] mode file ...\n", progname);
  exit(2);
}

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
}

static void error_msg(const char *progname, const char *m1, const char *m2) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, m1, m2);
}

static mode_form_e parse_mode(const char *mode) {
  switch (mode[0]) {
  case 'u':
  case 'g':
  case 'a':
  case 'o':
    return MODE_SYMBOLIC;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
    return MODE_OCTAL;
  default:
    return MODE_BAD;
  }
}

// static void parse_octal(const char *mode) {
//   return;
// }

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

  switch (mode) {
  case MODE_OCTAL:
    break;
  case MODE_SYMBOLIC:
    break;
  case MODE_BAD:
    error_msg(argv[0], "Invalid file mode", argv[optind]);
    exit(2);
  }

  return 0;
}
