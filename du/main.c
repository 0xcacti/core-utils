#include <errno.h>
#include <fts.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define BLOCK_SIZE 512

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

typedef struct {
  long long s;
  bool active;
} dirsum_t;

typedef struct {
  size_t len;
  size_t capacity;
  dirsum_t *data;
} dirsum_stack_t;

static void dirsum_stack_init(dirsum_stack_t *ds) {
  ds->len = 0;
  ds->capacity = 0;
  ds->data = NULL;
}

static int dirsum_stack_push(dirsum_stack_t *ds, dirsum_t d) {
  // printf("pushing\n");
  if (ds->len >= ds->capacity) {
    size_t new_cap = ds->capacity == 0 ? 8 : ds->capacity * 2;
    dirsum_t *new_data = realloc(ds->data, new_cap * sizeof(dirsum_stack_t));
    if (!new_data) return -1;
    ds->data = new_data;
    ds->capacity = new_cap;
  }
  ds->data[ds->len++] = d;
  return 0;
}

static dirsum_t dirsum_stack_pop(dirsum_stack_t *ds) {
  if (ds->len == 0) return (dirsum_t){.s = -1, .active = false};
  return ds->data[--ds->len];
}

static dirsum_t dirsum_stack_peek(dirsum_stack_t *ds) {
  if (ds->len == 0) return (dirsum_t){.s = -1, .active = false};
  return ds->data[ds->len - 1];
}

static void dirsum_stack_top_sum(dirsum_stack_t *ds, long long addend) {
  ds->data[ds->len - 1].s += addend;
}

static void dirsum_stack_free(dirsum_stack_t *ds) {
  free(ds->data);
  ds->len = 0;
  ds->capacity = 0;
}

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s [-x] [-h | -k] [-a | -s | -d depth] [file ...]\n", progname);
  exit(2);
}

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
}

static int parse_nonnegative_int(const char *s, const char *progname) {
  char *end = NULL;
  errno = 0;
  long value = strtol(s, &end, 10);
  if (errno == ERANGE || value < 0 || value > INT_MAX) usage(progname);
  if (end == s || *end != '\0') usage(progname);
  return (int)value;
}

static long long simple_block_size(char *path) {
  struct stat st;
  int r = stat(path, &st);
  if (r < 0) return -1;

  return (long long)st.st_blocks;
}

static int du_path(char *path, flags_t flags) {
  // fts walk
  int fts_flags = FTS_PHYSICAL | FTS_NOCHDIR;
  if (flags.one_file_system) fts_flags |= FTS_XDEV;

  (void)fts_flags;
  char *paths[2];
  paths[0] = path;
  paths[1] = NULL;
  FTS *fts = fts_open(paths, fts_flags, NULL);
  if (fts == NULL) return -1;
  enum { SKIPPED = 1 };

  FTSENT *ent = NULL;
  dirsum_stack_t ds = {0};
  dirsum_stack_init(&ds);
  while ((ent = fts_read(fts)) != NULL) {
    switch (ent->fts_info) {
    case FTS_D: {
      long long sz = simple_block_size(ent->fts_path);
      if (sz < 0) {
        int saved = errno;
        dirsum_stack_free(&ds);
        fts_close(fts);
        errno = saved;
        return -1;
      }

      if (dirsum_stack_push(&ds, (dirsum_t){.s = sz, .active = true}) < 0) {
        int saved = errno;
        dirsum_stack_free(&ds);
        fts_close(fts);
        errno = saved;
        return -1;
      }
      break;
    }
    case FTS_DP: {
      dirsum_t curr = dirsum_stack_pop(&ds);
      if (curr.s < 0) {
        int saved = errno;
        dirsum_stack_free(&ds);
        fts_close(fts);
        errno = saved;
        return -1;
      }
      fprintf(stdout, "%lld %s\n", curr.s, ent->fts_path);
      dirsum_stack_top_sum(&ds, curr.s);
      break;
    }
    case FTS_F: {
      long long sz = simple_block_size(ent->fts_path);
      if (sz < 0) {
        int saved = errno;
        dirsum_stack_free(&ds);
        fts_close(fts);
        errno = saved;
        return -1;
      }
      if (dirsum_stack_peek(&ds).s < 0) {
        if (ent->fts_level == 0 || flags.print_mode == PRINT_ALL) {
          fprintf(stdout, "%lld %s\n", sz, ent->fts_path);
        }
      } else {
        dirsum_stack_top_sum(&ds, sz);
      }
      break;
    }
    case FTS_DNR: // fails for this directory path, could be modified to simply warn
    case FTS_ERR:
    case FTS_NS: {
      int saved = errno;
      dirsum_stack_free(&ds);
      fts_close(fts);
      errno = saved;
      return -1;
    }
    default:
      fprintf(stderr, "unhandled FTS case\n");
    }
  }
  fts_close(fts);
  dirsum_stack_free(&ds);
  return 0;
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

  for (int i = optind; i < argc; i++) {
    if (du_path(argv[i], flags) < 0) error_errno(argv[0], argv[i]);
  }
}
