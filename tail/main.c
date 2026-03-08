#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLOCK_SIZE 512 // 512 bytes

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

typedef struct {
  char *buf;
  size_t len;
} line_t;

typedef struct {
  size_t len;
  size_t capacity;
  line_t *data;
} line_stack_t;

typedef struct {
  size_t start;
  size_t length;
  size_t capacity;
  char *buf;
} bytes_ring_t;

static void bytes_ring_init(bytes_ring_t *b, size_t want) {
  char *buf = malloc(want);
  b->buf = buf;
  b->length = 0;
  b->capacity = want;
  b->start = 0;
}
static void bytes_ring_free(bytes_ring_t *b) {
  free(b->buf);
  b->length = 0;
  b->capacity = 0;
  b->start = 0;
}

static size_t min_size(size_t x, size_t y) {
  return x < y ? x : y;
}

static void bytes_ring_append(bytes_ring_t *b, char *buf, size_t buf_len) {
  if (buf_len >= b->capacity) {
    buf = buf + (buf_len - b->capacity);
    buf_len = b->capacity;
  }

  size_t write_pos = (b->start + b->length) % b->capacity;
  size_t distance_to_end = b->capacity - write_pos;
  size_t to_write = distance_to_end < buf_len ? distance_to_end : buf_len;
  size_t remaining = buf_len - to_write;
  memcpy(b->buf + write_pos, buf, to_write);
  if (remaining > 0) {
    memcpy(b->buf, buf + to_write, remaining);
  }
  size_t total = b->length + buf_len;
  size_t discarded = 0;
  if (total > b->capacity) {
    discarded = total - b->capacity;
  }
  b->start = (b->start + discarded) % b->capacity;
  b->length = min_size(b->capacity, buf_len + b->length);
}

static void stack_init(line_stack_t *stack) {
  stack->len = 0;
  stack->capacity = 0;
  stack->data = NULL;
}

static int stack_push(line_stack_t *stack, line_t elem) {
  if (stack->len >= stack->capacity) {
    size_t new_cap = stack->capacity == 0 ? 8 : stack->capacity * 2;
    line_t *double_stack = realloc(stack->data, new_cap * sizeof(line_t));
    if (double_stack == NULL) {
      return -1;
    }
    stack->data = double_stack;
    stack->capacity = new_cap;
  }
  stack->data[stack->len++] = elem;
  return 0;
}

static line_t stack_pop(line_stack_t *stack) {
  if (stack->len == 0) return (line_t){0};
  return stack->data[--stack->len];
}

static void stack_free(line_stack_t *stack) {
  for (size_t i = 0; i < stack->len; i++) {
    free(stack->data[i].buf);
  }
  free(stack->data);
  stack->data = NULL;
  stack->len = 0;
  stack->capacity = 0;
}

static void stack_drop_oldest(line_stack_t *stack) {
  if (stack->len == 0) return;
  free(stack->data[0].buf);
  if (stack->len > 1) {
    memmove(&stack->data[0], &stack->data[1], (stack->len - 1) * sizeof(line_t));
  }
  stack->len--;
}

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s [-F | -f | -r] [-qv] [-b number | -c number | -n number] [file ...]\n",
          progname);
  exit(2);
}

static bool same_file(const struct stat *a, const struct stat *b) {
  return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
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

static off_t max(off_t x, off_t y) {
  return (x >= y) ? x : y;
}

static int write_bytes(int outfd, char *bytes, size_t bytes_len) {
  size_t off = 0;
  while (off < bytes_len) {
    ssize_t n = write(outfd, bytes + off, bytes_len - off);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    off += (size_t)n;
  }
  return 0;
}

static int follow_fd(int fd, int outfd) {
  if (lseek(fd, 0, SEEK_END) < 0) return -1;

  char buf[64 * 1024];
  for (;;) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
      if (write_bytes(outfd, buf, (size_t)n) < 0) {
        return -1;
      }
      continue;
    }

    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) return -1;

    off_t pos = lseek(fd, 0, SEEK_CUR);
    if (pos < 0) return -1;

    if (pos > st.st_size) {
      if (lseek(fd, 0, SEEK_SET) < 0) return -1;
    }

    sleep(1);
  }
}

