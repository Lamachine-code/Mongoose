#include "value.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

Value value_nil(void) {
    Value value = {.type = VALUE_NIL};
    return value;
}

Value value_bool(bool boolean) {
    Value value = {.type = VALUE_BOOL, .as.boolean = boolean};
    return value;
}

Value value_number(double number) {
    Value value = {.type = VALUE_NUMBER, .as.number = number};
    return value;
}

bool value_is_falsey(Value value) {
    return value.type == VALUE_NIL ||
           (value.type == VALUE_BOOL && !value.as.boolean);
}

Value value_array(Array *array) {
    Value value = {.type = VALUE_ARRAY, .as.array = array};
    return value;
}

Value value_string(String *string) {
    Value value = {.type = VALUE_STRING, .as.string = string};
    return value;
}

Value value_dictionary(Dictionary *dictionary) {
    Value value = {.type = VALUE_DICTIONARY, .as.dictionary = dictionary};
    return value;
}

Value value_closure(Closure *closure) {
    Value value = {.type = VALUE_CLOSURE, .as.closure = closure};
    return value;
}

Closure *closure_new(uint8_t function_index, size_t upvalue_count) {
    if (upvalue_count > SIZE_MAX / sizeof(Upvalue *)) return NULL;
    Closure *closure = calloc(1, sizeof *closure);
    if (!closure) return NULL;
    if (upvalue_count) {
        closure->upvalues = calloc(upvalue_count, sizeof *closure->upvalues);
        if (!closure->upvalues) { free(closure); return NULL; }
    }
    closure->function_index = function_index;
    closure->upvalue_count = upvalue_count;
    return closure;
}

void value_closure_free(Closure *closure) {
    if (!closure) return;
    free(closure->upvalues);
    free(closure);
}

Upvalue *upvalue_new_open(Value *location) {
    if (!location) return NULL;
    Upvalue *upvalue = calloc(1, sizeof *upvalue);
    if (!upvalue) return NULL;
    upvalue->location = location;
    upvalue->closed = value_nil();
    return upvalue;
}

Upvalue *upvalue_new_closed(Value value) {
    Upvalue *upvalue = calloc(1, sizeof *upvalue);
    if (!upvalue) return NULL;
    upvalue->closed = value;
    upvalue->location = &upvalue->closed;
    return upvalue;
}

Value upvalue_get(const Upvalue *upvalue) {
    return upvalue && upvalue->location ? *upvalue->location : value_nil();
}

void upvalue_set(Upvalue *upvalue, Value value) {
    if (upvalue && upvalue->location) *upvalue->location = value;
}

void upvalue_close(Upvalue *upvalue) {
    if (!upvalue || !upvalue->location || upvalue->location == &upvalue->closed)
        return;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
}

void value_upvalue_free(Upvalue *upvalue) {
    free(upvalue);
}

String *string_new(const char *chars, size_t length) {
    if (length == SIZE_MAX || (length > 0 && !chars)) return NULL;
    String *string = malloc(sizeof *string);
    if (!string) return NULL;
    string->chars = malloc(length + 1);
    if (!string->chars) { free(string); return NULL; }
    if (length) memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->length = length;
    string->marked = false;
    return string;
}

void value_string_free(String *string) {
    if (!string) return;
    free(string->chars);
    free(string);
}

void value_array_free(Array *array) {
    if (!array) return;
    free(array->items);
    free(array);
}

void value_dictionary_free(Dictionary *dictionary) {
    if (!dictionary) return;
    free(dictionary->entries);
    free(dictionary);
}

void value_print(FILE *stream, Value value) {
    switch (value.type) {
        case VALUE_NIL: fputs("nil", stream); break;
        case VALUE_BOOL: fputs(value.as.boolean ? "true" : "false", stream); break;
        case VALUE_NUMBER: fprintf(stream, "%.15g", value.as.number); break;
        case VALUE_ARRAY: fputs("<array>", stream); break;
        case VALUE_DICTIONARY: fputs("<dictionary>", stream); break;
        case VALUE_CLOSURE: fputs("<closure>", stream); break;
        case VALUE_STRING:
            if (value.as.string && value.as.string->length)
                fwrite(value.as.string->chars, 1, value.as.string->length, stream);
            break;
    }
}
