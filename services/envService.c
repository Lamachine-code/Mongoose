#include "../types/environment.h"
#include "../utils/errorUtils.c"

#include <stdlib.h>
#include <string.h>

#define INITIAL_ENVIRONMENT_CAPACITY 8

/*
 * Ensure that the Environment has room for another Symbol.
 */
static bool ensureCapacity(Environment* environment) {
    if (environment->count < environment->capacity) {
        return true;
    }

    int newCapacity;

    if (environment->capacity == 0) {
        newCapacity = INITIAL_ENVIRONMENT_CAPACITY;
    } else {
        newCapacity = environment->capacity * 2;
    }

    Symbol* resized = realloc(
        environment->symbols,
        sizeof(Symbol) * newCapacity
    );

    if (resized == NULL) {
        return false;
    }

    environment->symbols = resized;
    environment->capacity = newCapacity;

    return true;
}

/*
 * Compare two zero-copy identifiers.
 *
 * The identifiers are represented as:
 *
 *     pointer + length
 *
 * rather than null-terminated strings.
 */
static bool identifierEquals(
  const char* left,
  int leftLength,
  const char* right,
  int rightLength
) {
  if (leftLength != rightLength) {
    return false;
  }

  return strncmp(left, right, leftLength) == 0;
}

/*
 * Search only the current Environment.
 *
 * This function intentionally does NOT inspect parent
 * environments.
 *
 * It is therefore suitable for duplicate-declaration checks.
 */
static Symbol* findLocalSymbol(
    Environment* environment,
    const char* name,
    int length
) {
    for (int i = 0; i < environment->count; i++) {
        Symbol* symbol = &environment->symbols[i];
        
        if (identifierEquals(
            name, 
            length,
            symbol->name,  
            symbol->length
        )) {
            return symbol;
        }
    }

    return NULL;
}

/*
 * Create a new lexical environment.
 */
Environment* initEnvironment(Environment* parent) {
    Environment* environment = ensureAlloc(malloc(sizeof(Environment)), "Environment");

    if (environment == NULL) {
        return NULL;
    }

    environment->parent = parent;
    environment->symbols = NULL;
    environment->count = 0;
    environment->capacity = 0;

    return environment;
}

/*
 * Destroy an Environment.
 */
void freeEnvironment(Environment* environment) {
    if (environment == NULL) {
        return;
    }

    /*
     * The Environment owns the symbols array.
     *
     * It does NOT own:
     * - symbol->name
     * - symbol->declaration
     * - environment->parent
     */
    free(environment->symbols);

    free(environment);
}

/*
 * Define a new Symbol in the current Environment.
 */
bool defineSymbol(
    Environment* environment,
    const char* name,
    int length,
    SymbolKind kind,
    ASTNode* declaration
) {
    if (environment == NULL || name == NULL || length <= 0) {
        return false;
    }    
    
    // 1. Checking whether the name already exists locally
    /*
     * Check only the current scope.
     *
     * This permits shadowing in nested scopes.
     */
    Symbol* symbol = findLocalSymbol(environment, name, length);
    if (symbol != NULL) {
        return false;
    }

    // 2. Growing the array
    /*
     * Make sure there is space for the new Symbol.
     */
    if (!ensureCapacity(environment)) {
        return false;
    }

    // 3. Constructing and store the Symbol
    /*
     * The new Symbol occupies the first unused slot.
     */
    Symbol* new_symbol = &environment->symbols[environment->count];
    new_symbol->declaration = declaration;
    new_symbol->kind = kind;
    new_symbol->length = length;
    new_symbol->name = name;

    // 4. Update count
    environment->count++;

    return true;
}

/* Recursive version (Risk of "stack overflow")
Symbol* resolveSymbol(
    Environment* environment,
    const char* name,
    int length
) {
    if (environment == NULL || name == NULL || length <= 0) {
        return NULL;
    }

    Symbol* symbol = findLocalSymbol(environment, name, length);
    if (symbol != NULL) {
        return symbol;
    }
    if (environment->parent == NULL) {
        return NULL;
    }

    return resolveSymbol(environment->parent, name, length);
}
*/

/*
 * Resolve a name through the lexical Environment chain.
 */
Symbol* resolveSymbol(
    Environment* environment,
    const char* name,
    int length
) {
    if (environment == NULL || name == NULL || length <= 0) {
        return NULL;
    }

    Environment* current = environment;

    // Iteration (loop) is safer and avoids stack overflow
    while (current != NULL) {
        Symbol* symbol = findLocalSymbol(
            current,
            name,
            length
        );

        if (symbol != NULL) {
            return symbol;
        }

        current = current->parent;
    }

    return NULL;
}