static int follow_name(const char *path, int *fd, int outfd) {
  if (lseek(*fd, 0, SEEK_END) < 0) return -1;

  struct stat current;
  if (fstat(*fd, &current) < 0) return -1;

  char buf[64 * 1024];

  for (;;) {
    ssize_t n = read(*fd, buf, sizeof(buf));
    if (n > 0) {
      if (write_bytes(outfd, buf, (size_t)n) < 0) {
        return -1;
      }
      continue;
    }

    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }

    struct stat path_st;
    if (stat(path, &path_st) == 0) {
      if (!same_file(&current, &path_st)) {
        int newfd = open(path, O_RDONLY);
        if (newfd >= 0) {
          if (close(*fd) < 0) {
            int saved = errno;
            close(newfd);
            errno = saved;
            return -1;
          }
          *fd = newfd;

          if (fstat(*fd, &current) < 0) return -1;
        }
      }
    } else if (errno != ENOENT) {
      return -1;
    }

    struct stat st;
    if (fstat(*fd, &st) < 0) return -1;

    off_t pos = lseek(*fd, 0, SEEK_CUR);
    if (pos < 0) return -1;

    if (pos > st.st_size) {
      if (lseek(*fd, 0, SEEK_SET) < 0) return -1;
    }
    sleep(1);
  }
}

static int write_bytes_ring(int outfd, bytes_ring_t *br) {
  size_t written = 0;
  while (written < br->length) {
    size_t pos = (br->start + written) % br->capacity;
    size_t remaining = br->length - written;
    size_t contiguous = br->capacity - pos;
    size_t chunk = min_size(remaining, contiguous);

    ssize_t n = write(outfd, br->buf + pos, chunk);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) {
      errno = EIO;
      return -1;
    }

    written += (size_t)n;
  }

  return 0;
}

static int write_lines(int outfd, flags_t flags, line_stack_t *s) {
  size_t wanted = flags.count.as.lines;
  size_t have = s->len;
  size_t len = wanted <= have ? wanted : have;

  if (flags.reverse) {
    for (size_t i = 0; i < len; i++) {
      line_t out = stack_pop(s);
      int r = write_bytes(outfd, out.buf, out.len);
      free(out.buf);
      if (r < 0) return -1;
    }
    return 0;
  }

  size_t start = have > len ? have - len : 0;
  for (size_t i = start; i < have; i++) {
    int r = write_bytes(outfd, s->data[i].buf, s->data[i].len);
    if (r < 0) return -1;
  }
  return 0;
}

