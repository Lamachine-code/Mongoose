#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../types/ast.h"
#include "../types/parser.h"
#include "../types/lexer.h"

void *ensureAlloc(void *ptr, const char *errorMsg);

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
    if (token.type == TOKEN_NUMBER) {
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
        consumeParser(parser, TOKEN_RPAREN, "Parsing Error: Unbalanced statement expression, expected ')'.\n");
        return expression;

    } else if (token.type == TOKEN_IDENTIFIER) {
        return allocateIdentifierNode(token);
    }

    fprintf(stderr, "Parsing Error (Line %d, Col %d): Unexpected syntax initialization option parsed.\n", token.line, token.col);
    exit(EXIT_FAILURE);
    return NULL;
}

void skipStatementSeparators(Parser* parser) {
    while(peekParser(parser).type == TOKEN_NEWLINE) {
        advanceParser(parser);
    }
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

/**
 * Sequential block reader architecture
 */
ASTNode* parseBlock(Parser* parser) {
    ASTNode* blockNode = allocateBlockNode();
    
    // The sequence loop continues parsing statements sequentially until it bumps
    // into either an 'else' boundary or a terminating 'stop' token.
    while (!checkParser(parser, TOKEN_STOP) && !checkParser(parser, TOKEN_ELSE)) {
        // Safety lock check: stop infinite tracking loops if file EOF is breached
        if (checkParser(parser, TOKEN_EOF)) {
            fprintf(stderr, "Parsing Error (Line %d, Col %d): Unexpected EOF reached inside unclosed statement block.\n", peekParser(parser).line, peekParser(parser).col);
            freeAST(blockNode);
            exit(1);
        }
        
        // Parse the nested sequential statement tree node
        ASTNode* stmt = parseStatement(parser);
        if (stmt != NULL) {
            
            /* TODO: Handle dynamic resizing of blockNode->as.block.statements */
            /* Track calculations: if (count == capacity) { double and realloc } */
            if (blockNode->as.block.count == blockNode->as.block.capacity) {
                // if statements is complete
                blockNode->as.block.capacity *= 2;
                ASTNode** temp = realloc(blockNode->as.block.statements, blockNode->as.block.capacity * sizeof(ASTNode*));
                ensureAlloc(temp, "Reallocation block statements");
                blockNode->as.block.statements = temp;
            }
            
            // Append parsed tree node reference into statements array
            blockNode->as.block.statements[blockNode->as.block.count++] = stmt;
        }
    }
    
    if (blockNode->as.block.count > 0) {
        ASTNode** temp = realloc(blockNode->as.block.statements, blockNode->as.block.count * sizeof(ASTNode*));
        ensureAlloc(temp, "Reallocation block statements");
        blockNode->as.block.statements = temp;
        blockNode->as.block.capacity = blockNode->as.block.count;
    } else {
        // If the block is completely empty, free the unused buffer array 
        // and explicitly anchor it to NULL.
        free(blockNode->as.block.statements);
        blockNode->as.block.statements = NULL;
        blockNode->as.block.capacity = 0;
    }

    return blockNode;
}

/**
 * Conditional branching parser workflow skeleton
 */
ASTNode* parseIf(Parser* parser) {
    // Step 1: Consume the leading TOKEN_IF keyword assertively
    consumeParser(parser, TOKEN_IF, "Expected 'if' keyword to initiate branch layout.");
    
    // Step 2: Parse the evaluation conditional criteria expression tree
    ASTNode* condition = parseExpression(parser, 0);
    
    // Step 3: Require explicit 'then' pairing structural keyword marker
    consumeParser(parser, TOKEN_THEN, "Expected 'then' keyword following execution condition check.");
    
    // Step 4: Extract statements block for the positive path
    ASTNode* thenBranch = parseBlock(parser);
    
    ASTNode* elseBranch = NULL;
    
    // Step 5: Check if lookahead pointer is identifying an 'else' optional path
    if (checkParser(parser, TOKEN_ELSE)) {
        // Advance past TOKEN_ELSE token
        advanceParser(parser); 
        
        // Recurse into block compiler extraction loops for negative path
        elseBranch = parseBlock(parser);
    }
    
    // Step 6: Guarantee scope context enclosure by verifying closing 'stop' keyword
    consumeParser(parser, TOKEN_STOP, "Expected matching structural 'stop' boundary keyword to seal condition block.");
    
    // Step 7: Factory structural parameters and forward node tracking upward
    return allocateIfNode(condition, thenBranch, elseBranch);
}
