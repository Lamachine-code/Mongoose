#ifndef AST_H
#define AST_H

#include <stdbool.h>

// 1. Enumeration used as an identity badge (the tag)
typedef enum {
  NODE_LITERAL,
  NODE_BOOL,
  NODE_IDENTIFIER,
  NODE_BINARY_OP,
  NODE_UNARY_OP,
  NODE_VAR_DECL,
  NODE_ASSIGN,
  NODE_INDEX,
  NODE_CALL,
  NODE_IF,
  NODE_FUNCTION,
  NODE_BLOCK
} ASTNodeType;

// Representation of precedence levels
typedef enum {
  PREC_NONE,
  PREC_ASSIGN, // =
  PREC_OR,
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,   // + -
  PREC_FACTOR, // * /
  PREC_UNARY,  // - !
  PREC_CALL,
  PREC_PRIMARY
} Precedence;

// Forward declaration to allow pointers in the union
typedef struct ASTNode ASTNode;

// 2. Data structures specific to each node type
typedef struct {
  double value; // For numeric literals in the MVP
} LiteralData;

typedef struct {
  const char *op; // Operator (e.g. "+", "-", "*", "/")
  // char op[4];
  ASTNode *left;  // Pointer to the left child
  ASTNode *right; // Pointer to the right child
} BinaryOpData;

typedef struct {
  const char *identifier; // Variable name (zero-copy or duplicated)
  ASTNode *initializer;   // Initialization expression (the child)
} VarDeclData;

typedef struct {
  bool value;
} BoolData;

typedef struct {
  const char *name;
} IdentifierData;

typedef struct {
  const char *op;
  ASTNode *operand;
} UnaryOpData;

typedef struct {
  ASTNode *target;
  ASTNode *value;
} AssignData;

typedef struct {
  ASTNode *array;
  ASTNode *index;
} IndexData;

typedef struct {
  ASTNode *callee;
  ASTNode **args;
  int argCount;
} CallData;

typedef struct {
  ASTNode *condition;
  ASTNode *thenBranch;
  ASTNode *elseBranch;
} IfData;

typedef struct {
  const char *name;
  char **params;
  int paramCount;
  ASTNode *body;
} FunctionData;

typedef struct {
  ASTNode **statements;
  int count;
} BlockData;

// 3. The main polymorphic structure
struct ASTNode {
  ASTNodeType type; // The discriminant (the tag)
  union {
    LiteralData literal;
    BoolData boolean;
    IdentifierData identifier;
    BinaryOpData binary_op;
    UnaryOpData unary_op;
    VarDeclData var_decl;
    AssignData assign;
    IndexData index;
    CallData call;
    IfData if_stmt;
    FunctionData function;
    BlockData block;
  } as; // 'as' gives clear access: node->as.literal.value
};

// Factory function signatures
ASTNode *allocateLiteralNode(double value);
ASTNode *allocateBoolNode(bool value);
ASTNode *allocateIdentifierNode(const char *name);
ASTNode *allocateBinaryOpNode(const char *op, ASTNode *left, ASTNode *right);
ASTNode *allocateUnaryOpNode(const char *op, ASTNode *operand);
ASTNode *allocateVarDeclNode(const char *name, ASTNode *value);
ASTNode *allocateAssignNode(ASTNode *target, ASTNode *value);
ASTNode *allocateIndexNode(ASTNode *array, ASTNode *index);
ASTNode *allocateCallNode(ASTNode *callee, ASTNode **args, int argCount);
ASTNode *allocateIfNode(ASTNode *condition, ASTNode *thenBranch,
                        ASTNode *elseBranch);
ASTNode *allocateFunctionNode(const char *name, char **params, int paramCount,
                              ASTNode *body);
ASTNode *allocateBlockNode(ASTNode **statements, int count);
void freeAST(ASTNode *node);

#endif // AST_H
