#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

// The options are as follows:

// -L    When creating a hard link to a symbolic link, create a hard link to the target of the
// symbolic link.  This is the default.  This option cancels the -P
//       option.

// -P    When creating a hard link to a symbolic link, create a hard link to the symbolic link
// itself.  This option cancels the -L option.

typedef enum {
  LINK_HARD,
  LINK_SYMBOLIC,
} link_mode_e;

typedef enum {
  SOURCE_SYMLINK_FOLLOW,
  SOURCE_SYMLINK_NO_FOLLOW,
} source_symlink_mode_e;

typedef enum {
  REPLACE_DEFAULT,
  REPLACE_FORCE,
  REPLACE_INTERACTIVE,
} replace_mode_e;

typedef struct {
  link_mode_e link_mode;             // default hard, -s => symbolic
  source_symlink_mode_e source_mode; // -L / -P, only meaningful for hard links
  replace_mode_e replace_mode;       // -f / -i
  bool verbose;                      // -v
  bool warn_dangling_source;         // -w, only meaningful with -s
  bool no_target_symlink_follow;     // -h, -n
  bool force_target_directory;       // -F, only meaningful with -s
} flags_t;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO,
          "usage: %s [-L | -P | -s [-F]] [-f | -iw] [-hnv] source_file [target_file]\n"
          "       %s [-L | -P | -s [-F]] [-f | -iw] [-hnv] source_file ... target_dir\n",
          progname, progname);
  exit(2);
}

int main(int argc, char *argv[]) {
  int ch;
  flags_t flags = {
      .link_mode = LINK_HARD,
      .source_mode = SOURCE_SYMLINK_FOLLOW,
      .replace_mode = REPLACE_DEFAULT,
      .verbose = false,
      .no_target_symlink_follow = false,
      .force_target_directory = false,
  };

  while ((ch = getopt(argc, argv, "LPsFfiwhnv")) != -1) {
    switch (ch) {
    case 'L':
      flags.source_mode = SOURCE_SYMLINK_FOLLOW;
      break;
    case 'P':
      flags.source_mode = SOURCE_SYMLINK_NO_FOLLOW;
      break;
    case 's':
      flags.link_mode = LINK_SYMBOLIC;
      break;
    case 'F':
      flags.force_target_directory = true;
      break;
    case 'f':
      flags.replace_mode = REPLACE_FORCE;
      flags.warn_dangling_source = false;
      break;
    case 'i':
      flags.replace_mode = REPLACE_INTERACTIVE;
      break;
    case 'w':
      flags.warn_dangling_source = true;
      break;
    case 'h':
    case 'n':
      flags.no_target_symlink_follow = true;
      break;
    case 'v':
      flags.verbose = true;
      break;
    default:
      usage(argv[0]);
    }
  }
  if (optind == argc) usage(argv[0]);

  return 0;
}
