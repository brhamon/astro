/*
 * gen_constants.c -- dump the reference DE440 named constants as CSV.
 *
 * con440.c (repo root) is a hand-captured transcription of the ephemeris's
 * record-2 name/value block, kept because NOVAS-C discards it (research doc
 * 1.6). test_constants replays this against the Constants the C++ reader parses
 * from record 2, so the two must agree.
 *
 * We reuse con440.c's `cnam[]`/`cons[]` arrays directly, renaming its main() so
 * it does not collide with ours. con440.c is added to this target's include
 * path by CMake.
 *
 * Usage:
 *   gen_constants [out.csv]     (stdout if out.csv omitted)
 *
 * CSV columns: name,value
 */

#define main con440_unused_main
#include "con440.c"
#undef main

#include <stdio.h>

int main(int argc, char** argv) {
  if (argc > 2) {
    fprintf(stderr, "usage: %s [out.csv]\n", argv[0]);
    return 2;
  }
  if (argc == 2 && !freopen(argv[1], "w", stdout)) {
    perror(argv[1]);
    return 1;
  }

  const int n = (int)NELTS(cnam);
  if ((size_t)n != NELTS(cons)) {
    fprintf(stderr, "con440.c: %d names but %zu values\n", n, NELTS(cons));
    return 1;
  }
  printf("name,value\n");
  for (int i = 0; i < n; ++i) printf("%s,%.17g\n", cnam[i], cons[i]);
  fprintf(stderr, "emitted %d reference constants\n", n);
  return 0;
}
