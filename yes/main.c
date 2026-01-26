#include <errno.h>
#include <signal.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  const char *str = (argc < 2) ? "y" : argv[1];
  signal(SIGPIPE, SIG_DFL);
  while (1) {
    if (puts(str) == EOF) {
      if (ferror(stdout) && errno == EPIPE) return 0;
      return 1;
    }
  }
}
