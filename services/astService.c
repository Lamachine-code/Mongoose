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
#include "../utils/stringUtils.c"

#define NEW_NODE(kind) \
    ( { ASTNode* n = malloc(sizeof(ASTNode)); \
        ensureAlloc(n, #kind); \
        n->type = kind; \
        n; } )


// Allocate memory to a node (ASTNode)
ASTNode *allocateLiteralNode(double value) {
  ASTNode *node =
      ensureAlloc((ASTNode *)malloc(sizeof(ASTNode)), "NODE_LITERAL");
  node->type = NODE_LITERAL;
  node->as.literal.value = value;
  return node;
}

ASTNode *allocateBinaryOpNode(const char *op, int length, ASTNode *left, ASTNode *right) {
  ASTNode *node =
      ensureAlloc((ASTNode *)malloc(sizeof(ASTNode)), "NODE_BINARY_OP");
  node->type = NODE_BINARY_OP;
	node->as.binary_op.length = length;
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
ASTNode* allocateUnaryOpNode(const char* op, int length, ASTNode* operand) {
    ASTNode* node = NEW_NODE(NODE_UNARY_OP);
    node->as.unary_op.op = op;  // (zero-copy)
    node->as.unary_op.length = length;
    node->as.unary_op.operand = operand;
    return node;
}

ASTNode* allocateBlockNode(void) {
    ASTNode* node = ensureAlloc((ASTNode*)malloc(sizeof(ASTNode)), "BlockNode");
    node->type = NODE_BLOCK;
    node->as.block.count = 0;
    node->as.block.capacity = 4; // Start with a small, reasonable baseline
    node->as.block.statements = (ASTNode**)malloc(sizeof(ASTNode*) * node->as.block.capacity);
    if (!node->as.block.statements) {
        fprintf(stderr, "Error: Memory allocation failed for block statements vector.\n");
        free(node);
        exit(EXIT_FAILURE);
    }
    return node;
}

ASTNode* allocateIfNode(ASTNode* condition, ASTNode* thenBranch, ASTNode* elseBranch) {
    ASTNode* node = ensureAlloc((ASTNode*)malloc(sizeof(ASTNode)), "IfNode");
    node->type = NODE_IF;
    node->as.if_stmt.condition = condition;
    node->as.if_stmt.thenBranch = thenBranch;
    node->as.if_stmt.elseBranch = elseBranch; // Can be NULL
    return node;
}

ASTNode* allocateIdentifierNode(Token token) {
    ASTNode* node = NEW_NODE(NODE_IDENTIFIER);
    node->as.identifier.name = token.start;
    node->as.identifier.length = token.length;
    return node;
}

ASTNode* allocateBoolNode(Token token) {
    ASTNode* node = NEW_NODE(NODE_BOOL);
    node->as.boolean.value = (*(token.start) == 't'); // true if starts with 't'
    return node;
}

static void freeBlockNode(ASTNode* blockNode) {
    // Step 1: Deeply clean every child expression/statement captured
    for (int i=0; i < blockNode->as.block.count; i++) {
        freeAST(blockNode->as.block.statements[i]);
    }
    // Step 2: Clear out the vector array table itself
    free(blockNode->as.block.statements);
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
        case NODE_IF:
            freeAST(node->as.if_stmt.condition);
            freeAST(node->as.if_stmt.thenBranch);
            if (node->as.if_stmt.elseBranch != NULL) {
                freeAST(node->as.if_stmt.elseBranch);
            }
            break;
        case NODE_IDENTIFIER:
            // No children or dynamically allocated internal pointers to free
            break;
        case NODE_BLOCK: {
            freeBlockNode(node);
            break;
        }
        case NODE_BOOL:
            break;
    }

    // Finally, free the current parent node
    free(node);
}

// Small utility function to assign precedence to our tokens
Precedence getPrecedence(TokenType type) {
	switch (type) {
        case TOKEN_AND:
        case TOKEN_OR:
            return PREC_LOGICAL_OP;
		case TOKEN_EQUAL:
		case TOKEN_NOTEQUAL:
			return PREC_EQUALITY;
        case TOKEN_GT:
        case TOKEN_LT:
        case TOKEN_GTEQ:
        case TOKEN_LTEQ:
            return PREC_COMP;
		case TOKEN_PLUS:
		case TOKEN_MINUS:
			return PREC_TERM;
		case TOKEN_STAR:
		case TOKEN_SLASH:
        case TOKEN_MODULO:
        case TOKEN_FLOOR_DIV:
			return PREC_FACTOR;
        case TOKEN_POWER:
            return PREC_POWER;
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

    switch (node->type) {

        case NODE_LITERAL:
            if (isInteger(node->as.literal.value)) {
                printf("%.g", node->as.literal.value);        
            } else {
                printf("%.2f", node->as.literal.value);        
            }
            break;
        case NODE_UNARY_OP:
            printf("(%.*s", node->as.unary_op.length, node->as.unary_op.op);
            printf(" ");
            printAST(node->as.unary_op.operand);
            printf(")");
            break;
        case NODE_VAR_DECL:
            printf("(let %.*s = ", node->as.var_decl.length, node->as.var_decl.identifier);
            printAST(node->as.var_decl.initializer);
            printf(")");
            break;
        case NODE_IDENTIFIER:
            printf("%.*s", node->as.identifier.length, node->as.identifier.name);
            break;
        default:
            printf("(%.*s", node->as.binary_op.length, node->as.binary_op.op);
            printf(" ");
            printAST(node->as.binary_op.left);
            printf(" ");
            printAST(node->as.binary_op.right);
            printf(")");
            break;
    }
}


// Print edge in Mermaid syntax
static void printMermaidEdge(FILE* fptr, ASTNode* parent, ASTNode* child) {
    fprintf(fptr, "%p --> %p\n", parent, child);
}

// A debugging tool that generates the tree structure as a Mermaid structural graph.
static void genASTMermaidRecursive(FILE* fptr, ASTNode* node) {
    switch (node->type) {
    
    case NODE_LITERAL:
        if (isInteger(node->as.literal.value)) {
            fprintf(fptr, "%p[%g]\n", node, node->as.literal.value);
        } else {
            fprintf(fptr, "%p[%.2f]\n", node, node->as.literal.value);
        }
        break;
    case NODE_UNARY_OP:
        fprintf(fptr, "%p[%.*s]\n", node, node->as.unary_op.length, node->as.unary_op.op);
        genASTMermaidRecursive(fptr, node->as.unary_op.operand);    // declare child
        printMermaidEdge(fptr, node, node->as.unary_op.operand);
        break;
    case NODE_VAR_DECL:
        fprintf(fptr, "%p[let %.*s]\n", node, node->as.var_decl.length, node->as.var_decl.identifier);
        genASTMermaidRecursive(fptr, node->as.var_decl.initializer);
        printMermaidEdge(fptr, node, node->as.var_decl.initializer);
        break;
	case NODE_IF:
		fprintf(fptr, "%p[IF]\n", node);
		fprintf(fptr, "%p[THEN]\n", node->as.if_stmt.thenBranch);
		printMermaidEdge(fptr, node, node->as.if_stmt.thenBranch);
		genASTMermaidRecursive(fptr, node->as.if_stmt.thenBranch);
		genASTMermaidRecursive(fptr, node->as.if_stmt.condition);
		printMermaidEdge(fptr, node, node->as.if_stmt.condition);
		if (node->as.if_stmt.elseBranch) {
			fprintf(fptr, "%p[ELSE]\n", node->as.if_stmt.elseBranch);
			printMermaidEdge(fptr, node, node->as.if_stmt.elseBranch);
			genASTMermaidRecursive(fptr, node->as.if_stmt.elseBranch);			
		}
        break;
	case NODE_BLOCK:
		for (int i=0; i < node->as.block.count; i++) {
			genASTMermaidRecursive(fptr, node->as.block.statements[i]);
			printMermaidEdge(fptr, node, node->as.block.statements[i]);
		}
        break;
    case NODE_IDENTIFIER:
        fprintf(fptr, "%p[%.*s]\n", node, node->as.identifier.length, node->as.identifier.name);
        break;
    case NODE_BOOL:
        fprintf(fptr, "%p[%s]\n", node, node->as.boolean.value ? "true": "false");
        break;
    default:
        const char* operator;
        switch (*(node->as.binary_op.op)) {
            case '/':
                if (node->as.binary_op.length == 2) {
                    operator = "‎//";
                    break;
                }
                operator = "÷";
                break;
            case '%':
                operator = "％";
                break;
            default:
                operator = node->as.binary_op.op;
                break;
        }
	    fprintf(fptr, "%p[%.*s]\n", node, tokenLen(operator), operator);

        genASTMermaidRecursive(fptr, node->as.binary_op.left);
        printMermaidEdge(fptr, node, node->as.binary_op.left);
        genASTMermaidRecursive(fptr, node->as.binary_op.right);
        printMermaidEdge(fptr, node, node->as.binary_op.right);
        break;
    }
}

void genASTMermaidRep(ASTNode* node) {
    FILE* fptr;
    fptr = safeFOpen("build/AST.mermaid", "w");
    fprintf(fptr, "graph TD\n");

    genASTMermaidRecursive(fptr, node);

    fclose(fptr);
}

