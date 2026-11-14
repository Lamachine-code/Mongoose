#ifndef MONGOOSE_VALUE_H
#define MONGOOSE_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct Value Value;
typedef struct Array {
    Value *items;
    size_t count;
    size_t capacity;
} Array;

typedef enum {
    VALUE_NIL,
    VALUE_BOOL,
    VALUE_NUMBER,
    VALUE_ARRAY
} ValueType;

struct Value {
    ValueType type;
    union {
        bool boolean;
        double number;
        Array *array;
    } as;
};

Value value_nil(void);
Value value_bool(bool value);
Value value_number(double value);
Value value_array(Array *value);
bool value_is_falsey(Value value);
void value_print(FILE *stream, Value value);
void value_array_free(Array *array);

#endif