int stream_copy(int infd, int outfd, flags_t flags) {
  char buf[64 * 1024];
  switch (flags.count.mode) {
  case MODE_LINES: {
    size_t want = flags.count.as.lines;
    line_stack_t lines = {0};
    stack_init(&lines);
    char *carry = NULL;
    size_t carry_len = 0;

    for (;;) {
      ssize_t n = read(infd, buf, sizeof(buf));
      if (n < 0) {
        if (errno == EINTR) continue;
        int saved = errno;
        free(carry);
        stack_free(&lines);
        errno = saved;
        return -1;
      }

      if (n == 0) {
        if (carry_len > 0) {
          char *line = malloc(carry_len);
          if (line == NULL) {
            int saved = errno;
            free(carry);
            stack_free(&lines);
            errno = saved;
            return -1;
          }
          memcpy(line, carry, carry_len);

          if (want > 0) {
            if (lines.len == want) stack_drop_oldest(&lines);
            if (stack_push(&lines, (line_t){.buf = line, .len = carry_len}) < 0) {
              int saved = errno;
              free(carry);
              stack_free(&lines);
              errno = saved;
              return -1;
            }
          } else {
            free(line);
          }
        }

        free(carry);
        int r = write_lines(outfd, flags, &lines);
        stack_free(&lines);
        return r;
      }
      char *p = buf;
      size_t remaining = (size_t)n;

      while (remaining > 0) {
        char *nl = memchr(p, '\n', remaining);

        if (nl == NULL) {
          char *new_carry = realloc(carry, carry_len + remaining);
          if (new_carry == NULL) {
            int saved = errno;
            free(carry);
            stack_free(&lines);
            errno = saved;
            return -1;
          }
          carry = new_carry;
          memcpy(carry + carry_len, p, remaining);
          carry_len += remaining;
          break;
        }

        size_t seg_len = (size_t)(nl - p) + 1;
        size_t line_len = carry_len + seg_len;
        char *line = malloc(line_len);
        if (line == NULL) {
          int saved = errno;
          free(carry);
          stack_free(&lines);
          errno = saved;
          return -1;
        }

        if (carry_len > 0) memcpy(line, carry, carry_len);
        memcpy(line + carry_len, p, seg_len);

        free(carry);
        carry = NULL;
        carry_len = 0;

        if (want > 0) {
          if (lines.len == want) stack_drop_oldest(&lines);
          if (stack_push(&lines, (line_t){.buf = line, .len = line_len}) < 0) {
            int saved = errno;
            free(carry);
            stack_free(&lines);
            errno = saved;
            return -1;
          }
        } else {
          free(line);
        }

        p += seg_len;
        remaining -= seg_len;
      }
    }
  }
  case MODE_BYTES: {
    size_t want = flags.count.as.bytes;
    if (want == 0) return 0;
    bytes_ring_t br = {0};
    bytes_ring_init(&br, want);
    if (br.buf == NULL) return -1;
    for (;;) {
      ssize_t n = read(infd, buf, sizeof(buf));
      if (n < 0) {
        if (errno == EINTR) continue;
        bytes_ring_free(&br);
        return -1;
      }
      if (n == 0) {
        int r = write_bytes_ring(outfd, &br);
        if (r < 0) {
          bytes_ring_free(&br);
          return -1;
        }
        break;
      }
      bytes_ring_append(&br, buf, n);
    }

    bytes_ring_free(&br);
    return 0;
  }

  case MODE_BLOCKS: {
    size_t want = flags.count.as.blocks;
    if (want == 0) return 0;
    if (want > SIZE_MAX / BLOCK_SIZE) {
      errno = EOVERFLOW;
      return -1;
    }

    bytes_ring_t br = {0};
    bytes_ring_init(&br, want * BLOCK_SIZE);
    if (br.buf == NULL) return -1;
    for (;;) {
      ssize_t n = read(infd, buf, sizeof(buf));
      if (n < 0) {
        if (errno == EINTR) continue;
        bytes_ring_free(&br);
        return -1;
      }
      if (n == 0) {
        int r = write_bytes_ring(outfd, &br);
        if (r < 0) {
          bytes_ring_free(&br);
          return -1;
        }
        break;
      }
      bytes_ring_append(&br, buf, n);
    }

    bytes_ring_free(&br);
    return 0;
  }
  default:
    fprintf(stderr, "invalid mode\n");
    exit(2);
  }
  return 0;
}

static ssize_t read_to_buffer(int fd, char *buf, size_t want) {
  size_t total = 0;
  while (total < want) {
    ssize_t n = read(fd, buf + total, want - total);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) break;
    total += (size_t)n;
  }
  return (ssize_t)total;
}

static int prepend(char *prefix, int prefix_len, char **str, size_t *str_len, size_t *str_cap) {
  size_t new_len = *str_len + prefix_len;
  if (new_len + 1 >= *str_cap) {
    size_t new_cap = (new_len + 1) * 2;
    char *new_str = realloc(*str, new_cap);
    if (new_str == NULL) {
      return -1;
    }
    *str = new_str;
    *str_cap = new_cap;
  }
  memmove(*str + prefix_len, *str, *str_len);
  memcpy(*str, prefix, prefix_len);
  *str_len = new_len;
  (*str)[new_len] = '\0';
  return 0;
}

