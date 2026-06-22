// ast.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../utils/errorUtils.c"
#include "../types/ast.h"
#include "../types/lexer.h"

// Allocate memory to a node (ASTNode)
ASTNode* allocateLiteralNode(double value) {
    ASTNode* node = ensureAlloc((ASTNode*)malloc(sizeof(ASTNode)), "NODE_LITERAL");
    node->type = NODE_LITERAL;
    node->as.literal.value = value;
    return node;
}

ASTNode* allocateBinaryOpNode(const char* op, ASTNode* left, ASTNode* right) {
    ASTNode* node = ensureAlloc( (ASTNode*) malloc(sizeof(ASTNode)), "NODE_BINARY_OP");
    node->type = NODE_BINARY_OP;
    node->as.binary_op.op = op;
    node->as.binary_op.left = left;
    node->as.binary_op.right = right;
}

ASTNode* allocateVarDeclNode(const char* name, ASTNode* value) {
    ASTNode* node = ensureAlloc( (ASTNode*) malloc(sizeof(ASTNode)), "NODE_VAR_DECL");
    node->type = NODE_VAR_DECL;
    node->as.var_decl.identifier = name;
    node->as.var_decl.initializer = value;
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

// Debug helper to print the tree in a structural form
void printAST(ASTNode* node) {
    if (node->type == NODE_LITERAL) {
        printf("%g", node->as.literal.value);
        // printf(" ");
        // return;
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

