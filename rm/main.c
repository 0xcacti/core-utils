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
#include <sys/stat.h>

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

static void usage(const char *progname) {
  fprintf(stderr, "Usage: %s [-f | -i] [-drv] file ...\n", progname);
  exit(EXIT_FAILURE);
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
  exit(2);
}

bool static is_illegal(const char *path) {
  char *p;
  struct stat sb;
  struct stat root;
  stat("/", &root);
  if (lstat(path, &sb) == 0 && root.st_ino == sb.st_ino && root.st_dev == sb.st_dev) {
    return true;
  }
  p = strrchr(path, '\0');
  while (--p > path && *p == '/') *p = '\0';

  // go to after last '/'
  if ((p = strrchr(path, '/')) != NULL) {
    p = p + 1;
  } else {
    p = (char *)path;
  }

  if (p[0] == '.' && p[1] == '\0') return true;
  if (p[0] == '.' && p[1] == '.' && p[2] == '\0') return true;

  return false;
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

  for (int i = optind; i < argc; i++) {
    if (is_illegal(argv[i])) {
      error_msg(argv[0], "refusing to remove '/', '.', or '..'");
    }
  }
}
