#include "value.h"
#include <stdlib.h>
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

String *string_new(const char *chars, size_t length) {
    if (length == (size_t)-1) return NULL;
    String *string = malloc(sizeof *string);
    if (!string) return NULL;
    string->chars = malloc(length + 1);
    if (!string->chars) { free(string); return NULL; }
    if (length) memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->length = length;
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

void value_print(FILE *stream, Value value) {
    switch (value.type) {
        case VALUE_NIL: fputs("nil", stream); break;
        case VALUE_BOOL: fputs(value.as.boolean ? "true" : "false", stream); break;
        case VALUE_NUMBER: fprintf(stream, "%.15g", value.as.number); break;
        case VALUE_ARRAY: fputs("<array>", stream); break;
        case VALUE_STRING:
            if (value.as.string && value.as.string->length)
                fwrite(value.as.string->chars, 1, value.as.string->length, stream);
            break;
    }
}
