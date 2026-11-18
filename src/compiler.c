#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPILER_GLOBAL_MAX 256
#define COMPILER_LOCAL_MAX 256
#define COMPILER_FUNCTION_MAX 255
#define COMPILER_SCOPE_MAX 64

typedef struct { Name name; uint8_t slot; bool active; } Global;
typedef struct { Name name; uint8_t slot; size_t depth; } Local;
typedef struct {
    Name name;
    uint8_t index;
    uint8_t arity;
    Node *declaration;
    int parent_index;
    bool top_level;
} Function;
typedef struct {
    size_t start;
    size_t break_variable_count, continue_variable_count;
    size_t break_jumps[256], break_count;
    size_t continue_jumps[256], continue_count;
    bool continue_to_increment;
} Loop;
typedef struct CompilerContext {
    struct CompilerContext *parent;
    uint8_t function_index;
    Local locals[COMPILER_LOCAL_MAX];
    size_t local_count;
    size_t scope_marks[COMPILER_SCOPE_MAX];
    size_t scope_depth;
    Loop loops[COMPILER_SCOPE_MAX];
    size_t loop_depth;
} CompilerContext;

typedef struct {
    Chunk *chunk;
    Diagnostic *error;
    Global globals[COMPILER_GLOBAL_MAX];
    size_t global_count;
    Function functions[COMPILER_FUNCTION_MAX];
    size_t function_count;
    size_t global_scope_marks[COMPILER_SCOPE_MAX];
    size_t global_scope_depth;
    Loop global_loops[COMPILER_SCOPE_MAX];
    size_t global_loop_depth;
    CompilerContext *context;
} Compiler;

static bool same_name(Name a, Name b) {
    return a.length == b.length && memcmp(a.name, b.name, a.length) == 0;
}
static bool fail(Compiler *c, Token t, const char *m) {
    snprintf(c->error->message, sizeof c->error->message, "%s", m);
    c->error->line = t.line; c->error->column = t.column; return false;
}
static bool fail_arity(Compiler *c, Token t, Name function, size_t expected, size_t received) {
    snprintf(c->error->message, sizeof c->error->message,
             "%.*s() expects %zu argument%s, received %zu",
             (int)function.length, function.name, expected,
             expected == 1 ? "" : "s", received);
    c->error->line = t.line;
    c->error->column = t.column;
    return false;
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
    if (!c->context) return -1;
    for (size_t i = c->context->local_count; i > 0; --i)
        if (same_name(c->context->locals[i - 1].name, n))
            return (int)c->context->locals[i - 1].slot;
    return -1;
}
static size_t *current_scope_depth(Compiler *c) {
    return c->context ? &c->context->scope_depth : &c->global_scope_depth;
}
static size_t *current_loop_depth(Compiler *c) {
    return c->context ? &c->context->loop_depth : &c->global_loop_depth;
}
static Loop *current_loops(Compiler *c) {
    return c->context ? c->context->loops : c->global_loops;
}
static int find_function(Compiler *c, Name n) {
    /* Until closures are emitted, only root-level functions are directly callable. */
    for (size_t i = 0; i < c->function_count; ++i)
        if (c->functions[i].top_level && same_name(c->functions[i].name, n))
            return (int)i;
    return -1;
}

static bool register_function(Compiler *c, Node *declaration,
                              int parent_index, bool top_level) {
    if (c->function_count == COMPILER_FUNCTION_MAX)
        return fail(c, declaration->token, "too many functions");
    if (declaration->as.function.param_count > UINT8_MAX)
        return fail(c, declaration->token, "too many parameters");

    uint8_t index = (uint8_t)c->function_count;
    c->functions[c->function_count++] = (Function){
        .name = declaration->as.function.name,
        .index = index,
        .arity = (uint8_t)declaration->as.function.param_count,
        .declaration = declaration,
        .parent_index = parent_index,
        .top_level = top_level
    };
    return true;
}

