#include <stdio.h>
#include <stdlib.h>

void* ensureAlloc(void* ptr, const char* errorMsg) {
    if (!ptr) {
        fprintf(stderr, "Allocation failed: %s\n", errorMsg);
        exit(EXIT_FAILURE); // more standard than exit(1)
    }
    return ptr; // allows inline use
}