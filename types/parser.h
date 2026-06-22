#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h" 

// The Parser structure that encapsulates the token stream
typedef struct {
    Token* tokens;      // Contiguous array of Tokens (zero-copy)
    int tokenCount;     // Total number of tokens in the array
    int current;        // Index of the token currently being analyzed
} Parser;

// Parser plumbing functions
void initParser(Parser* parser, Token* tokens, int tokenCount);
Token peekParser(Parser* parser);
bool checkParser(Parser* parser, TokenType type);
Token advanceParser(Parser* parser);
Token consumeParser(Parser* parser, TokenType type, const char* message);

#endif // PARSER_H