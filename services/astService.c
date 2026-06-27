// ast.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../types/ast.h"
#include "../types/lexer.h"
#include "../utils/errorUtils.c"
#include "../utils/fileUtils.c"
#include "../utils/charUtils.c"
#include "../utils/numberUtils.c"

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

ASTNode *allocateVarDeclNode(const char *name, int length, ASTNode *value) {
  ASTNode *node = ensureAlloc((ASTNode *)malloc(sizeof(ASTNode)), "NODE_VAR_DECL");
  node->type = NODE_VAR_DECL;
  node->as.var_decl.identifier = name;
  node->as.var_decl.length = length;
  node->as.var_decl.initializer = value;
  return node;
}

// Factory for Unary Node
ASTNode* allocateUnaryOpNode(const char* op, ASTNode* operand) {
    ASTNode* node = ensureAlloc((ASTNode*)malloc(sizeof(ASTNode)), "UNARY_OP");
    node->as.unary_op.op = op;  // (zero-copy)
    node->type = NODE_UNARY_OP;
    node->as.unary_op.operand = operand;
    return node;
}

void freeAST(ASTNode* node) {
    if (node == NULL) return;

    // Recursive cleanup based on the node's identity badge (the tag)
    switch (node->type) {
        case NODE_LITERAL:
            // No children or dynamically allocated internal pointers to free
            break;
        case NODE_BINARY_OP:
            freeAST(node->as.binary_op.left);
            freeAST(node->as.binary_op.right);
            break;
        case NODE_UNARY_OP:
            freeAST(node->as.unary_op.operand);
            break;
        case NODE_VAR_DECL:
            freeAST(node->as.var_decl.initializer);
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

// Ex:
//        *
//       / \
//     (-)  2
//      |
//      5
// Result: (* (- 5) 2)
// Debug helper to print the tree in a structural form
void printAST(ASTNode *node) {
    if (node == NULL)
        return;

    if (node->type == NODE_LITERAL) {
        if (isInteger(node->as.literal.value)) {
            printf("%.g", node->as.literal.value);        
        } else {
            printf("%.2f", node->as.literal.value);        
        }
    }
    else if (node->type == NODE_UNARY_OP) {
        printf("(%c", *(node->as.unary_op.op));
        printf(" ");
        printAST(node->as.unary_op.operand);
        printf(")");
    }
    else if (node->type == NODE_VAR_DECL) {
        printf("(let %.*s = ", node->as.var_decl.length, node->as.var_decl.identifier);
        printAST(node->as.var_decl.initializer);
        printf(")");
    }
    else {
        printf("(%c", *(node->as.binary_op.op));
        printf(" ");
        printAST(node->as.binary_op.left);
        printf(" ");
        printAST(node->as.binary_op.right);
        printf(")");
    }
}


// Print edge in Mermaid syntax
static void printMermaidEdge(FILE* fptr, ASTNode* parent, ASTNode* child) {
    fprintf(fptr, "%p --> %p\n", parent, child);
}

// A debugging tool that generates the tree structure as a Mermaid structural graph.
static void genASTMermaidRecursive(FILE* fptr, ASTNode* node) {
    if (node->type == NODE_LITERAL) {
        if (isInteger(node->as.literal.value)) {
            fprintf(fptr, "%p[%g]\n", node, node->as.literal.value);
        } else {
            fprintf(fptr, "%p[%.2f]\n", node, node->as.literal.value);
        }
    }
    else if (node->type == NODE_UNARY_OP) {
        fprintf(fptr, "%p[%c]\n", node, *(node->as.unary_op.op));
        genASTMermaidRecursive(fptr, node->as.unary_op.operand);    // declare child
        printMermaidEdge(fptr, node, node->as.unary_op.operand);
    }
    else if (node->type == NODE_VAR_DECL) {
        fprintf(fptr, "%p[let %.*s]\n", node, node->as.var_decl.length, node->as.var_decl.identifier);
        genASTMermaidRecursive(fptr, node->as.var_decl.initializer);
        printMermaidEdge(fptr, node, node->as.var_decl.initializer);
    } else {
        char operatorChar = *(node->as.binary_op.op);
        char* operatorString = charToString(operatorChar);
        fprintf(fptr, "%p[%s]\n", node, operatorChar == '/' ? "÷" : operatorString);
        free(operatorString);
        genASTMermaidRecursive(fptr, node->as.binary_op.left);
        printMermaidEdge(fptr, node, node->as.binary_op.left);
        genASTMermaidRecursive(fptr, node->as.binary_op.right);
        printMermaidEdge(fptr, node, node->as.binary_op.right);
    }
}

void genASTMermaidRep(ASTNode* node) {
    FILE* fptr;
    fptr = safeFOpen("build/AST.mermaid", "w");
    fprintf(fptr, "graph TD\n");

    genASTMermaidRecursive(fptr, node);

    fclose(fptr);
}

