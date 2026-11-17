#include "vm.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void vm_init(VM *vm) {
    if (vm) *vm = (VM){0};
}

void vm_free(VM *vm) {
    if (!vm) return;
    for (size_t i = 0; i < vm->array_count; ++i)
        value_array_free(vm->arrays[i]);
    free(vm->arrays);
    for (size_t i = 0; i < vm->string_count; ++i)
        value_string_free(vm->strings[i]);
    free(vm->strings);
    for (size_t i = 0; i < vm->dictionary_count; ++i)
        value_dictionary_free(vm->dictionaries[i]);
    free(vm->dictionaries);
    *vm = (VM){0};
}


/* Decode one UTF-8 scalar value. Reject overlong forms, surrogates and
   values above U+10FFFF. */
static bool utf8_next(const char *bytes, size_t length, size_t offset,
                      size_t *width) {
    if (offset >= length) return false;
    unsigned char c = (unsigned char)bytes[offset];
    if (c <= 0x7f) { *width = 1; return true; }
    if (c >= 0xc2 && c <= 0xdf) {
        if (length - offset < 2) return false;
        unsigned char c1 = (unsigned char)bytes[offset + 1];
        if ((c1 & 0xc0) != 0x80) return false;
        *width = 2; return true;
    }
    if (c >= 0xe0 && c <= 0xef) {
        if (length - offset < 3) return false;
        unsigned char c1 = (unsigned char)bytes[offset + 1];
        unsigned char c2 = (unsigned char)bytes[offset + 2];
        if ((c1 & 0xc0) != 0x80 || (c2 & 0xc0) != 0x80 ||
            (c == 0xe0 && c1 < 0xa0) || (c == 0xed && c1 >= 0xa0))
            return false;
        *width = 3; return true;
    }
    if (c >= 0xf0 && c <= 0xf4) {
        if (length - offset < 4) return false;
        unsigned char c1 = (unsigned char)bytes[offset + 1];
        unsigned char c2 = (unsigned char)bytes[offset + 2];
        unsigned char c3 = (unsigned char)bytes[offset + 3];
        if ((c1 & 0xc0) != 0x80 || (c2 & 0xc0) != 0x80 ||
            (c3 & 0xc0) != 0x80 || (c == 0xf0 && c1 < 0x90) ||
            (c == 0xf4 && c1 >= 0x90))
            return false;
        *width = 4; return true;
    }
    return false;
}

static bool utf8_count(const String *string, size_t *count) {
    size_t offset = 0, total = 0;
    while (offset < string->length) {
        size_t width;
        if (!utf8_next(string->chars, string->length, offset, &width))
            return false;
        offset += width;
        ++total;
    }
    *count = total;
    return true;
}

static bool utf8_offset(const String *string, size_t character_index,
                        size_t *byte_offset) {
    size_t offset = 0, index = 0;
    while (index < character_index && offset < string->length) {
        size_t width;
        if (!utf8_next(string->chars, string->length, offset, &width))
            return false;
        offset += width;
        ++index;
    }
    if (index != character_index) return false;
    *byte_offset = offset;
    return true;
}

static bool register_array(VM *vm, Array *array) {
    if (vm->array_count == vm->array_capacity) {
        size_t capacity = vm->array_capacity < 8 ? 8 : vm->array_capacity * 2;
        Array **arrays = realloc(vm->arrays, capacity * sizeof *arrays);
        if (!arrays) return false;
        vm->arrays = arrays;
        vm->array_capacity = capacity;
    }
    vm->arrays[vm->array_count++] = array;
    return true;
}

static bool register_string(VM *vm, String *string) {
    if (vm->string_count == vm->string_capacity) {
        size_t capacity = vm->string_capacity < 8 ? 8 : vm->string_capacity * 2;
        String **strings = realloc(vm->strings, capacity * sizeof *strings);
        if (!strings) return false;
        vm->strings = strings;
        vm->string_capacity = capacity;
    }
    vm->strings[vm->string_count++] = string;
    return true;
}

static bool array_index(Value value, size_t *index) {
    if (value.type != VALUE_NUMBER || !isfinite(value.as.number) ||
        value.as.number < 0.0 || value.as.number > 65535.0 ||
        floor(value.as.number) != value.as.number)
        return false;
    *index = (size_t)value.as.number;
    return true;
}

