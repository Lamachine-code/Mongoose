// ast.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
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

ASTNode* allocateFunctionDeclNode(const char* name, int length, const char** parameters, int paramCount, ASTNode* body) {
    ASTNode* node = NEW_NODE(NODE_FUNCTION_DECL);
    node->as.funcDecl.name = name;
    node->as.funcDecl.length = length;
    node->as.funcDecl.parameters = parameters; // Transfers ownership of the string pointer table
    node->as.funcDecl.paramCount = paramCount;
    node->as.funcDecl.body = body;
    return node;
}

ASTNode* allocateCallNode(const char* name, int length) {
    ASTNode* node = NEW_NODE(NODE_CALL);
    node->as.call.name = name;
    node->as.call.length = length;
    node->as.call.argCount = 0;
    node->as.call.argCapacity = 4; // Baseline initialization chunk
    node->as.call.arguments = (ASTNode**)malloc(sizeof(ASTNode*) * node->as.call.argCapacity);
    return node;
}

ASTNode* allocateIndexingNode(ASTNode* target, ASTNode* index) {
    ASTNode* node = NEW_NODE(NODE_INDEX);
    node->as.index_node.target = target;
    node->as.index_node.index = index;

    return node;
}

ASTNode* allocateReturnNode(ASTNode* value) {
    ASTNode* node = NEW_NODE(NODE_RETURN);
    node->as.return_node.value = value;

    return node;
}

ASTNode* allocateAssignNode(ASTNode* target, ASTNode* value) {
    ASTNode* node = NEW_NODE(NODE_ASSIGN);
    node->as.assign.target = target;
    node->as.assign.value = value;

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
        case NODE_FUNCTION_DECL:
            // 1. Free parameter name table (names point to zero-copy lexemes, do not free strings)
            if (node->as.funcDecl.parameters) {
                free(node->as.funcDecl.parameters);
            }
            // 2. Deeply clean up the body block subtree
            freeAST(node->as.funcDecl.body);
            break;

        case NODE_CALL:
            // 1. Walk through evaluated argument subtrees and free each recursively
            for (int i = 0; i < node->as.call.argCount; i++) {
                freeAST(node->as.call.arguments[i]);
            }
            // 2. Free the internal argument pointer vector table itself
            if (node->as.call.arguments) {
                free(node->as.call.arguments);
            }
            break;
        case NODE_INDEX:
            freeAST(node->as.index_node.target);
            freeAST(node->as.index_node.index);
            break;
        case NODE_RETURN:
            freeAST(node->as.return_node.value);
            break;
        case NODE_ASSIGN:
            freeAST(node->as.assign.target);
            freeAST(node->as.assign.value);
            break;
        case NODE_BOOL:
            break;
    }

    // Finally, free the current parent node
    free(node);
}

