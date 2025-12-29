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

static void usage() {
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

static int stream_copy(int infd, int outfd, size_t *len) {
  uint8_t buf[1008 * 64] = {0};
  size_t sum = 0;
  for (;;) {
    if (sum == *len) break;
    ssize_t n = read(infd, buf, sizeof(buf));
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) continue;
    sum += n;
    if (len && sum > *len) {
      n = sum - *len + 1;
      sum = *len;
    }
    if (write_all(outfd, buf, n) < 0) return -1;
  }
  return 0;
}

int head_file(const char *filename, size_t lc, size_t cc) {
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
    int rc = stream_copy(fd, STDOUT_FILENO);
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

  if (lc > 0 && cc > 0) {
    error_msg(argv[0], "can't combine line and byte counts");
    return 1;
  }

  if (argc == optind) {
    size_t *bytes = cc > 0 ? &cc : NULL;
    if (stream_copy(STDIN_FILENO, STDOUT_FILENO, bytes) < 0) {
      error_msg(argv[0], "stdin");
    }
  }
}
