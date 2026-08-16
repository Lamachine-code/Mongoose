#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <stdbool.h>

/*
 * Forward declaration.
 *
 * The Environment only stores a pointer to an ASTNode.
 * It does not need the complete ast.h definition here.
 */
typedef struct ASTNode ASTNode;

/*
 * Describes what kind of declaration a Symbol represents.
 */
typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER
} SymbolKind;

/*
 * Compiler information associated with one declaration.
 *
 * Ownership:
 * - name: non-owning pointer into the original source buffer.
 * - declaration: non-owning pointer to an AST node.
 */
typedef struct {
    const char* name;
    int length;

    SymbolKind kind;

    ASTNode* declaration;
} Symbol;

/*
 * Represents one lexical scope.
 *
 * Each Environment owns its symbols array.
 * The parent pointer is non-owning.
 */
typedef struct Environment {
    struct Environment* parent;

    Symbol* symbols;

    int count;
    int capacity;
} Environment;


/*
 * Create a new lexical environment.
 *
 * parent == NULL represents the global environment.
 */
Environment* initEnvironment(Environment* parent);

/*
 * Destroy an Environment and the symbol array it owns.
 *
 * The parent Environment, identifier strings, and AST nodes
 * are not destroyed.
 */
void freeEnvironment(Environment* environment);

/*
 * Define a symbol in the current scope.
 *
 * Returns:
 *   true  -> symbol successfully defined
 *   false -> invalid input, duplicate declaration,
 *            or allocation failure.
 *
 * Duplicate checking is LOCAL to the supplied Environment.
 * This allows shadowing in child environments.
 */
bool defineSymbol(
    Environment* environment,
    const char* name,
    int length,
    SymbolKind kind,
    ASTNode* declaration
);

/*
 * Resolve a symbol starting from the supplied Environment.
 *
 * The current scope is searched first, followed by its
 * parent, then the parent's parent, etc.
 *
 * Returns:
 *   Symbol* -> resolved declaration
 *   NULL    -> symbol does not exist in any visible scope
 */
Symbol* resolveSymbol(
    Environment* environment,
    const char* name,
    int length
);

#endif /* ENVIRONMENT_H */