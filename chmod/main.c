#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef enum {
  SYMLINK_NONE, // -P
  SYMLINK_CMD,  // -H
  SYMLINK_ALL,  // -L
} symlink_behavior_e;

typedef struct {
  bool force;     // -f
  bool no_follow; // -h
  bool verbose;   // -v
  bool recurse;   // -R
  symlink_behavior_e sym_mode;
} flags_t;

typedef enum {
  MODE_OCTAL,
  MODE_SYMBOLIC,
  MODE_BAD,
} update_mode_e;

typedef struct {
  update_mode_e form;
  bool setuid;
  bool setgid;
  int user;
  int group;
  int other;
  int all;
} parsed_mode_t;

typedef enum {
  CHMOD_OK,
  CHMOD_ERRNO,
  CHMOD_BAD_MODE,
} chmod_result_e;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s [-fhv] [-R [-H | -L | -P]] mode file ...\n", progname);
  exit(2);
}

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
}

static void error_msg(const char *progname, const char *m1, const char *m2) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, m1, m2);
}

// static int current_mode(const char *file, mode_t *out) {
//   struct stat st;
//   int r = stat(file, &st);
//   if (r < 0) return -1;
//   *out = st.st_mode & 07777;
//   return 0;
// }

static update_mode_e parse_mode(const char *mode) {
  switch (mode[0]) {
  case 'u':
  case 'g':
  case 'a':
  case 'o':
    return MODE_SYMBOLIC;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
    return MODE_OCTAL;
  default:
    return MODE_BAD;
  }
}

static int parse_octal(const char *s, mode_t *out) {
  char *end;
  unsigned long value;
  if (s == NULL || *s == '\0') {
    return -1;
  }

  for (const char *c = s; *c != '\0'; c++) {
    if (*c < '0' || *c > '7') return -1;
  }

  errno = 0;
  value = strtoul(s, &end, 8);
  if (errno != 0 || *end != '\0') return -1;
  if (value > 07777UL) return -1;

  *out = (mode_t)value;
  return 0;
}

static int target_mode(const char *mode_str, const char *f, mode_t *out) {
  update_mode_e mode_form = parse_mode(mode_str);
  if (mode_form == MODE_BAD) return CHMOD_BAD_MODE;

  if (mode_form == MODE_OCTAL) {
    if (parse_octal(f, out) < 0) return CHMOD_BAD_MODE;
    return 0;
  } else if (mode_form == MODE_SYMBOLIC) {
    fprintf(stdout, "Not supported yet!!\n");
    exit(1);
  }

  return CHMOD_OK;
}

int main(int argc, char *argv[]) {
  int ch;
  flags_t flags = {0};
  flags.sym_mode = SYMLINK_NONE;
  while ((ch = getopt(argc, argv, "fhvRPHL")) != -1) {
    switch (ch) {
    case 'f':
      flags.force = true;
      break;
    case 'h':
      flags.no_follow = true;
      break;
    case 'v':
      flags.verbose = true;
      break;
    case 'R':
      flags.recurse = true;
      break;
    case 'P':
      flags.sym_mode = SYMLINK_NONE;
      break;
    case 'H':
      flags.sym_mode = SYMLINK_CMD;
      break;
    case 'L':
      flags.sym_mode = SYMLINK_ALL;
      break;
    default:
      usage(argv[0]);
    }
  }

  int num_args = argc - optind;
  if (num_args < 2) usage(argv[0]);

  for (int i = optind + 1; i < argc; i++) {
    mode_t target = {0};
    chmod_result_e r = target_mode(argv[optind], argv[i], &target);
    switch (r) {
    case CHMOD_BAD_MODE:
      error_msg(argv[0], "Invalid file mode", argv[optind]);
      break;
    case CHMOD_ERRNO:
      error_errno(argv[0], argv[optind]);
      break;
    case CHMOD_OK:
      break;
    }
    if (chmod(argv[i], target) < 0) {
      error_errno(argv[0], argv[i]);
    } else {
      if (flags.verbose) fprintf(stdout, "%s\n", argv[i]);
    }
  }

  return 0;
}
