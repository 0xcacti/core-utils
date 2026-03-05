#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
  size_t len;
  size_t capacity;
  char **data;
} line_stack_t;

void stack_init(line_stack_t *stack) {
  stack->len = 0;
  stack->capacity = 0;
  stack->data = NULL;
}

void stack_push(line_stack_t *stack, char *elem) {
  if (stack->len >= stack->capacity) {
    int new_cap = stack->capacity == 0 ? 8 : stack->capacity * 2;
    char **double_stack = realloc(stack->data, new_cap * sizeof(char *));
    if (double_stack == NULL) {
      perror("malloc");
      exit(2);
    }
    stack->data = double_stack;
    stack->capacity = new_cap;
  }
  stack->data[stack->len++] = elem;
}

char *stack_pop(line_stack_t *stack) {
  if (stack->len == 0) return NULL;
  return stack->data[--stack->len];
}

void stack_free(line_stack_t *stack) {
  for (size_t i = 0; i < stack->len; i++) {
    free(stack->data[i]);
  }
  free(stack->data);
  stack->data = NULL;
}

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

static off_t max(off_t x, off_t y) {
  return (x >= y) ? x : y;
}

static off_t min(off_t x, off_t y) {
  return (x >= y) ? y : x;
}

void write_lines(int outfd, flags_t flags, line_stack_t *s) {
  size_t wanted = flags.count.as.lines;
  size_t have = s->len;
  size_t len = wanted <= have ? wanted : have;
  for (size_t i = 0; i < len; i++) {
    char *out = NULL;
    if (flags.reverse) {
      out = stack_pop(s);
    } else {
      out = s->data[i];
    }
    ssize_t n = write(outfd, out, strlen(out));
    if (n < 0) {
      perror("write");
      exit(1);
    }
    n = write(outfd, "\n", 1);
    if (n < 0) {
      perror("write");
      exit(1);
    }
  }
}

void write_bytes(int outfd, char *bytes, size_t bytes_len) {
  ssize_t n = write(outfd, bytes, bytes_len);
  if (n < 0) {
    perror("write");
    exit(EXIT_FAILURE);
  }
}

int stream_copy(int infd, int outfd, flags_t flags) {
  char buf[64 * 1024];
  switch (flags.count.mode) {
  case MODE_LINES: {
    line_stack_t lines = {0};
    stack_init(&lines);
    char *carry = NULL;
    size_t carry_len = 0;
    for (;;) {
      ssize_t n = read(infd, buf, sizeof(buf));
      if (n < 0) {
        if (errno == EINTR) continue;
        return -1;
      }
      if (n == 0) {
        if (carry_len > 0) {
          char *line = malloc(carry_len + 1);
          if (line == NULL) return -1;
          memcpy(line, carry, carry_len);
          line[carry_len] = '\0';
          stack_push(&lines, line);
        }
        free(carry);
        write_lines(outfd, flags, &lines);
        return 0;
      }
      char *buf_p = (char *)buf;
      size_t remaining = n;
      for (;;) {
        char *nl = memchr(buf_p, '\n', remaining);
        if (nl == NULL) {
          if (remaining > 0) {
            char *new_carry = realloc(carry, carry_len + remaining);
            if (new_carry == NULL) {
              perror("malloc");
              exit(1);
            }
            carry = new_carry;
            memcpy(carry + carry_len, buf_p, remaining);
            carry_len += remaining;
          }
          break;
        }
        size_t seg_len = nl - buf_p;
        size_t line_len = carry_len + seg_len;
        char *line = malloc(line_len + 1);
        if (line == NULL) return -1;
        if (carry_len > 0) memcpy(line, carry, carry_len);
        if (seg_len > 0) memcpy(line + carry_len, buf_p, seg_len);
        line[line_len] = '\0';
        stack_push(&lines, line);
        free(carry);
        carry = NULL;
        carry_len = 0;
        size_t consumed = seg_len + 1;
        buf_p += consumed;
        remaining -= consumed;
      }
    }
  }
  case MODE_BYTES: {
    break;
  }

  case MODE_BLOCKS:

  default:
    fprintf(stderr, "not implemented yet\n");
  }

  return 0;
}