static int tail_regular_last_bytes(int infd, int outfd, off_t start, size_t want) {
  if (lseek(infd, start, SEEK_SET) < 0) return -1;
  char *out = malloc(want);
  if (out == NULL) {
    errno = ENOMEM;
    return -1;
  }
  ssize_t r = read_to_buffer(infd, out, want);
  if (r < 0) {
    int saved = errno;
    free(out);
    errno = saved;
    return -1;
  }
  r = (int)write_bytes(outfd, out, (size_t)r);
  if (r < 0) {
    int saved = errno;
    free(out);
    errno = saved;
    return -1;
  }
  free(out);
  return 0;
}

static int tail_regular_lines(int fd, flags_t flags) {
  size_t want = flags.count.as.lines;

  line_stack_t s = {0};
  stack_init(&s);
  off_t end = lseek(fd, 0, SEEK_END);
  if (end < 0) {
    stack_free(&s);
    return -1;
  }
  off_t pos = end;

  size_t carry_cap = BLOCK_SIZE;
  size_t carry_len = 0;
  char *carry = malloc(BLOCK_SIZE);
  if (carry == NULL) {
    int saved = errno;
    stack_free(&s);
    errno = saved;
    return -1;
  }

  if (pos > 0) {
    char last = 0;
    if (lseek(fd, pos - 1, SEEK_SET) < 0) {
      int saved = errno;
      free(carry);
      stack_free(&s);
      errno = saved;
      return -1;
    }
    int r = read(fd, &last, 1);
    if (r < 0) {
      int saved = errno;
      free(carry);
      stack_free(&s);
      errno = saved;
      return -1;
    }
    if (r == 1 && last == '\n') {
      carry[0] = '\n';
      carry_len = 1;
      pos -= 1;
    }
  }
  size_t have_lines = 0;

  char buf[BLOCK_SIZE];
  while (pos > 0 && have_lines < want) {
    off_t block_start = max(0, pos - BLOCK_SIZE);
    if (lseek(fd, block_start, SEEK_SET) < 0) {
      int saved = errno;
      free(carry);
      stack_free(&s);
      errno = saved;
      return -1;
    }
    int n = read(fd, buf, pos - block_start);
    if (n < 0) {
      int saved = errno;
      free(carry);
      stack_free(&s);
      errno = saved;
      return -1;
    }
    pos = block_start;
    int right = n;

    for (int i = n - 1; i >= 0; i--) {
      if (buf[i] == '\n') {
        int start = i + 1;
        int slice_len = right - start;
        if (prepend(buf + start, slice_len, &carry, &carry_len, &carry_cap) < 0) {
          int saved = errno;
          free(carry);
          stack_free(&s);
          errno = saved;
          return -1;
        }
        char *line = malloc(carry_len);
        if (line == NULL) {
          int saved = errno;
          free(carry);
          stack_free(&s);
          errno = saved;
          return -1;
        }
        memcpy(line, carry, carry_len);
        if (stack_push(&s, (line_t){.buf = line, .len = carry_len}) < 0) {
          int saved = errno;
          free(carry);
          stack_free(&s);
          errno = saved;
          return -1;
        }
        carry_len = 0;
        right = i + 1;
        have_lines++;
        if (have_lines == want) break;
      }
    }
    if (have_lines < want && right > 0) {
      if (prepend(buf, right, &carry, &carry_len, &carry_cap) < 0) {
        int saved = errno;
        free(carry);
        stack_free(&s);
        errno = saved;
        return -1;
      }
    }
  }

  if (have_lines < want && carry_len > 0) {
    char *line = malloc(carry_len);
    if (line == NULL) {
      int saved = errno;
      free(carry);
      stack_free(&s);
      errno = saved;
      return -1;
    }
    memcpy(line, carry, carry_len);
    if (stack_push(&s, (line_t){.buf = line, .len = carry_len}) < 0) {
      int saved = errno;
      free(carry);
      stack_free(&s);
      errno = saved;
      return -1;
    }
    have_lines++;
    carry_len = 0;
  }
  flags.reverse = !flags.reverse;
  int r = write_lines(STDOUT_FILENO, flags, &s);
  stack_free(&s);
  free(carry);
  return r;
}

