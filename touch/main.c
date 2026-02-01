#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "Usage: %s [-achm] [-t [[CC]YY]MMDDhhmm[.SS]] [-r file] file ...\n",
          progname);
  exit(2);
}

static void error_msg(const char *progname, const char *msg) {
  dprintf(STDERR_FILENO, "%s: %s\n", progname, msg);
  exit(2);
}

int parse_time(char *s, struct timespec *ts) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char *dot = strchr(s, '.');

  if (dot == NULL) {
    ts->tv_sec = 0;
  } else {
    if (strlen(dot) != 3) {
      return -1;
    } else {
      *dot++ = '\0';
      t->tm_sec = (dot[0] - '0') * 10 + (dot[1] - '0');
    }
  }
  size_t len = strlen(s);
  switch (len) {
  case 12: {
    t->tm_year =
        ((s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0')) - 1900;
    t->tm_mon = ((s[4] - '0') * 10 + (s[5] - '0')) - 1;
    t->tm_mday = (s[6] - '0') * 10 + (s[7] - '0');
    t->tm_hour = (s[8] - '0') * 10 + (s[9] - '0');
    t->tm_min = (s[10] - '0') * 10 + (s[11] - '0');
    break;
  }
  case 10: {
    int y = ((s[0] - '0') * 10 + (s[1] - '0'));
    if (y < 69) {
      y += 2000;
    } else {
      y += 1900;
    }
    t->tm_year = y - 1900;
    t->tm_mon = ((s[2] - '0') * 10 + (s[3] - '0')) - 1;
    t->tm_mday = (s[4] - '0') * 10 + (s[5] - '0');
    t->tm_hour = (s[6] - '0') * 10 + (s[7] - '0');
    t->tm_min = (s[8] - '0') * 10 + (s[9] - '0');
    break;
  }
  case 8: {
    t->tm_mon = ((s[0] - '0') * 10 + (s[1] - '0')) - 1;
    t->tm_mday = (s[2] - '0') * 10 + (s[3] - '0');
    t->tm_hour = (s[4] - '0') * 10 + (s[5] - '0');
    t->tm_min = (s[6] - '0') * 10 + (s[7] - '0');
    break;
  }
  default:
    return -1;
  }
  printf("Parsed time: %04d-%02d-%02d %02d:%02d:%02d\n", t->tm_year + 1900, t->tm_mon + 1,
         t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
  return 0;
}

int main(int argc, char *argv[]) {
  int ch;
  struct stat st;
  struct timespec timespecs[2];
  bool opt_a, opt_c, opt_h, opt_m = false;
  char *t_str, *r_path = NULL;

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

  if (t_str != NULL) {
    parse_time(t_str, &timespecs[0]);
  }

  // when not specified, default to -a and -m
  if (!opt_a && !opt_m) {
    opt_a = true;
    opt_m = true;
  }

  // access time
  timespecs[0] = (struct timespec){
      .tv_sec = 0,
      .tv_nsec = 0,
  };

  // modification time
  timespecs[1] = (struct timespec){
      .tv_sec = 0,
      .tv_nsec = 0,
  };

  while (optind != argc) {
    printf("Filename: %s\n", argv[optind]);
    char *path = argv[optind];
    bool exists = true;
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

    futimens(fd, timespecs);
  }
}
