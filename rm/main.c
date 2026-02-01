// SYNOPSIS
//      rm [-f | -i] [-dIRrvWx] file ...
//      unlink [--] file
//
// DESCRIPTION
//      The rm utility attempts to remove the non-directory type files specified on the command
//      line.  If the permissions of the file do not permit writing, and the standard input device
//      is a terminal, the user is prompted (on the standard error output) for confirmation.
//
//      The options are as follows:
//
//      -d      Attempt to remove directories as well as other types of files.
//
//      -f      Attempt to remove the files without prompting for confirmation, regardless of the
//      file's permissions.  If the file does not exist, do not display a
//              diagnostic message or modify the exit status to reflect an error.  The -f option
//              overrides any previous -i options.
//
//      -i      Request confirmation before attempting to remove each file, regardless of the file's
//      permissions, or whether or not the standard input device is a
//              terminal.  The -i option overrides any previous -f options.
//
//      -r      Attempt to remove the file hierarchy rooted in each file argument.  The -R option
//      implies the -d option.  If the -i option is specified, the user is
//              prompted for confirmation before each directory's contents are processed (as well as
//              before the attempt is made to remove the directory).  If the user does not respond
//              affirmatively, the file hierarchy rooted in that directory is skipped.
//
//      -v      Be verbose when deleting files, showing them as they are removed.
//
//      The rm utility removes symbolic links, not the files referenced by the links.
//      It is an error to attempt to remove the files /, . or ...
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct flags {
  bool d_flag;
  bool f_flag;
  bool i_flag;
  bool r_flag;
  bool v_flag;
} flags;

struct flags flags = {
    .d_flag = false,
    .f_flag = false,
    .i_flag = false,
    .r_flag = false,
    .v_flag = false,
};

static void usage() {
  fprintf(stderr, "Usage: rm [-f | -i] [-drv] file ...\n");
  exit(EXIT_FAILURE);
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
  exit(2);
}

static void resolve(const char *path) {
  char *p;
}

int main(int argc, char *argv[]) {
  int ch;
  while ((ch = getopt(argc, argv, "dfirv")) != -1) {
    switch (ch) {
    case 'd':
      flags.d_flag = true;
      break;
    case 'f':
      flags.f_flag = true;
      break;
    case 'i':
      flags.i_flag = true;
      break;
    case 'r':
      flags.r_flag = true;
      break;
    case 'v':
      flags.v_flag = true;
      break;
    default:
      usage();
      break;
    }
  }

  if (optind >= argc && !flags.f_flag) usage();

  if (argv[optind][0] == '.' && argv[optind][1] == '\0') {
    error_msg(argv[0], "\".\" and \"..\" may not be removed");
  }
  if (argv[optind][0] == '.' && argv[optind][1] == '.' && argv[optind][2] == '\0') {
    error_msg(argv[0], "\".\" and \"..\" may not be removed");
  }
}