static bool array_set(Array *array, size_t index, Value value) {
    if (index >= 65536) return false;
    if (index >= array->capacity) {
        size_t capacity = array->capacity < 8 ? 8 : array->capacity;
        while (capacity <= index) capacity *= 2;
        if (capacity > 65536) capacity = 65536;
        Value *items = realloc(array->items, capacity * sizeof *items);
        if (!items) return false;
        array->items = items;
        array->capacity = capacity;
    }
    while (array->count <= index)
        array->items[array->count++] = value_nil();
    array->items[index] = value;
    return true;
}

static bool register_dictionary(VM *vm, Dictionary *dictionary) {
    if (vm->dictionary_count == vm->dictionary_capacity) {
        size_t capacity = vm->dictionary_capacity < 8 ? 8 : vm->dictionary_capacity * 2;
        Dictionary **items = realloc(vm->dictionaries, capacity * sizeof *items);
        if (!items) return false;
        vm->dictionaries = items;
        vm->dictionary_capacity = capacity;
    }
    vm->dictionaries[vm->dictionary_count++] = dictionary;
    return true;
}

static bool dictionary_set(Dictionary *dictionary, String *key, Value value) {
    for (size_t i = 0; i < dictionary->count; ++i) {
        String *existing = dictionary->entries[i].key;
        if (existing->length == key->length &&
            memcmp(existing->chars, key->chars, key->length) == 0) {
            dictionary->entries[i].value = value;
            return true;
        }
    }
    if (dictionary->count == dictionary->capacity) {
        size_t capacity = dictionary->capacity < 8 ? 8 : dictionary->capacity * 2;
        DictionaryEntryValue *entries = realloc(dictionary->entries, capacity * sizeof *entries);
        if (!entries) return false;
        dictionary->entries = entries;
        dictionary->capacity = capacity;
    }
    dictionary->entries[dictionary->count++] = (DictionaryEntryValue){key, value};
    return true;
}

static Value dictionary_get(Dictionary *dictionary, String *key) {
    for (size_t i = 0; i < dictionary->count; ++i) {
        String *existing = dictionary->entries[i].key;
        if (existing->length == key->length &&
            memcmp(existing->chars, key->chars, key->length) == 0)
            return dictionary->entries[i].value;
    }
    return value_nil();
}

static bool push(VM *vm, Value value) {
    if (vm->stack_count == VM_STACK_MAX) return false;
    vm->stack[vm->stack_count++] = value;
    return true;
}

static bool pop(VM *vm, Value *value) {
    if (vm->stack_count == 0) return false;
    *value = vm->stack[--vm->stack_count];
    return true;
}

static bool pop_numbers(VM *vm, double *left, double *right) {
    Value a, b;
    if (vm->stack_count < 2) return false;
    if (!pop(vm, &b) || !pop(vm, &a)) return false;
    if (a.type != VALUE_NUMBER || b.type != VALUE_NUMBER) return false;
    *left = a.as.number;
    *right = b.as.number;
    return true;
}

