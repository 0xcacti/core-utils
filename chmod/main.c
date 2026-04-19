#include <errno.h>
#include <fts.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
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

typedef enum {
  OP_ADD,
  OP_REMOVE,
  OP_SET,
} symbolic_op_e;

enum {
  WHO_U = 1 << 0,
  WHO_G = 1 << 1,
  WHO_O = 1 << 2,
  WHO_A = WHO_U | WHO_G | WHO_O,
};

enum {
  PERM_R = 1 << 0,
  PERM_W = 1 << 1,
  PERM_X = 1 << 2,
};

typedef struct {
  bool who_omitted;
  unsigned who_mask;
  symbolic_op_e op;
  unsigned perm_mask;
} symbolic_clause_t;

typedef struct {
  update_mode_e kind;
  mode_t octal_mode;
  mode_t creation_mask;
  symbolic_clause_t *clauses;
  size_t clause_count;
} mode_update_t;

static int push_clause(mode_update_t *out, symbolic_clause_t clause) {
  size_t new_count = out->clause_count + 1;
  symbolic_clause_t *new_clauses = realloc(out->clauses, new_count * sizeof(*new_clauses));
  if (new_clauses == NULL) return -1;
  out->clauses = new_clauses;
  out->clauses[out->clause_count] = clause;
  out->clause_count = new_count;
  return 0;
}

static void free_mode_update(mode_update_t *mu) {
  free(mu->clauses);
  mu->clauses = NULL;
  mu->clause_count = 0;
}

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

