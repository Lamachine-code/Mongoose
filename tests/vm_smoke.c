#include "chunk.h"
#include "vm.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool emit(Chunk *chunk, OpCode opcode) {
    return chunk_write(chunk, (uint8_t)opcode, 1);
}

static bool emit_global(Chunk *chunk, OpCode opcode, uint8_t index) {
    return emit(chunk, opcode) && chunk_write(chunk, index, 1);
}

static bool emit_constant(Chunk *chunk, Value value) {
    return chunk_write_constant(chunk, value, 1);
}

static bool emit_ephemeral_arrays(Chunk *chunk, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (!emit(chunk, OP_NEW_ARRAY) || !emit(chunk, OP_POP))
            return false;
    return true;
}

static bool run_chunk(Chunk *chunk, VM *vm, const char *scenario) {
    VMResult status = vm_run(vm, chunk);
    if (status == VM_OK) return true;
    fprintf(stderr, "GC scenario '%s' failed (status=%d, runs=%zu, arrays=%zu, strings=%zu, dictionaries=%zu)\n",
            scenario, (int)status, vm->gc_runs, vm->array_count,
            vm->string_count, vm->dictionary_count);
    return false;
}

static bool test_gc_nested_arrays(void) {
    Chunk chunk;
    VM vm;
    chunk_init(&chunk);
    vm_init(&vm);
    bool built =
        emit(&chunk, OP_NEW_ARRAY) &&
        emit_global(&chunk, OP_DEFINE_GLOBAL, 0) &&
        emit_global(&chunk, OP_GET_GLOBAL, 0) &&
        emit_constant(&chunk, value_number(0)) &&
        emit(&chunk, OP_NEW_ARRAY) &&
        emit(&chunk, OP_SET_INDEX) &&
        emit(&chunk, OP_POP) &&
        emit_ephemeral_arrays(&chunk, 70) &&
        emit_global(&chunk, OP_GET_GLOBAL, 0) &&
        emit(&chunk, OP_RETURN);
    bool passed = built && run_chunk(&chunk, &vm, "nested arrays") &&
        vm.gc_runs > 0 && vm.result.type == VALUE_ARRAY &&
        vm.array_count <= 11 && vm.result.as.array->count == 1 &&
        vm.result.as.array->items[0].type == VALUE_ARRAY;
    if (!passed)
        fprintf(stderr, "Nested-array GC assertions failed (runs=%zu, arrays=%zu)\n",
                vm.gc_runs, vm.array_count);
    vm_free(&vm);
    chunk_free(&chunk);
    return passed;
}

static bool test_gc_dictionary_retains_array(void) {
    Chunk chunk;
    VM vm;
    uint16_t key_index;
    String *key = string_new("payload", 7);
    chunk_init(&chunk);
    vm_init(&vm);
    bool built = key && chunk_add_constant(&chunk, value_string(key), &key_index) &&
        emit(&chunk, OP_NEW_DICTIONARY) &&
        emit_global(&chunk, OP_DEFINE_GLOBAL, 0) &&
        emit_global(&chunk, OP_GET_GLOBAL, 0) &&
        emit(&chunk, OP_CONSTANT) &&
        chunk_write(&chunk, (uint8_t)(key_index & 0xff), 1) &&
        chunk_write(&chunk, (uint8_t)(key_index >> 8), 1) &&
        emit(&chunk, OP_NEW_ARRAY) &&
        emit(&chunk, OP_SET_INDEX) &&
        emit(&chunk, OP_POP) &&
        emit_ephemeral_arrays(&chunk, 70) &&
        emit_global(&chunk, OP_GET_GLOBAL, 0) &&
        emit(&chunk, OP_CONSTANT) &&
        chunk_write(&chunk, (uint8_t)(key_index & 0xff), 1) &&
        chunk_write(&chunk, (uint8_t)(key_index >> 8), 1) &&
        emit(&chunk, OP_GET_INDEX) &&
        emit(&chunk, OP_RETURN);
    bool passed = built && run_chunk(&chunk, &vm, "dictionary retains array") &&
        vm.gc_runs > 0 && vm.result.type == VALUE_ARRAY &&
        vm.dictionary_count == 1 && vm.array_count <= 11;
    if (!passed)
        fprintf(stderr, "Dictionary-retention GC assertions failed (runs=%zu, arrays=%zu, dictionaries=%zu)\n",
                vm.gc_runs, vm.array_count, vm.dictionary_count);
    vm_free(&vm);
    chunk_free(&chunk);
    return passed;
}

static bool test_gc_circular_array_reference(void) {
    Chunk chunk;
    VM vm;
    chunk_init(&chunk);
    vm_init(&vm);
    bool built =
        emit(&chunk, OP_NEW_ARRAY) &&
        emit_global(&chunk, OP_DEFINE_GLOBAL, 0) &&
        emit_global(&chunk, OP_GET_GLOBAL, 0) &&
        emit_constant(&chunk, value_number(0)) &&
        emit_global(&chunk, OP_GET_GLOBAL, 0) &&
        emit(&chunk, OP_SET_INDEX) &&
        emit(&chunk, OP_POP) &&
        emit_ephemeral_arrays(&chunk, 70) &&
        emit_global(&chunk, OP_GET_GLOBAL, 0) &&
        emit(&chunk, OP_RETURN);
    bool passed = built && run_chunk(&chunk, &vm, "circular array reference") &&
        vm.gc_runs > 0 && vm.result.type == VALUE_ARRAY &&
        vm.result.as.array->count == 1 &&
        vm.result.as.array->items[0].type == VALUE_ARRAY &&
        vm.result.as.array->items[0].as.array == vm.result.as.array &&
        vm.array_count <= 11;
    if (!passed)
        fprintf(stderr, "Circular-reference GC assertions failed (runs=%zu, arrays=%zu)\n",
                vm.gc_runs, vm.array_count);
    vm_free(&vm);
    chunk_free(&chunk);
    return passed;
}

