#pragma once

#include <stdbool.h>
// #include "lexer.h" 
// #include "ast.h"

typedef Token Token;
typedef ASTNode ASTNode;
typedef TokenType TokenType;
typedef Precedence Precedence;

// The Parser structure that encapsulates the token stream
typedef struct {
    Token* tokens;      // Contiguous array of Tokens (zero-copy)
    int tokenCount;     // Total number of tokens in the array
    int current;        // Index of the token currently being analyzed
} Parser;

// Core parser utilities
// Parser plumbing functions
void initParser(Parser* parser, Token* tokens, int tokenCount);
Token peekParser(Parser* parser);
bool checkParser(Parser* parser, TokenType type);
Token advanceParser(Parser* parser);
Token consumeParser(Parser* parser, TokenType type, const char* message);

// Parsing functions
ASTNode* parseStatement(Parser* parser);
ASTNode* parseVarDecl(Parser* parser);
ASTNode* parseBlock(Parser* parser);
ASTNode* parseExpression(Parser* parser, Precedence precedence);