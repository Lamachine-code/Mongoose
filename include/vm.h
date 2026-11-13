#ifndef MONGOOSE_VM_H
#define MONGOOSE_VM_H
#include "chunk.h"
#include <stdbool.h>
#include <stddef.h>
#define VM_STACK_MAX 256
#define VM_GLOBAL_MAX 256
typedef enum {
 VM_OK, VM_STACK_OVERFLOW, VM_STACK_UNDERFLOW, VM_INVALID_BYTECODE,
 VM_TYPE_ERROR, VM_DIVISION_BY_ZERO, VM_UNDEFINED_GLOBAL
} VMResult;
typedef struct {
 Value stack[VM_STACK_MAX]; size_t stack_count;
 Value globals[VM_GLOBAL_MAX]; bool global_defined[VM_GLOBAL_MAX];
 Value result;
} VM;
void vm_init(VM *vm);
VMResult vm_run(VM *vm, const Chunk *chunk);
#endif
