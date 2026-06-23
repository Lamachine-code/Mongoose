// ast.c
#include "../types/ast.h"
#include "../types/lexer.h"
#include "../utils/errorUtils.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Allocate memory to a node (ASTNode)
ASTNode *allocateLiteralNode(double value) {
  ASTNode *node =
      ensureAlloc((ASTNode *)malloc(sizeof(ASTNode)), "NODE_LITERAL");
  node->type = NODE_LITERAL;
  node->as.literal.value = value;
  return node;
}

ASTNode *allocateBinaryOpNode(const char *op, ASTNode *left, ASTNode *right) {
  ASTNode *node =
      ensureAlloc((ASTNode *)malloc(sizeof(ASTNode)), "NODE_BINARY_OP");
  node->type = NODE_BINARY_OP;
  node->as.binary_op.op = op;
  node->as.binary_op.left = left;
  node->as.binary_op.right = right;
  return node;
}

ASTNode *allocateVarDeclNode(const char *name, ASTNode *value) {
  ASTNode *node =
      ensureAlloc((ASTNode *)malloc(sizeof(ASTNode)), "NODE_VAR_DECL");
  node->type = NODE_VAR_DECL;
  node->as.var_decl.identifier = name;
  node->as.var_decl.initializer = value;
  return node;
}

ASTNode *allocateBoolNode(bool value) {
  ASTNode *node = ensureAlloc(malloc(sizeof(ASTNode)), "NODE_BOOL");
  node->type = NODE_BOOL;
  node->as.boolean.value = value;
  return node;
}

ASTNode *allocateIdentifierNode(const char *name) {
  ASTNode *node = ensureAlloc(malloc(sizeof(ASTNode)), "NODE_IDENTIFIER");
  node->type = NODE_IDENTIFIER;
  node->as.identifier.name = name;
  return node;
}

ASTNode *allocateUnaryOpNode(const char *op, ASTNode *operand) {
  ASTNode *node = ensureAlloc(malloc(sizeof(ASTNode)), "NODE_UNARY_OP");
  node->type = NODE_UNARY_OP;
  node->as.unary_op.op = op;
  node->as.unary_op.operand = operand;
  return node;
}

ASTNode *allocateAssignNode(ASTNode *target, ASTNode *value) {
  ASTNode *node = ensureAlloc(malloc(sizeof(ASTNode)), "NODE_ASSIGN");
  node->type = NODE_ASSIGN;
  node->as.assign.target = target;
  node->as.assign.value = value;
  return node;
}

ASTNode *allocateIndexNode(ASTNode *array, ASTNode *index) {
  ASTNode *node = ensureAlloc(malloc(sizeof(ASTNode)), "NODE_INDEX");
  node->type = NODE_INDEX;
  node->as.index.array = array;
  node->as.index.index = index;
  return node;
}

ASTNode *allocateCallNode(ASTNode *callee, ASTNode **args, int argCount) {
  ASTNode *node = ensureAlloc(malloc(sizeof(ASTNode)), "NODE_CALL");
  node->type = NODE_CALL;
  node->as.call.callee = callee;
  node->as.call.args = args;
  node->as.call.argCount = argCount;
  return node;
}

ASTNode *allocateIfNode(ASTNode *condition, ASTNode *thenBranch,
                        ASTNode *elseBranch) {
  ASTNode *node = ensureAlloc(malloc(sizeof(ASTNode)), "NODE_IF");
  node->type = NODE_IF;
  node->as.if_stmt.condition = condition;
  node->as.if_stmt.thenBranch = thenBranch;
  node->as.if_stmt.elseBranch = elseBranch;
  return node;
}

ASTNode *allocateFunctionNode(const char *name, char **params, int paramCount,
                              ASTNode *body) {
  ASTNode *node = ensureAlloc(malloc(sizeof(ASTNode)), "NODE_FUNCTION");
  node->type = NODE_FUNCTION;
  node->as.function.name = name;
  node->as.function.params = params;
  node->as.function.paramCount = paramCount;
  node->as.function.body = body;
  return node;
}

ASTNode *allocateBlockNode(ASTNode **statements, int count) {
  ASTNode *node = ensureAlloc(malloc(sizeof(ASTNode)), "NODE_BLOCK");
  node->type = NODE_BLOCK;
  node->as.block.statements = statements;
  node->as.block.count = count;
  return node;
}

