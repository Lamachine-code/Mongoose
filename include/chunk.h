#ifndef MONGOOSE_CHUNK_H
#define MONGOOSE_CHUNK_H
#include "value.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {
 OP_CONSTANT, OP_NIL, OP_TRUE, OP_FALSE, OP_POP, OP_ADD, OP_SUBTRACT,
 OP_MULTIPLY, OP_DIVIDE, OP_FLOOR_DIVIDE, OP_MODULO, OP_POWER, OP_NEGATE,
 OP_NOT, OP_EQUAL, OP_GREATER, OP_LESS, OP_GREATER_EQUAL, OP_LESS_EQUAL,
 OP_RETURN, OP_DEFINE_GLOBAL, OP_GET_GLOBAL, OP_SET_GLOBAL, OP_PRINT,
 OP_JUMP, OP_JUMP_IF_FALSE, OP_JUMP_IF_TRUE, OP_JUMP_IF_FALSE_KEEP, OP_JUMP_IF_TRUE_KEEP, OP_GET_LOCAL, OP_SET_LOCAL, OP_CALL
} OpCode;
typedef struct { size_t offset; uint8_t arity; } FunctionInfo;
typedef struct {
 uint8_t *code; int *lines; size_t count, capacity;
 Value *constants; size_t constant_count, constant_capacity;
 FunctionInfo *functions; size_t function_count, function_capacity;
} Chunk;
void chunk_init(Chunk *chunk);
void chunk_free(Chunk *chunk);
bool chunk_write(Chunk *chunk, uint8_t byte, int line);
bool chunk_add_constant(Chunk *chunk, Value value, uint16_t *index);
bool chunk_write_constant(Chunk *chunk, Value value, int line);
bool chunk_add_function(Chunk *chunk, size_t offset, uint8_t arity, uint8_t *index);
#endif