/* Find declarations nested in a function body, preserving lexical ancestry.
   This pass records prototypes only; nested bodies are not executable yet. */
static bool discover_nested_functions(Compiler *c, Node *node,
                                      int parent_index) {
    if (!node) return true;
    switch (node->kind) {
        case N_FUNCTION: {
            if (!register_function(c, node, parent_index, false)) return false;
            int nested_index = (int)c->function_count - 1;
            return discover_nested_functions(c, node->as.function.body,
                                              nested_index);
        }
        case N_BLOCK:
            for (size_t i = 0; i < node->as.list.count; ++i)
                if (!discover_nested_functions(c, node->as.list.items[i],
                                               parent_index)) return false;
            return true;
        case N_IF:
            return discover_nested_functions(c, node->as.conditional.then_branch,
                                             parent_index) &&
                   discover_nested_functions(c, node->as.conditional.else_branch,
                                             parent_index);
        case N_WHILE:
            return discover_nested_functions(c, node->as.loop.body, parent_index);
        case N_FOR:
            return discover_nested_functions(c, node->as.for_loop.body, parent_index);
        case N_FOR_EACH:
            return discover_nested_functions(c, node->as.for_each.body, parent_index);
        default:
            return true;
    }
}
static bool add_global(Compiler *c, Name n, Token t, uint8_t *slot) {
    size_t mark = c->global_scope_depth
        ? c->global_scope_marks[c->global_scope_depth - 1] : 0;
    for (size_t i = mark; i < c->global_count; ++i)
        if (c->globals[i].active && same_name(c->globals[i].name, n))
            return fail(c, t, "duplicate variable in the same scope");
    if (c->global_count == COMPILER_GLOBAL_MAX)
        return fail(c, t, "too many global variables");
    *slot = (uint8_t)c->global_count;
    c->globals[c->global_count++] = (Global){n, *slot, true};
    return true;
}
static bool add_local(Compiler *c, Name n, Token t, uint8_t *slot) {
    size_t mark=(*current_scope_depth(c))?c->context->scope_marks[(*current_scope_depth(c))-1]:0;
    for(size_t i=mark;i<c->context->local_count;i++)
        if(same_name(c->context->locals[i].name,n))
            return fail(c,t,"duplicate local variable in the same scope");
    if(c->context->local_count==COMPILER_LOCAL_MAX)
        return fail(c,t,"too many local variables");
    *slot=(uint8_t)c->context->local_count;
    c->context->locals[c->context->local_count++]=(Local){n,*slot,(*current_scope_depth(c))}; return true;
}
static bool enter_scope(Compiler *c, Token t) {
    size_t *depth = current_scope_depth(c);
    if (*depth == COMPILER_SCOPE_MAX)
        return fail(c, t, "compiler scope nesting limit exceeded");
    if (c->context)
        c->context->scope_marks[(*depth)++] = c->context->local_count;
    else
        c->global_scope_marks[(*depth)++] = c->global_count;
    return true;
}
static bool leave_scope(Compiler *c, Token t) {
    size_t *depth = current_scope_depth(c);
    if (!*depth) return true;
    size_t mark = c->context
        ? c->context->scope_marks[--(*depth)]
        : c->global_scope_marks[--(*depth)];
    if (c->context) {
        while (c->context->local_count > mark) {
            if (!emit(c, OP_POP, t)) return false;
            --c->context->local_count;
        }
    } else {
        for (size_t i = mark; i < c->global_count; ++i) {
            if (c->globals[i].active &&
                !emit_slot(c, OP_CLEAR_GLOBAL, c->globals[i].slot, t)) return false;
            c->globals[i].active = false;
        }
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
static bool emit_loop(Compiler*c,size_t target,Token t) {
    if(!emit(c,OP_LOOP,t))return false;
    size_t distance=c->chunk->count+2-target;
    if(distance>UINT16_MAX)return fail(c,t,"loop body exceeds 65535 bytes");
    return emit_byte(c,(uint8_t)(distance&0xffu),t)&&
           emit_byte(c,(uint8_t)(distance>>8),t);
}
static bool emit_loop_cleanup(Compiler *c, size_t variable_count, Token t) {
    if ((c->context != NULL)) {
        for (size_t i = c->context->local_count; i > variable_count; --i)
            if (!emit(c, OP_POP, t)) return false;
    } else {
        for (size_t i = variable_count; i < c->global_count; ++i)
            if (c->globals[i].active && !emit_slot(c, OP_CLEAR_GLOBAL, c->globals[i].slot, t)) return false;
    }
    return true;
}
static bool expression(Compiler*c,Node*n);
static bool compile_string(Compiler *c, Node *n) {
    const char *raw = n->token.start + 1;
    size_t raw_length = n->token.length - 2;
    char *decoded = malloc(raw_length + 1);
    if (!decoded) return fail(c, n->token, "out of memory decoding string literal");
    size_t length = 0;
    for (size_t i = 0; i < raw_length; ++i) {
        char ch = raw[i];
        if (ch == '\\') {
            if (++i >= raw_length) { free(decoded); return fail(c, n->token, "invalid string escape"); }
            switch (raw[i]) {
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case '\\': ch = '\\'; break;
                case '"': ch = '"'; break;
                case '0': ch = '\0'; break;
                default: free(decoded); return fail(c, n->token, "unknown string escape");
            }
        }
        decoded[length++] = ch;
    }
    String *string = string_new(decoded, length);
    free(decoded);
    if (!string) return fail(c, n->token, "out of memory creating string constant");
    uint16_t index;
    if (!chunk_add_constant(c->chunk, value_string(string), &index)) {
        value_string_free(string);
        return fail(c, n->token, "cannot add string constant");
    }
    return emit(c, OP_CONSTANT, n->token) &&
           emit_byte(c, (uint8_t)(index & 0xffu), n->token) &&
           emit_byte(c, (uint8_t)(index >> 8), n->token);
}
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
        case N_NIL:return emit(c,OP_NIL,n->token);
        case N_STRING:return compile_string(c,n);
        case N_IDENT: {
            int slot=(c->context != NULL)?find_local(c,n->as.name):-1;
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
        case N_ARRAY:
            if (!emit(c, OP_NEW_ARRAY, n->token)) return false;
            for (size_t i = 0; i < n->as.list.count; ++i) {
                if (!expression(c, n->as.list.items[i]) ||
                    !emit(c, OP_ARRAY_LITERAL_ADD, n->token)) return false;
            }
            return true;
        case N_DICTIONARY:
            if (!emit(c, OP_NEW_DICTIONARY, n->token)) return false;
            for (size_t i = 0; i < n->as.list.count; i += 2) {
                if (!expression(c, n->as.list.items[i]) ||
                    !expression(c, n->as.list.items[i + 1]) ||
                    !emit(c, OP_DICTIONARY_LITERAL_ADD, n->token)) return false;
            }
            return true;
        case N_INDEX:
            return expression(c,n->as.index.target) &&
                   expression(c,n->as.index.index) &&
                   emit(c,OP_GET_INDEX,n->token);
        case N_CALL: {
            if(n->as.call.name.length==9&&!memcmp(n->as.call.name.name,"new_array",9)) {
                if(n->as.call.arg_count!=0)
                    return fail_arity(c,n->token,n->as.call.name,0,n->as.call.arg_count);
                return emit(c,OP_NEW_ARRAY,n->token);
            }
            if(n->as.call.name.length==5&&!memcmp(n->as.call.name.name,"print",5)) {
                if(n->as.call.arg_count!=1)return fail_arity(c,n->token,n->as.call.name,1,n->as.call.arg_count);
                return expression(c,n->as.call.args[0])&&emit(c,OP_PRINT,n->token)&&emit(c,OP_NIL,n->token);
            }            if(n->as.call.name.length==6&&!memcmp(n->as.call.name.name,"length",6)) {
                if(n->as.call.arg_count!=1)return fail_arity(c,n->token,n->as.call.name,1,n->as.call.arg_count);
                return expression(c,n->as.call.args[0])&&emit(c,OP_LENGTH,n->token);
            }
            if(n->as.call.name.length==4&&!memcmp(n->as.call.name.name,"push",4)) {
                if(n->as.call.arg_count!=2)return fail_arity(c,n->token,n->as.call.name,2,n->as.call.arg_count);
                return expression(c,n->as.call.args[0])&&expression(c,n->as.call.args[1])&&emit(c,OP_ARRAY_PUSH,n->token);
            }
            if(n->as.call.name.length==3&&!memcmp(n->as.call.name.name,"pop",3)) {
                if(n->as.call.arg_count!=1)return fail_arity(c,n->token,n->as.call.name,1,n->as.call.arg_count);
                return expression(c,n->as.call.args[0])&&emit(c,OP_ARRAY_POP,n->token);
            }
            if(n->as.call.name.length==4&&!memcmp(n->as.call.name.name,"keys",4)) {
                if(n->as.call.arg_count!=1)return fail_arity(c,n->token,n->as.call.name,1,n->as.call.arg_count);
                return expression(c,n->as.call.args[0])&&emit(c,OP_DICTIONARY_KEYS,n->token);
            }
            if(n->as.call.name.length==6&&!memcmp(n->as.call.name.name,"values",6)) {
                if(n->as.call.arg_count!=1)return fail_arity(c,n->token,n->as.call.name,1,n->as.call.arg_count);
                return expression(c,n->as.call.args[0])&&emit(c,OP_DICTIONARY_VALUES,n->token);
            }
            if(n->as.call.name.length==7&&!memcmp(n->as.call.name.name,"has_key",7)) {
                if(n->as.call.arg_count!=2)return fail_arity(c,n->token,n->as.call.name,2,n->as.call.arg_count);
                return expression(c,n->as.call.args[0])&&expression(c,n->as.call.args[1])&&emit(c,OP_DICTIONARY_HAS_KEY,n->token);
            }
            if(n->as.call.name.length==6&&!memcmp(n->as.call.name.name,"remove",6)) {
                if(n->as.call.arg_count!=2)return fail_arity(c,n->token,n->as.call.name,2,n->as.call.arg_count);
                return expression(c,n->as.call.args[0])&&expression(c,n->as.call.args[1])&&emit(c,OP_ARRAY_REMOVE,n->token);
            }
            if(n->as.call.name.length==6&&!memcmp(n->as.call.name.name,"delete",6)) {
                if(n->as.call.arg_count!=2)return fail_arity(c,n->token,n->as.call.name,2,n->as.call.arg_count);
                return expression(c,n->as.call.args[0])&&expression(c,n->as.call.args[1])&&emit(c,OP_DICTIONARY_DELETE,n->token);
            }
            if(n->as.call.name.length==9&&!memcmp(n->as.call.name.name,"substring",9)) {
                if(n->as.call.arg_count!=3)return fail_arity(c,n->token,n->as.call.name,3,n->as.call.arg_count);
                for(size_t i=0;i<3;i++)if(!expression(c,n->as.call.args[i]))return false;
                return emit(c,OP_SUBSTRING,n->token);
            }
            int fn=find_function(c,n->as.call.name);
            if(fn<0)return fail(c,n->token,"unknown function in bytecode compiler");
            if(n->as.call.arg_count!=c->functions[fn].arity)return fail_arity(c,n->token,n->as.call.name,c->functions[fn].arity,n->as.call.arg_count);
            for(size_t i=0;i<n->as.call.arg_count;i++)if(!expression(c,n->as.call.args[i]))return false;
            return emit_slot(c,OP_CALL,(uint8_t)fn,n->token);
        }
        case N_ASSIGN: {
            Node*t=n->as.assign.target;
            bool compound = n->token.type != T_ASSIGN;
            OpCode arithmetic;
            switch(n->token.type) {
                case T_PLUS_EQUAL: arithmetic=OP_ADD; break;
                case T_MINUS_EQUAL: arithmetic=OP_SUBTRACT; break;
                case T_STAR_EQUAL: arithmetic=OP_MULTIPLY; break;
                case T_SLASH_EQUAL: arithmetic=OP_DIVIDE; break;
                default: arithmetic=OP_NIL; break;
            }
            if(t->kind==N_INDEX) {
                if(!expression(c,t->as.index.target) ||
                   !expression(c,t->as.index.index)) return false;
                if(compound) {
                    if(!emit(c,OP_GET_INDEX_KEEP,n->token) ||
                       !expression(c,n->as.assign.value) ||
                       !emit(c,arithmetic,n->token)) return false;
                } else if(!expression(c,n->as.assign.value)) return false;
                return emit(c,OP_SET_INDEX,n->token);
            }
            if(t->kind!=N_IDENT)return fail(c,n->token,"invalid assignment target");
            int slot=(c->context != NULL)?find_local(c,t->as.name):-1;
            if(slot>=0) {
                if(compound && !emit_slot(c,OP_GET_LOCAL,(uint8_t)slot,n->token))return false;
                if(!expression(c,n->as.assign.value))return false;
                if(compound && !emit(c,arithmetic,n->token))return false;
                return emit_slot(c,OP_SET_LOCAL,(uint8_t)slot,n->token);
            }
            slot=find_global(c,t->as.name);
            if(slot<0)return fail(c,n->token,"assignment to unknown variable");
            if(compound && !emit_slot(c,OP_GET_GLOBAL,(uint8_t)slot,n->token))return false;
            if(!expression(c,n->as.assign.value))return false;
            if(compound && !emit(c,arithmetic,n->token))return false;
            return emit_slot(c,OP_SET_GLOBAL,(uint8_t)slot,n->token);
        }
        default:return fail(c,n->token,"syntax is not executable yet");
    }
}
static bool statement(Compiler*c,Node*n) {
    if(!n)return true;
    switch(n->kind) {
        case N_FUNCTION:return true;
        case N_VAR: {
            if((c->context != NULL)) {
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
        case N_WHILE: {
            if ((*current_loop_depth(c)) == COMPILER_SCOPE_MAX)
                return fail(c, n->token, "loop nesting limit exceeded");
            Loop *loop = &current_loops(c)[(*current_loop_depth(c))++];
            memset(loop, 0, sizeof *loop);
            loop->start = c->chunk->count;
            loop->break_variable_count = (c->context != NULL) ? c->context->local_count : c->global_count;
            loop->continue_variable_count = loop->break_variable_count;
            size_t exit;
            bool ok = expression(c, n->as.loop.condition) &&
                      emit_jump(c, OP_JUMP_IF_FALSE, n->token, &exit) &&
                      statement(c, n->as.loop.body) &&
                      emit_loop(c, loop->start, n->token) &&
                      patch_jump(c, exit, n->token);
            for (size_t i = 0; ok && i < loop->break_count; ++i)
                ok = patch_jump(c, loop->break_jumps[i], n->token);
            --(*current_loop_depth(c));
            return ok;
        }
        case N_FOR: {
            if((*current_loop_depth(c))==COMPILER_SCOPE_MAX)return fail(c,n->token,"loop nesting limit exceeded");
            if(!enter_scope(c,n->token))return false;
            Loop *loop=&current_loops(c)[(*current_loop_depth(c))++];memset(loop,0,sizeof *loop);
            loop->break_variable_count=(c->context != NULL)?c->context->local_count:c->global_count;
            static const char hidden_name[]="\001for_end";
            Name hidden={hidden_name,sizeof hidden_name-1};
            uint8_t var_slot=0,end_slot=0;
            bool ok=expression(c,n->as.for_loop.start);
            if(ok&&(c->context != NULL))ok=add_local(c,n->as.for_loop.name,n->token,&var_slot);
            else if(ok)ok=add_global(c,n->as.for_loop.name,n->token,&var_slot)&&emit_slot(c,OP_DEFINE_GLOBAL,var_slot,n->token);
            if(ok)ok=expression(c,n->as.for_loop.end);
            if(ok&&(c->context != NULL))ok=add_local(c,hidden,n->token,&end_slot);
            else if(ok)ok=add_global(c,hidden,n->token,&end_slot)&&emit_slot(c,OP_DEFINE_GLOBAL,end_slot,n->token);
            loop->continue_variable_count=(c->context != NULL)?c->context->local_count:c->global_count;
            loop->continue_to_increment=true;loop->start=c->chunk->count;
            size_t exit=0;
            if(ok)ok=(c->context != NULL)?emit_slot(c,OP_GET_LOCAL,var_slot,n->token)&&emit_slot(c,OP_GET_LOCAL,end_slot,n->token):emit_slot(c,OP_GET_GLOBAL,var_slot,n->token)&&emit_slot(c,OP_GET_GLOBAL,end_slot,n->token);
            if(ok)ok=emit(c,OP_LESS_EQUAL,n->token)&&emit_jump(c,OP_JUMP_IF_FALSE,n->token,&exit)&&statement(c,n->as.for_loop.body);
            for(size_t i=0;ok&&i<loop->continue_count;i++)ok=patch_jump(c,loop->continue_jumps[i],n->token);
            if(ok)ok=(c->context != NULL)?emit_slot(c,OP_GET_LOCAL,var_slot,n->token):emit_slot(c,OP_GET_GLOBAL,var_slot,n->token);
            if(ok)ok=chunk_write_constant(c->chunk,value_number(1),n->token.line)||fail(c,n->token,"cannot add for-loop increment constant");
            if(ok)ok=emit(c,OP_ADD,n->token);
            if(ok)ok=(c->context != NULL)?emit_slot(c,OP_SET_LOCAL,var_slot,n->token):emit_slot(c,OP_SET_GLOBAL,var_slot,n->token);
            if(ok)ok=emit(c,OP_POP,n->token)&&emit_loop(c,loop->start,n->token)&&patch_jump(c,exit,n->token);
            --(*current_loop_depth(c));
            bool cleaned=leave_scope(c,n->token);
            for(size_t i=0;ok&&cleaned&&i<loop->break_count;i++)ok=patch_jump(c,loop->break_jumps[i],n->token);
            return ok&&cleaned;
        }
        case N_FOR_EACH: {
            if ((*current_loop_depth(c)) == COMPILER_SCOPE_MAX)
                return fail(c, n->token, "loop nesting limit exceeded");
            if (!enter_scope(c, n->token)) return false;
            Loop *loop = &current_loops(c)[(*current_loop_depth(c))++];
            memset(loop, 0, sizeof *loop);
            loop->break_variable_count = (c->context != NULL) ? c->context->local_count : c->global_count;
            static const char array_name[] = "\001for_array";
            static const char index_name[] = "\001for_index";
            static const char count_name[] = "\001for_count";
            Name array_hidden = {array_name, sizeof array_name - 1};
            Name index_hidden = {index_name, sizeof index_name - 1};
            Name count_hidden = {count_name, sizeof count_name - 1};
            uint8_t array_slot = 0, index_slot = 0, count_slot = 0, value_slot = 0;
            bool ok = expression(c, n->as.for_each.iterable) &&
                      emit(c, OP_ITER_INIT, n->token);
            if (ok && (c->context != NULL)) ok = add_local(c, array_hidden, n->token, &array_slot);
            else if (ok) ok = add_global(c, array_hidden, n->token, &array_slot) &&
                              emit_slot(c, OP_DEFINE_GLOBAL, array_slot, n->token);
            if (ok) ok = chunk_write_constant(c->chunk, value_number(0), n->token.line) ||
                         fail(c, n->token, "cannot add iterator index constant");
            if (ok && (c->context != NULL)) ok = add_local(c, index_hidden, n->token, &index_slot);
            else if (ok) ok = add_global(c, index_hidden, n->token, &index_slot) &&
                              emit_slot(c, OP_DEFINE_GLOBAL, index_slot, n->token);
            if (ok) ok = (c->context != NULL)
                ? emit_slot(c, OP_GET_LOCAL, array_slot, n->token)
                : emit_slot(c, OP_GET_GLOBAL, array_slot, n->token);
            if (ok) ok = emit(c, OP_LENGTH, n->token);
            if (ok && (c->context != NULL)) ok = add_local(c, count_hidden, n->token, &count_slot);
            else if (ok) ok = add_global(c, count_hidden, n->token, &count_slot) &&
                              emit_slot(c, OP_DEFINE_GLOBAL, count_slot, n->token);
            if (ok) ok = emit(c, OP_NIL, n->token);
            if (ok && (c->context != NULL)) ok = add_local(c, n->as.for_each.name, n->token, &value_slot);
            else if (ok) ok = add_global(c, n->as.for_each.name, n->token, &value_slot) &&
                              emit_slot(c, OP_DEFINE_GLOBAL, value_slot, n->token);
            loop->continue_variable_count = (c->context != NULL) ? c->context->local_count : c->global_count;
            loop->continue_to_increment = true;
            loop->start = c->chunk->count;
            size_t exit = 0;
            if (ok) ok = (c->context != NULL)
                ? emit_slot(c, OP_GET_LOCAL, index_slot, n->token) &&
                  emit_slot(c, OP_GET_LOCAL, count_slot, n->token)
                : emit_slot(c, OP_GET_GLOBAL, index_slot, n->token) &&
                  emit_slot(c, OP_GET_GLOBAL, count_slot, n->token);
            if (ok) ok = emit(c, OP_LESS, n->token) &&
                         emit_jump(c, OP_JUMP_IF_FALSE, n->token, &exit);
            if (ok) ok = (c->context != NULL)
                ? emit_slot(c, OP_GET_LOCAL, array_slot, n->token) &&
                  emit_slot(c, OP_GET_LOCAL, index_slot, n->token)
                : emit_slot(c, OP_GET_GLOBAL, array_slot, n->token) &&
                  emit_slot(c, OP_GET_GLOBAL, index_slot, n->token);
            if (ok) ok = emit(c, OP_GET_INDEX, n->token) &&
                         ((c->context != NULL)
                            ? emit_slot(c, OP_SET_LOCAL, value_slot, n->token)
                            : emit_slot(c, OP_SET_GLOBAL, value_slot, n->token)) &&
                         emit(c, OP_POP, n->token) &&
                         statement(c, n->as.for_each.body);
            for (size_t i = 0; ok && i < loop->continue_count; ++i)
                ok = patch_jump(c, loop->continue_jumps[i], n->token);
            if (ok) ok = (c->context != NULL)
                ? emit_slot(c, OP_GET_LOCAL, index_slot, n->token)
                : emit_slot(c, OP_GET_GLOBAL, index_slot, n->token);
            if (ok) ok = chunk_write_constant(c->chunk, value_number(1), n->token.line) ||
                         fail(c, n->token, "cannot add iterator increment constant");
            if (ok) ok = emit(c, OP_ADD, n->token) &&
                         ((c->context != NULL)
                            ? emit_slot(c, OP_SET_LOCAL, index_slot, n->token)
                            : emit_slot(c, OP_SET_GLOBAL, index_slot, n->token)) &&
                         emit(c, OP_POP, n->token) &&
                         emit_loop(c, loop->start, n->token) &&
                         patch_jump(c, exit, n->token);
            --(*current_loop_depth(c));
            bool cleaned = leave_scope(c, n->token);
            for (size_t i = 0; ok && cleaned && i < loop->break_count; ++i)
                ok = patch_jump(c, loop->break_jumps[i], n->token);
            return ok && cleaned;
        }
        case N_BREAK: {
            if (!(*current_loop_depth(c))) return fail(c, n->token, "break outside loop");
            Loop *loop = &current_loops(c)[(*current_loop_depth(c)) - 1];
            if (loop->break_count == sizeof loop->break_jumps / sizeof loop->break_jumps[0])
                return fail(c, n->token, "too many break statements in loop");
            if (!emit_loop_cleanup(c, loop->break_variable_count, n->token)) return false;
            return emit_jump(c, OP_JUMP, n->token,
                             &loop->break_jumps[loop->break_count++]);
        }
        case N_CONTINUE: {
            if (!(*current_loop_depth(c))) return fail(c,n->token,"continue outside loop");
            Loop *loop=&current_loops(c)[(*current_loop_depth(c))-1];
            if (!emit_loop_cleanup(c,loop->continue_variable_count,n->token)) return false;
            if (!loop->continue_to_increment) return emit_loop(c,loop->start,n->token);
            if (loop->continue_count==256) return fail(c,n->token,"too many continue statements in loop");
            return emit_jump(c,OP_JUMP,n->token,&loop->continue_jumps[loop->continue_count++]);
        }
        case N_RETURN:
            if(!(c->context != NULL))return fail(c,n->token,"return outside function");
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
    /* Register root functions first so their existing indices remain stable. */
    for (size_t i = 0; i < root->as.list.count; ++i) {
        Node *item = root->as.list.items[i];
        if (item->kind != N_FUNCTION) continue;
        for (size_t j = 0; j < c.function_count; ++j)
            if (c.functions[j].top_level &&
                same_name(c.functions[j].name, item->as.function.name))
                return fail(&c, item->token, "duplicate function");
        if (!register_function(&c, item, -1, true)) return false;
    }
    /* Then discover nested declarations under each root function. */
    for (size_t i = 0; i < c.function_count; ++i) {
        if (!c.functions[i].top_level) continue;
        Node *declaration = c.functions[i].declaration;
        if (!discover_nested_functions(&c, declaration->as.function.body,
                                       (int)i)) return false;
    }
    /* Reserve prototype indices now. Nested offsets stay placeholders until
       the later closure-compilation stage emits their bodies. */
    for (size_t i = 0; i < c.function_count; ++i) {
        Function *function = &c.functions[i];
        uint8_t index;
        if (!chunk_add_function(chunk, 0, function->arity,
                                function->name.name, function->name.length,
                                &index) || index != function->index)
            return fail(&c, function->declaration->token,
                        "cannot register function prototype");
    }
    for(size_t i=0;i<root->as.list.count;i++)if(root->as.list.items[i]->kind!=N_FUNCTION)
        if(!statement(&c,root->as.list.items[i]))return false;
    if(!emit(&c,OP_NIL,root->token)||!emit(&c,OP_RETURN,root->token))return false;
    for (size_t i = 0; i < c.function_count; ++i) {
        Function *function = &c.functions[i];
        if (!function->top_level) continue;
        Node *f = function->declaration;
        chunk->functions[function->index].offset = chunk->count;
        CompilerContext function_context = {
            .parent = NULL,
            .function_index = function->index
        };
        c.context = &function_context;
        for (size_t p = 0; p < f->as.function.param_count; ++p) {
            uint8_t slot;
            if (!add_local(&c, f->as.function.params[p], f->token, &slot))
                return false;
            (void)slot;
        }
        if (!statement(&c, f->as.function.body)) return false;
        if (!emit(&c, OP_NIL, f->token) ||
            !emit(&c, OP_RETURN, f->token)) return false;
        c.context = NULL;
    }
    return true;
}
