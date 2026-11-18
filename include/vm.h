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
 VM_CALL_OVERFLOW, VM_ARITY_ERROR, VM_ARRAY_REQUIRED, VM_INDEX_ERROR, VM_OUT_OF_MEMORY, VM_UTF8_ERROR, VM_DICTIONARY_REQUIRED, VM_STRING_REQUIRED, VM_INDEXABLE_REQUIRED
} VMResult;
typedef struct { size_t return_ip; size_t base; uint8_t function_index; int call_line; } CallFrame;
typedef struct {
 Value stack[VM_STACK_MAX]; size_t stack_count;
 Value globals[VM_GLOBAL_MAX]; bool global_defined[VM_GLOBAL_MAX];
 Value result;
 int error_line;
 Array **arrays; size_t array_count, array_capacity;
 String **strings; size_t string_count, string_capacity;
 Dictionary **dictionaries; size_t dictionary_count, dictionary_capacity;
 Closure **closures; size_t closure_count, closure_capacity;
 Upvalue **upvalues; size_t upvalue_count, upvalue_capacity;
 Upvalue *open_upvalues;
 size_t gc_threshold, gc_runs;
 CallFrame frames[VM_FRAMES_MAX]; size_t frame_count;
} VM;
void vm_init(VM *vm);
void vm_free(VM *vm);
bool vm_register_closure(VM *vm, Closure *closure);
bool vm_register_upvalue(VM *vm, Upvalue *upvalue);
Upvalue *vm_capture_upvalue(VM *vm, size_t stack_index);
void vm_close_upvalues(VM *vm, size_t first_stack_index);
VMResult vm_run(VM *vm, const Chunk *chunk);
#endif
