#include "chunk.h"
#include "vm.h"
#include <math.h>
#include <stdio.h>

int main(void) {
    Chunk chunk;
    VM vm;
    chunk_init(&chunk);
    vm_init(&vm);

    if (!chunk_write_constant(&chunk, value_number(10), 1) ||
        !chunk_write_constant(&chunk, value_number(20), 1) ||
        !chunk_write(&chunk, (uint8_t)OP_ADD, 1) ||
        !chunk_write(&chunk, (uint8_t)OP_RETURN, 1)) {
        fputs("could not construct bytecode chunk\n", stderr);
        chunk_free(&chunk);
        return 1;
    }

    VMResult status = vm_run(&vm, &chunk);
    bool passed = status == VM_OK && vm.result.type == VALUE_NUMBER &&
                  fabs(vm.result.as.number - 30.0) < 1e-12;
    if (!passed) {
        fprintf(stderr, "VM smoke test failed (status=%d)\n", (int)status);
        chunk_free(&chunk);
        return 1;
    }

    puts("VM smoke test passed: 10 + 20 = 30");
    chunk_free(&chunk);
    return 0;
}
