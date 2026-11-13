#include "compiler.h"
#include <stdio.h>
#include <string.h>

#define COMPILER_GLOBAL_MAX 256
#define COMPILER_LOCAL_MAX 256
#define COMPILER_FUNCTION_MAX 255
#define COMPILER_SCOPE_MAX 64

typedef struct { Name name; uint8_t slot; bool active; } Global;
typedef struct { Name name; uint8_t slot; size_t depth; } Local;
typedef struct { Name name; uint8_t index; uint8_t arity; } Function;
typedef struct {
    Chunk *chunk;
    Diagnostic *error;
    Global globals[COMPILER_GLOBAL_MAX];
    size_t global_count;
    Local locals[COMPILER_LOCAL_MAX];
    size_t local_count;
    size_t scope_marks[COMPILER_SCOPE_MAX];
    size_t scope_depth;
    Function functions[COMPILER_FUNCTION_MAX];
    size_t function_count;
    bool in_function;
} Compiler;

static bool same_name(Name a, Name b) {
    return a.length == b.length && memcmp(a.name, b.name, a.length) == 0;
}
static bool fail(Compiler *c, Token t, const char *m) {
    snprintf(c->error->message, sizeof c->error->message, "%s", m);
    c->error->line = t.line; c->error->column = t.column; return false;
}
static bool emit(Compiler *c, OpCode op, Token t) {
    return chunk_write(c->chunk, (uint8_t)op, t.line) ||
           fail(c, t, "out of memory while emitting bytecode");
}
static bool emit_byte(Compiler *c, uint8_t b, Token t) {
    return chunk_write(c->chunk, b, t.line) ||
           fail(c, t, "out of memory while emitting bytecode");
}
static bool emit_slot(Compiler *c, OpCode op, uint8_t slot, Token t) {
    return emit(c, op, t) && emit_byte(c, slot, t);
}
static int find_global(Compiler *c, Name n) {
    for (size_t i=c->global_count;i>0;--i)
        if(c->globals[i-1].active&&same_name(c->globals[i-1].name,n))
            return (int)c->globals[i-1].slot;
    return -1;
}
static int find_local(Compiler *c, Name n) {
    for(size_t i=c->local_count;i>0;--i)
        if(same_name(c->locals[i-1].name,n)) return (int)c->locals[i-1].slot;
    return -1;
}
static int find_function(Compiler *c, Name n) {
    for(size_t i=0;i<c->function_count;i++)
        if(same_name(c->functions[i].name,n)) return (int)i;
    return -1;
}
static bool add_global(Compiler *c, Name n, Token t, uint8_t *slot) {
    size_t mark=c->scope_depth?c->scope_marks[c->scope_depth-1]:0;
    for(size_t i=mark;i<c->global_count;i++)
        if(c->globals[i].active&&same_name(c->globals[i].name,n))
            return fail(c,t,"duplicate variable in the same scope");
    if(c->global_count==COMPILER_GLOBAL_MAX)
        return fail(c,t,"too many global variables");
    *slot=(uint8_t)c->global_count;
    c->globals[c->global_count++]=(Global){n,*slot,true}; return true;
}
static bool add_local(Compiler *c, Name n, Token t, uint8_t *slot) {
    size_t mark=c->scope_depth?c->scope_marks[c->scope_depth-1]:0;
    for(size_t i=mark;i<c->local_count;i++)
        if(same_name(c->locals[i].name,n))
            return fail(c,t,"duplicate local variable in the same scope");
    if(c->local_count==COMPILER_LOCAL_MAX)
        return fail(c,t,"too many local variables");
    *slot=(uint8_t)c->local_count;
    c->locals[c->local_count++]=(Local){n,*slot,c->scope_depth}; return true;
}
static bool enter_scope(Compiler *c, Token t) {
    if(c->scope_depth==COMPILER_SCOPE_MAX)
        return fail(c,t,"compiler scope nesting limit exceeded");
    c->scope_marks[c->scope_depth++]=c->in_function?c->local_count:c->global_count;
    return true;
}
static bool leave_scope(Compiler *c, Token t) {
    if(!c->scope_depth)return true;
    size_t mark=c->scope_marks[--c->scope_depth];
    if(c->in_function) {
        while(c->local_count>mark) {
            if(!emit(c,OP_POP,t))return false;
            --c->local_count;
        }
    } else {
        for(size_t i=mark;i<c->global_count;i++)c->globals[i].active=false;
    }
    return true;
}
static bool emit_jump(Compiler*c,OpCode op,Token t,size_t*operand) {
    if(!emit(c,op,t))return false;
    *operand=c->chunk->count;
    return emit_byte(c,0xff,t)&&emit_byte(c,0xff,t);
}
static bool patch_jump(Compiler*c,size_t at,Token t) {
    size_t d=c->chunk->count-(at+2);
    if(d>UINT16_MAX)return fail(c,t,"branch exceeds 65535 bytes");
    c->chunk->code[at]=(uint8_t)d;c->chunk->code[at+1]=(uint8_t)(d>>8);return true;
}
static bool expression(Compiler*c,Node*n);
static bool binary(Compiler*c,Node*n) {
    TokenType type=n->as.binary.op.type;
    if(type==T_AND||type==T_OR) {
        size_t jump;
        if(!expression(c,n->as.binary.left))return false;
        if(!emit_jump(c,type==T_AND?OP_JUMP_IF_FALSE_KEEP:OP_JUMP_IF_TRUE_KEEP,n->token,&jump))return false;
        if(!emit(c,OP_POP,n->token)||!expression(c,n->as.binary.right))return false;
        return patch_jump(c,jump,n->token);
    }
    if(!expression(c,n->as.binary.left)||!expression(c,n->as.binary.right))return false;
    OpCode op;
    switch(type) {
        case T_PLUS:op=OP_ADD;break; case T_MINUS:op=OP_SUBTRACT;break;
        case T_STAR:op=OP_MULTIPLY;break; case T_SLASH:op=OP_DIVIDE;break;
        case T_FLOOR_DIV:op=OP_FLOOR_DIVIDE;break; case T_PERCENT:op=OP_MODULO;break;
        case T_POWER:op=OP_POWER;break; case T_EQ:op=OP_EQUAL;break;
        case T_NE:return emit(c,OP_EQUAL,n->token)&&emit(c,OP_NOT,n->token);
        case T_GT:op=OP_GREATER;break;case T_LT:op=OP_LESS;break;
        case T_GE:op=OP_GREATER_EQUAL;break;case T_LE:op=OP_LESS_EQUAL;break;
        default:return fail(c,n->token,"unsupported binary operator");
    }
    return emit(c,op,n->token);
}
static bool expression(Compiler*c,Node*n) {
    if(!n)return emit(c,OP_NIL,(Token){.line=1});
    switch(n->kind) {
        case N_NUMBER:return chunk_write_constant(c->chunk,value_number(n->as.number),n->token.line)||fail(c,n->token,"cannot add constant");
        case N_BOOL:return emit(c,n->as.boolean?OP_TRUE:OP_FALSE,n->token);
        case N_IDENT: {
            int slot=c->in_function?find_local(c,n->as.name):-1;
            if(slot>=0)return emit_slot(c,OP_GET_LOCAL,(uint8_t)slot,n->token);
            slot=find_global(c,n->as.name);
            if(slot<0)return fail(c,n->token,"unknown variable in bytecode compiler");
            return emit_slot(c,OP_GET_GLOBAL,(uint8_t)slot,n->token);
        }
        case N_UNARY:
            if(!expression(c,n->as.unary.operand))return false;
            if(n->as.unary.op.type==T_MINUS)return emit(c,OP_NEGATE,n->token);
            if(n->as.unary.op.type==T_NOT)return emit(c,OP_NOT,n->token);
            return fail(c,n->token,"unsupported unary operator");
        case N_BINARY:return binary(c,n);
        case N_CALL: {
            if(n->as.call.name.length==5&&!memcmp(n->as.call.name.name,"print",5)) {
                if(n->as.call.arg_count!=1)return fail(c,n->token,"print() expects one argument");
                return expression(c,n->as.call.args[0])&&emit(c,OP_PRINT,n->token)&&emit(c,OP_NIL,n->token);
            }
            int fn=find_function(c,n->as.call.name);
            if(fn<0)return fail(c,n->token,"unknown function in bytecode compiler");
            if(n->as.call.arg_count!=c->functions[fn].arity)return fail(c,n->token,"function called with incorrect number of arguments");
            for(size_t i=0;i<n->as.call.arg_count;i++)if(!expression(c,n->as.call.args[i]))return false;
            return emit_slot(c,OP_CALL,(uint8_t)fn,n->token);
        }
        case N_ASSIGN: {
            Node*t=n->as.assign.target;
            if(t->kind!=N_IDENT)return fail(c,n->token,"only variable assignment is supported");
            int slot=c->in_function?find_local(c,t->as.name):-1;
            if(slot>=0)return expression(c,n->as.assign.value)&&emit_slot(c,OP_SET_LOCAL,(uint8_t)slot,n->token);
            slot=find_global(c,t->as.name);
            if(slot<0)return fail(c,n->token,"assignment to unknown variable");
            return expression(c,n->as.assign.value)&&emit_slot(c,OP_SET_GLOBAL,(uint8_t)slot,n->token);
        }
        default:return fail(c,n->token,"syntax is not executable yet");
    }
}
static bool statement(Compiler*c,Node*n) {
    if(!n)return true;
    switch(n->kind) {
        case N_FUNCTION:return true;
        case N_VAR: {
            if(c->in_function) {
                uint8_t slot;
                if(!expression(c,n->as.var.value)||!add_local(c,n->as.var.name,n->token,&slot))return false;
                (void)slot; return true;
            }
            uint8_t slot;
            return expression(c,n->as.var.value)&&add_global(c,n->as.var.name,n->token,&slot)&&emit_slot(c,OP_DEFINE_GLOBAL,slot,n->token);
        }
        case N_IF: {
            size_t otherwise,end;
            if(!expression(c,n->as.conditional.condition)||
               !emit_jump(c,OP_JUMP_IF_FALSE,n->token,&otherwise)||
               !statement(c,n->as.conditional.then_branch)||
               !emit_jump(c,OP_JUMP,n->token,&end)||
               !patch_jump(c,otherwise,n->token))return false;
            if(n->as.conditional.else_branch&&!statement(c,n->as.conditional.else_branch))return false;
            return patch_jump(c,end,n->token);
        }
        case N_RETURN:
            if(!c->in_function)return fail(c,n->token,"return outside function");
            return expression(c,n->as.ret.value)&&emit(c,OP_RETURN,n->token);
        case N_BLOCK: {
            if(!enter_scope(c,n->token))return false;
            bool ok=true;
            for(size_t i=0;i<n->as.list.count&&ok;i++)ok=statement(c,n->as.list.items[i]);
            return leave_scope(c,n->token)&&ok;
        }
        default:return expression(c,n)&&emit(c,OP_POP,n->token);
    }
}
bool compile_ast(const Ast*ast,Chunk*chunk,Diagnostic*error) {
    if(!ast||!ast->root||!chunk||!error)return false;
    memset(error,0,sizeof*error);
    Compiler c={.chunk=chunk,.error=error};
    Node*root=ast->root;
    if(root->kind!=N_PROGRAM)return fail(&c,root->token,"expected program root");
    for(size_t i=0;i<root->as.list.count;i++)if(root->as.list.items[i]->kind==N_FUNCTION) {
        Node*f=root->as.list.items[i];
        if(c.function_count==COMPILER_FUNCTION_MAX)return fail(&c,f->token,"too many functions");
        if(find_function(&c,f->as.function.name)>=0)return fail(&c,f->token,"duplicate function");
        c.functions[c.function_count]=(Function){f->as.function.name,(uint8_t)c.function_count,(uint8_t)f->as.function.param_count};
        if(f->as.function.param_count>UINT8_MAX)return fail(&c,f->token,"too many parameters");
        ++c.function_count;
    }
    for(size_t i=0;i<root->as.list.count;i++)if(root->as.list.items[i]->kind!=N_FUNCTION)
        if(!statement(&c,root->as.list.items[i]))return false;
    if(!emit(&c,OP_NIL,root->token)||!emit(&c,OP_RETURN,root->token))return false;
    for(size_t i=0;i<root->as.list.count;i++)if(root->as.list.items[i]->kind==N_FUNCTION) {
        Node*f=root->as.list.items[i];
        uint8_t index;
        if(!chunk_add_function(chunk,chunk->count,(uint8_t)f->as.function.param_count,&index))
            return fail(&c,f->token,"cannot register function");
        (void)index;
        c.in_function=true;c.local_count=0;c.scope_depth=0;
        for(size_t p=0;p<f->as.function.param_count;p++) {
            uint8_t slot;
            if(!add_local(&c,f->as.function.params[p],f->token,&slot))return false;
            (void)slot;
        }
        if(!statement(&c,f->as.function.body))return false;
        if(!emit(&c,OP_NIL,f->token)||!emit(&c,OP_RETURN,f->token))return false;
        c.in_function=false;c.local_count=0;c.scope_depth=0;
    }
    return true;
}
