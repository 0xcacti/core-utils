#include <getopt.h>
#include <stdio.h>
#include <sys/stat.h>

static void usage(const char *progname) {
  dprintf(STDERR_FILENO,
          "Usage: %s [-achm] [-A [-][[hh]mm]SS] [-t [[CC]YY]MMDDhhmm[.SS]] [-r file] [-d "
          "YYYY-MM-DDThh:mm:SS[.frac][tz]] file ...\n",
          progname);
  // usage: touch [-A [-][[hh]mm]SS] [-achm] [-r file] [-t [[CC]YY]MMDDhhmm[.SS]]
  //  [-d YYYY-MM-DDThh:mm:SS[.frac][tz]] file ...
}

int main(int argc, char *argv[]) {
  int ch;

  while ((ch = getopt(argc, argv, "achm")) != -1) {
    switch (ch) {
    case 'a':
      break;
    case 'c':
      break;
    case 'h':
      break;
    case 'm':
      break;
    default:
      break;
    }
  }
}