// Small utility function to assign precedence to our tokens
Precedence getPrecedence(TokenType type) {
	switch (type) {
        case TOKEN_ASSIGN:
            return PREC_ASSIGN;
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
        case TOKEN_LPAREN:
        case TOKEN_LBRACKET:
            return PREC_POSTFIX;
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


// Helper to print a consistent Mermaid edge
static void printMermaidEdge(FILE* fptr, void* parent, void* child) {
    fprintf(fptr, "    %p --> %p\n", parent, child);
}

// Helper to print a node declaration with its label
static void printMermaidNode(FILE* fptr, void* id, const char* fmt, ...) {
    va_list args;
    fprintf(fptr, "    %p[", id);       // print node ID and opening bracket
    va_start(args, fmt);                // initialize variable argument list
    vfprintf(fptr, fmt, args);          // print the formatted label
    va_end(args);                       // clean up
    fprintf(fptr, "]\n");               // close bracket and newline
}

// A cleaned up, safe, and highly readable recursive AST printer
static void genASTMermaidRecursive(FILE* fptr, ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_LITERAL: {
            if (isInteger(node->as.literal.value)) {
                printMermaidNode(fptr, node, "%g", node->as.literal.value);
            } else {
                printMermaidNode(fptr, node, "%.2f", node->as.literal.value);
            }
            break;
        }
        case NODE_BOOL: {
            printMermaidNode(fptr, node, "%s", node->as.boolean.value ? "true" : "false");
            break;
        }
        case NODE_IDENTIFIER: {
            printMermaidNode(fptr, node, "%.*s", node->as.identifier.length, node->as.identifier.name);
            break;
        }
        case NODE_UNARY_OP: {
            printMermaidNode(fptr, node, "%.*s", node->as.unary_op.length, node->as.unary_op.op);
            
            genASTMermaidRecursive(fptr, node->as.unary_op.operand);
            printMermaidEdge(fptr, node, node->as.unary_op.operand);
            break;
        }
        case NODE_VAR_DECL: {
            printMermaidNode(fptr, node, "let %.*s", node->as.var_decl.length, node->as.var_decl.identifier);
            
            genASTMermaidRecursive(fptr, node->as.var_decl.initializer);
            printMermaidEdge(fptr, node, node->as.var_decl.initializer);
            break;
        }
        case NODE_IF: {
            printMermaidNode(fptr, node, "IF");

            // Define and link Condition
            genASTMermaidRecursive(fptr, node->as.if_stmt.condition);
            printMermaidEdge(fptr, node, node->as.if_stmt.condition);

            // Define and link Then branch
            genASTMermaidRecursive(fptr, node->as.if_stmt.thenBranch);
            printMermaidEdge(fptr, node, node->as.if_stmt.thenBranch);

            // Define and link Else branch (if it exists)
            if (node->as.if_stmt.elseBranch) {
                genASTMermaidRecursive(fptr, node->as.if_stmt.elseBranch);
                printMermaidEdge(fptr, node, node->as.if_stmt.elseBranch);
            }
            break;
        }
        case NODE_BLOCK: {
            printMermaidNode(fptr, node, "BLOCK");
            for (int i = 0; i < node->as.block.count; i++) {
                genASTMermaidRecursive(fptr, node->as.block.statements[i]);
                printMermaidEdge(fptr, node, node->as.block.statements[i]);
            }
            break;
        }
        case NODE_FUNCTION_DECL: {
            printMermaidNode(fptr, node, "FUNC_DECL: %.*s", node->as.funcDecl.length, node->as.funcDecl.name);
            
            // Generate Parameters as a sub-list or individual nodes linked to the function decl
            for (int i = 0; i < node->as.funcDecl.paramCount; i++) {
                // We use a unique compound pointer identity for the parameter text shape
                void* paramId = (char*)node + i + 1; 
                printMermaidNode(fptr, paramId, "Param: %.*s", tokenLen(node->as.funcDecl.parameters[i]), node->as.funcDecl.parameters[i]);
                printMermaidEdge(fptr, node, paramId);
            }

            // Link the body
            if (node->as.funcDecl.body) {
                genASTMermaidRecursive(fptr, node->as.funcDecl.body);
                printMermaidEdge(fptr, node, node->as.funcDecl.body);
            }
            break;
        }
        case NODE_CALL: {
            // Print the function call node itself
            printMermaidNode(fptr, node, "CALL: %.*s", node->as.call.length, node->as.call.name);

            // Print each argument as a child node
            for (int i = 0; i < node->as.call.argCount; i++) {
                ASTNode* arg = node->as.call.arguments[i];
                if (arg) {
                    genASTMermaidRecursive(fptr, arg);
                    printMermaidEdge(fptr, node, arg);
                }
            }
            break;
        }
        case NODE_BINARY_OP: { // Explicitly named case instead of default
            const char* op_str = node->as.binary_op.op;
            int op_len = node->as.binary_op.length;

            if (op_len == 2 && op_str[0] == '/' && op_str[1] == '/') {
                printMermaidNode(fptr, node, "‎//");
            }
            else if (op_len == 1 && op_str[0] == '/') {
                printMermaidNode(fptr, node, "÷");
            } else if (op_len == 1 && op_str[0] == '%') {
                printMermaidNode(fptr, node, "％");
            } else if (op_str[0] == '%') {
                printMermaidNode(fptr, node, "%%"); // Mermaid requires %% to escape % signs safely
            } else {
                printMermaidNode(fptr, node, "%.*s", op_len, op_str);
            }

            genASTMermaidRecursive(fptr, node->as.binary_op.left);
            printMermaidEdge(fptr, node, node->as.binary_op.left);
            
            genASTMermaidRecursive(fptr, node->as.binary_op.right);
            printMermaidEdge(fptr, node, node->as.binary_op.right);
            break;
        }
        case NODE_INDEX: {
            printMermaidNode(fptr, node, "INDEX");

            // Print target (the array being indexed)
            genASTMermaidRecursive(fptr, node->as.index_node.target);
            printMermaidEdge(fptr, node, node->as.index_node.target);

            // Print index expression
            genASTMermaidRecursive(fptr, node->as.index_node.index);
            printMermaidEdge(fptr, node, node->as.index_node.index);
            break;
        }
        case NODE_RETURN: {
            printMermaidNode(fptr, node, "RETURN");
            genASTMermaidRecursive(fptr, node->as.return_node.value);
            printMermaidEdge(fptr, node, node->as.return_node.value);
            break;
        }
        case NODE_ASSIGN: {
            printMermaidNode(fptr, node, "=");
            genASTMermaidRecursive(fptr, node->as.assign.target);
            printMermaidEdge(fptr, node, node->as.assign.target);
            genASTMermaidRecursive(fptr, node->as.assign.value);
            printMermaidEdge(fptr, node, node->as.assign.value);
            break;
        }
        default:
            // Safe fallback so your compiler warning levels or missing features don't crash it
            printMermaidNode(fptr, node, "UNKNOWN_NODE_TYPE");
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

