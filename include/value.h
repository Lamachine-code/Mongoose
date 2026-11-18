#ifndef MONGOOSE_VALUE_H
#define MONGOOSE_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct Value Value;
typedef struct String { char *chars; size_t length; bool marked; } String;
typedef struct Array { Value *items; size_t count, capacity; bool marked; } Array;
typedef struct Dictionary Dictionary;
typedef struct Closure Closure;
typedef struct Upvalue Upvalue;
typedef enum { VALUE_NIL, VALUE_BOOL, VALUE_NUMBER, VALUE_ARRAY, VALUE_STRING, VALUE_DICTIONARY, VALUE_CLOSURE } ValueType;

struct Value {
    ValueType type;
    union {
        bool boolean;
        double number;
        Array *array;
        String *string;
        Dictionary *dictionary;
        Closure *closure;
    } as;
};

typedef struct DictionaryEntryValue { String *key; Value value; } DictionaryEntryValue;
typedef struct Dictionary { DictionaryEntryValue *entries; size_t count, capacity; bool marked; } Dictionary;

struct Upvalue {
    Value *location;
    Value closed;
    Upvalue *next_open;
    bool marked;
};

struct Closure {
    uint8_t function_index;
    Upvalue **upvalues;
    size_t upvalue_count;
    bool marked;
};

Value value_nil(void);
Value value_bool(bool value);
Value value_number(double value);
Value value_array(Array *value);
Value value_string(String *value);
Value value_dictionary(Dictionary *value);
Value value_closure(Closure *value);
Closure *closure_new(uint8_t function_index, size_t upvalue_count);
void value_closure_free(Closure *closure);
Upvalue *upvalue_new_open(Value *location);
Upvalue *upvalue_new_closed(Value value);
Value upvalue_get(const Upvalue *upvalue);
void upvalue_set(Upvalue *upvalue, Value value);
void upvalue_close(Upvalue *upvalue);
void value_upvalue_free(Upvalue *upvalue);
String *string_new(const char *chars, size_t length);
void value_string_free(String *string);
bool value_is_falsey(Value value);
void value_print(FILE *stream, Value value);
void value_array_free(Array *array);
void value_dictionary_free(Dictionary *dictionary);

#endif
