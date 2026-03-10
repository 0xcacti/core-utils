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
  while ((ent = fts_read(fts)) != NULL) {
    switch (ent->fts_info) {
    case FTS_DNR: // TODO
      break;
    case FTS_D:
    case FTS_DP:
      break;
    case FTS_F: {
      struct stat st;
      int r = stat(path, &st);
      if (r < 0) { // what does du do if it fails
        int saved = errno;
        fts_close(fts);
        errno = saved;
        return -1;
      }
      fprintf(stdout, "%lld %s\n", (long long)st.st_blocks, path);
    }
    }
  }
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
