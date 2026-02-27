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

static void parse_err(count_e r, const char *progname, const char *arg) {
  if (r == COUNT_INVALID) {
    errno = EINVAL;
    fprintf(stderr, "%s: %s -- %s: %s\n", progname, strerror(errno), arg, "Invalid argument");
  }
  if (r == COUNT_RANGE) {
    errno = ERANGE;
    fprintf(stderr, "%s: %s -- %s: %s\n", progname, strerror(errno), arg, "Result too large");
  }
  exit(2);
}

static void error_errno(const char *progname, int err_num, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, strerror(err_num), msg);
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

int write_all(int outfd, const char *buf, size_t len) {
  const uint8_t *p = (const uint8_t *)buf;
  size_t off = 0;
  while (off < len) {
    ssize_t n = write(outfd, p + off, len - off);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) {
      errno = EIO;
      return -1;
    }
    off += (size_t)n;
  }
  return 0;
}

int stream_copy(int infd, int outfd, flags_t flags) {
  char buf[64 * 1024];

  for (;;) {
    ssize_t n = read(infd, &buf, sizeof(buf));
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) return 0;
    if (write_all(outfd, buf, (size_t)n) < 0) return -1;
  }
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
      size_t lines = 0;
      count_e r = parse_count(optarg, &lines);
      if (r != COUNT_OK) parse_err(r, argv[0], optarg);
      flags.count.mode = MODE_LINES;
      flags.count.as.lines = lines;
      break;
    }
    case 'c': {
      size_t bytes = 0;
      count_e r = parse_count(optarg, &bytes);
      if (r != COUNT_OK) parse_err(r, argv[0], optarg);
      flags.count.mode = MODE_BYTES;
      flags.count.as.bytes = bytes;
      break;
    }
    case 'b': {
      size_t blocks = 0;
      count_e r = parse_count(optarg, &blocks);
      if (r != COUNT_OK) parse_err(r, argv[0], optarg);
      flags.count.mode = MODE_BYTES;
      flags.count.as.blocks = blocks;
      break;
    }
    default:
      usage(argv[0]);
    }
  }

  if (argc == optind) {
    if (stream_copy(STDIN_FILENO, STDOUT_FILENO) < 0) {
      error_errno(argv[0], errno, "stdin");
      return 1;
    }
  }
}
