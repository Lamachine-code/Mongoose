#include "../services/parserService.c"
#include "../services/astService.c"
#include <stdio.h>

//      ↓
// Ex: let total = (5 + 3) * 2
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
    } else {
        stmt = parseExpression(parser, PREC_NONE);
    }

    // Consume trailing newline (end of instruction)
    skipStatementSeparators(parser);

    return stmt;
}



ASTNode* parseExpression(Parser* parser, Precedence precedence) {

    // Token lhs_token = consumeParser(parser, TOKEN_NUMBER, "We were expecting a number.");
    Token lhs_token = advanceParser(parser);
    ASTNode* lhs = parsePrefix(parser, lhs_token);

    while (true)
    {
        Token op = peekParser(parser);
        int op_precedence = getPrecedence(op.type);
        if ( op.type == TOKEN_EOF || op_precedence <= precedence ) {
            break;
        }

        advanceParser(parser);
        ASTNode* rhs = parseExpression(parser, op_precedence);
        lhs = allocateBinaryOpNode(op.start, op.length, lhs, rhs);
    }

    return lhs;
}