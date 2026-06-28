#ifndef AST_H
#define AST_H

#include <stdbool.h>

// 1. Enumeration used as an identity badge (the tag)
typedef enum {
  NODE_LITERAL,
  NODE_BINARY_OP,
  NODE_UNARY_OP,   
  NODE_VAR_DECL,
  NODE_BLOCK,     /* <--- New Node Tag */
  NODE_IF         /* <--- New Node Tag */
} ASTNodeType;

// Representation of precedence levels
typedef enum {
  PREC_NONE,
  PREC_CONP,
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
  int length;     // Lexeme string length tracking
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

/* Block node data structure for sequential statements */
typedef struct {
    struct ASTNode** statements; // Dynamic array of ASTNode pointers
    int count;                  // Current statement index tracked
    int capacity;               // Allocated capacity of the array
} BlockData;

/* If statement structural routing pointers */
typedef struct {
    struct ASTNode* condition;   // Evaluated expression tree
    struct ASTNode* thenBranch;  // Rooted NODE_BLOCK for positive outcome
    struct ASTNode* elseBranch;  // Optional NODE_BLOCK or NULL
} IfData;


// 3. The main polymorphic structure
struct ASTNode {
    ASTNodeType type;  // The discriminant (the tag)
    union {
        LiteralData literal;
        BinaryOpData binary_op;
        VarDeclData var_decl;
        UnaryOpData unary_op;
        BlockData block;
        IfData if_stmt;
    } as; // 'as' gives clear access: node->as.literal.value
};

// Factory function signatures
ASTNode *allocateLiteralNode(double value);
ASTNode *allocateBinaryOpNode(const char *op, int length, ASTNode *left, ASTNode *right);
ASTNode *allocateUnaryOpNode(const char *op, ASTNode *operand);
ASTNode *allocateVarDeclNode(const char *name, int length, ASTNode *value);
ASTNode* allocateBlockNode(void);
ASTNode* allocateIfNode(ASTNode* condition, ASTNode* thenBranch, ASTNode* elseBranch);
void freeAST(ASTNode *node);

#endif // AST_H
