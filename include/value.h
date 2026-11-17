#ifndef MONGOOSE_VALUE_H
#define MONGOOSE_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct Value Value;
typedef struct String { char *chars; size_t length; } String;
typedef struct Array { Value *items; size_t count, capacity; } Array;
typedef struct DictionaryEntry { String *key; Value *unused_forward_declaration; } DictionaryEntry;
typedef enum { VALUE_NIL, VALUE_BOOL, VALUE_NUMBER, VALUE_ARRAY, VALUE_STRING, VALUE_DICTIONARY } ValueType;

struct Value {
    ValueType type;
    union {
        bool boolean;
        double number;
        Array *array;
        String *string;
        struct Dictionary *dictionary;
    } as;
};

typedef struct DictionaryEntryValue { String *key; Value value; } DictionaryEntryValue;
typedef struct Dictionary { DictionaryEntryValue *entries; size_t count, capacity; } Dictionary;

Value value_nil(void);
Value value_bool(bool value);
Value value_number(double value);
Value value_array(Array *value);
Value value_string(String *value);
Value value_dictionary(Dictionary *value);
String *string_new(const char *chars, size_t length);
void value_string_free(String *string);
bool value_is_falsey(Value value);
void value_print(FILE *stream, Value value);
void value_array_free(Array *array);
void value_dictionary_free(Dictionary *dictionary);

#endif
