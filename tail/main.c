#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { COUNT_OK, COUNT_INVALID, COUNT_RANGE } count_e;
typedef enum { MODE_LINES, MODE_BLOCKS, MODE_BYTES } mode_e;

typedef struct {
  mode_e mode;
  union {
    size_t lines;
    size_t blocks;
    size_t bytes;
  } as;
} count_t;

typedef struct {
  bool follow;
  bool super_follow;
  bool reverse;
  bool quiet;
  bool verbose;
  bool blocks;
  count_t count;
} flags_t;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s [-F | -f | -r] [-qv] [-b number | -c number | -n number] [file ...]\n",
          progname);
  exit(2);
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
  exit(2);
}

static void error_errno(const char *progname, int err_num, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s %s\n", progname, strerror(err_num), msg);
  exit(2);
}

static count_e parse_count(const char *s, size_t *out) {
  errno = 0;
  char *end = NULL;
  long val = strtol(s, &end, 10);
  if (end == s || *end != '\0') return COUNT_INVALID;
  if (errno == ERANGE || val > INT_MAX) return COUNT_RANGE;
  if (val <= 0) return COUNT_INVALID;

  *out = (size_t)val;
  return COUNT_OK;
}

int main(int argc, char *argv[]) {
  int ch;

  flags_t flags = {0};
  flags.count = (count_t){
      .mode = MODE_LINES,
      .as = {.lines = 10},
  };

  while ((ch = getopt(argc, argv, "fFrqvb:c:n:")) != -1) {
    switch (ch) {
    case 'f':
      flags.follow = true;
      break;
    case 'F':
      flags.super_follow = true;
      break;
    case 'r':
      flags.reverse = true;
      break;
    case 'q':
      flags.quiet = true;
      break;
    case 'v':
      flags.verbose = true;
      break;
    case 'n': {
      size_t lns = 0;
      count_e r = parse_count(optarg, &lns);
      if (r == COUNT_INVALID) {
        errno = EINVAL;
        fprintf(stderr, "%s: %s -- %s: %s\n", argv[0], strerror(errno), optarg, "Invalid argument");
        exit(2);
      }
      if (r == COUNT_RANGE) {
        errno = ERANGE;
        fprintf(stderr, "%s: %s -- %s: %s\n", argv[0], strerror(errno), optarg, "Result too large");
        exit(2);
      }
      flags.count.as.lines = lns;
      break;
    }
    default:
      usage(argv[0]);
    }
  }
  printf("count: %d\n", flags.count.as.lines);
}