static bool test_gc_shared_runtime_string(void) {
    Chunk chunk;
    VM vm;
    String *left = string_new("left", 4);
    String *right = string_new("right", 5);
    chunk_init(&chunk);
    vm_init(&vm);
    bool built = left && right &&
        emit(&chunk, OP_NEW_ARRAY) &&
        emit_global(&chunk, OP_DEFINE_GLOBAL, 0) &&
        emit_constant(&chunk, value_string(left)) &&
        emit_constant(&chunk, value_string(right)) &&
        emit(&chunk, OP_ADD) &&
        emit_global(&chunk, OP_DEFINE_GLOBAL, 1);
    for (size_t i = 0; built && i < 2; ++i) {
        built = emit_global(&chunk, OP_GET_GLOBAL, 0) &&
            emit_constant(&chunk, value_number((double)i)) &&
            emit_global(&chunk, OP_GET_GLOBAL, 1) &&
            emit(&chunk, OP_SET_INDEX) &&
            emit(&chunk, OP_POP);
    }
    built = built && emit_ephemeral_arrays(&chunk, 70) &&
        emit_global(&chunk, OP_GET_GLOBAL, 0) &&
        emit(&chunk, OP_RETURN);
    bool passed = built && run_chunk(&chunk, &vm, "shared runtime string") &&
        vm.gc_runs > 0 && vm.result.type == VALUE_ARRAY &&
        vm.result.as.array->count == 2 &&
        vm.result.as.array->items[0].type == VALUE_STRING &&
        vm.result.as.array->items[1].type == VALUE_STRING &&
        vm.result.as.array->items[0].as.string ==
            vm.result.as.array->items[1].as.string &&
        vm.result.as.array->items[0].as.string->length == 9 &&
        memcmp(vm.result.as.array->items[0].as.string->chars, "leftright", 9) == 0 &&
        vm.string_count == 1;
    if (!passed)
        fprintf(stderr, "Shared-string GC assertions failed (runs=%zu, strings=%zu)\n",
                vm.gc_runs, vm.string_count);
    vm_free(&vm);
    chunk_free(&chunk);
    return passed;
}

static bool test_gc_function_local_root(void) {
    Chunk chunk;
    VM vm;
    chunk_init(&chunk);
    vm_init(&vm);
    /* Main calls function 0; its body starts immediately after CALL + RETURN. */
    bool built = emit(&chunk, OP_CALL) &&
        chunk_write(&chunk, 0, 1) &&
        emit(&chunk, OP_RETURN);
    size_t function_offset = chunk.count;
    built = built && emit(&chunk, OP_NEW_ARRAY) &&
        emit_ephemeral_arrays(&chunk, 70) &&
        emit(&chunk, OP_RETURN);
    uint8_t function_index = 0;
    built = built && chunk_add_function(&chunk, function_offset, 0,
                                        "make_and_collect", 16, &function_index) &&
        function_index == 0;
    bool passed = built && run_chunk(&chunk, &vm, "function local root") &&
        vm.gc_runs > 0 && vm.result.type == VALUE_ARRAY &&
        vm.result.as.array->count == 0 && vm.array_count == 1;
    if (!passed)
        fprintf(stderr, "Function-local GC assertions failed (runs=%zu, arrays=%zu)\n",
                vm.gc_runs, vm.array_count);
    vm_free(&vm);
    chunk_free(&chunk);
    return passed;
}

int main(void) {
    Chunk chunk;
    VM vm;
    chunk_init(&chunk);
    vm_init(&vm);

    if (!emit_constant(&chunk, value_number(10)) ||
        !emit_constant(&chunk, value_number(20)) ||
        !emit(&chunk, OP_ADD) ||
        !emit(&chunk, OP_RETURN)) {
        fputs("could not construct bytecode chunk\n", stderr);
        chunk_free(&chunk);
        return 1;
    }

    VMResult status = vm_run(&vm, &chunk);
    bool arithmetic_passed = status == VM_OK && vm.result.type == VALUE_NUMBER &&
        fabs(vm.result.as.number - 30.0) < 1e-12;
    vm_free(&vm);
    chunk_free(&chunk);
    if (!arithmetic_passed) {
        fprintf(stderr, "VM arithmetic smoke test failed (status=%d)\n", (int)status);
        return 1;
    }

    if (!test_gc_nested_arrays() ||
        !test_gc_dictionary_retains_array() ||
        !test_gc_circular_array_reference() ||
        !test_gc_shared_runtime_string() ||
        !test_gc_function_local_root())
        return 1;

    puts("VM smoke test passed: arithmetic and tracing GC object-graph scenarios");
    return 0;
}
