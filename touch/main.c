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

static void error_errno(const char *progname, const char *filename) {
  dprintf(STDERR_FILENO, "%s: %s: %s\n", progname, filename, strerror(errno));
  exit(2);
}

int parse_time_t(char *s, struct timespec *ts) {
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
  ts[0].tv_sec = mktime(t);
  ts[1].tv_sec = ts[0].tv_sec;
  if (ts[0].tv_sec == -1) {
    return -1;
  }
  ts[0].tv_nsec = 0;
  ts[1].tv_nsec = 0;
  return 0;
}

int parse_time_r(char *ref_file, struct timespec *ts) {
  struct stat st;
  if (stat(ref_file, &st) != 0) {
    return -1;
  }
  ts[0].tv_sec = st.st_atimespec.tv_sec;
  ts[0].tv_nsec = 0;
  ts[1].tv_sec = st.st_mtimespec.tv_sec;
  ts[1].tv_nsec = 0;
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
    case 't': {
      t_str = optarg;
      int s = parse_time_t(t_str, timespecs);
      if (s != 0) {
        error_msg(argv[0],
                  "out of range or illegal time specification: [-t [[CC]YY]MMDDhhmm[.SS]]");
      }
      break;
    }
    case 'r': {
      r_path = optarg;
      int s = parse_time_r(r_path, timespecs);
      if (s != 0) error_errno(argv[0], r_path);
      break;
    }
    default:
      usage(argv[0]);
    }
  }

  // when not specified, default to -a and -m
  if (!opt_a && !opt_m) {
    opt_a = true;
    opt_m = true;
  } else if (!opt_a) {
    timespecs[0].tv_nsec = UTIME_OMIT;
  } else if (!opt_m) {
    timespecs[1].tv_nsec = UTIME_OMIT;
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
