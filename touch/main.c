#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "Usage: %s [-achm] [-t [[CC]YY]MMDDhhmm[.SS]] [-r file] file ...\n",
          progname);
  exit(2);
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
    }
  }

  // when not specified, default to -a and -m
  if (!opt_a && !opt_m) {
    opt_a = true;
    opt_m = true;
  }

  while (optind != argc) {
    printf("Filename: %s\n", argv[optind]);
    char *path = argv[optind];
    bool exists = true;
    struct stat st;
    int ret = stat(path, &st);
    if (ret != 0) {
      if (errno == ENOENT) {
        exists = false;
      } else {
        perror("stat");
        return 1;
      }
    }

    if (!exists && opt_c) {
      optind++;
      continue;
    }

    int fd = open(path, O_CREAT | O_WRONLY, 0666);
    if (fd == -1) {
      perror("open");
      return 1;
    }
  }
}
