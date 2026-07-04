#ifndef AST_H
#define AST_H

#include <stdbool.h>

// 1. Enumeration used as an identity badge (the tag)
typedef enum {
  NODE_LITERAL,
  NODE_BINARY_OP,
  NODE_UNARY_OP,   
  NODE_VAR_DECL,
  NODE_BLOCK,
  NODE_IF,
  NODE_IDENTIFIER,
  NODE_BOOL,
  NODE_FUNCTION_DECL,
  NODE_CALL,
  NODE_INDEX,
  NODE_RETURN
} ASTNodeType;

// Representation of precedence levels
typedef enum {
  PREC_NONE,
  PREC_LOGICAL_OP,  // 
  PREC_EQUALITY,
  PREC_COMP,
  PREC_TERM,    // + -
  PREC_FACTOR,  // * / %
  PREC_UNARY,   // - !
  PREC_POWER,   // ^
  PREC_POSTFIX, // () []
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
  int length;
  ASTNode* operand;  // operand can be a LiteralData (-3) or an entire expression ( -(3 + 2) )
} UnaryOpData;

/* Block node data structure for sequential statements */
typedef struct {
  struct ASTNode** statements;  // Dynamic array of ASTNode pointers
  int count;                    // Current statement index tracked
  int capacity;                 // Allocated capacity of the array
} BlockData;

typedef struct {
  const char* name;
  int length;
} IdentifierData;


/* If statement structural routing pointers */
typedef struct {
  struct ASTNode* condition;   // Evaluated expression tree
  struct ASTNode* thenBranch;  // Rooted NODE_BLOCK for positive outcome
  struct ASTNode* elseBranch;  // Optional NODE_BLOCK or NULL
} IfData;

typedef struct {
  bool value;
} BooleanData;

typedef struct {
  const char* name;           // Zero-copy identifier pointer, name of the function
  int length;                 // Length of the token/the function name
  const char** parameters;    // Dynamic or fixed array of parameter names
  int paramCount;             // Number of parameters exepted
  struct ASTNode* body;       // Pointer to a NODE_BLOCK node
} FuncDeclData;

// NODE_CALL payload
typedef struct {
  const char* name;             // Target function name (zero-copy)
  int length;                   // Length of the name
  struct ASTNode** arguments;   // Dynamic array of ASTNode* sub-expressions
  int argCount;                 // Number of arguments passed to the target function
  int argCapacity;              // Used if parsed using a growable vector
} callData;

// Array indexing
typedef struct {
  ASTNode* target;   // e.g. myArray
  ASTNode* index;    // e.g. 0, size, or an expression (e.g. myArray[3 + index])
} IndexData;

// Return Stmt
typedef struct {
  ASTNode* value; // expression being returned
} ReturnData;

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
    IdentifierData identifier;
    BooleanData boolean;
    FuncDeclData funcDecl;
    callData call;
    IndexData index_node;
    ReturnData return_node;
  } as; // 'as' gives clear access: node->as.literal.value
};

// Factory function signatures
ASTNode *allocateLiteralNode(double value);
ASTNode *allocateBinaryOpNode(const char *op, int length, ASTNode *left, ASTNode *right);
ASTNode *allocateUnaryOpNode(const char *op, int length, ASTNode *operand);
ASTNode *allocateVarDeclNode(const char *name, int length, ASTNode *value);
ASTNode* allocateBlockNode(void);
ASTNode* allocateIfNode(ASTNode* condition, ASTNode* thenBranch, ASTNode* elseBranch);
ASTNode* allocateIdentifierNode(Token token);
ASTNode* allocateBoolNode(Token token);
ASTNode* allocateFunctionDeclNode(const char* name, int length, const char** parameters, int paramCount, ASTNode* body);
ASTNode* allocateCallNode(const char* name, int length);
ASTNode* allocateIndexingNode(ASTNode* target, ASTNode* index);
ASTNode* allocateReturnNode(ASTNode* value);
void freeAST(ASTNode *node);

#endif // AST_H
