#include "mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Lexical scopes and context-sensitive control-flow validation. */
typedef struct Scope {
    struct Scope *parent;
    Name *names;
    int *kinds;
    size_t count, capacity;
} Scope;

static bool same(Name a, Name b) {
    return a.length == b.length && memcmp(a.name, b.name, a.length) == 0;
}

static int lookup(Scope *scope, Name name) {
    for (; scope; scope = scope->parent)
        for (size_t i = 0; i < scope->count; ++i)
            if (same(scope->names[i], name)) return scope->kinds[i];
    return 0;
}

static bool define(Scope *scope, Name name, int kind, Token token, Diagnostic *error) {
    for (size_t i = 0; i < scope->count; ++i) {
        if (same(scope->names[i], name)) {
            snprintf(error->message, sizeof error->message,
                     "duplicate declaration '%.*s'", (int)name.length, name.name);
            error->line = token.line; error->column = token.column;
            return false;
        }
    }
    if (scope->count == scope->capacity) {
        size_t capacity = scope->capacity ? scope->capacity * 2 : 8;
        Name *names = realloc(scope->names, capacity * sizeof *names);
        if (!names) return false;
        scope->names = names;
        int *kinds = realloc(scope->kinds, capacity * sizeof *kinds);
        if (!kinds) return false;
        scope->kinds = kinds;
        scope->capacity = capacity;
    }
    scope->names[scope->count] = name;
    scope->kinds[scope->count++] = kind;
    return true;
}

static bool sem(Node *node, Scope *scope, bool in_function,
                unsigned loop_depth, Diagnostic *error);

static bool semblock(Node *node, Scope *parent, bool in_function,
                     unsigned loop_depth, Diagnostic *error) {
    Scope local = {.parent = parent};
    for (size_t i = 0; i < node->as.list.count; ++i) {
        Node *item = node->as.list.items[i];
        if (item->kind == N_FUNCTION &&
            !define(&local, item->as.function.name, 2, item->token, error))
            goto failure;
    }
    for (size_t i = 0; i < node->as.list.count; ++i)
        if (!sem(node->as.list.items[i], &local, in_function, loop_depth, error))
            goto failure;
    free(local.names); free(local.kinds);
    return true;
failure:
    free(local.names); free(local.kinds);
    return false;
}

static bool sem(Node *node, Scope *scope, bool in_function,
                unsigned loop_depth, Diagnostic *error) {
    if (!node) return true;
    switch (node->kind) {
        case N_PROGRAM:
        case N_BLOCK:
            return semblock(node, scope, in_function, loop_depth, error);
        case N_VAR:
            return sem(node->as.var.value, scope, in_function, loop_depth, error) &&
                   define(scope, node->as.var.name, 1, node->token, error);
        case N_IDENT:
            if (!lookup(scope, node->as.name)) {
                snprintf(error->message, sizeof error->message,
                         "undefined identifier '%.*s'",
                         (int)node->as.name.length, node->as.name.name);
                error->line = node->token.line; error->column = node->token.column;
                return false;
            }
            return true;
        case N_FUNCTION: {
            Scope local = {.parent = scope};
            for (size_t i = 0; i < node->as.function.param_count; ++i) {
                if (!define(&local, node->as.function.params[i], 1, node->token, error)) {
                    free(local.names); free(local.kinds); return false;
                }
            }
            /* A function declared inside a loop cannot break out of its enclosing function's loop. */
            bool ok = semblock(node->as.function.body, &local, true, 0, error);
            free(local.names); free(local.kinds);
            return ok;
        }
        case N_IF:
            return sem(node->as.conditional.condition, scope, in_function, loop_depth, error) &&
                   sem(node->as.conditional.then_branch, scope, in_function, loop_depth, error) &&
                   sem(node->as.conditional.else_branch, scope, in_function, loop_depth, error);
        case N_WHILE:
            return sem(node->as.loop.condition, scope, in_function, loop_depth, error) &&
                   sem(node->as.loop.body, scope, in_function, loop_depth + 1, error);
        case N_FOR: {
            if (!sem(node->as.for_loop.start, scope, in_function, loop_depth, error) ||
                !sem(node->as.for_loop.end, scope, in_function, loop_depth, error)) return false;
            Scope local = {.parent = scope};
            bool ok = define(&local, node->as.for_loop.name, 1, node->token, error) &&
                      sem(node->as.for_loop.body, &local, in_function, loop_depth + 1, error);
            free(local.names); free(local.kinds);
            return ok;
        }
        case N_BREAK:
        case N_CONTINUE:
            if (loop_depth == 0) {
                snprintf(error->message, sizeof error->message, "%s outside loop",
                         node->kind == N_BREAK ? "break" : "continue");
                error->line = node->token.line; error->column = node->token.column;
                return false;
            }
            return true;
        case N_RETURN:
            if (!in_function) {
                snprintf(error->message, sizeof error->message, "return outside function");
                error->line = node->token.line; error->column = node->token.column;
                return false;
            }
            return sem(node->as.ret.value, scope, in_function, loop_depth, error);
        case N_CALL:
            if (!((node->as.call.name.length == 9 &&
                   !memcmp(node->as.call.name.name, "new_array", 9)) ||
                  (node->as.call.name.length == 5 &&
                   !memcmp(node->as.call.name.name, "print", 5))) &&
                lookup(scope, node->as.call.name) != 2) {
                snprintf(error->message, sizeof error->message,
                         "unknown function '%.*s'", (int)node->as.call.name.length,
                         node->as.call.name.name);
                error->line = node->token.line; error->column = node->token.column;
                return false;
            }
            for (size_t i = 0; i < node->as.call.arg_count; ++i)
                if (!sem(node->as.call.args[i], scope, in_function, loop_depth, error))
                    return false;
            return true;
        case N_UNARY:
            return sem(node->as.unary.operand, scope, in_function, loop_depth, error);
        case N_BINARY:
            return sem(node->as.binary.left, scope, in_function, loop_depth, error) &&
                   sem(node->as.binary.right, scope, in_function, loop_depth, error);
        case N_INDEX:
            return sem(node->as.index.target, scope, in_function, loop_depth, error) &&
                   sem(node->as.index.index, scope, in_function, loop_depth, error);
        case N_ASSIGN:
            if (node->as.assign.target->kind != N_IDENT &&
                node->as.assign.target->kind != N_INDEX) {
                snprintf(error->message, sizeof error->message, "invalid assignment target");
                error->line = node->token.line; error->column = node->token.column;
                return false;
            }
            if (node->as.assign.target->kind == N_IDENT &&
                lookup(scope, node->as.assign.target->as.name) != 1) {
                snprintf(error->message, sizeof error->message, "assignment to undefined variable");
                error->line = node->token.line; error->column = node->token.column;
                return false;
            }
            return sem(node->as.assign.target, scope, in_function, loop_depth, error) &&
                   sem(node->as.assign.value, scope, in_function, loop_depth, error);
        default:
            return true;
    }
}

bool analyze_ast(const Ast *ast, Diagnostic *error) {
    if (!ast || !ast->root || !error) return false;
    memset(error, 0, sizeof *error);
    Scope globals = {0};
    bool ok = sem(ast->root, &globals, false, 0, error);
    free(globals.names); free(globals.kinds);
    return ok;
}
