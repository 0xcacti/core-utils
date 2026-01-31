#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "Usage: %s [-achm] [-t [[CC]YY]MMDDhhmm[.SS]] [-r file] file ...\n",
          progname);
}

int main(int argc, char *argv[]) {
  int ch;

  bool opt_a = false;
  bool opt_c = false;
  bool opt_h = false;
  bool opt_m = false;
  char *t_str = NULL;
  char *r_path = NULL;

  while ((ch = getopt(argc, argv, "achmt:r:")) != -1) {
    switch (ch) {
    case 'a':
      break;
    case 'c':
      break;
    case 'h':
      break;
    case 'm':
      break;
    case 't':
      break;
    case 'r':
      break;
    default:
      break;
    }
  }
}