static update_mode_e parse_mode(const char *mode) {
  switch (mode[0]) {
  case 'u':
  case 'g':
  case 'a':
  case 'o':
  case '+':
  case '-':
  case '=':
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

static int parse_who(const char **mode_str, unsigned *out, bool *omitted) {
  const char *s = *mode_str;
  unsigned who = 0;
  *omitted = false;
  while (*s == 'u' || *s == 'g' || *s == 'o' || *s == 'a') {
    switch (*s) {
    case 'u':
      who |= WHO_U;
      break;
    case 'g':
      who |= WHO_G;
      break;
    case 'o':
      who |= WHO_O;
      break;
    case 'a':
      who |= WHO_A;
      break;
    }
    s++;
  }
  if (who == 0) *omitted = true;
  *mode_str = s;
  *out = who;
  return 0;
}

static int parse_op(const char **mode_str, symbolic_op_e *out) {
  switch (**mode_str) {
  case '+':
    *out = OP_ADD;
    break;
  case '-':
    *out = OP_REMOVE;
    break;
  case '=':
    *out = OP_SET;
    break;
  default:
    return -1;
  }
  (*mode_str)++;
  return 0;
}

static int parse_perm(const char **mode_str, unsigned *out) {
  const char *s = *mode_str;
  unsigned perm = 0;
  if (*s != 'r' && *s != 'w' && *s != 'x') return -1;

  while (*s == 'r' || *s == 'w' || *s == 'x') {
    switch (*s) {
    case 'r':
      perm |= PERM_R;
      break;
    case 'w':
      perm |= PERM_W;
      break;
    case 'x':
      perm |= PERM_X;
      break;
    }
    s++;
  }
  *mode_str = s;
  *out = perm;
  return 0;
}

static int parse_symbolic(const char *mode_str, mode_update_t *out) {
  out->kind = MODE_SYMBOLIC;
  out->octal_mode = 0;
  out->clauses = NULL;
  out->clause_count = 0;
  while (*mode_str != '\0') {
    symbolic_clause_t clause = {0};
    if (parse_who(&mode_str, &clause.who_mask, &clause.who_omitted) < 0) return -1;
    if (parse_op(&mode_str, &clause.op) < 0) return -1;
    if (parse_perm(&mode_str, &clause.perm_mask) < 0) return -1;
    if (push_clause(out, clause) < 0) return -1;
    if (*mode_str == '\0') break;
    if (*mode_str != ',') return -1;
    mode_str++;
  }

  return 0;
}

static chmod_result_e parse_mode_update(const char *mode_str, mode_update_t *out) {
  update_mode_e mode_form = parse_mode(mode_str);
  if (mode_form == MODE_BAD) return CHMOD_BAD_MODE;

  out->kind = mode_form;

  if (mode_form == MODE_OCTAL) {
    if (parse_octal(mode_str, &out->octal_mode) < 0) return CHMOD_BAD_MODE;
    return CHMOD_OK;
  }

  if (parse_symbolic(mode_str, out) < 0) return CHMOD_BAD_MODE;
  return CHMOD_OK;
}

static mode_t who_clear_mask(unsigned who_mask) {
  mode_t mask = 0;
  if ((who_mask & WHO_U) != 0) mask |= 0700;
  if ((who_mask & WHO_G) != 0) mask |= 0070;
  if ((who_mask & WHO_O) != 0) mask |= 0007;
  return mask;
}

static mode_t who_set_mask(unsigned who_mask, unsigned perm_mask) {
  mode_t mask = 0;
  if ((who_mask & WHO_U) != 0) {
    if ((perm_mask & PERM_R) != 0) mask |= 0400;
    if ((perm_mask & PERM_W) != 0) mask |= 0200;
    if ((perm_mask & PERM_X) != 0) mask |= 0100;
  }

  if ((who_mask & WHO_G) != 0) {
    if ((perm_mask & PERM_R) != 0) mask |= 0040;
    if ((perm_mask & PERM_W) != 0) mask |= 0020;
    if ((perm_mask & PERM_X) != 0) mask |= 0010;
  }

  if ((who_mask & WHO_O) != 0) {
    if ((perm_mask & PERM_R) != 0) mask |= 0004;
    if ((perm_mask & PERM_W) != 0) mask |= 0002;
    if ((perm_mask & PERM_X) != 0) mask |= 0001;
  }

  return mask;
}

static void compute_target_mode(const mode_update_t *update, mode_t old_mode, mode_t *out) {
  if (update->kind == MODE_OCTAL) {
    *out = update->octal_mode;
    return;
  }

  mode_t new_mode = old_mode & 07777;

  for (size_t i = 0; i < update->clause_count; i++) {
    symbolic_clause_t clause = update->clauses[i];

    mode_t clear_mask;
    mode_t set_mask;
    if (clause.who_omitted) {
      mode_t all_bits = who_set_mask(WHO_A, clause.perm_mask);
      mode_t effective_bits = all_bits & ~update->creation_mask;
      clear_mask = effective_bits;
      set_mask = effective_bits;
    } else {
      clear_mask = who_clear_mask(clause.who_mask);
      set_mask = who_set_mask(clause.who_mask, clause.perm_mask);
    }

    switch (clause.op) {
    case OP_ADD:
      new_mode |= set_mask;
      break;
    case OP_REMOVE:
      new_mode &= ~set_mask;
      break;
    case OP_SET:
      new_mode = (new_mode & ~clear_mask) | set_mask;
      break;
    }
  }

  *out = new_mode;
}

static chmod_result_e chmod_file(const char *file, mode_t new_mode, flags_t flags) {
  if (flags.no_follow) {
    if (lchmod(file, new_mode) < 0) return CHMOD_ERRNO;
  } else {
    if (chmod(file, new_mode) < 0) return CHMOD_ERRNO;
  }
  return CHMOD_OK;
}

static chmod_result_e chmod_dir(const char *file, mode_update_t *mu, flags_t flags) {
  int fts_flags = FTS_NOCHDIR;
  switch (flags.sym_mode) {
  case SYMLINK_NONE:
    fts_flags |= FTS_PHYSICAL;
    break;
  case SYMLINK_CMD:
    fts_flags |= FTS_PHYSICAL | FTS_COMFOLLOW;
    break;
  case SYMLINK_ALL:
    fts_flags |= FTS_LOGICAL;
    break;
  }

  char *paths[2];
  paths[0] = (char *)file;
  paths[1] = NULL;

  FTS *fts = fts_open(paths, fts_flags, NULL);
  if (fts == NULL) return CHMOD_ERRNO;

  chmod_result_e ret = CHMOD_OK;
  FTSENT *ent = NULL;
  while ((ent = fts_read(fts)) != NULL) {
    switch (ent->fts_info) {
    case FTS_F:
    case FTS_DP: {
      mode_t new_mode;
      compute_target_mode(mu, ent->fts_statp->st_mode, &new_mode);
      if (chmod_file(ent->fts_accpath, new_mode, flags) != CHMOD_OK) {
        if (!flags.force) ret = CHMOD_ERRNO;
      }
      break;
    }

    case FTS_D:
    case FTS_SL:
    case FTS_SLNONE:
      break;

    case FTS_DNR:
    case FTS_ERR:
    case FTS_NS:
    case FTS_DC:
      if (!flags.force) ret = CHMOD_ERRNO;
      break;
    }
  }

  if (fts_close(fts) < 0 && !flags.force) return CHMOD_ERRNO;
  return ret;
}

static chmod_result_e chmod_target(const char *file, mode_update_t *mu, flags_t flags) {
  int r;
  struct stat st;
  mode_t new_mode;

  if (flags.sym_mode == SYMLINK_ALL) {
    r = stat(file, &st);
  } else {
    r = lstat(file, &st);
  }

  if (r < 0) return CHMOD_ERRNO;

  if (flags.recurse) {
    if (S_ISDIR(st.st_mode)) return chmod_dir(file, mu, flags);

    if (flags.sym_mode == SYMLINK_CMD && S_ISLNK(st.st_mode)) {
      struct stat target_st;
      if (stat(file, &target_st) < 0) return CHMOD_ERRNO;
      if (S_ISDIR(target_st.st_mode)) return chmod_dir(file, mu, flags);
    }
  }

  compute_target_mode(mu, st.st_mode, &new_mode);
  return chmod_file(file, new_mode, flags);
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

  int ret = 0;

  mode_t creation_mask = umask(0);
  umask(creation_mask);
  mode_update_t update = {0, .creation_mask = creation_mask};
  chmod_result_e parse_result = parse_mode_update(argv[optind], &update);
  switch (parse_result) {
  case CHMOD_BAD_MODE:
    error_msg(argv[0], "Invalid file mode", argv[optind]);
    exit(2);
    break;
  case CHMOD_ERRNO:
    fprintf(stdout, "This should be unreachable\n");
    break;
  case CHMOD_OK:
    break;
  }

  for (int i = optind + 1; i < argc; i++) {
    chmod_result_e r = chmod_target(argv[i], &update, flags);
    switch (r) {
    case CHMOD_OK:
      if (flags.verbose) fprintf(stdout, "%s\n", argv[i]);
      break;
    case CHMOD_ERRNO:
      if (!flags.force) {
        error_errno(argv[0], argv[i]);
        ret = 1;
      }
      break;
    case CHMOD_BAD_MODE:
    default:
      fprintf(stdout, "This should be unreachable\n");
      break;
    }
  }

  free_mode_update(&update);
  return ret;
}
