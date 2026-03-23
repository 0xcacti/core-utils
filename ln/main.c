#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
  bool super_force;
  bool hard_link;
  bool force;
} flags_t;

// The options are as follows:

// -F    If the target file already exists and is a directory, then remove it so that the link may
// occur.  The -F option should be used with either -f or -i options.
//       If neither -f nor -i is specified, -f is implied.  The -F option is a no-op unless -s is
//       specified.

// -L    When creating a hard link to a symbolic link, create a hard link to the target of the
// symbolic link.  This is the default.  This option cancels the -P
//       option.

// -P    When creating a hard link to a symbolic link, create a hard link to the symbolic link
// itself.  This option cancels the -L option.

// -f    If the target file already exists, then unlink it so that the link may occur.  (The -f
// option overrides any previous -i and -w options.)

// -h    If the target_file or target_dir is a symbolic link, do not follow it.  This is most useful
// with the -f option, to replace a symlink which may point to a
//       directory.

// -i    Cause ln to write a prompt to standard error if the target file exists.  If the response
// from the standard input begins with the character ‘y’ or ‘Y’, then
//       unlink the target file so that the link may occur.  Otherwise, do not attempt the link.
//       (The -i option overrides any previous -f options.)

// -n    Same as -h, for compatibility with other ln implementations.

// -s    Create a symbolic link.

// -v    Cause ln to be verbose, showing files as they are processed.

// -w    Warn if the source of a symbolic link does not currently exist.

static void usage(const char *progname) {
  dprintf(STDERR_FILENO,
          "usage: %s [-L | -P | -s [-F]] [-f | -iw] [-hnv] source_file [target_file]\n"
          "       %s [-L | -P | -s [-F]] [-f | -iw] [-hnv] source_file ... target_dir\n",
          progname, progname);
  exit(2);
}

int main(int argc, char *argv[]) {
  int ch;
  flags_t flags = {0};

  while ((ch = getopt(argc, argv, "h")) != -1) {
    switch (ch) {
    case 'h':
      flags.hard = true;
      break;
    default:
      usage(argv[0]);
    }
  }
  if (optind == argc) usage(argv[0]);

  return 0;
}
