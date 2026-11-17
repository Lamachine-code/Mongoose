#include "chunk.h"
#include <stdlib.h>
#include <stdint.h>

void chunk_init(Chunk *chunk) {
    *chunk = (Chunk){0};
}

void chunk_free(Chunk *chunk) {
    free(chunk->code);
    free(chunk->lines);
    for (size_t i = 0; i < chunk->constant_count; ++i)
        if (chunk->constants[i].type == VALUE_STRING)
            value_string_free(chunk->constants[i].as.string);
    free(chunk->constants);
    free(chunk->functions);
    chunk_init(chunk);
}

static bool chunk_capacity(size_t current, size_t minimum,
                            size_t element_size, size_t *result) {
    if (!result || element_size == 0) return false;
    size_t maximum = SIZE_MAX / element_size;
    if (minimum > maximum) return false;
    size_t capacity = current < 8 ? 8 : current;
    if (capacity > maximum) capacity = maximum;
    while (capacity < minimum) {
        if (capacity > maximum / 2) {
            capacity = minimum;
            break;
        }
        capacity *= 2;
    }
    if (capacity < minimum || capacity > maximum) return false;
    *result = capacity;
    return true;
}

static bool grow_code(Chunk *chunk) {
    size_t capacity;
    if (!chunk_capacity(chunk->capacity, chunk->count + 1,
                        sizeof *chunk->code, &capacity) ||
        capacity > SIZE_MAX / sizeof *chunk->lines) return false;
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
        size_t capacity;
        if (!chunk_capacity(chunk->constant_capacity, chunk->constant_count + 1,
                            sizeof *chunk->constants, &capacity)) return false;
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
                        const char *name, size_t name_length, uint8_t *index) {
    if (chunk->function_count >= UINT8_MAX) return false;
    if (chunk->function_count == chunk->function_capacity) {
        size_t capacity;
        if (!chunk_capacity(chunk->function_capacity, chunk->function_count + 1,
                            sizeof *chunk->functions, &capacity)) return false;
        FunctionInfo *functions =
            realloc(chunk->functions, capacity * sizeof *functions);
        if (!functions) return false;
        chunk->functions = functions;
        chunk->function_capacity = capacity;
    }
    *index = (uint8_t)chunk->function_count;
    chunk->functions[chunk->function_count++] =
        (FunctionInfo){offset, arity, name, name_length};
    return true;
}
