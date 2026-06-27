#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../types/ast.h"
#include "../types/parser.h"

// Initialize the parser with the array of tokens provided by the Lexer
void initParser(Parser* parser, Token* tokens, int tokenCount) {
    parser->tokens = tokens;
    parser->tokenCount = tokenCount;
    parser->current = 0; // Always start at the first token (index 0)
}

// Peek at the current token without advancing the cursor
Token peekParser(Parser* parser) {
    // Safety: if we go past the array, return the last token (usually EOF)
    if (parser->current >= parser->tokenCount) {
        return parser->tokens[parser->tokenCount - 1];
    }
    return parser->tokens[parser->current];
}

// Check if the current token is of an expected type, without consuming it
bool checkParser(Parser* parser, TokenType type) {
    if (parser->current >= parser->tokenCount) return false;
    return peekParser(parser).type == type;
}

// =================================================================
// TODO: Complete the two functions below
// =================================================================

// Advance the cursor by one token and return the token that was passed.
// Be careful not to overflow the array (can't advance past tokenCount - 1).
// Parser golden rule for advance: "If I'm not at the end, increment, but in ALL cases return the token that was previously pointed to."
Token advanceParser(Parser* parser) {
    // 1. Save the currently pointed token
    Token t = peekParser(parser);
    
    // 2. Increment ONLY if there are still tokens to read
    if (parser->current < parser->tokenCount) {
        parser->current++;
    }
    
    // 3. Return the saved token
    return t;
}

// Expect a specific token type. If it's correct, consume it (advance).
// If it's not the expected type, print the error message and exit(1).
Token consumeParser(Parser* parser, TokenType type, const char* message) {
    // 1. Use checkParser to verify the current token matches the expected 'type'
    bool isValid = checkParser(parser, type);
    
    // 2. If yes: call advanceParser() and return its result
    if (isValid) {
        Token t = advanceParser(parser);
        return t;
    }
    // 3. If not: print the message with fprintf(stderr, ...) and call exit(1)
    // Properly write to the standard error stream (stderr)
    Token currentToken = peekParser(parser);
    fprintf(stderr, "Syntax error : %s (Line %d, Col %d) \n", message, currentToken.line, currentToken.col);
    exit(EXIT_FAILURE);

    // TODO: Implement the logic here
}

// Updated Prefix Handling supporting nested grouping constraints
// Intercepts the token stream, if the token is '(',
// it resets the priority by calling a 
// brand-new sub-expression with an initial priority of 
// zero: parseExpression(parser, 0). Once this sub-expression is 
// collected, it immediately requires a closing parenthesis ) 
// via our secure plumbing function consumeParser
ASTNode* parsePrefix(Parser* parser, Token token) {
    if (token.type == TOKEN_INT) {
        // Simple conversion wrapper logic
        double val = strtod(token.start, NULL);
        return allocateLiteralNode(val);

    } else if (token.type == TOKEN_MINUS) {
        ASTNode* operand = parseExpression(parser, PREC_UNARY);
        return allocateUnaryOpNode(token.start, operand);

    } else if (token.type == TOKEN_LPAREN) {
        // Isolate evaluation environment priority
        ASTNode* expression = parseExpression(parser, PREC_NONE);
        
        // Assertively close out grouping token window boundary
        consumeParser(parser, TOKEN_RPAREN, "Syntax error: Unbalanced statement expression, expected ')'.\n");
        return expression;
    }
    
    fprintf(stderr, "Parsing Error (Line %d, Col %d): Unexpected syntax initialization option parsed.\n", token.line, token.col);
    exit(EXIT_FAILURE);
    return NULL;
}

// Variable Declarations Compiler Layer
// Extract the identifier, consume the “=” and call parseExpression(parser, 0)
// to retrieve the tree of the initial value before validating the statement
// parseVarDecl() retrieves this final tree and wraps it in a VarDecl node. Mission accomplished!
ASTNode* parseVarDecl(Parser* parser) {
    // Consume 'let' keyword token
    consumeParser(parser, TOKEN_LET, "Expected 'let' statement identifier.\n");
    
    // Process and capture variable name via zero-copy architecture
    Token varToken = consumeParser(parser, TOKEN_IDENTIFIER, "Expected variable name identifier.\n");
    
    // Validate assignment operator symbol
    consumeParser(parser, TOKEN_ASSIGN, "Expected '=' assignment operator following variable name.\n");
    
    // Parse the subsequent value assignments
    ASTNode* initializer = parseExpression(parser, PREC_NONE);  // Equivalent to: parseExpression(parser, 0)
    
    // package up into our Tagged Union payload configuration
    return allocateVarDeclNode(varToken.start, varToken.length, initializer);
}
