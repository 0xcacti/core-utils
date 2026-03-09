#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  bool set;
  int d;
} depth_t;

typedef struct {
  bool h_no_fs_mounts;
  bool x_human_readable;
  bool k_block_kib;
  bool a_all_files;
  bool s_summary;
  bool a_all;
  depth_t d;

} flags_t;

static void usage(const char *progname) {
  dprintf(STDERR_FILENO, "%s [-x] [-h | -k] [-a | -s | -d] [file ...]\n", progname);
  exit(2);
}

int main(int argc, char **argv) {
  usage(argv[0]);
}
