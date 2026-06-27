#ifndef AST_H
#define AST_H

#include <stdbool.h>

// 1. Enumeration used as an identity badge (the tag)
typedef enum {
    NODE_LITERAL,
    NODE_BINARY_OP,
    NODE_UNARY_OP,   
    NODE_VAR_DECL
} ASTNodeType;

// Representation of precedence levels
typedef enum {
  PREC_NONE,
  PREC_TERM,   // + -
  PREC_FACTOR, // * /
  PREC_UNARY,  // - !
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
    const char* identifier;     // Variable name (zero-copy or duplicated)
    int length;                 // Lexeme string length tracking
    ASTNode* initializer;       // Initialization expression (the child)
} VarDeclData;

typedef struct
{
  const char* op;
  ASTNode* operand;  // operand can be a LiteralData (-3) or an entire expression ( -(3 + 2) )
} UnaryOpData;


// 3. The main polymorphic structure
struct ASTNode {
    ASTNodeType type;  // The discriminant (the tag)
    union {
        LiteralData literal;
        BinaryOpData binary_op;
        VarDeclData var_decl;
        UnaryOpData unary_op;
    } as; // 'as' gives clear access: node->as.literal.value
};

// Factory function signatures
ASTNode *allocateLiteralNode(double value);
ASTNode *allocateBinaryOpNode(const char *op, ASTNode *left, ASTNode *right);
ASTNode *allocateUnaryOpNode(const char *op, ASTNode *operand);
ASTNode *allocateVarDeclNode(const char *name, int length, ASTNode *value);
void freeAST(ASTNode *node);

#endif // AST_H
