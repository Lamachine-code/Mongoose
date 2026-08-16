#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void *ensureAlloc(void *ptr, const char *errorMsg) {
  if (!ptr) {
    fprintf(stderr, "Memory allocation failed for: %s\n", errorMsg);
    exit(EXIT_FAILURE); // more standard than exit(1)
  }
  return ptr; // allows inline use
}

void printSyntaxErrMsg(int line, int col, char* msg) {
  fprintf(stderr, "Syntax Error (Line %d, Col %d): %s", line, col, msg);
}

void printErrMsg(int line, int col, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  fprintf(stderr, "Error (Line %d, Col %d): ", line, col);
  vfprintf(stderr, msg, args);
  fprintf(stderr, "\n");
  va_end(args);
}

