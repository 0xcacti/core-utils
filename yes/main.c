#include <stdio.h>

int main(int argc, char *argv[]) {
  char *str = NULL;
  if (argc < 2) {
    str = "y";
  } else {
    str = argv[1];
  }
  while (1) {
    printf("%s\n", str);
  }
}