VMResult vm_run(VM *vm, const Chunk *chunk) {
    if (!vm || !chunk || (chunk->count && !chunk->code))
        return VM_INVALID_BYTECODE;

    size_t ip = 0;
    while (ip < chunk->count) {
        uint8_t instruction = chunk->code[ip++];
        Value a, b;
        double left, right;

        switch ((OpCode)instruction) {
            case OP_CONSTANT: {
                if (ip + 1 >= chunk->count) return VM_INVALID_BYTECODE;
                uint16_t index = (uint16_t)chunk->code[ip] |
                    (uint16_t)((uint16_t)chunk->code[ip + 1] << 8);
                ip += 2;
                if (index >= chunk->constant_count || !chunk->constants)
                    return VM_INVALID_BYTECODE;
                if (!push(vm, chunk->constants[index]))
                    return VM_STACK_OVERFLOW;
                break;
            }
            case OP_NIL:
                if (!push(vm, value_nil())) return VM_STACK_OVERFLOW;
                break;
            case OP_TRUE:
                if (!push(vm, value_bool(true))) return VM_STACK_OVERFLOW;
                break;
            case OP_FALSE:
                if (!push(vm, value_bool(false))) return VM_STACK_OVERFLOW;
                break;
            case OP_POP:
                if (!pop(vm, &a)) return VM_STACK_UNDERFLOW;
                break;
            case OP_NEGATE:
            case OP_NOT:
                if (!pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (instruction == OP_NEGATE) {
                    if (a.type != VALUE_NUMBER) return VM_TYPE_ERROR;
                    a = value_number(-a.as.number);
                } else {
                    a = value_bool(value_is_falsey(a));
                }
                if (!push(vm, a)) return VM_STACK_OVERFLOW;
                break;
            case OP_ADD:
                if (vm->stack_count < 2) return VM_STACK_UNDERFLOW;
                if (!pop(vm, &b) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (a.type == VALUE_STRING && b.type == VALUE_STRING && a.as.string && b.as.string) {
                    size_t length = a.as.string->length + b.as.string->length;
                    if (length < a.as.string->length) return VM_OUT_OF_MEMORY;
                    char *buffer = malloc(length ? length : 1);
                    if (!buffer) return VM_OUT_OF_MEMORY;
                    memcpy(buffer, a.as.string->chars, a.as.string->length);
                    memcpy(buffer + a.as.string->length, b.as.string->chars, b.as.string->length);
                    String *joined = string_new(buffer, length);
                    free(buffer);
                    if (!joined) return VM_OUT_OF_MEMORY;
                    if (!register_string(vm, joined)) { value_string_free(joined); return VM_OUT_OF_MEMORY; }
                    if (!push(vm, value_string(joined))) return VM_STACK_OVERFLOW;
                } else {
                    if (a.type != VALUE_NUMBER || b.type != VALUE_NUMBER) return VM_TYPE_ERROR;
                    if (!push(vm, value_number(a.as.number + b.as.number))) return VM_STACK_OVERFLOW;
                }
                break;
            case OP_SUBTRACT:
            case OP_MULTIPLY:
            case OP_DIVIDE:
            case OP_FLOOR_DIVIDE:
            case OP_MODULO:
            case OP_POWER:
                if (vm->stack_count < 2) return VM_STACK_UNDERFLOW;
                if (!pop_numbers(vm, &left, &right)) return VM_TYPE_ERROR;
                if ((instruction == OP_DIVIDE ||
                     instruction == OP_FLOOR_DIVIDE ||
                     instruction == OP_MODULO) && right == 0.0)
                    return VM_DIVISION_BY_ZERO;
                switch ((OpCode)instruction) {
                    case OP_SUBTRACT: left -= right; break;
                    case OP_MULTIPLY: left *= right; break;
                    case OP_DIVIDE: left /= right; break;
                    case OP_FLOOR_DIVIDE: left = floor(left / right); break;
                    case OP_MODULO: left = fmod(left, right); break;
                    case OP_POWER: left = pow(left, right); break;
                    default: break;
                }
                if (!push(vm, value_number(left))) return VM_STACK_OVERFLOW;
                break;
            case OP_EQUAL:
            case OP_GREATER:
            case OP_LESS:
            case OP_GREATER_EQUAL:
            case OP_LESS_EQUAL:
                if (vm->stack_count < 2) return VM_STACK_UNDERFLOW;
                if (!pop(vm, &b) || !pop(vm, &a))
                    return VM_STACK_UNDERFLOW;
                if (instruction == OP_EQUAL) {
                    bool equal = a.type == b.type &&
                        (a.type == VALUE_NIL ||
                         (a.type == VALUE_BOOL &&
                          a.as.boolean == b.as.boolean) ||
                         (a.type == VALUE_NUMBER &&
                          a.as.number == b.as.number) ||
                         (a.type == VALUE_ARRAY &&
                          a.as.array == b.as.array) ||
                         (a.type == VALUE_DICTIONARY &&
                          a.as.dictionary == b.as.dictionary) ||
                         (a.type == VALUE_STRING && a.as.string && b.as.string &&
                          a.as.string->length == b.as.string->length &&
                          memcmp(a.as.string->chars, b.as.string->chars,
                                 a.as.string->length) == 0));
                    if (!push(vm, value_bool(equal)))
                        return VM_STACK_OVERFLOW;
                } else {
                    if (a.type != VALUE_NUMBER || b.type != VALUE_NUMBER)
                        return VM_TYPE_ERROR;
                    bool comparison;
                    switch ((OpCode)instruction) {
                        case OP_GREATER: comparison = a.as.number > b.as.number; break;
                        case OP_LESS: comparison = a.as.number < b.as.number; break;
                        case OP_GREATER_EQUAL: comparison = a.as.number >= b.as.number; break;
                        default: comparison = a.as.number <= b.as.number; break;
                    }
                    if (!push(vm, value_bool(comparison)))
                        return VM_STACK_OVERFLOW;
                }
                break;
            case OP_CLEAR_GLOBAL: {
                if (ip >= chunk->count) return VM_INVALID_BYTECODE;
                uint8_t slot = chunk->code[ip++];
                vm->global_defined[slot] = false;
                vm->globals[slot] = value_nil();
                break;
            }
            case OP_DEFINE_GLOBAL:
            case OP_GET_GLOBAL:
            case OP_SET_GLOBAL: {
                if (ip >= chunk->count) return VM_INVALID_BYTECODE;
                uint8_t slot = chunk->code[ip++];
                if (instruction == OP_DEFINE_GLOBAL) {
                    if (vm->global_defined[slot])
                        return VM_INVALID_BYTECODE;
                    if (!pop(vm, &a)) return VM_STACK_UNDERFLOW;
                    vm->globals[slot] = a;
                    vm->global_defined[slot] = true;
                } else {
                    if (!vm->global_defined[slot])
                        return VM_UNDEFINED_GLOBAL;
                    if (instruction == OP_GET_GLOBAL) {
                        if (!push(vm, vm->globals[slot]))
                            return VM_STACK_OVERFLOW;
                    } else {
                        if (vm->stack_count == 0) return VM_STACK_UNDERFLOW;
                        vm->globals[slot] = vm->stack[vm->stack_count - 1];
                    }
                }
                break;
            }
            case OP_JUMP:
            case OP_LOOP:
            case OP_JUMP_IF_FALSE:
            case OP_JUMP_IF_TRUE:
            case OP_JUMP_IF_FALSE_KEEP:
            case OP_JUMP_IF_TRUE_KEEP: {
                if (ip + 1 >= chunk->count) return VM_INVALID_BYTECODE;
                uint16_t offset = (uint16_t)chunk->code[ip] |
                    (uint16_t)((uint16_t)chunk->code[ip + 1] << 8);
                ip += 2;
                bool conditional = instruction != OP_JUMP && instruction != OP_LOOP;
                bool keep = instruction == OP_JUMP_IF_FALSE_KEEP ||
                            instruction == OP_JUMP_IF_TRUE_KEEP;
                bool truthy = vm->stack_count
                    ? !value_is_falsey(vm->stack[vm->stack_count - 1]) : false;
                if (conditional && vm->stack_count == 0)
                    return VM_STACK_UNDERFLOW;
                bool should_jump = !conditional ||
                    ((instruction == OP_JUMP_IF_FALSE ||
                      instruction == OP_JUMP_IF_FALSE_KEEP) ? !truthy : truthy);
                if (conditional && !keep && !pop(vm, &a))
                    return VM_STACK_UNDERFLOW;
                if (should_jump) {
                    if (instruction == OP_LOOP) {
                        if (offset > ip) return VM_INVALID_BYTECODE;
                        ip -= offset;
                    } else {
                        if (offset > chunk->count - ip) return VM_INVALID_BYTECODE;
                        ip += offset;
                    }
                }
                break;
            }
            case OP_PRINT:
                if (!pop(vm, &a)) return VM_STACK_UNDERFLOW;
                value_print(stdout, a);
                fputc('\n', stdout);
                break;
            case OP_GET_LOCAL:
            case OP_SET_LOCAL: {
                if (ip >= chunk->count || vm->frame_count == 0)
                    return VM_INVALID_BYTECODE;
                uint8_t slot = chunk->code[ip++];
                size_t absolute = vm->frames[vm->frame_count - 1].base + slot;
                if (absolute >= vm->stack_count) return VM_INVALID_BYTECODE;
                if (instruction == OP_GET_LOCAL) {
                    if (!push(vm, vm->stack[absolute])) return VM_STACK_OVERFLOW;
                } else {
                    if (vm->stack_count == 0) return VM_STACK_UNDERFLOW;
                    vm->stack[absolute] = vm->stack[vm->stack_count - 1];
                }
                break;
            }
            case OP_NEW_ARRAY: {
                Array *array = calloc(1, sizeof *array);
                if (!array) return VM_OUT_OF_MEMORY;
                if (!register_array(vm, array)) {
                    value_array_free(array);
                    return VM_OUT_OF_MEMORY;
                }
                if (!push(vm, value_array(array))) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_NEW_DICTIONARY: {
                Dictionary *dictionary = calloc(1, sizeof *dictionary);
                if (!dictionary) return VM_OUT_OF_MEMORY;
                if (!register_dictionary(vm, dictionary)) {
                    value_dictionary_free(dictionary);
                    return VM_OUT_OF_MEMORY;
                }
                if (!push(vm, value_dictionary(dictionary))) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_DICTIONARY_LITERAL_ADD: {
                if (!pop(vm, &b) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                Value dictionary_value;
                if (!pop(vm, &dictionary_value)) return VM_STACK_UNDERFLOW;
                if (dictionary_value.type != VALUE_DICTIONARY || !dictionary_value.as.dictionary)
                    return VM_DICTIONARY_REQUIRED;
                if (a.type != VALUE_STRING || !a.as.string) return VM_TYPE_ERROR;
                if (!dictionary_set(dictionary_value.as.dictionary, a.as.string, b))
                    return VM_OUT_OF_MEMORY;
                if (!push(vm, dictionary_value)) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_GET_INDEX: {
                if (!pop(vm, &b) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                size_t index;
                if (a.type == VALUE_STRING && a.as.string) {
                    size_t byte_offset, width, character_count;
                    if (!array_index(b, &index)) return VM_INDEX_ERROR;
                    if (!utf8_count(a.as.string, &character_count)) return VM_UTF8_ERROR;
                    if (index >= character_count ||
                        !utf8_offset(a.as.string, index, &byte_offset) ||
                        !utf8_next(a.as.string->chars, a.as.string->length,
                                   byte_offset, &width))
                        return VM_INDEX_ERROR;
                    String *character = string_new(a.as.string->chars + byte_offset, width);
                    if (!character) return VM_OUT_OF_MEMORY;
                    if (!register_string(vm, character)) { value_string_free(character); return VM_OUT_OF_MEMORY; }
                    if (!push(vm, value_string(character))) return VM_STACK_OVERFLOW;
                } else if (a.type == VALUE_ARRAY && a.as.array) {
                    if (!array_index(b, &index) || index >= a.as.array->count) return VM_INDEX_ERROR;
                    if (!push(vm, a.as.array->items[index])) return VM_STACK_OVERFLOW;
                } else if (a.type == VALUE_DICTIONARY && a.as.dictionary) {
                    if (b.type != VALUE_STRING || !b.as.string) return VM_TYPE_ERROR;
                    if (!push(vm, dictionary_get(a.as.dictionary, b.as.string))) return VM_STACK_OVERFLOW;
                } else {
                    return VM_ARRAY_REQUIRED;
                }
                break;
            }
            case OP_LENGTH: {
                if (!pop(vm, &a)) return VM_STACK_UNDERFLOW;
                size_t count;
                if (a.type == VALUE_ARRAY && a.as.array) {
                    count = a.as.array->count;
                } else if (a.type == VALUE_DICTIONARY && a.as.dictionary) {
                    count = a.as.dictionary->count;
                } else if (a.type == VALUE_STRING && a.as.string) {
                    if (!utf8_count(a.as.string, &count)) return VM_UTF8_ERROR;
                } else {
                    return VM_TYPE_ERROR;
                }
                if (!push(vm, value_number((double)count))) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_SUBSTRING: {
                Value start_value, length_value;
                if (!pop(vm, &length_value) || !pop(vm, &start_value) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                size_t start_index, character_length, start_byte, end_byte;
                size_t character_count;
                if (a.type != VALUE_STRING || !a.as.string ||
                    !array_index(start_value, &start_index) ||
                    !array_index(length_value, &character_length))
                    return VM_INDEX_ERROR;
                if (!utf8_count(a.as.string, &character_count)) return VM_UTF8_ERROR;
                if (start_index > character_count ||
                    character_length > character_count - start_index ||
                    !utf8_offset(a.as.string, start_index, &start_byte) ||
                    !utf8_offset(a.as.string, start_index + character_length, &end_byte))
                    return VM_INDEX_ERROR;
                String *part = string_new(a.as.string->chars + start_byte,
                                          end_byte - start_byte);
                if (!part) return VM_OUT_OF_MEMORY;
                if (!register_string(vm, part)) { value_string_free(part); return VM_OUT_OF_MEMORY; }
                if (!push(vm, value_string(part))) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_ARRAY_PUSH: {
                if (!pop(vm, &b) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (a.type != VALUE_ARRAY || !a.as.array) return VM_ARRAY_REQUIRED;
                size_t new_count = a.as.array->count + 1;
                if (new_count > 65536 || !array_set(a.as.array, a.as.array->count, b))
                    return new_count > 65536 ? VM_INDEX_ERROR : VM_OUT_OF_MEMORY;
                if (!push(vm, value_number((double)a.as.array->count)))
                    return VM_STACK_OVERFLOW;
                break;
            }
            case OP_ARRAY_LITERAL_ADD: {
                if (!pop(vm, &b) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (a.type != VALUE_ARRAY || !a.as.array) return VM_ARRAY_REQUIRED;
                if (a.as.array->count >= 65536 ||
                    !array_set(a.as.array, a.as.array->count, b))
                    return a.as.array->count >= 65536 ? VM_INDEX_ERROR : VM_OUT_OF_MEMORY;
                if (!push(vm, a)) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_ARRAY_POP: {
                if (!pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (a.type != VALUE_ARRAY || !a.as.array) return VM_ARRAY_REQUIRED;
                Value value = value_nil();
                if (a.as.array->count) {
                    value = a.as.array->items[--a.as.array->count];
                    a.as.array->items[a.as.array->count] = value_nil();
                }
                if (!push(vm, value)) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_SET_INDEX: {
                Value value;
                if (!pop(vm, &value) || !pop(vm, &b) || !pop(vm, &a))
                    return VM_STACK_UNDERFLOW;
                size_t index;
                if (a.type == VALUE_DICTIONARY && a.as.dictionary) {
                    if (b.type != VALUE_STRING || !b.as.string) return VM_TYPE_ERROR;
                    if (!dictionary_set(a.as.dictionary, b.as.string, value)) return VM_OUT_OF_MEMORY;
                } else {
                    if (a.type != VALUE_ARRAY || !a.as.array) return VM_ARRAY_REQUIRED;
                    if (!array_index(b, &index)) return VM_INDEX_ERROR;
                    if (!array_set(a.as.array, index, value))
                        return index >= 65536 ? VM_INDEX_ERROR : VM_OUT_OF_MEMORY;
                }
                if (!push(vm, value)) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_CALL: {
                if (ip >= chunk->count) return VM_INVALID_BYTECODE;
                uint8_t index = chunk->code[ip++];
                if (!chunk->functions || index >= chunk->function_count)
                    return VM_INVALID_BYTECODE;
                FunctionInfo function = chunk->functions[index];
                if (function.offset >= chunk->count) return VM_INVALID_BYTECODE;
                if (vm->stack_count < function.arity) return VM_STACK_UNDERFLOW;
                if (vm->frame_count == VM_FRAMES_MAX) return VM_CALL_OVERFLOW;
                size_t base = vm->stack_count - function.arity;
                vm->frames[vm->frame_count++] = (CallFrame){ip, base};
                ip = function.offset;
                break;
            }
            case OP_RETURN: {
                if (!pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (vm->frame_count == 0) {
                    vm->result = a;
                    return VM_OK;
                }
                CallFrame frame = vm->frames[--vm->frame_count];
                vm->stack_count = frame.base;
                if (!push(vm, a)) return VM_STACK_OVERFLOW;
                ip = frame.return_ip;
                break;
            }
            default:
                return VM_INVALID_BYTECODE;
        }
    }
    return VM_INVALID_BYTECODE;
}
