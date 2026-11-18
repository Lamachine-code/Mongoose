#include "chunk.h"
#include "vm.h"
#include <math.h>
#include <stdlib.h>
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

static bool test_upvalue_open_close(void) {
    Value slot = value_number(10);
    Upvalue *upvalue = upvalue_new_open(&slot);
    if (!upvalue) return false;
    upvalue_set(upvalue, value_number(20));
    bool open_ok = upvalue_get(upvalue).type == VALUE_NUMBER &&
        upvalue_get(upvalue).as.number == 20;
    upvalue_close(upvalue);
    slot = value_number(30);
    bool closed_ok = upvalue_get(upvalue).type == VALUE_NUMBER &&
        upvalue_get(upvalue).as.number == 20;
    upvalue_set(upvalue, value_number(40));
    closed_ok = closed_ok && upvalue_get(upvalue).as.number == 40;
    value_upvalue_free(upvalue);
    return open_ok && closed_ok;
}

static bool test_vm_shares_and_closes_open_upvalues(void) {
    VM vm;
    vm_init(&vm);
    vm.stack[0] = value_number(10);
    vm.stack[1] = value_number(15);
    vm.stack[2] = value_number(20);
    vm.stack_count = 3;

    Upvalue *first = vm_capture_upvalue(&vm, 0);
    Upvalue *same = vm_capture_upvalue(&vm, 0);
    Upvalue *last = vm_capture_upvalue(&vm, 2);
    bool shared = first && first == same && last && first != last &&
        vm.open_upvalues == last && last->next_open == first;
    if (shared) {
        upvalue_set(same, value_number(11));
        shared = vm.stack[0].as.number == 11 &&
            upvalue_get(first).as.number == 11;
    }

    vm_close_upvalues(&vm, 2);
    bool partial_close = vm.open_upvalues == first &&
        last->location == &last->closed &&
        first->location == &vm.stack[0];
    vm.stack[2] = value_number(99);
    partial_close = partial_close && upvalue_get(last).as.number == 20;

    vm_close_upvalues(&vm, 0);
    bool full_close = vm.open_upvalues == NULL &&
        first->location == &first->closed;
    vm.stack[0] = value_number(88);
    full_close = full_close && upvalue_get(first).as.number == 11;

    bool passed = shared && partial_close && full_close &&
        vm.upvalue_count == 2;
    if (!passed)
        fprintf(stderr, "Open-upvalue lifecycle assertions failed\\n");
    vm_free(&vm);
    return passed;
}

static bool test_gc_open_upvalue_roots_stack_value(void) {
    Chunk chunk;
    VM vm;
    chunk_init(&chunk);
    vm_init(&vm);

    Array *captured = calloc(1, sizeof *captured);
    vm.arrays = calloc(1, sizeof *vm.arrays);
    bool built = captured && vm.arrays;
    if (built) {
        vm.array_capacity = 1;
        vm.arrays[vm.array_count++] = captured;
        /* Use a high slot so bytecode execution below cannot reuse and
           overwrite the captured stack location after it leaves the root range. */
        const size_t captured_slot = VM_STACK_MAX - 1;
        vm.stack[captured_slot] = value_array(captured);
        vm.stack_count = captured_slot + 1;
        Upvalue *upvalue = vm_capture_upvalue(&vm, captured_slot);
        built = upvalue != NULL;
        /* Simulate the stack slot leaving the ordinary root range. The open
           capture list must keep both the cell and its current value alive. */
        vm.stack_count = 0;
        vm.gc_threshold = 1;
        built = built && emit_ephemeral_arrays(&chunk, 70) &&
            emit(&chunk, OP_NIL) && emit(&chunk, OP_RETURN);
    }
    bool passed = built && run_chunk(&chunk, &vm, "open upvalue roots stack value") &&
        vm.gc_runs > 0 && vm.upvalue_count == 1 && vm.open_upvalues != NULL &&
        vm.array_count <= 11 &&
        upvalue_get(vm.open_upvalues).type == VALUE_ARRAY &&
        upvalue_get(vm.open_upvalues).as.array == captured;
    if (!passed)
        fprintf(stderr, "Open-upvalue GC root assertions failed (runs=%zu, upvalues=%zu, arrays=%zu)\\n",
                vm.gc_runs, vm.upvalue_count, vm.array_count);
    vm_free(&vm);
    chunk_free(&chunk);
    return passed;
}

static bool test_gc_closure_retains_captured_array(void) {
    Chunk chunk;
    VM vm;
    chunk_init(&chunk);
    vm_init(&vm);

    Array *captured = calloc(1, sizeof *captured);
    Upvalue *upvalue = captured ? upvalue_new_closed(value_array(captured)) : NULL;
    Closure *closure = upvalue ? closure_new(0, 1) : NULL;
    bool built = captured && upvalue && closure;
    bool array_registered = false;
    bool upvalue_registered = false;
    bool closure_registered = false;
    if (built) {
        closure->upvalues[0] = upvalue;
        vm.arrays = calloc(1, sizeof *vm.arrays);
        built = vm.arrays != NULL;
        if (built) {
            vm.array_capacity = 1;
            vm.arrays[vm.array_count++] = captured;
            array_registered = true;
            upvalue_registered = vm_register_upvalue(&vm, upvalue);
            closure_registered = upvalue_registered &&
                vm_register_closure(&vm, closure);
            built = closure_registered;
        }
    }
    if (!upvalue_registered) value_upvalue_free(upvalue);
    if (!closure_registered) value_closure_free(closure);
    if (!array_registered) value_array_free(captured);
    if (built) {
        vm.stack[vm.stack_count++] = value_closure(closure);
        vm.gc_threshold = 1;
        built = emit_ephemeral_arrays(&chunk, 70) &&
            emit(&chunk, OP_NIL) && emit(&chunk, OP_RETURN);
    }
    bool passed = built && run_chunk(&chunk, &vm, "closure retains captured array") &&
        vm.gc_runs > 0 && vm.closure_count == 1 && vm.upvalue_count == 1 &&
        vm.array_count <= 11 &&
        upvalue_get(vm.closures[0]->upvalues[0]).type == VALUE_ARRAY &&
        upvalue_get(vm.closures[0]->upvalues[0]).as.array == captured;
    if (!passed)
        fprintf(stderr, "Closure-capture GC assertions failed (runs=%zu, closures=%zu, upvalues=%zu, arrays=%zu)\\n",
                vm.gc_runs, vm.closure_count, vm.upvalue_count, vm.array_count);
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
        emit_ephemeral_arrays(&chunk, 130) &&
        emit(&chunk, OP_RETURN);
    uint8_t function_index = 0;
    built = built && chunk_add_function(&chunk, function_offset, 0,
                                        "make_and_collect", 16, &function_index) &&
        function_index == 0;
    bool passed = built && run_chunk(&chunk, &vm, "function local root") &&
        vm.gc_runs > 0 && vm.result.type == VALUE_ARRAY &&
        vm.result.as.array->count == 0 && vm.array_count <= 10;
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
        !test_upvalue_open_close() ||
        !test_vm_shares_and_closes_open_upvalues() ||
        !test_gc_open_upvalue_roots_stack_value() ||
        !test_gc_closure_retains_captured_array() ||
        !test_gc_function_local_root())
        return 1;

    puts("VM smoke test passed: arithmetic and tracing GC object-graph scenarios");
    return 0;
}
