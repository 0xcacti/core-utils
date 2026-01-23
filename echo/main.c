#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int suppress_newline = 0;

  int ch;
  while ((ch = getopt(argc, argv, "n")) != -1) {
    switch (ch) {
    case 'n':
      suppress_newline = 1;
      break;
    }
  }

  char *last = argv[optind];
  if (last == NULL) {
    if (!suppress_newline) printf("\n");
    return 0;
  }

  for (int i = optind; i < argc; i++) {
    char *elem = argv[i];
    if (i == argc - 1) {

      size_t len = strlen(elem);
      if (len >= 2 && elem[len - 2] == '\\' && elem[len - 1] == 'c') {
        elem[len - 2] = '\0';
        suppress_newline = 1;
      }

      if (suppress_newline) {
        printf("%s", elem);
      } else {
        printf("%s\n", elem);
      }
    } else {
      printf("%s ", elem);
    }
  }

  return 0;
}
