#ifndef MONGOOSE_CHUNK_H
#define MONGOOSE_CHUNK_H
#include "value.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {
 OP_CONSTANT, OP_NIL, OP_TRUE, OP_FALSE, OP_POP, OP_ADD, OP_SUBTRACT,
 OP_MULTIPLY, OP_DIVIDE, OP_FLOOR_DIVIDE, OP_MODULO, OP_POWER, OP_NEGATE,
 OP_NOT, OP_EQUAL, OP_GREATER, OP_LESS, OP_RETURN, OP_DEFINE_GLOBAL,
 OP_GET_GLOBAL, OP_SET_GLOBAL, OP_PRINT
} OpCode;
typedef struct {
 uint8_t *code; int *lines; size_t count, capacity;
 Value *constants; size_t constant_count, constant_capacity;
} Chunk;
void chunk_init(Chunk *chunk);
void chunk_free(Chunk *chunk);
bool chunk_write(Chunk *chunk, uint8_t byte, int line);
bool chunk_add_constant(Chunk *chunk, Value value, uint16_t *index);
bool chunk_write_constant(Chunk *chunk, Value value, int line);
#endif
