#include "chunk.h"
#include <stdlib.h>

void chunk_init(Chunk *chunk) {
    *chunk = (Chunk){0};
}

void chunk_free(Chunk *chunk) {
    free(chunk->code);
    free(chunk->lines);
    free(chunk->constants);
    free(chunk->functions);
    chunk_init(chunk);
}

static bool grow_code(Chunk *chunk) {
    size_t capacity = chunk->capacity < 8 ? 8 : chunk->capacity * 2;
    uint8_t *code = malloc(capacity * sizeof *code);
    int *lines = malloc(capacity * sizeof *lines);
    if (!code || !lines) {
        free(code);
        free(lines);
        return false;
    }
    for (size_t i = 0; i < chunk->count; ++i) {
        code[i] = chunk->code[i];
        lines[i] = chunk->lines[i];
    }
    free(chunk->code);
    free(chunk->lines);
    chunk->code = code;
    chunk->lines = lines;
    chunk->capacity = capacity;
    return true;
}

bool chunk_write(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->count == chunk->capacity && !grow_code(chunk)) return false;
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    ++chunk->count;
    return true;
}

bool chunk_add_constant(Chunk *chunk, Value value, uint16_t *index) {
    if (chunk->constant_count > UINT16_MAX) return false;
    if (chunk->constant_count == chunk->constant_capacity) {
        size_t capacity = chunk->constant_capacity < 8 ? 8 : chunk->constant_capacity * 2;
        Value *constants = realloc(chunk->constants, capacity * sizeof *constants);
        if (!constants) return false;
        chunk->constants = constants;
        chunk->constant_capacity = capacity;
    }
    *index = (uint16_t)chunk->constant_count;
    chunk->constants[chunk->constant_count++] = value;
    return true;
}

bool chunk_write_constant(Chunk *chunk, Value value, int line) {
    uint16_t index;
    if (!chunk_add_constant(chunk, value, &index)) return false;
    return chunk_write(chunk, (uint8_t)OP_CONSTANT, line) &&
           chunk_write(chunk, (uint8_t)(index & 0xffu), line) &&
           chunk_write(chunk, (uint8_t)(index >> 8), line);
}

bool chunk_add_function(Chunk *chunk, size_t offset, uint8_t arity,
                        uint8_t *index) {
    if (chunk->function_count >= UINT8_MAX) return false;
    if (chunk->function_count == chunk->function_capacity) {
        size_t capacity = chunk->function_capacity < 8
            ? 8 : chunk->function_capacity * 2;
        FunctionInfo *functions =
            realloc(chunk->functions, capacity * sizeof *functions);
        if (!functions) return false;
        chunk->functions = functions;
        chunk->function_capacity = capacity;
    }
    *index = (uint8_t)chunk->function_count;
    chunk->functions[chunk->function_count++] =
        (FunctionInfo){offset, arity};
    return true;
}
