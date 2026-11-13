#include "compiler.h"
#include <stdio.h>
#include <string.h>

#define COMPILER_GLOBAL_MAX 256

typedef struct { Name name; uint8_t slot; } Global;
typedef struct {
    Chunk *chunk;
    Diagnostic *error;
    Global globals[COMPILER_GLOBAL_MAX];
    size_t global_count;
} Compiler;

static bool same_name(Name a, Name b) {
    return a.length == b.length && memcmp(a.name, b.name, a.length) == 0;
}

static bool fail(Compiler *c, Token token, const char *message) {
    snprintf(c->error->message, sizeof c->error->message, "%s", message);
    c->error->line = token.line;
    c->error->column = token.column;
    return false;
}

static bool emit(Compiler *c, OpCode op, Token token) {
    return chunk_write(c->chunk, (uint8_t)op, token.line) ||
           fail(c, token, "out of memory while emitting bytecode");
}

static bool emit_slot(Compiler *c, OpCode op, uint8_t slot, Token token) {
    return emit(c, op, token) &&
           (chunk_write(c->chunk, slot, token.line) ||
            fail(c, token, "out of memory while emitting bytecode"));
}

static int find_global(Compiler *c, Name name) {
    for (size_t i = 0; i < c->global_count; ++i)
        if (same_name(c->globals[i].name, name)) return (int)c->globals[i].slot;
    return -1;
}

static bool add_global(Compiler *c, Name name, Token token, uint8_t *slot) {
    int found = find_global(c, name);
    if (found >= 0) return fail(c, token, "duplicate global variable");
    if (c->global_count == COMPILER_GLOBAL_MAX)
        return fail(c, token, "too many global variables (maximum is 256)");
    *slot = (uint8_t)c->global_count;
    c->globals[c->global_count++] = (Global){name, *slot};
    return true;
}

static bool expression(Compiler *c, Node *node);

static bool binary(Compiler *c, Node *node) {
    if (!expression(c, node->as.binary.left) ||
        !expression(c, node->as.binary.right)) return false;
    OpCode op;
    switch (node->as.binary.op.type) {
        case T_PLUS: op = OP_ADD; break;
        case T_MINUS: op = OP_SUBTRACT; break;
        case T_STAR: op = OP_MULTIPLY; break;
        case T_SLASH: op = OP_DIVIDE; break;
        case T_FLOOR_DIV: op = OP_FLOOR_DIVIDE; break;
        case T_PERCENT: op = OP_MODULO; break;
        case T_POWER: op = OP_POWER; break;
        case T_EQ: op = OP_EQUAL; break;
        case T_NE:
            return emit(c, OP_EQUAL, node->token) && emit(c, OP_NOT, node->token);
        case T_GT: op = OP_GREATER; break;
        case T_LT: op = OP_LESS; break;
        default: return fail(c, node->as.binary.op, "this binary operator is not implemented by the bytecode compiler yet");
    }
    return emit(c, op, node->token);
}

static bool expression(Compiler *c, Node *node) {
    if (!node) return emit(c, OP_NIL, (Token){.line = 1});
    switch (node->kind) {
        case N_NUMBER:
            return chunk_write_constant(c->chunk, value_number(node->as.number), node->token.line) ||
                   fail(c, node->token, "unable to add numeric constant");
        case N_BOOL:
            return emit(c, node->as.boolean ? OP_TRUE : OP_FALSE, node->token);
        case N_IDENT: {
            int slot = find_global(c, node->as.name);
            if (slot < 0) return fail(c, node->token, "variable is not available in the runtime compiler");
            return emit_slot(c, OP_GET_GLOBAL, (uint8_t)slot, node->token);
        }
        case N_UNARY:
            if (!expression(c, node->as.unary.operand)) return false;
            if (node->as.unary.op.type == T_MINUS) return emit(c, OP_NEGATE, node->token);
            if (node->as.unary.op.type == T_NOT) return emit(c, OP_NOT, node->token);
            return fail(c, node->token, "unsupported unary operator");
        case N_BINARY: return binary(c, node);
        case N_CALL:
            if (node->as.call.name.length == 5 &&
                memcmp(node->as.call.name.name, "print", 5) == 0) {
                if (node->as.call.arg_count != 1)
                    return fail(c, node->token, "print() expects exactly one argument");
                return expression(c, node->as.call.args[0]) && emit(c, OP_PRINT, node->token) && emit(c, OP_NIL, node->token);
            }
            return fail(c, node->token, "function calls are not implemented by the runtime compiler yet");
        case N_ASSIGN: {
            Node *target = node->as.assign.target;
            if (target->kind != N_IDENT)
                return fail(c, node->token, "only variable assignment is supported by the runtime compiler");
            int slot = find_global(c, target->as.name);
            if (slot < 0) return fail(c, node->token, "assignment target is not a global variable");
            return expression(c, node->as.assign.value) &&
                   emit_slot(c, OP_SET_GLOBAL, (uint8_t)slot, node->token);
        }
        default: return fail(c, node->token, "this syntax is not implemented by the bytecode compiler yet");
    }
}

static bool statement(Compiler *c, Node *node) {
    if (!node) return true;
    switch (node->kind) {
        case N_VAR: {
            uint8_t slot;
            if (!expression(c, node->as.var.value) ||
                !add_global(c, node->as.var.name, node->token, &slot)) return false;
            return emit_slot(c, OP_DEFINE_GLOBAL, slot, node->token);
        }
        case N_FUNCTION: return fail(c, node->token, "function declarations are not executable yet");
        case N_IF: return fail(c, node->token, "if statements are not executable yet");
        case N_RETURN: return fail(c, node->token, "return statements are not executable yet");
        case N_BLOCK: return fail(c, node->token, "nested blocks are not executable yet");
        default:
            return expression(c, node) && emit(c, OP_POP, node->token);
    }
}

bool compile_ast(const Ast *ast, Chunk *chunk, Diagnostic *error) {
    if (!ast || !ast->root || !chunk || !error) return false;
    memset(error, 0, sizeof *error);
    Compiler compiler = {.chunk = chunk, .error = error};
    Node *root = ast->root;
    if (root->kind != N_PROGRAM) return fail(&compiler, root->token, "expected program root");
    for (size_t i = 0; i < root->as.list.count; ++i)
        if (!statement(&compiler, root->as.list.items[i])) return false;
    return emit(&compiler, OP_NIL, root->token) &&
           emit(&compiler, OP_RETURN, root->token);
}