int tail_file(char *progname, char *path, flags_t flags) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return -1;

  struct stat st;
  if (fstat(fd, &st) < 0) {
    int saved = errno;
    close(fd);
    errno = saved;
    return -1;
  }

  if (!S_ISREG(st.st_mode)) {
    if (flags.follow || flags.super_follow) {
      fprintf(stderr, "%s: cannot use -f or -F with non-regular files\n", progname);
      close(fd);
      exit(2);
    }
    int rc = stream_copy(fd, STDOUT_FILENO, flags);
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

  switch (flags.count.mode) {
  case MODE_LINES: {
    if (tail_regular_lines(fd, flags) < 0) {
      int saved = errno;
      close(fd);
      errno = saved;
      return -1;
    }
    break;
  }
  case MODE_BYTES: {
    off_t want = (off_t)flags.count.as.bytes;
    off_t end = lseek(fd, 0, SEEK_END);
    if (want > end) want = end;
    off_t start = end - want;
    if (tail_regular_last_bytes(fd, STDOUT_FILENO, start, (size_t)want) < 0) {
      int saved = errno;
      close(fd);
      errno = saved;
      return -1;
    }
    break;
  }
  case MODE_BLOCKS: {
    off_t end = lseek(fd, 0, SEEK_END);
    size_t blocks_wanted = flags.count.as.blocks;
    if (blocks_wanted > SIZE_MAX / BLOCK_SIZE) {
      errno = EOVERFLOW;
      close(fd);
      return -1;
    }
    off_t want = blocks_wanted * BLOCK_SIZE;
    off_t start = end;
    if (blocks_wanted > 1) {
      start = end - (blocks_wanted - 1) * (off_t)BLOCK_SIZE;
      if (start < 0) start = 0;
    }
    if (tail_regular_last_bytes(fd, STDOUT_FILENO, start, (size_t)want) < 0) {
      int saved = errno;
      close(fd);
      errno = saved;
      return -1;
    }
    break;
  }
  default:
    fprintf(stderr, "invalid mode\n");
    close(fd);
    exit(2);
  }

  if (flags.follow) {
    if (follow_fd(fd, STDOUT_FILENO) < 0) {
      int saved = errno;
      close(fd);
      errno = saved;
      return -1;
    }
  }
  if (flags.super_follow) {
    if (follow_name(path, &fd, STDOUT_FILENO) < 0) {
      int saved = errno;
      close(fd);
      errno = saved;
      return -1;
    }
  }
  close(fd);
  return 0;
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
      flags.count.mode = MODE_BLOCKS;
      flags.count.as.blocks = blocks;
      break;
    }
    default:
      usage(argv[0]);
    }
  }

  if (flags.count.mode != MODE_LINES && flags.reverse) {
    fprintf(stderr, "%s: cannot use -r with bytes or blocks mode\n", argv[0]);
    exit(2);
  }

  if ((flags.follow || flags.super_follow) && (argc - optind != 1)) {
    fprintf(stderr, "%s: -f and -F currently support exactly one file\n", argv[0]);
    exit(2);
  }

  if (argc == optind) {
    if (flags.follow || flags.super_follow) {
      fprintf(stderr, "%s: cannot use -f and -F with stdin\n", argv[0]);
      exit(2);
    }

    if (stream_copy(STDIN_FILENO, STDOUT_FILENO, flags) < 0) {
      error_errno(argv[0], errno, "stdin");
      return 1;
    }
    return 0;
  }

  if (!flags.quiet && !flags.verbose) {
    if (argc - optind == 1) {
      flags.quiet = true;
    } else {
      flags.verbose = true;
    }
  }

  int exit_code = 0;
  for (int i = optind; i < argc; i++) {
    char *filename = argv[i];
    if (!flags.quiet) fprintf(stdout, "==> %s <==\n", filename);
    if (tail_file(argv[0], filename, flags) < 0) {
      error_errno(argv[0], errno, filename);
      exit_code = 1;
    }
  }
  return exit_code;
}
