#include <stdio.h>
#include <stdlib.h>

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
