#include "../services/parserService.c"
#include "../services/astService.c"
#include <stdio.h>

// High-level discriminator function
// If it encounters the keyword “let” (token TOKEN_LET), it calls the parseVarDecl() function
// If it does not find a statement keyword, it treats the line as a simple expression.
ASTNode* parseStatement(Parser* parser) {
    // Skip leading newlines
    skipStatementSeparators(parser);

    ASTNode* stmt = NULL;
    if (checkParser(parser, TOKEN_LET)) {
        stmt = parseVarDecl(parser);
    } else if (checkParser(parser, TOKEN_IF)) {
        stmt = parseIf(parser);
    } else if (checkParser(parser, TOKEN_FUNCTION)) {
        stmt = parseFunctionDecl(parser);
    }else if (checkParser(parser, TOKEN_RETURN)) {
        stmt = parseReturnStmt(parser);
    } else {
        stmt = parseExpression(parser, PREC_NONE);
    }

    // Consume trailing newline (end of instruction)
    skipStatementSeparators(parser);

    return stmt;
}

ASTNode* parseExpression(Parser* parser, Precedence precedence) {
    Token lhs_token = advanceParser(parser);
    ASTNode* lhs = parsePrefix(parser, lhs_token);

    while (true) {
        Token op = peekParser(parser);
        
        // 1. Determine priority of current lookahead token
        Precedence op_precedence = getPrecedence(op.type);

        // 2. If the next operator has lower or equal precedence, we stop chaining LHS
        if (op.type == TOKEN_EOF || op_precedence <= precedence) {
            break;
        }

        // 3. Now we safely consume the operator token 
        advanceParser(parser);

        lhs = parseInfix(parser, lhs, op, op_precedence);
    }

    return lhs;
}