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
    fprintf(stderr, "Parsing Error (Line %d, Col %d): %s\n", currentToken.line, currentToken.col, message);
    exit(EXIT_FAILURE);
}

// Updated Prefix Handling supporting nested grouping constraints
// Intercepts the token stream, if the token is '(',
// it resets the priority by calling a 
// brand-new sub-expression with an initial priority of 
// zero: parseExpression(parser, 0). Once this sub-expression is 
// collected, it immediately requires a closing parenthesis ) 
// via our secure plumbing function consumeParser
ASTNode* parsePrefix(Parser* parser, Token token) {// token = add

    switch (token.type) {

        case TOKEN_NUMBER:
            // Simple conversion wrapper logic
            double val = strtod(token.start, NULL);
            return allocateLiteralNode(val);

        case TOKEN_MINUS:
        case TOKEN_NOT:
            ASTNode* operand = parseExpression(parser, PREC_UNARY);
            return allocateUnaryOpNode(token.start, token.length, operand);

        case TOKEN_LPAREN:
            // Isolate evaluation environment priority
            ASTNode* expression = parseExpression(parser, PREC_NONE);
            // Assertively close out grouping token window boundary
            consumeParser(parser, TOKEN_RPAREN, "Parsing Error: Unbalanced statement expression, expected ')'.\n");
            return expression;

        case TOKEN_IDENTIFIER:
            return allocateIdentifierNode(token);

        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return allocateBoolNode(token);

        default:
            printf("%.*s", token.length, token.start);
            fprintf(stderr, "Parsing Error (Line %d, Col %d): Unexpected syntax initialization option parsed.\n", token.line, token.col);
            exit(EXIT_FAILURE);
            return NULL;
    }
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
    // The loop does not consume the STOP and ELSE tokens
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
            
            /* Handle dynamic resizing of blockNode->as.block.statements */
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
        blockNode->as.block.statements = ensureAlloc(temp, "Reallocation block statements");
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

ASTNode* parseInfixIndexing(Parser* parser, ASTNode* target) {
    ASTNode* indexNode = parseExpression(parser, PREC_NONE);
    consumeParser(parser, TOKEN_RBRACKET, "Expected ')' to close indexing.");

    return allocateIndexingNode(target, indexNode);
}

ASTNode* parseInfix(Parser* parser, ASTNode* lhs, Token operator, int op_precedence) {
        // 4. Dispatch based on the infix operator type
        switch (operator.type) {
            case TOKEN_LPAREN: {
                // At this point, we can already be sure that “operator.type” is of type TOKEN_LPAREN, so there's no need to check in parseInfixCall.
                // Because TOKEN_LPAREN was advanced over, parseInfixCall immediately parses arguments!
                return parseInfixCall(parser, lhs);
            }
            case TOKEN_LBRACKET:
                // At this point, we can already be sure that “operator.type” is of type TOKEN_LBRACKET, so there's no need to check in parseInfixIndexing.
                return parseInfixIndexing(parser, lhs);
            default: {
                // Standard binary tracking arithmetic (+, -, *, /)
                ASTNode* rhs = parseExpression(parser, op_precedence);
                return allocateBinaryOpNode(operator.start, operator.length, lhs, rhs);
            }            
        }

}

ASTNode* parseFunctionDecl(Parser* parser) {
    consumeParser(parser, TOKEN_FUNCTION, "Expected 'function' keyword.");
    
    // Zero-copy tracking: extract target name directly from identifier token lifetime
    Token token = consumeParser(parser, TOKEN_IDENTIFIER, "Expected function name.");
    const char* funcName = token.start;

    consumeParser(parser, TOKEN_LPAREN, "Expected '(' after function name.");

    // Parse parameters
    int paramCount = 0;
    int paramCapacity = 4;
    //                                               ⬇
    // Ex: function fillEvens(arr, index, max, value)
    const char** parameters = (const char**)malloc(sizeof(const char*) * paramCapacity);

    if (!checkParser(parser, TOKEN_RPAREN)) {   // check if the function exepects parameter(s). Ex: « function print() » vs « function add(a, b) »
        do {
            // Consume one param
            // Since we've already verified that the function expects at least one parameter
            Token paramToken = consumeParser(parser, TOKEN_IDENTIFIER, "Expected parameter name.");
            
            // Dynamic parameter array resize loop
            if (paramCount >= paramCapacity) {
                paramCapacity *= 2;
                parameters = (const char**)realloc(parameters, sizeof(const char*) * paramCapacity);
            }
            // Add the parameter to the list of params of the node
            parameters[paramCount++] = paramToken.start; // zero-copy pointer link

            if (checkParser(parser, TOKEN_COMMA)) {
                advanceParser(parser); // Consume comma
            } else {
                break;
            }
        } while (true);
    }
    
    consumeParser(parser, TOKEN_RPAREN, "Expected ')' after parameter list.");
    
    // Parse the function body block (scoped between parameter list and closing 'stop')
    // “parseBlock” does not consume the stop token of the block in which it is called
    ASTNode* body = parseBlock(parser); // Continues reading statements until TOKEN_STOP
    
    consumeParser(parser, TOKEN_STOP, "Expected 'stop' to seal function structure.");

    return allocateFunctionDeclNode(funcName, token.length, parameters, paramCount, body);
}

ASTNode* parseInfixCall(Parser* parser, ASTNode* left) {
    // Safety check: The left-hand expression node must be an identifier to be callable
    if (left->type != NODE_IDENTIFIER) { 
        // Handle gracefully: error report or fallback
        fprintf(stderr, "Syntax Error: Left-hand side of call is not a callable identifier.\n");
        return left;
    }
    
    const char* funcName = left->as.identifier.name;
    int length = left->as.identifier.length;
    
    // Recycle the left node container safely
    free(left);
    
    // Allocate call node container 
    ASTNode* callNode = allocateCallNode(funcName, length);

    // If we are here, '(', which triggered this infix rule, has ALREADY been advanced over!
    // We check if the function expects argument(s). Ex: "print()" vs "add(a, b)"
    if (!checkParser(parser, TOKEN_RPAREN)) {
        do {
            // Parse argument expression at the lowest precedence level (PREC_NONE / 0)
            ASTNode* argument = parseExpression(parser, PREC_NONE); 
            
            // Dynamic capacity growth
            if (callNode->as.call.argCount >= callNode->as.call.argCapacity) {
                callNode->as.call.argCapacity *= 2;
                callNode->as.call.arguments = (ASTNode**)realloc(
                    callNode->as.call.arguments, 
                    sizeof(ASTNode*) * callNode->as.call.argCapacity
                );
            }
            
            callNode->as.call.arguments[callNode->as.call.argCount++] = argument;

            if (checkParser(parser, TOKEN_COMMA)) {
                advanceParser(parser); // Consume ',' and move to next parameter token
            } else {
                break;
            }
        } while (true);
    }

    consumeParser(parser, TOKEN_RPAREN, "Expected ')' to close function argument scope.");
    return callNode;
}