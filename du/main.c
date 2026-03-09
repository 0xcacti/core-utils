#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
  FORMAT_DEFAULT,
  FORMAT_KIB,
  FORMAT_HUMAN,
} format_e;

typedef enum {
  PRINT_DEFAULT,
  PRINT_ALL,
  PRINT_SUMMARY,
  PRINT_MAX_DEPTH,
} print_e;

typedef struct {
  bool one_file_system;
  format_e format_mode;
  print_e print_mode;
  int max_depth;
} flags_t;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s [-x] [-h | -k] [-a | -s | -d depth] [file ...]\n", progname);
  exit(2);
}

static int parse_nonnegative_int(const char *s, const char *progname) {
  char *end = NULL;
  errno = 0;
  long value = strtol(s, &end, 10);
  if (errno == ERANGE || value < 0 || value > INT_MAX) usage(progname);
  if (end == s || *end != '\0') usage(progname);
  return (int)value;
}

int main(int argc, char **argv) {
  int ch;

  flags_t flags = {0};
  flags.format_mode = FORMAT_DEFAULT;
  flags.print_mode = PRINT_DEFAULT;

  bool format_set = false;
  bool print_set = false;
  while ((ch = getopt(argc, argv, "xhkasd:")) != -1) {
    switch (ch) {
    case 'x':
      flags.one_file_system = true;
      break;
    case 'h':
      if (format_set) usage(argv[0]);
      format_set = true;
      flags.format_mode = FORMAT_HUMAN;
      break;
    case 'k':
      if (format_set) usage(argv[0]);
      format_set = true;
      flags.format_mode = FORMAT_KIB;
      break;
    case 'a':
      if (print_set) usage(argv[0]);
      print_set = true;
      flags.print_mode = PRINT_ALL;
      break;
    case 's':
      if (print_set) usage(argv[0]);
      print_set = true;
      flags.print_mode = PRINT_SUMMARY;
      break;
    case 'd': {
      if (print_set) usage(argv[0]);
      print_set = true;
      flags.print_mode = PRINT_MAX_DEPTH;
      int md = parse_nonnegative_int(argv[0], optarg);
      flags.max_depth = md;
      break;
    }
    default:
      usage(argv[0]);
    }
  }
}
