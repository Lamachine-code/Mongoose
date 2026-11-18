#include "vm.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void vm_init(VM *vm) {
    if (vm) { *vm = (VM){0}; vm->gc_threshold = 64; }
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
    for (size_t i = 0; i < vm->closure_count; ++i)
        value_closure_free(vm->closures[i]);
    free(vm->closures);
    for (size_t i = 0; i < vm->upvalue_count; ++i)
        value_upvalue_free(vm->upvalues[i]);
    free(vm->upvalues);
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

/* Compute a geometrically grown capacity without overflowing either the
   element count or the byte size passed to the allocator. */
static bool next_capacity(size_t current, size_t minimum,
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

static bool register_array(VM *vm, Array *array) {
    if (vm->array_count == vm->array_capacity) {
        size_t capacity;
        if (!next_capacity(vm->array_capacity, vm->array_count + 1,
                           sizeof *vm->arrays, &capacity)) return false;
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
        size_t capacity;
        if (!next_capacity(vm->string_capacity, vm->string_count + 1,
                           sizeof *vm->strings, &capacity)) return false;
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
        size_t capacity;
        if (!next_capacity(array->capacity, index + 1, sizeof *array->items,
                           &capacity) || capacity > 65536) return false;
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
        size_t capacity;
        if (!next_capacity(vm->dictionary_capacity, vm->dictionary_count + 1,
                           sizeof *vm->dictionaries, &capacity)) return false;
        Dictionary **items = realloc(vm->dictionaries, capacity * sizeof *items);
        if (!items) return false;
        vm->dictionaries = items;
        vm->dictionary_capacity = capacity;
    }
    vm->dictionaries[vm->dictionary_count++] = dictionary;
    return true;
}

bool vm_register_closure(VM *vm, Closure *closure) {
    if (!vm || !closure) return false;
    if (vm->closure_count == vm->closure_capacity) {
        size_t capacity;
        if (!next_capacity(vm->closure_capacity, vm->closure_count + 1,
                           sizeof *vm->closures, &capacity)) return false;
        Closure **items = realloc(vm->closures, capacity * sizeof *items);
        if (!items) return false;
        vm->closures = items;
        vm->closure_capacity = capacity;
    }
    vm->closures[vm->closure_count++] = closure;
    return true;
}

bool vm_register_upvalue(VM *vm, Upvalue *upvalue) {
    if (!vm || !upvalue) return false;
    if (vm->upvalue_count == vm->upvalue_capacity) {
        size_t capacity;
        if (!next_capacity(vm->upvalue_capacity, vm->upvalue_count + 1,
                           sizeof *vm->upvalues, &capacity)) return false;
        Upvalue **items = realloc(vm->upvalues, capacity * sizeof *items);
        if (!items) return false;
        vm->upvalues = items;
        vm->upvalue_capacity = capacity;
    }
    vm->upvalues[vm->upvalue_count++] = upvalue;
    return true;
}

/* Keep open cells ordered by descending stack index. This makes closing all
   captures belonging to a departing frame a bounded walk over the list head. */
Upvalue *vm_capture_upvalue(VM *vm, size_t stack_index) {
    if (!vm || stack_index >= vm->stack_count || stack_index >= VM_STACK_MAX)
        return NULL;

    Upvalue **cursor = &vm->open_upvalues;
    while (*cursor) {
        size_t existing_index =
            (size_t)((*cursor)->location - vm->stack);
        if (existing_index <= stack_index) break;
        cursor = &(*cursor)->next_open;
    }
    if (*cursor && (*cursor)->location == &vm->stack[stack_index])
        return *cursor;

    Upvalue *upvalue = upvalue_new_open(&vm->stack[stack_index]);
    if (!upvalue) return NULL;
    if (!vm_register_upvalue(vm, upvalue)) {
        value_upvalue_free(upvalue);
        return NULL;
    }
    upvalue->next_open = *cursor;
    *cursor = upvalue;
    return upvalue;
}

void vm_close_upvalues(VM *vm, size_t first_stack_index) {
    if (!vm || first_stack_index > VM_STACK_MAX) return;
    while (vm->open_upvalues) {
        Upvalue *upvalue = vm->open_upvalues;
        size_t stack_index = (size_t)(upvalue->location - vm->stack);
        if (stack_index < first_stack_index) break;
        vm->open_upvalues = upvalue->next_open;
        upvalue->next_open = NULL;
        upvalue_close(upvalue);
    }
}

/* A single OP_POP removes exactly the top slot. Closing only that slot keeps
   unrelated open cells intact; frame unwinding still uses the range helper. */
static void vm_close_upvalue_at(VM *vm, size_t stack_index) {
    if (!vm || stack_index >= VM_STACK_MAX) return;
    Upvalue **cursor = &vm->open_upvalues;
    while (*cursor) {
        size_t existing_index = (size_t)((*cursor)->location - vm->stack);
        if (existing_index < stack_index) break;
        if (existing_index == stack_index) {
            Upvalue *upvalue = *cursor;
            *cursor = upvalue->next_open;
            upvalue->next_open = NULL;
            upvalue_close(upvalue);
            return;
        }
        cursor = &(*cursor)->next_open;
    }
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
        size_t capacity;
        if (!next_capacity(dictionary->capacity, dictionary->count + 1,
                           sizeof *dictionary->entries, &capacity)) return false;
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

static bool dictionary_has_key(Dictionary *dictionary, String *key) {
    for (size_t i = 0; i < dictionary->count; ++i) {
        String *existing = dictionary->entries[i].key;
        if (existing->length == key->length &&
            memcmp(existing->chars, key->chars, key->length) == 0)
            return true;
    }
    return false;
}

static bool dictionary_delete(Dictionary *dictionary, String *key) {
    for (size_t i = 0; i < dictionary->count; ++i) {
        String *existing = dictionary->entries[i].key;
        if (existing->length == key->length &&
            memcmp(existing->chars, key->chars, key->length) == 0) {
            for (size_t j = i + 1; j < dictionary->count; ++j)
                dictionary->entries[j - 1] = dictionary->entries[j];
            --dictionary->count;
            dictionary->entries[dictionary->count] = (DictionaryEntryValue){0};
            return true;
        }
    }
    return false;
}


/* Marking only flips flags; the repeated scan avoids a temporary worklist
   allocation and handles cyclic arrays/dictionaries safely. */
static bool mark_value(Value value) {
    switch (value.type) {
        case VALUE_ARRAY:
            if (value.as.array && !value.as.array->marked) {
                value.as.array->marked = true;
                return true;
            }
            break;
        case VALUE_STRING:
            if (value.as.string && !value.as.string->marked) {
                value.as.string->marked = true;
                return true;
            }
            break;
        case VALUE_DICTIONARY:
            if (value.as.dictionary && !value.as.dictionary->marked) {
                value.as.dictionary->marked = true;
                return true;
            }
            break;
        case VALUE_CLOSURE:
            if (value.as.closure && !value.as.closure->marked) {
                value.as.closure->marked = true;
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

static size_t object_count(const VM *vm) {
    size_t total = vm->array_count;
    if (vm->string_count > SIZE_MAX - total) return SIZE_MAX;
    total += vm->string_count;
    if (vm->dictionary_count > SIZE_MAX - total) return SIZE_MAX;
    total += vm->dictionary_count;
    if (vm->closure_count > SIZE_MAX - total) return SIZE_MAX;
    total += vm->closure_count;
    if (vm->upvalue_count > SIZE_MAX - total) return SIZE_MAX;
    return total + vm->upvalue_count;
}

static void collect_garbage(VM *vm, const Chunk *chunk) {
    for (size_t i = 0; i < vm->stack_count; ++i)
        (void)mark_value(vm->stack[i]);
    for (size_t i = 0; i < VM_GLOBAL_MAX; ++i)
        if (vm->global_defined[i]) (void)mark_value(vm->globals[i]);
    (void)mark_value(vm->result);
    /* A running frame keeps its closure alive even if it is no longer on
       the operand stack. */
    for (size_t i = 0; i < vm->frame_count; ++i)
        if (vm->frames[i].closure)
            (void)mark_value(value_closure(vm->frames[i].closure));
    for (size_t i = 0; i < chunk->constant_count; ++i)
        (void)mark_value(chunk->constants[i]);
    /* An open capture cell must survive even before a closure references it. */
    for (Upvalue *upvalue = vm->open_upvalues; upvalue;
         upvalue = upvalue->next_open)
        upvalue->marked = true;

    bool changed;
    do {
        changed = false;
        for (size_t i = 0; i < vm->array_count; ++i) {
            Array *array = vm->arrays[i];
            if (!array->marked) continue;
            for (size_t j = 0; j < array->count; ++j)
                if (mark_value(array->items[j])) changed = true;
        }
        for (size_t i = 0; i < vm->dictionary_count; ++i) {
            Dictionary *dictionary = vm->dictionaries[i];
            if (!dictionary->marked) continue;
            for (size_t j = 0; j < dictionary->count; ++j) {
                if (dictionary->entries[j].key &&
                    !dictionary->entries[j].key->marked) {
                    dictionary->entries[j].key->marked = true;
                    changed = true;
                }
                if (mark_value(dictionary->entries[j].value)) changed = true;
            }
        }
        for (size_t i = 0; i < vm->closure_count; ++i) {
            Closure *closure = vm->closures[i];
            if (!closure->marked) continue;
            for (size_t j = 0; j < closure->upvalue_count; ++j) {
                Upvalue *upvalue = closure->upvalues[j];
                if (upvalue && !upvalue->marked) {
                    upvalue->marked = true;
                    changed = true;
                }
            }
        }
        for (size_t i = 0; i < vm->upvalue_count; ++i) {
            Upvalue *upvalue = vm->upvalues[i];
            if (upvalue->marked && mark_value(upvalue_get(upvalue)))
                changed = true;
        }
    } while (changed);

    size_t kept = 0;
    for (size_t i = 0; i < vm->array_count; ++i) {
        Array *array = vm->arrays[i];
        if (array->marked) {
            array->marked = false;
            vm->arrays[kept++] = array;
        } else {
            value_array_free(array);
        }
    }
    vm->array_count = kept;

    kept = 0;
    for (size_t i = 0; i < vm->dictionary_count; ++i) {
        Dictionary *dictionary = vm->dictionaries[i];
        if (dictionary->marked) {
            dictionary->marked = false;
            vm->dictionaries[kept++] = dictionary;
        } else {
            value_dictionary_free(dictionary);
        }
    }
    vm->dictionary_count = kept;

    kept = 0;
    for (size_t i = 0; i < vm->string_count; ++i) {
        String *string = vm->strings[i];
        if (string->marked) {
            string->marked = false;
            vm->strings[kept++] = string;
        } else {
            value_string_free(string);
        }
    }
    vm->string_count = kept;

    kept = 0;
    for (size_t i = 0; i < vm->closure_count; ++i) {
        Closure *closure = vm->closures[i];
        if (closure->marked) {
            closure->marked = false;
            vm->closures[kept++] = closure;
        } else {
            value_closure_free(closure);
        }
    }
    vm->closure_count = kept;

    kept = 0;
    for (size_t i = 0; i < vm->upvalue_count; ++i) {
        Upvalue *upvalue = vm->upvalues[i];
        if (upvalue->marked) {
            upvalue->marked = false;
            vm->upvalues[kept++] = upvalue;
        } else {
            value_upvalue_free(upvalue);
        }
    }
    vm->upvalue_count = kept;

    size_t live = object_count(vm);
    vm->gc_threshold = live > SIZE_MAX / 2 ? SIZE_MAX : live * 2;
    if (vm->gc_threshold < 64) vm->gc_threshold = 64;
    ++vm->gc_runs;
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
        if (vm->gc_threshold == 0) vm->gc_threshold = 64;
        if (object_count(vm) >= vm->gc_threshold)
            collect_garbage(vm, chunk);
        size_t instruction_offset = ip;
        uint8_t instruction = chunk->code[ip++];
        vm->error_line = chunk->lines ? chunk->lines[instruction_offset] : 0;
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
                if (vm->stack_count == 0) return VM_STACK_UNDERFLOW;
                vm_close_upvalue_at(vm, vm->stack_count - 1);
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
            case OP_CLOSURE: {
                if (ip >= chunk->count || vm->frame_count == 0 ||
                    !chunk->functions)
                    return VM_INVALID_BYTECODE;
                uint8_t function_index = chunk->code[ip++];
                if (function_index >= chunk->function_count)
                    return VM_INVALID_BYTECODE;
                FunctionInfo *function = &chunk->functions[function_index];
                CallFrame *parent = &vm->frames[vm->frame_count - 1];
                if (function->upvalue_count > CHUNK_UPVALUE_MAX)
                    return VM_INVALID_BYTECODE;

                Closure *closure = closure_new(function_index,
                                               function->upvalue_count);
                if (!closure) return VM_OUT_OF_MEMORY;
                for (size_t i = 0; i < function->upvalue_count; ++i) {
                    UpvalueDescriptor descriptor = function->upvalues[i];
                    if (descriptor.is_local) {
                        size_t absolute = parent->base + descriptor.index;
                        if (absolute < parent->base ||
                            absolute >= vm->stack_count) {
                            value_closure_free(closure);
                            return VM_INVALID_BYTECODE;
                        }
                        closure->upvalues[i] = vm_capture_upvalue(vm, absolute);
                    } else {
                        if (!parent->closure ||
                            descriptor.index >= parent->closure->upvalue_count ||
                            !parent->closure->upvalues ||
                            !parent->closure->upvalues[descriptor.index]) {
                            value_closure_free(closure);
                            return VM_INVALID_BYTECODE;
                        }
                        closure->upvalues[i] =
                            parent->closure->upvalues[descriptor.index];
                    }
                    if (!closure->upvalues[i]) {
                        value_closure_free(closure);
                        return VM_OUT_OF_MEMORY;
                    }
                }
                if (!vm_register_closure(vm, closure)) {
                    value_closure_free(closure);
                    return VM_OUT_OF_MEMORY;
                }
                if (!push(vm, value_closure(closure)))
                    return VM_STACK_OVERFLOW;
                break;
            }
            case OP_GET_UPVALUE:
            case OP_SET_UPVALUE: {
                if (ip >= chunk->count || vm->frame_count == 0)
                    return VM_INVALID_BYTECODE;
                uint8_t slot = chunk->code[ip++];
                CallFrame *frame = &vm->frames[vm->frame_count - 1];
                if (!frame->closure ||
                    slot >= frame->closure->upvalue_count ||
                    !frame->closure->upvalues ||
                    !frame->closure->upvalues[slot])
                    return VM_INVALID_BYTECODE;
                Upvalue *upvalue = frame->closure->upvalues[slot];
                if (instruction == OP_GET_UPVALUE) {
                    if (!push(vm, upvalue_get(upvalue)))
                        return VM_STACK_OVERFLOW;
                } else {
                    if (vm->stack_count == 0) return VM_STACK_UNDERFLOW;
                    upvalue_set(upvalue, vm->stack[vm->stack_count - 1]);
                }
                break;
            }
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
            case OP_ITER_INIT: {
                if (!pop(vm, &a)) return VM_STACK_UNDERFLOW;
                Array *snapshot = calloc(1, sizeof *snapshot);
                if (!snapshot) return VM_OUT_OF_MEMORY;
                if (!register_array(vm, snapshot)) {
                    value_array_free(snapshot);
                    return VM_OUT_OF_MEMORY;
                }
                if (a.type == VALUE_ARRAY && a.as.array) {
                    for (size_t i = 0; i < a.as.array->count; ++i)
                        if (!array_set(snapshot, i, a.as.array->items[i]))
                            return VM_OUT_OF_MEMORY;
                } else if (a.type == VALUE_DICTIONARY && a.as.dictionary) {
                    for (size_t i = 0; i < a.as.dictionary->count; ++i)
                        if (!array_set(snapshot, i,
                                       value_string(a.as.dictionary->entries[i].key)))
                            return VM_OUT_OF_MEMORY;
                } else {
                    return VM_TYPE_ERROR;
                }
                if (!push(vm, value_array(snapshot))) return VM_STACK_OVERFLOW;
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
                if (a.type != VALUE_STRING || !a.as.string) return VM_STRING_REQUIRED;
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
                    if (b.type != VALUE_STRING || !b.as.string) return VM_STRING_REQUIRED;
                    if (!push(vm, dictionary_get(a.as.dictionary, b.as.string))) return VM_STACK_OVERFLOW;
                } else {
                    return VM_INDEXABLE_REQUIRED;
                }
                break;
            }
            case OP_GET_INDEX_KEEP: {
                if (!pop(vm, &b) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (!push(vm, a) || !push(vm, b)) return VM_STACK_OVERFLOW;
                size_t index;
                if (a.type == VALUE_ARRAY && a.as.array) {
                    if (!array_index(b, &index) || index >= a.as.array->count)
                        return VM_INDEX_ERROR;
                    if (!push(vm, a.as.array->items[index])) return VM_STACK_OVERFLOW;
                } else if (a.type == VALUE_DICTIONARY && a.as.dictionary) {
                    if (b.type != VALUE_STRING || !b.as.string) return VM_TYPE_ERROR;
                    if (!push(vm, dictionary_get(a.as.dictionary, b.as.string)))
                        return VM_STACK_OVERFLOW;
                } else {
                    return VM_INDEXABLE_REQUIRED;
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
            case OP_DICTIONARY_KEYS:
            case OP_DICTIONARY_VALUES: {
                if (!pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (a.type != VALUE_DICTIONARY || !a.as.dictionary)
                    return VM_DICTIONARY_REQUIRED;
                Array *array = calloc(1, sizeof *array);
                if (!array) return VM_OUT_OF_MEMORY;
                if (!register_array(vm, array)) {
                    value_array_free(array);
                    return VM_OUT_OF_MEMORY;
                }
                for (size_t i = 0; i < a.as.dictionary->count; ++i) {
                    Value item = instruction == OP_DICTIONARY_KEYS
                        ? value_string(a.as.dictionary->entries[i].key)
                        : a.as.dictionary->entries[i].value;
                    if (!array_set(array, i, item)) return VM_OUT_OF_MEMORY;
                }
                if (!push(vm, value_array(array))) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_DICTIONARY_HAS_KEY: {
                if (!pop(vm, &b) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (a.type != VALUE_DICTIONARY || !a.as.dictionary)
                    return VM_DICTIONARY_REQUIRED;
                if (b.type != VALUE_STRING || !b.as.string)
                    return VM_TYPE_ERROR;
                if (!push(vm, value_bool(dictionary_has_key(a.as.dictionary, b.as.string))))
                    return VM_STACK_OVERFLOW;
                break;
            }
            case OP_ARRAY_REMOVE: {
                if (!pop(vm, &b) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (a.type != VALUE_ARRAY || !a.as.array) return VM_ARRAY_REQUIRED;
                size_t index;
                if (!array_index(b, &index) || index >= a.as.array->count)
                    return VM_INDEX_ERROR;
                Value removed = a.as.array->items[index];
                for (size_t i = index + 1; i < a.as.array->count; ++i)
                    a.as.array->items[i - 1] = a.as.array->items[i];
                --a.as.array->count;
                a.as.array->items[a.as.array->count] = value_nil();
                if (!push(vm, removed)) return VM_STACK_OVERFLOW;
                break;
            }
            case OP_DICTIONARY_DELETE: {
                if (!pop(vm, &b) || !pop(vm, &a)) return VM_STACK_UNDERFLOW;
                if (a.type != VALUE_DICTIONARY || !a.as.dictionary)
                    return VM_DICTIONARY_REQUIRED;
                if (b.type != VALUE_STRING || !b.as.string) return VM_TYPE_ERROR;
                if (!push(vm, value_bool(dictionary_delete(a.as.dictionary, b.as.string))))
                    return VM_STACK_OVERFLOW;
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
                vm->frames[vm->frame_count++] = (CallFrame){ip, base, index, vm->error_line, NULL};
                ip = function.offset;
                break;
            }
            case OP_CALL_VALUE: {
                if (ip >= chunk->count) return VM_INVALID_BYTECODE;
                uint8_t argument_count = chunk->code[ip++];
                if (vm->stack_count < (size_t)argument_count + 1)
                    return VM_STACK_UNDERFLOW;
                size_t callee_slot = vm->stack_count - (size_t)argument_count - 1;
                Value callee = vm->stack[callee_slot];
                if (callee.type != VALUE_CLOSURE || !callee.as.closure)
                    return VM_TYPE_ERROR;
                Closure *closure = callee.as.closure;
                if (!chunk->functions ||
                    closure->function_index >= chunk->function_count)
                    return VM_INVALID_BYTECODE;
                FunctionInfo function = chunk->functions[closure->function_index];
                if (function.offset >= chunk->count)
                    return VM_INVALID_BYTECODE;
                if (argument_count != function.arity)
                    return VM_ARITY_ERROR;
                if (vm->frame_count == VM_FRAMES_MAX)
                    return VM_CALL_OVERFLOW;

                /* Compact [callee, arg0, ...] into [arg0, ...]. This keeps
                   the established frame convention: base points at arg0. */
                for (size_t i = 0; i < argument_count; ++i)
                    vm->stack[callee_slot + i] =
                        vm->stack[callee_slot + i + 1];
                --vm->stack_count;
                vm->frames[vm->frame_count++] = (CallFrame){
                    ip, callee_slot, closure->function_index,
                    vm->error_line, closure
                };
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
                vm_close_upvalues(vm, frame.base);
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
