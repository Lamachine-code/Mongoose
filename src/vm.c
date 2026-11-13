#include "vm.h"
#include <math.h>

void vm_init(VM *vm) { if (vm) *vm = (VM){0}; }
static bool push(VM *vm, Value v) {
 if (vm->stack_count == VM_STACK_MAX) return false;
 vm->stack[vm->stack_count++] = v; return true;
}
static bool pop(VM *vm, Value *v) {
 if (!vm->stack_count) return false;
 *v = vm->stack[--vm->stack_count]; return true;
}
static bool pop_numbers(VM *vm, double *left, double *right) {
 Value a, b;
 if (vm->stack_count < 2) return false;
 if (!pop(vm,&b) || !pop(vm,&a)) return false;
 if (a.type != VALUE_NUMBER || b.type != VALUE_NUMBER) return false;
 *left=a.as.number; *right=b.as.number; return true;
}
VMResult vm_run(VM *vm, const Chunk *chunk) {
 if (!vm || !chunk || (chunk->count && !chunk->code)) return VM_INVALID_BYTECODE;
 size_t ip=0;
 while (ip<chunk->count) {
  uint8_t instruction=chunk->code[ip++]; Value a,b; double left,right;
  switch ((OpCode)instruction) {
   case OP_CONSTANT: {
    if (ip+1>=chunk->count) return VM_INVALID_BYTECODE;
    uint16_t index=(uint16_t)chunk->code[ip] | (uint16_t)((uint16_t)chunk->code[ip+1]<<8); ip+=2;
    if (index>=chunk->constant_count || !chunk->constants) return VM_INVALID_BYTECODE;
    if (!push(vm,chunk->constants[index])) return VM_STACK_OVERFLOW; break;
   }
   case OP_NIL: if(!push(vm,value_nil())) return VM_STACK_OVERFLOW; break;
   case OP_TRUE: if(!push(vm,value_bool(true))) return VM_STACK_OVERFLOW; break;
   case OP_FALSE: if(!push(vm,value_bool(false))) return VM_STACK_OVERFLOW; break;
   case OP_POP: if(!pop(vm,&a)) return VM_STACK_UNDERFLOW; break;
   case OP_NEGATE: case OP_NOT:
    if(!pop(vm,&a)) return VM_STACK_UNDERFLOW;
    if(instruction==OP_NEGATE) {
     if(a.type!=VALUE_NUMBER) return VM_TYPE_ERROR;
     a=value_number(-a.as.number);
    } else a=value_bool(value_is_falsey(a));
    if(!push(vm,a)) return VM_STACK_OVERFLOW; break;
   case OP_ADD: case OP_SUBTRACT: case OP_MULTIPLY: case OP_DIVIDE:
   case OP_FLOOR_DIVIDE: case OP_MODULO: case OP_POWER:
    if(vm->stack_count<2) return VM_STACK_UNDERFLOW;
    if(!pop_numbers(vm,&left,&right)) return VM_TYPE_ERROR;
    if((instruction==OP_DIVIDE||instruction==OP_FLOOR_DIVIDE||instruction==OP_MODULO)&&right==0.0) return VM_DIVISION_BY_ZERO;
    switch((OpCode)instruction) {
     case OP_ADD:left+=right;break; case OP_SUBTRACT:left-=right;break;
     case OP_MULTIPLY:left*=right;break; case OP_DIVIDE:left/=right;break;
     case OP_FLOOR_DIVIDE:left=floor(left/right);break;
     case OP_MODULO:left=fmod(left,right);break; case OP_POWER:left=pow(left,right);break;
     default:break;
    }
    if(!push(vm,value_number(left))) return VM_STACK_OVERFLOW; break;
   case OP_EQUAL: case OP_GREATER: case OP_LESS:
    if(vm->stack_count<2) return VM_STACK_UNDERFLOW;
    if(!pop(vm,&b)||!pop(vm,&a)) return VM_STACK_UNDERFLOW;
    if(instruction==OP_EQUAL) {
     bool eq=a.type==b.type && (a.type==VALUE_NIL ||
      (a.type==VALUE_BOOL&&a.as.boolean==b.as.boolean) ||
      (a.type==VALUE_NUMBER&&a.as.number==b.as.number));
     if(!push(vm,value_bool(eq))) return VM_STACK_OVERFLOW;
    } else {
     if(a.type!=VALUE_NUMBER||b.type!=VALUE_NUMBER) return VM_TYPE_ERROR;
     bool cmp=instruction==OP_GREATER?a.as.number>b.as.number:a.as.number<b.as.number;
     if(!push(vm,value_bool(cmp))) return VM_STACK_OVERFLOW;
    } break;
   case OP_DEFINE_GLOBAL: case OP_GET_GLOBAL: case OP_SET_GLOBAL: {
    if(ip>=chunk->count) return VM_INVALID_BYTECODE;
    uint8_t slot=chunk->code[ip++];
    if(instruction==OP_DEFINE_GLOBAL) {
     if(vm->global_defined[slot]||!pop(vm,&a)) return vm->global_defined[slot]?VM_INVALID_BYTECODE:VM_STACK_UNDERFLOW;
     vm->globals[slot]=a; vm->global_defined[slot]=true;
    } else if(!vm->global_defined[slot]) return VM_UNDEFINED_GLOBAL;
    else if(instruction==OP_GET_GLOBAL) {
     if(!push(vm,vm->globals[slot])) return VM_STACK_OVERFLOW;
    } else {
     if(!vm->stack_count) return VM_STACK_UNDERFLOW;
     vm->globals[slot]=vm->stack[vm->stack_count-1];
    } break;
   }
   case OP_PRINT:
    if(!pop(vm,&a)) return VM_STACK_UNDERFLOW;
    value_print(stdout,a); fputc('\n',stdout); break;
   case OP_RETURN: if(!pop(vm,&vm->result)) return VM_STACK_UNDERFLOW; return VM_OK;
   default:return VM_INVALID_BYTECODE;
  }
 }
 return VM_INVALID_BYTECODE;
}
