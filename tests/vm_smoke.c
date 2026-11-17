#include "chunk.h"
#include "vm.h"
#include <math.h>
#include <stdio.h>

static bool test_garbage_collection(void) {
    Chunk chunk;
    VM vm;
    chunk_init(&chunk);
    vm_init(&vm);
    bool built =
        chunk_write(&chunk, (uint8_t)OP_NEW_ARRAY, 1) &&
        chunk_write(&chunk, (uint8_t)OP_DEFINE_GLOBAL, 1) &&
        chunk_write(&chunk, 0, 1) &&
        chunk_write(&chunk, (uint8_t)OP_GET_GLOBAL, 1) &&
        chunk_write(&chunk, 0, 1) &&
        chunk_write_constant(&chunk, value_number(0), 1) &&
        chunk_write(&chunk, (uint8_t)OP_NEW_ARRAY, 1) &&
        chunk_write(&chunk, (uint8_t)OP_SET_INDEX, 1) &&
        chunk_write(&chunk, (uint8_t)OP_POP, 1);
    for (size_t i = 0; built && i < 70; ++i)
        built = chunk_write(&chunk, (uint8_t)OP_NEW_ARRAY, 1) &&
                chunk_write(&chunk, (uint8_t)OP_POP, 1);
    built = built &&
        chunk_write(&chunk, (uint8_t)OP_GET_GLOBAL, 1) &&
        chunk_write(&chunk, 0, 1) &&
        chunk_write(&chunk, (uint8_t)OP_RETURN, 1);
    VMResult status = built ? vm_run(&vm, &chunk) : VM_INVALID_BYTECODE;
    bool passed = status == VM_OK && vm.gc_runs > 0 &&
                  vm.result.type == VALUE_ARRAY && vm.array_count == 10 &&
                  vm.result.as.array->count == 1 &&
                  vm.result.as.array->items[0].type == VALUE_ARRAY;
    if (!passed)
        fprintf(stderr, "GC test failed (status=%d, runs=%zu, arrays=%zu)\\n",
                (int)status, vm.gc_runs, vm.array_count);
    vm_free(&vm);
    chunk_free(&chunk);
    return passed;
}

\nint main(void) {
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

    if (!test_garbage_collection()) return 1;\n    puts("VM smoke test passed: arithmetic and tracing garbage collection");
    chunk_free(&chunk);
    return 0;
}
