#include "vm.h"
#include <math.h>
#include <stdlib.h>

void vm_init(VM *vm) {
    if (vm) *vm = (VM){0};
}

void vm_free(VM *vm) {
    if (!vm) return;
    for (size_t i = 0; i < vm->array_count; ++i)
        value_array_free(vm->arrays[i]);
    free(vm->arrays);
    *vm = (VM){0};
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
                    case OP_ADD: left += right; break;
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
                          a.as.array == b.as.array));
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
            case OP_GET_INDEX: {
                if (!pop(vm, &b) || !pop(vm, &a))
                    return VM_STACK_UNDERFLOW;
                size_t index;
                if (a.type != VALUE_ARRAY || !a.as.array)
                    return VM_ARRAY_REQUIRED;
                if (!array_index(b, &index) || index >= a.as.array->count)
                    return VM_INDEX_ERROR;
                if (!push(vm, a.as.array->items[index]))
                    return VM_STACK_OVERFLOW;
                break;
            }
            case OP_SET_INDEX: {
                Value value;
                if (!pop(vm, &value) || !pop(vm, &b) || !pop(vm, &a))
                    return VM_STACK_UNDERFLOW;
                size_t index;
                if (a.type != VALUE_ARRAY || !a.as.array)
                    return VM_ARRAY_REQUIRED;
                if (!array_index(b, &index)) return VM_INDEX_ERROR;
                if (!array_set(a.as.array, index, value))
                    return index >= 65536 ? VM_INDEX_ERROR : VM_OUT_OF_MEMORY;
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
