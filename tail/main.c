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

// int write_all(int outfd, const char *buf, size_t len) {
//   const uint8_t *p = (const uint8_t *)buf;
//   size_t off = 0;
//   while (off < len) {
//     ssize_t n = write(outfd, p + off, len - off);
//     if (n < 0) {
//       if (errno == EINTR) continue;
//       return -1;
//     }
//     if (n == 0) {
//       errno = EIO;
//       return -1;
//     }
//     off += (size_t)n;
//   }
//   return 0;
// }

void write_lines(int outfd, flags_t flags, line_stack_t *s) {
  size_t wanted = flags.count.as.lines;
  size_t have = s->len;
  size_t len = wanted <= have ? wanted : have;
  for (size_t i = 0; i < len; i++) {
    char *out = stack_pop(s);
    ssize_t n = write(outfd, out, strlen(out));
    if (n < 0) {
      perror("write");
      exit(1);
    }
  }
}

int stream_copy(int infd, int outfd, flags_t flags) {
  (void)flags;
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
  case MODE_BLOCKS:
  case MODE_BYTES:
  default:
    fprintf(stderr, "not implemented yet");
    exit(2);
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
    if (stream_copy(STDIN_FILENO, STDOUT_FILENO, flags) < 0) {
      error_errno(argv[0], errno, "stdin");
      return 1;
    }
  }
}