void prepend(char *prefix, int prefix_len, char **str, size_t *str_len, size_t *str_cap) {
  size_t new_len = *str_len + prefix_len;
  if (new_len + 1 >= *str_cap) {
    size_t new_cap = (new_len + 1) * 2;
    char *new_str = realloc(*str, new_cap);
    if (new_str == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    *str = new_str;
    *str_cap = new_cap;
  }
  memmove(*str + prefix_len, *str, *str_len);
  memcpy(*str, prefix, prefix_len);
  *str_len = new_len;
  (*str)[new_len] = '\0';
}

int tail_file(char *path, flags_t flags) {

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
    size_t want = flags.count.as.lines;

    line_stack_t s = {0};
    stack_init(&s);
    off_t end = lseek(fd, 0, SEEK_END);
    off_t pos = end;
    if (pos > 0) {
      char last = 0;
      if (lseek(fd, pos - 1, SEEK_SET) < 0) {
        perror("lseek");
        exit(EXIT_FAILURE);
      }
      int r = read(fd, &last, 1);
      if (r < 0) {
        perror("read");
        exit(EXIT_FAILURE);
      }
      if (r == 1 && last == '\n') {
        pos -= 1;
      }
    }
    size_t carry_cap = BLOCK_SIZE;
    size_t carry_len = 0;
    char *carry = malloc(BLOCK_SIZE);
    if (carry == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    size_t have_lines = 0;

    char buf[BLOCK_SIZE];
    while (pos > 0 && have_lines < want) {
      off_t block_start = max(0, pos - BLOCK_SIZE);
      lseek(fd, block_start, SEEK_SET);
      int n = read(fd, buf, pos - block_start);
      if (n < 0) {
        perror("read");
        exit(EXIT_FAILURE);
      }
      pos = block_start;
      int right = n;

      for (int i = n - 1; i >= 0; i--) {
        if (buf[i] == '\n') {
          int start = i + 1;
          int slice_len = right - start;
          prepend(buf + start, slice_len, &carry, &carry_len, &carry_cap);
          char *line = malloc(carry_len + 1);
          if (line == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
          }
          memcpy(line, carry, carry_len);
          line[carry_len] = '\0';
          stack_push(&s, line);
          carry_len = 0;
          right = i;
          have_lines++;
          if (have_lines == want) break;
        }
      }
      if (have_lines < want && right > 0) {
        prepend(buf, right, &carry, &carry_len, &carry_cap);
      }
    }

    if (have_lines < want && carry_len > 0) {
      char *line = malloc(carry_len + 1);
      if (line == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
      }
      memcpy(line, carry, carry_len);
      line[carry_len] = '\0';
      stack_push(&s, line);
      have_lines++;
      carry_len = 0;
    }
    flags.reverse = !flags.reverse;
    write_lines(STDOUT_FILENO, flags, &s);
    stack_free(&s);
    free(carry);
    break;
  }
  case MODE_BYTES: {
    off_t want = (off_t)flags.count.as.bytes;
    off_t end = lseek(fd, 0, SEEK_END);
    if (want > end) want = end;
    lseek(fd, end - want, SEEK_SET);
    char *out = malloc(want);
    if (out == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    ssize_t n = read(fd, out, want);
    if (n < 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }
    write_bytes(STDOUT_FILENO, flags, out, want);
    break;
  }
  case MODE_BLOCKS:
  default:
    fprintf(stderr, "not implemented yet\n");
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

  if (argc == optind) {
    if (stream_copy(STDIN_FILENO, STDOUT_FILENO, flags) < 0) {
      error_errno(argv[0], errno, "stdin");
      return 1;
    }
  }

  // requirements
  // no quiet or verbose specified,
  // if quiet or verbose that overrides
  if (!flags.quiet && !flags.verbose) {
    if (argc - optind == 1) {
      flags.quiet = true;
    } else {
      flags.verbose = true;
    }
  }

  // if queiet
  int exit_code = 0;
  for (int i = optind; i < argc; i++) {
    char *filename = argv[i];
    if (!flags.quiet) fprintf(stdout, "==> %s <==\n", filename);
    if (tail_file(filename, flags) < 0) {
      error_errno(argv[0], errno, filename);
      exit_code = 1;
    }
  }
  return exit_code;
}
