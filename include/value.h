#ifndef MONGOOSE_VALUE_H
#define MONGOOSE_VALUE_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    VALUE_NIL,
    VALUE_BOOL,
    VALUE_NUMBER
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
    } as;
} Value;

Value value_nil(void);
Value value_bool(bool value);
Value value_number(double value);
bool value_is_falsey(Value value);
void value_print(FILE *stream, Value value);

#endif
