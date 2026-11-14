#ifndef MONGOOSE_VM_H
#define MONGOOSE_VM_H
#include "chunk.h"
#include <stdbool.h>
#include <stddef.h>
#define VM_STACK_MAX 256
#define VM_GLOBAL_MAX 256
#define VM_FRAMES_MAX 64
typedef enum {
 VM_OK, VM_STACK_OVERFLOW, VM_STACK_UNDERFLOW, VM_INVALID_BYTECODE,
 VM_TYPE_ERROR, VM_DIVISION_BY_ZERO, VM_UNDEFINED_GLOBAL,
 VM_CALL_OVERFLOW, VM_ARITY_ERROR, VM_ARRAY_REQUIRED, VM_INDEX_ERROR, VM_OUT_OF_MEMORY
} VMResult;
typedef struct {
 size_t return_ip;
 size_t base;
} CallFrame;
typedef struct {
 Value stack[VM_STACK_MAX]; size_t stack_count;
 Value globals[VM_GLOBAL_MAX]; bool global_defined[VM_GLOBAL_MAX];
 Value result;
 Array **arrays; size_t array_count, array_capacity;
 CallFrame frames[VM_FRAMES_MAX]; size_t frame_count;
} VM;
void vm_init(VM *vm);
void vm_free(VM *vm);
VMResult vm_run(VM *vm, const Chunk *chunk);
#endif
