#include "value.h"

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

void value_print(FILE *stream, Value value) {
    switch (value.type) {
        case VALUE_NIL: fputs("nil", stream); break;
        case VALUE_BOOL: fputs(value.as.boolean ? "true" : "false", stream); break;
        case VALUE_NUMBER: fprintf(stream, "%.15g", value.as.number); break;
    }
}
