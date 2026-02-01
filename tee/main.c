#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ignore_sigint(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
}

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "Usage: %s [-ai] [file...]\n", progname);
  exit(2);
}

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
  exit(2);
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

static int stream_copy(int infd, int *outfds, size_t outfd_count) {
  uint8_t buf[64 * 1024];
  for (;;) {
    ssize_t n = read(infd, buf, sizeof(buf));
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) return 0;
    for (size_t i = 0; i < outfd_count; i++) {
      if (write_all(outfds[i], buf, (size_t)n) < 0) return -1;
    }
  }
}

int main(int argc, char **argv) {
  int opt;
  bool opt_a = false;
  bool opt_i = false;
  (void)opt_i; // currently unused
  while ((opt = getopt(argc, argv, "ai")) != -1) {
    switch (opt) {
    case 'a':
      opt_a = true;
      break;
    case 'i':
      opt_i = true;
      break;
    default:
      usage(argv[0]);
    }
  }

  if (opt_i) {
    ignore_sigint();
  }

  size_t files_specified = (size_t)(argc - optind);
  size_t outfd_count = files_specified + 1; // +1 for stdout
  int *outfds = malloc(sizeof(int) * outfd_count);
  if (outfds == NULL) error_errno(argv[0], "malloc");
  outfds[0] = STDOUT_FILENO;
  int flags = O_WRONLY | O_CREAT;
  if (opt_a) {
    flags |= O_APPEND;
  } else {
    flags |= O_TRUNC;
  }

  for (size_t i = 0; i < files_specified; i++) {
    const char *filename = argv[optind + (int)i];
    int fd = open(filename, flags, 0666);
    if (fd < 0) error_errno(argv[0], filename);
    outfds[i + 1] = fd;
  }

  int rc = stream_copy(STDIN_FILENO, outfds, outfd_count);
  if (rc != 0) error_errno(argv[0], "read/write");

  for (size_t i = 0; i < files_specified; i++) {
    close(outfds[i + 1]);
  }
  free(outfds);
  return 0;
}
