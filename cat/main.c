#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static void usage() {
  dprintf(STDERR_FILENO, "Usage: mcat [file ...]\n");
}

static void error_msg(const char *filename) {
  dprintf(STDERR_FILENO, "mcat: %s: %s\n", filename, strerror(errno));
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

static int stream_copy(int infd, int outfd) {
  uint8_t buf[64 * 1024];
  for (;;) {
    ssize_t n = read(infd, buf, sizeof(buf));
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) return 0;
    if (write_all(outfd, buf, (size_t)n) < 0) return -1;
  }
}

int cat_file(const char *filename) {
  if (strcmp(filename, "-") == 0) {
    return stream_copy(STDIN_FILENO, STDOUT_FILENO);
  }

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

  size_t sz = st.st_size;
  if (sz == 0) {
    close(fd);
    return 0;
  }

  void *p = mmap(0, sz, PROT_READ, MAP_PRIVATE, fd, 0);
  if (p == MAP_FAILED) {
    int saved = errno;
    close(fd);
    errno = saved;
    return -1;
  }

  int rc = write_all(STDOUT_FILENO, p, sz);
  int write_errno = 0;
  if (rc < 0) write_errno = errno;

  int munmap_rc = munmap(p, sz);
  int munmap_err = 0;
  if (munmap_rc < 0) munmap_err = errno;

  int close_rc = close(fd);
  int close_err = 0;
  if (close_rc < 0) close_err = errno;

  if (rc < 0) {
    errno = write_errno;
    return -1;
  }

  if (munmap_rc < 0) {
    errno = munmap_err;
    return -1;
  }

  if (close_rc < 0) {
    errno = close_err;
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  int ch;

  while ((ch = getopt(argc, argv, "")) != -1) {
    switch (ch) {
    case '?':
    default:
      usage();
      return 1;
    }
  }

  if (optind == argc) {
    if (stream_copy(STDIN_FILENO, STDOUT_FILENO) < 0) {
      error_msg("stdin");
      return 1;
    }
    return 0;
  }

  int exit_code = 0;
  for (int i = optind; i < argc; i++) {
    char *filename = argv[i];
    if (cat_file(filename) < 0) {
      error_msg(filename);
      exit_code = 1;
    }
  }
  return exit_code;
}
