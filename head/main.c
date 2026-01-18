#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum { COUNT_OK, COUNT_INVALID, COUNT_RANGE } count_e;
typedef enum { MODE_LINES, MODE_BYTES, MODE_DEFAULT } mode_e;

typedef struct {
  mode_e mode;
  union {
    size_t lines;
    size_t bytes;
  } as;
} count_t;

static void usage(void) {
  dprintf(STDERR_FILENO, "Usage: head [-n lines | -c bytes] [file ...]\n");
}

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
}

static count_e parse_count(const char *s, size_t *out) {
  errno = 0;
  char *end = NULL;
  long val = strtol(s, &end, 10);
  if (end == s || *end != '\0') return COUNT_INVALID;
  if (errno == ERANGE || val > INT_MAX) return COUNT_RANGE;
  if (val <= 0) return COUNT_INVALID;

  *out = (int)val;
  return COUNT_OK;
}

static int write_all(int fd, const void *buf, size_t len) {
  const uint8_t *p = (const uint8_t *)buf;
  size_t off = 0;
  while (off < len) {
    ssize_t n = write(fd, p + off, len - off);
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

static int stream_copy(int infd, int outfd, count_t count) {
  uint8_t buf[1024 * 64] = {0};
  size_t sum = 0;
  switch (count.mode) {
  case MODE_DEFAULT:
  case MODE_LINES: {
    size_t lines_so_far = 0;
    for (;;) {
      if (lines_so_far >= count.as.lines) break;
      ssize_t n = read(infd, buf, sizeof(buf));
      if (n < 0) {
        if (errno == EINTR) continue;
        return -1;
      }
      if (n == 0) break;

      char *buf_p = (char *)buf;
      char *p = NULL;
      size_t remaining = n;
      while ((p = memchr(buf_p, '\n', remaining)) != NULL) {
        if (lines_so_far >= count.as.lines) break;
        size_t pos = (size_t)(p - buf_p) + 1;
        if (write_all(outfd, buf_p, pos) < 0) return -1;
        lines_so_far++;
        buf_p += pos;
        remaining -= pos;
      }
      if (lines_so_far < count.as.lines && remaining > 0) {
        if (write_all(outfd, buf_p, remaining) < 0) return -1;
      }
    }
    break;
  }
  case MODE_BYTES: {
    size_t bytes_so_far = 0;
    for (;;) {
      if (bytes_so_far >= count.as.bytes) break;
      ssize_t n = read(infd, buf, sizeof(buf));
      if (n < 0) {
        if (errno == EINTR) continue;
        return -1;
      }
      if (n == 0) break;
      sum += n;
      if (bytes_so_far > count.as.bytes) {
        n = sum - count.as.bytes + 1;
        sum = count.as.bytes;
      }
      if (write_all(outfd, buf, n) < 0) return -1;
    }
    break;
  }
  }
  return 0;
}

int head_file(const char *filename, count_t c) {
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    int saved = errno;
    close(fd);
    errno = saved;
    return -1;
  }

  if (!S_ISREG(st.st_mode)) {
    int rc = stream_copy(fd, STDOUT_FILENO, c);
    int stream_errno = 0;
    if (rc < 0) stream_errno = errno;
    int close_rc = close(fd);
    int close_errno = 0;
    if (close_rc < 0) close_errno = errno;
    if (rc < 0) {
      errno = stream_errno;
      return -1;
    }
    if (close_rc < 0) {
      errno = close_errno;
      return -1;
    }

    return 0;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  int ch;

  size_t lc = 0;
  size_t cc = 0;

  while ((ch = getopt(argc, argv, "n:c:")) != -1) {
    switch (ch) {
    case 'n': {
      count_e res = parse_count(optarg, &lc);
      if (res != COUNT_OK) {
        dprintf(STDERR_FILENO, "%s: illegal line count -- %s\n", argv[0], optarg);
        return 1;
      }
      break;
    }
    case 'c': {
      count_e res = parse_count(optarg, &cc);
      if (res != COUNT_OK) {
        dprintf(STDERR_FILENO, "%s: illegal byte count -- %s\n", argv[0], optarg);
        return 1;
      }
      break;
    }
    default:
      usage();
    }
  }

  count_t c = {0};
  if (lc > 0 && cc > 0) {
    error_msg(argv[0], "can't combine line and byte counts");
    return 1;
  } else if (lc > 0) {
    c.mode = MODE_LINES;
    c.as.lines = lc;
  } else if (cc > 0) {
    c.mode = MODE_BYTES;
    c.as.bytes = cc;
  } else {
    c.mode = MODE_DEFAULT;
    c.as.lines = 10;
  }

  if (argc == optind) {
    if (stream_copy(STDIN_FILENO, STDOUT_FILENO, c) < 0) {
      error_msg(argv[0], "stdin");
      return 1;
    }
    return 0;
  }

  int exit_code = 0;
  for (int i = optind; i < argc; i++) {
    char *filename = argv[i];
    if (head_file(filename, c) < 0) {
      error_errno(argv[0], filename);
      exit_code = 1;
    }
  }
  return exit_code;
}