void freeAST(ASTNode *node) {
  if (node == NULL)
    return;

  // Recursive cleanup based on the node's identity badge (the tag)
  switch (node->type) {
  case NODE_LITERAL:
    // No children or dynamically allocated internal pointers to free
    break;
  case NODE_BOOL:
    break;
  case NODE_IDENTIFIER:
    free((void *)node->as.identifier.name);
    break;
  case NODE_BINARY_OP:
    freeAST(node->as.binary_op.left);
    freeAST(node->as.binary_op.right);
    free((void *)node->as.binary_op.op);
    break;
  case NODE_UNARY_OP:
    freeAST(node->as.unary_op.operand);
    free((void *)node->as.unary_op.op);
    break;
  case NODE_VAR_DECL:
    freeAST(node->as.var_decl.initializer);
    free((void *)node->as.var_decl.identifier);
    break;
  case NODE_ASSIGN:
    freeAST(node->as.assign.target);
    freeAST(node->as.assign.value);
    break;
  case NODE_INDEX:
    freeAST(node->as.index.array);
    freeAST(node->as.index.index);
    break;
  case NODE_CALL:
    freeAST(node->as.call.callee);
    for (int i = 0; i < node->as.call.argCount; i++) {
      freeAST(node->as.call.args[i]);
    }
    free(node->as.call.args);
    break;
  case NODE_IF:
    freeAST(node->as.if_stmt.condition);
    freeAST(node->as.if_stmt.thenBranch);
    freeAST(node->as.if_stmt.elseBranch);
    break;
  case NODE_FUNCTION:
    for (int i = 0; i < node->as.function.paramCount; i++) {
      free(node->as.function.params[i]);
    }
    free(node->as.function.params);
    freeAST(node->as.function.body);
    free((void *)node->as.function.name);
    break;
  case NODE_BLOCK:
    for (int i = 0; i < node->as.block.count; i++) {
      freeAST(node->as.block.statements[i]);
    }
    free(node->as.block.statements);
    break;
  }

  // Finally, free the current parent node
  free(node);
}

// Small utility function to assign precedence to our tokens
Precedence getPrecedence(TokenType type) {
  switch (type) {
  case TOKEN_PLUS:
  case TOKEN_MINUS:
    return PREC_TERM;
  case TOKEN_STAR:
  case TOKEN_SLASH:
    return PREC_FACTOR;
  default:
    return PREC_NONE;
  }
}

// Debug helper to print the tree in a structural form
void printAST(ASTNode *node) {
  if (node == NULL)
    return;
  switch (node->type) {
  case NODE_LITERAL:
    printf("%g", node->as.literal.value);
    // printf(" ");
    // return;
    break;
  case NODE_BOOL:
    printf("%s", node->as.boolean.value ? "true" : "false");
    break;
  case NODE_IDENTIFIER:
    printf("%s", node->as.identifier.name);
    break;
  case NODE_BINARY_OP:
    printf("(%s ", node->as.binary_op.op);
    printAST(node->as.binary_op.left);
    printf(" ");
    printAST(node->as.binary_op.right);
    printf(")");
    break;
  case NODE_UNARY_OP:
    printf("(%s ", node->as.unary_op.op);
    printAST(node->as.unary_op.operand);
    printf(")");
    break;
  case NODE_VAR_DECL:
    printf("(let %s ", node->as.var_decl.identifier);
    printAST(node->as.var_decl.initializer);
    printf(")");
    break;
  case NODE_ASSIGN:
    printf("(= ");
    printAST(node->as.assign.target);
    printf(" ");
    printAST(node->as.assign.value);
    printf(")");
    break;
  case NODE_INDEX:
    printf("(index ");
    printAST(node->as.index.array);
    printf(" ");
    printAST(node->as.index.index);
    printf(")");
    break;
  case NODE_CALL:
    printf("(call ");
    printAST(node->as.call.callee);
    for (int i = 0; i < node->as.call.argCount; i++) {
      printf(" ");
      printAST(node->as.call.args[i]);
    }
    printf(")");
    break;
  case NODE_IF:
    printf("(if ");
    printAST(node->as.if_stmt.condition);
    printf(" then ");
    printAST(node->as.if_stmt.thenBranch);
    if (node->as.if_stmt.elseBranch != NULL) {
      printf(" else ");
      printAST(node->as.if_stmt.elseBranch);
    }
    printf(")");
    break;
  case NODE_FUNCTION:
    printf("(function %s (", node->as.function.name);
    for (int i = 0; i < node->as.function.paramCount; i++) {
      if (i > 0)
        printf(" ");
      printf("%s", node->as.function.params[i]);
    }
    printf(") ");
    printAST(node->as.function.body);
    printf(")");
    break;
  case NODE_BLOCK:
    printf("(block");
    for (int i = 0; i < node->as.block.count; i++) {
      printf(" ");
      printAST(node->as.block.statements[i]);
    }
    printf(")");
    break;
  }
}
