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
      opt_a = true;
      break;
    case 'c':
      opt_c = true;
      break;
    case 'h':
      opt_h = true;
      break;
    case 'm':
      opt_m = true;
      break;
    case 't':
      t_str = optarg;
      break;
    case 'r':
      r_path = optarg;
      break;
    default:
      usage(argv[0]);
      return 1;
    }
  }
}
